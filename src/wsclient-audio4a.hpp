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

#ifndef LIBSOUNDMANAGER_H
#define LIBSOUNDMANAGER_H
#include <vector>
#include <map>
#include <string>
#include <functional>
#include <json-c/json.h>
#include <systemd/sd-event.h>
extern "C"
{
#include <afb/afb-wsj1.h>
#include <afb/afb-ws-client.h>
}

typedef enum {
  AUDIO4A_ENDPOINT_SINK,
  AUDIO4A_ENDPOINT_SOURCE,
    
} EndPointType4aT;

class WsClientAudio4a
{
public:
    WsClientAudio4a();
    ~WsClientAudio4a();
    WsClientAudio4a(const WsClientAudio4a &) = delete;
    WsClientAudio4a &operator=(const WsClientAudio4a &) = delete;
    int init(int port, const std::string& token);

    using handler_fun = std::function<void(struct json_object*)>;

    enum EventType_SM {
       Event_AsyncSetSourceState = 1    /*arg key: {sourceID, handle, sourceState}*/
    };

    /* Method */
    int registerSource(const std::string& sourceName);
    int stream_open(const std::string& audioRole, EndPointType4aT endPointType, const int endpointID);
    int stream_open(const std::string& audioRole, const std::string& endPointString, const int endpointID);
    int connect(int sourceID, const std::string& sinkName);
    int stream_close(int streamID);

    int set_stream_state(int streamID, const std::string& state, const bool mute);

    int call(const std::string& verb, struct json_object* arg);
    int call(const char* verb, struct json_object* arg);
    int subscribe(const std::string& event_name);
    int unsubscribe(const std::string& event_name);
    void set_event_handler(enum EventType_SM et, handler_fun f);
    
    void register_callback(
        void (*event_cb)(const std::string& event, struct json_object* event_contents),
        void (*reply_cb)(struct json_object* reply_contents),
        void (*hangup_cb)(void) = nullptr);
    void register_callback(
        void (*reply_cb)(struct json_object* reply_contents),
        void (*hangup_cb)(void) = nullptr);

private:
    int init_event();
    int initialize_websocket();
    int dispatch_event(const std::string& event, struct json_object* ev_contents);

    void (*onEvent)(const std::string& event, struct json_object* event_contents);
    void (*onReply)(struct json_object* reply);
    void (*onHangup)(void);

    struct afb_wsj1* sp_websock;
    struct afb_wsj1_itf minterface;
    sd_event* mploop;
    int mport;
    std::string mtoken;
    std::vector<int> msourceIDs;
    std::map<EventType_SM, handler_fun> handlers;
    EventType_SM const NumItems = (EventType_SM)(Event_AsyncSetSourceState + 1);

public:
    /* Don't use/ Internal only */
    void on_hangup(void *closure, struct afb_wsj1 *wsj);
    void on_call(void *closure, const char *api, const char *verb, struct afb_wsj1_msg *msg);
    void on_event(void *closure, const char *event, struct afb_wsj1_msg *msg);
    void on_reply(void *closure, struct afb_wsj1_msg *msg);
};

#endif /* LIBSOUNDMANAGER_H */
