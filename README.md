# Fulup notes moving from Genivi AudioManager to AGO Audio-4A

## Initial call is done with:

    stream_open(const string& audioRole, EndPointType4aT endPointType, const int endpointID);
    - AudioRole should be define in Audio-4A Policy config
    - EndPointType4aT is sink or source
    - EndpointID might be null in which case 1st one is from config is selected

    Function return a streamID that should be use for further action (stream_state, stream_close,...)


## Terminology changes:

    As logic from Genivi audio manager and AGL audio-4a are not 100% aligned some mapping is required:

    - Genivi->sourceID     AGL->endpointID (attached to an audio_role)
    - Genivi->connectionID AGL->streamID   (main handle to an active stream)

## Question about QT/JDOC
    The way I understand QML code and I'm not an expert.
    The JSON response is not parsed by libaudioclient but is store without any change it within QT app jDoc object. 
    As JSON elements return by Genevi and Audio-4a diverge this impose to change QT/Client code

```
    Radio.qml (line 165) 

        connectionID = content.response.mainstreamID

    should probably be replaced by something like:

        connectionID = content.response.stream_id
```


## MISC

 *  AGL Audio-4A maintain a session by client. As a result if a client does not provide a StreamID all stream attached to that
    client will be free.

 * Events subscription: Audio-4a automatically subscribe a given client to its open stream state changes. Client should only
   subscribe to get extra informations on volume/property change. Subscription call handle both subscribe/unsubscribe.

 * Event Name: Audio-4a create by stream events in order to send event only the client using corresponding stream. As the result
   filtering on event should be done on the value and not on the event name. My guest is that:
    * in libclient event selection should be limited to check if event comes from "audio-4a" (just to filter potential unsolicited broadcasted events.
    * in QML I'm not sure about the test on sourceID that should become a test on endpointID

 * Lib Client as many API defined 'eg: volume' that does not seems to be used. 





## Typo
    Replace 'closeed' by 'closed'