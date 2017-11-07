/*
 * Copyright (c) 2017 TOYOTA MOTOR CORPORATION
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <stdarg.h>
#include <sys/socket.h>
#include <iostream>
#include <algorithm>
#include "wrap-json.h"
#include "ahl-interface.h"
#include "wsclient-audio4a.hpp"

#define ELOG(args,...) _ELOG(__FUNCTION__,__LINE__,args,##__VA_ARGS__)
#ifdef DEBUGMODE
#define DLOG(args,...) _DLOG(__FUNCTION__,__LINE__,args,##__VA_ARGS__)
#else
#define DLOG(args,...)
#endif
static void _DLOG(const char* func, const int line, const char* log, ...);
static void _ELOG(const char* func, const int line, const char* log, ...);

using namespace std;

static bool has_verb(const std::string& verb);
static const char API[] = "ahl4a"; // audio-4a high level API

static const std::vector<std::string> api_list{
    std::string("stream_open"),
    std::string("stream_close"),
    std::string("get_endpoints"),
    std::string("set_stream_state"),
    std::string("get_stream_info"),
    std::string("volume"),
    std::string("get_endpoint_info"),
    std::string("property"),
    std::string("event_subscription"),
    };

static const std::vector<std::string> event_list{
    std::string("asyncSetSourceState"),
    std::string("newMainConnection"),
    std::string("volumeChanged"),
    std::string("removedMainConnection"),
    std::string("sinkMuteStateChanged"),
    std::string("mainConnectionStateChanged"),
    std::string("setRoutingReady"),
    std::string("setRoutingRundown"),
    std::string("asyncConnect")};

static void _on_hangup_static(void *closure, struct afb_wsj1 *wsj) {
    static_cast<WsClientAudio4a*> (closure)->on_hangup(NULL, wsj);
}

static void _on_call_static(void *closure, const char *api, const char *verb, struct afb_wsj1_msg *msg) {
    /* WsClientAudio4a is not called from other process */
}

static void _on_event_static(void* closure, const char* event, struct afb_wsj1_msg *msg) {
    static_cast<WsClientAudio4a*> (closure)->on_event(NULL, event, msg);
}

static void _on_reply_static(void *closure, struct afb_wsj1_msg *msg) {
    static_cast<WsClientAudio4a*> (closure)->on_reply(NULL, msg);
}

WsClientAudio4a::WsClientAudio4a() {
}

WsClientAudio4a::~WsClientAudio4a() {
    if (mploop) {
        sd_event_unref(mploop);
    }
    if (sp_websock != NULL) {
        afb_wsj1_unref(sp_websock);
    }
}

/**
 * This function is initialization function
 *
 * #### Parameters
 * - port  [in] : This argument should be specified to the port number to be used for websocket
 * - token [in] : This argument should be specified to the token to be used for websocket
 *
 * #### Return
 * Returns 0 on success or -1 in case of transmission error.
 *
 * #### Note
 *
 */
int WsClientAudio4a::init(int port, const string& token) {
    int ret;
    if (port > 0 && token.size() > 0) {
        mport = port;
        mtoken = token;
    } else {
        ELOG("port and token should be > 0, Initial port and token uses.");
        return -1;
    }

    ret = initialize_websocket();
    if (ret != 0) {
        ELOG("Failed to initialize websocket");
        return -1;
    }
    ret = init_event();
    if (ret != 0) {
        ELOG("Failed to initialize websocket");
        return -1;
    }
    return 0;
}

int WsClientAudio4a::initialize_websocket() {
    mploop = NULL;
    onEvent = nullptr;
    onReply = nullptr;
    int ret = sd_event_default(&mploop);
    if (ret < 0) {
        ELOG("Failed to create event loop");
        goto END;
    }
    /* Initialize interface from websocket */
    {
        minterface.on_hangup = _on_hangup_static;
        minterface.on_call = _on_call_static;
        minterface.on_event = _on_event_static;
        string muri = "ws://localhost:" + to_string(mport) + "/api?token=" + mtoken;
        sp_websock = afb_ws_client_connect_wsj1(mploop, muri.c_str(), &minterface, this);
    }
    if (sp_websock == NULL) {
        ELOG("Failed to create websocket connection");
        goto END;
    }

    return 0;
END:
    if (mploop) {
        sd_event_unref(mploop);
    }
    return -1;
}

int WsClientAudio4a::init_event() {
    /* subscribe most important event for sound right */
    return subscribe(string("asyncSetSourceState"));
}

/**
 * This function register callback function for reply/event message from sound manager
 *
 * #### Parameters
 * - event_cb  [in] : This argument should be specified to the callback for subscribed event



 * - reply_cb  [in] : This argument should be specified to the reply callback for call function
 * - hangup_cb [in] : This argument should be specified to the hangup callback for call function. nullptr is defaulty set.
 *
 * #### Return
 *
 * #### Note
 * Event callback is invoked by sound manager for event you subscribed.
 * If you would like to get event, please call subscribe function before/after this function
 */
void WsClientAudio4a::register_callback(
        void (*event_cb)(const string& event, struct json_object* event_contents),
        void (*reply_cb)(struct json_object* reply_contents),
        void (*hangup_cb)(void)) {
    onEvent = event_cb;
    onReply = reply_cb;
    onHangup = hangup_cb;
}

/**
 * This function is overload of register_callback. This registers callback function for reply/event message from sound manager
 *
 * #### Parameters
 * - reply_cb [in] : This argument should be specified to the reply callback for call function
 * - hangup_cb [in] : This argument should be specified to the hangup callback for call function. nullptr is defaulty set.
 *
 * #### Return
 *
 * #### Note
 * Event callback is invoked by sound manager for event you subscribed.
 * This function for convinience for user uses set_event_handler
 * If you would like to get event, please call subscribe function before/after this function
 */
void WsClientAudio4a::register_callback(
        void (*reply_cb)(struct json_object* reply_contents),
        void (*hangup_cb)(void)) {
    onReply = reply_cb;
    onHangup = hangup_cb;
}

/**
 * This function calls registerSource of Audio Manager via WebSocket
 * registerSource is registration as source for policy management
 *
 * #### Parameters
 * - sourceName [in] : This argument should be specified to the source name (e.g. "MediaPlayer")
 *
 * #### Return
 * - Returns 0 on success or -1 in case of transmission error.
 *
 * #### Note
 * This function must be called to get source ID
 * mainConnectionID is returned by async reply function
 *
 */
int WsClientAudio4a::registerSource(const string& sourceName) {
    if (!sp_websock) {
        return -1;
    }
    struct json_object* j_obj = json_object_new_object();
    struct json_object* jsn = json_object_new_string(sourceName.c_str());
    json_object_object_add(j_obj, "appname", jsn);
    return this->call(__FUNCTION__, j_obj);
}

/**
 * This function calls connect of Audio Manager via WebSocket
 * connect is to get sound right
 *
 * #### Parameters
 * - sourceID [in] : This argument should be specified to the sourceID as int. This parameter is returned value of registerSource
 * - sinkID   [in] : This argument should be specified to the sinkID as int. ID is specified by AudioManager
 *
 * #### Return
 * - Returns 0 on success or -1 in case of transmission error.
 *
 * #### Note
 * This function must be called to get source right
 * connectionID is returned by reply event
 *
 */
int WsClientAudio4a::stream_open(const string& audioRole, const string& endPointString, const int endpointID) {

    if (!sp_websock) return -1;

    json_object* j_obj;

    int err = wrap_json_pack(&j_obj, "{s:s,s:s}", "audio_role", audioRole.c_str(), "endpoint_type", endPointString.c_str(), "endpoint_id", endpointID);
    if (err) return -1;

    return this->call(__FUNCTION__, j_obj);

}

/**
 * This function calls connect of Audio Manager via WebSocket
 * connect is to get sound right
 *
 * #### Parameters
 * - audioRole [in] : Audio Role as defined within audio-4a Configuration
 * - endPointType [in] : Either AUDIO4A_ENDPOINT_SINK or AUDIO4A_ENDPOINT_SOURCE
 *
 * #### Return
 * - Returns 0 on success or -1 in case of transmission error.
 *
 * #### Note
 * This function must be called to get source right
 * connectionID is returned by reply event
 *
 */
int WsClientAudio4a::stream_open(const string& audioRole, EndPointType4aT endPointType, const int endpointID) {

    if (!sp_websock) return -1;

    const char* endPointEnum;
    switch (endPointType) {
        case AUDIO4A_ENDPOINT_SINK:
            endPointEnum = AHL_ENDPOINTTYPE_SINK;
            break;
            
        case AUDIO4A_ENDPOINT_SOURCE:
            endPointEnum = AHL_ENDPOINTTYPE_SOURCE;
            break;

        default: return -1;
    }

    json_object* j_obj = json_object_new_object();
    int err = wrap_json_pack(&j_obj, "{s:s,s:s,s:i*}", "audio_role",  audioRole.c_str(), "endpoint_type", endPointEnum, "endpoint_id", endpointID);
    if (err) return -1;
    
    return this->call(__FUNCTION__, j_obj);
}


/**
 * This function calls the disconnect of Audio Manager via WebSocket
 *
 * #### Parameters
 * - connectionID  [in] : This parameter is returned value of connect
 *
 * #### Return
 * - Returns 0 on success or -1 in case of transmission error.
 *
 * #### Note
 *
 *
 */
int WsClientAudio4a::stream_close(int streamID) {

    if (!sp_websock) return -1;

    json_object* j_obj = json_object_new_object();
    int err = wrap_json_pack(&j_obj, "{i*}", streamID);
    if (err) return -1;
    return this->call(__FUNCTION__, j_obj);
}

/**
 * This function calls the set_stream_state of Audio Manager via WebSocket
 *
 * #### Parameters
 * - sourceID  [in] : is NULL change all stream for current client, otherwise change on one stream
 * - state     [in] : idle / idle
 * - mute      [in] : true / false 
 *
 * #### Return
 * - Returns 0 on success or -1 in case of transmission error.
 *
 * #### Note
 * This function must be called when application get asyncSetSourceState event
 * Input handle number attached in asyncSetSourceState and error number(0 is acknowledge)
 */
int WsClientAudio4a::set_stream_state(int streamID, const string& state, const bool mute) {
    if (!sp_websock) return -1;
    
    json_object* j_obj = json_object_new_object();
    int err = wrap_json_pack(&j_obj, "{s:i*, s:s, s:b*}", "stream_id", streamID, "state", state, "mute", mute);
    if (err) return -1;
    
    return this->call(__FUNCTION__, j_obj);
}

/**
 * This function calls the API of Audio Manager via WebSocket
 *
 * #### Parameters
 * - verb [in] : This argument should be specified to the API name (e.g. "connect")
 * - arg  [in] : This argument should be specified to the argument of API. And this argument expects JSON object
 *
 * #### Return
 * - Returns 0 on success or -1 in case of transmission error.
 *
 * #### Note
 * To call Audio Manager's APIs, the application should set its function name, arguments to JSON format.
 *
 */
int WsClientAudio4a::call(const string& verb, struct json_object* arg) {
    int ret;
    if (!sp_websock) {
        return -1;
    }
    if (!has_verb(verb)) {
        ELOG("verb doesn't exit");
        return -1;
    }
    ret = afb_wsj1_call_j(sp_websock, API, verb.c_str(), arg, _on_reply_static, this);
    if (ret < 0) {
        ELOG("Failed to call verb:%s", verb.c_str());
    }
    return ret;
}

/**
 * This function calls the API of Audio Manager via WebSocket
 * This function is overload function of "call"
 *
 * #### Parameters
 * - verb [in] : This argument should be specified to the API name (e.g. "connect")
 * - arg  [in] : This argument should be specified to the argument of API. And this argument expects JSON object
 *
 * #### Return
 * - Returns 0 on success or -1 in case of transmission error.
 *
 * #### Note
 * To call Audio Manager's APIs, the application should set its function name, arguments to JSON format.
 *
 */
int WsClientAudio4a::call(const char* verb, struct json_object* arg) {
    int ret;
    if (!sp_websock) {
        return -1;
    }
    if (!has_verb(string(verb))) {
        ELOG("verb doesn't exit");
        return -1;
    }
    ret = afb_wsj1_call_j(sp_websock, API, verb, arg, _on_reply_static, this);
    if (ret < 0) {
        ELOG("Failed to call verb:%s", verb);
    }
    return ret;
}

/**
 * Register callback function for each event
 *
 * #### Parameters
 * - event_name [in] : This argument should be specified to the event name
 *
 * #### Return
 * - Returns 0 on success or -1 in case of transmission error.
 *
 * #### Note
 * This function enables to get an event to your callback function.
 * Regarding the list of event name, please refer to CommandSender API and RountingSender API.
 *
 */
int WsClientAudio4a::subscribe(const string& event_name) {

    if (!sp_websock) return -1;

    json_object* j_obj = json_object_new_object();
    int err = wrap_json_pack(&j_obj, "{[s],s:i}", event_name, "subscribe", 1);
    if (err) return -1;

    return this->call("event_subscription", j_obj);
}

/**
 * Unregister callback function for each event
 *
 * #### Parameters
 * - event_name [in] : This argument should be specified to the event name
 *
 * #### Return
 * - Returns 0 on success or -1 in case of transmission error.
 *
 * #### Note
 * This function disables to get an event to your callback function.
 *
 */
int WsClientAudio4a::unsubscribe(const string& event_name) {

    if (!sp_websock) return -1;

    json_object* j_obj = json_object_new_object();
    int err = wrap_json_pack(&j_obj, "{[s],s:i}", event_name, "subscribe", 0);
    if (err) return -1;

    return this->call("event_subscription", j_obj);
}

/**
 * This function calls the set_stream_state of Audio Manager via WebSocket
 *
 * #### Parameters
 * - EventType_AsyncSetSourceState     [in] : This parameter is EventType of soundmanager
 * - handler_func  [in] : This parameter is callback function
 *
 * #### Return
 * - Returns 0 on success or -1 in case of transmission error.
 *
 * #### Note
 * This function must be called when application get asyncSetSourceState event
 * Input handle number attached in asyncSetSourceState and error number(0 is acknowledge)
 */
void WsClientAudio4a::set_event_handler(enum EventType_SM et, handler_fun f) {
    if (et > 1 && et < NumItems) {
        this->handlers[et] = std::move(f);
    }
}

/************* Callback Function *************/

void WsClientAudio4a::on_hangup(void *closure, struct afb_wsj1 *wsj) {
    DLOG("%s called", __FUNCTION__);
    if (onHangup != nullptr) {
        onHangup();
    }
}

void WsClientAudio4a::on_call(void *closure, const char *api, const char *verb, struct afb_wsj1_msg *msg) {
}

/*
 * event is like "soundmanager/newMainConnection"
 * msg is like {"event":"soundmanager\/newMainConnection","data":{"mainConnectionID":3,"sourceID":101,"sinkID":100,"delay":0,"connectionState":4},"jtype":"afb-event"})}
 *               ^key^   ^^^^^^^^^^^^ value ^^^^^^^^^^^^
 * so you can get
        event name : struct json_object obj = json_object_object_get(msg,"event")
 */
void WsClientAudio4a::on_event(void *closure, const char *event, struct afb_wsj1_msg *msg) {
    /* check event is for us */
    string ev = string(event);
    if (ev.find(API) == string::npos) {
        /* It's not us */
        return;
    }
    struct json_object* ev_contents = afb_wsj1_msg_object_j(msg);
    if ((onEvent != nullptr)) {
        onEvent(ev, ev_contents);
    } else {
    }

    dispatch_event(ev, ev_contents);

    json_object_put(ev_contents);
}

void WsClientAudio4a::on_reply(void *closure, struct afb_wsj1_msg *msg) {
    struct json_object* reply = afb_wsj1_msg_object_j(msg);
    /*struct json_object *json_data = json_object_object_get(reply, "response");
    struct json_object *jverb = json_object_object_get(json_data, "verb");
    const char* cverb = json_object_get_string(jverb);
    DLOG("cverb is %s",cverb);
    string verb = string(cverb);
    DLOG("verb is %s",verb.c_str());

    if(verb == "registerSource"){
            struct json_object *jsourceID = json_object_object_get(json_data, "sourceID");
            int sourceID = json_object_get_int(jsourceID);
            msourceIDs.push_back(sourceID);
            DLOG("my sourceID is created: %d", sourceID);
    }*/
    if (onReply != nullptr) {
        onReply(reply);
    }
    json_object_put(reply);
}

int WsClientAudio4a::dispatch_event(const string &event, json_object* event_contents) {
    //dipatch event
    EventType_SM x;

    if (event.find(event_list[0].c_str())) {
        x = Event_AsyncSetSourceState;
    } else {
        return -1;
    }
    auto i = this->handlers.find(x);
    if (i != handlers.end()) {
        i->second(event_contents);
        return 0;
    } else {
        return -1;
    }
}

/* Internal Function in libsoundmanager */

static void _ELOG(const char* func, const int line, const char* log, ...) {
    char *message;
    va_list args;
    va_start(args, log);
    if (log == NULL || vasprintf(&message, log, args) < 0)
        message = NULL;
    cout << "[ERROR: soundmanager]" << func << "(" << line << "):" << message << endl;
    va_end(args);
    free(message);
}

static void _DLOG(const char* func, const int line, const char* log, ...) {
    char *message;
    va_list args;
    va_start(args, log);
    if (log == NULL || vasprintf(&message, log, args) < 0)
        message = NULL;
    cout << "[DEBUG: soundmanager]" << func << "(" << line << "):" << message << endl;
    va_end(args);
    free(message);
}

static bool has_verb(const string& verb) {
    if (find(api_list.begin(), api_list.end(), verb) != api_list.end())
        return true;
    else
        return false;
}
