//--------------------------
// myIOTWebServer.h
//--------------------------
// Derives from ESP32 (dead simple) WebServer to allow
// for sending any kind of responses.
//
// Right now I'm too lazy to switch to a better AsyncWebServer
// or write my own.
//
// Initially implemented only in the "custom link" call chain
// of the myIOTHTTPServer to allow the fridgeController to
// send the plotting data to the javascript as binary octet
// stream, one record at a time.

#pragma once

#include <WebServer.h>

#define RESPONSE_HANDLED        "RESPONSE_HANDLED"
    // if returned to the myIOTHTTP from custom_link,
    // the HTTPServer will not send anything else

class myIOTWebServer : public WebServer
{
public:

    myIOTWebServer(int port = 80);

    bool startBinaryResponse(const char *mime_type, uint32_t content_length);
    bool writeBinaryData(const char *data, int len);
    bool finishBinaryResponse();

    int getArg(const char *name, int def_value);
        // get an integer query argument with a default value if missing


    uint32_t m_content_len;
    uint32_t m_content_written;
    
};


extern myIOTWebServer *myiot_web_server;


