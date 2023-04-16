//--------------------------
// myIOTWebSockets.h
//--------------------------

#pragma once

#include "myIOTTypes.h"
#include <Arduino.h>
#include <WebSocketsServer.h>


#if WITH_WS
#define WS_TASK



class myIOTWebSockets
{
    public:

        myIOTWebSockets();
        ~myIOTWebSockets();

        static void setup();
        static void begin();
        static void end();

        #ifndef WS_TASK
            static void loop();
        #endif

        static void broadcast(const char *msg);

        static void onFileSystemChanged(bool sdcard);


    protected:

        static myIOTWebSockets *m_this;

        static WebSocketsServer m_web_sockets;

        static String deviceInfoJson();
        static String fileDirectoryJson(bool sdcard);
        static void webSocketEvent(uint8_t num, WStype_t ws_type, uint8_t *data, size_t len);

        static void sendTXT(int num, const char *msg);
        static void onDeleteFile(int num, String filename);

        #ifdef WS_TASK
            static void webSocketTask(void *param);
        #endif

};



extern myIOTWebSockets my_web_sockets;


#endif  // WITH_WS
