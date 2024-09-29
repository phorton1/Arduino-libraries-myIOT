//--------------------------
// myIOTWebServer.cpp
//--------------------------

#include "myIOTWebServer.h"
#include "myIOTLog.h"

#define DEBUG_BINARY_RESPONSE   0

myIOTWebServer *myiot_web_server;


myIOTWebServer::myIOTWebServer(int port) :
    WebServer(port)
{
    myiot_web_server = this;
}


int myIOTWebServer::getArg(const char *name, int def_value)
    // get an integer query argument with a default value if missing
{
    String val = arg(name);     // arg() is a standard WebServer method
    if (val != "")
        return val.toInt();
    return def_value;
}



bool myIOTWebServer::startBinaryResponse(const char* mime_type, uint32_t content_length)
{
    #if DEBUG_BINARY_RESPONSE
        String len_str = content_length == CONTENT_LENGTH_UNKNOWN ?
            "CONTENT_LENGTH_UNKNOWN" : String(content_length);
        LOGD("startBinaryResponse(%s,%s)",mime_type,len_str.c_str());
    #endif

    _contentLength = content_length;
        // base class uses member variable to switch to chunked,
        // NOT that which is passed in _prepareHeader!!
        
    m_content_len = content_length;
    m_content_written = 0;

	// LOGD("content_length=%u CONTENT_LENGTH_UNKNOWN=%u",content_length,CONTENT_LENGTH_UNKNOWN);

    String header;
    // "binary/octet-stream"

    // set to HTTP 1.0 to send binary data without chunking
    uint8_t save_version = _currentVersion;
    if (content_length == CONTENT_LENGTH_UNKNOWN)
        _currentVersion = 0;
    _prepareHeader(header, 200, mime_type, content_length);
    _currentVersion = save_version;

    int header_len = header.length();
    int bytes = _currentClientWrite(header.c_str(), header_len);
    if (bytes != header_len)
    {
        LOGE("startBinaryResponse sent(%d/%d) content_len(%d)",bytes,header_len,content_length);
        return false;
    }
    return true;
}

#include <errno.h>

bool myIOTWebServer::writeBinaryData(const char *data, int len)
{
    #if DEBUG_BINARY_RESPONSE
        LOGD("writeBinaryData(%d)",len);
    #endif

    if (len<=0 || (m_content_len != CONTENT_LENGTH_UNKNOWN && m_content_written + len > m_content_len))
    {
        LOGE("writeBinaryData(%d) attempted at byte(%d) of (%d)",len,m_content_written,m_content_len);
        return false;
    }
    int bytes = _currentClientWrite(data, len);
    if (bytes != len)
    {
        LOGE("ERROR(%d) writeBinaryData(%d/%d) failed at byte(%d) of (%d)",errno,bytes,len,m_content_written,m_content_len);
        return false;
    }
    m_content_written += bytes;
    return true;
}

bool myIOTWebServer::finishBinaryResponse()
{
    #if DEBUG_BINARY_RESPONSE
        LOGD("finishBinaryResponse()");
    #endif

    int bytes = _currentClientWrite("\r\n", 2);
    if (bytes != 2)
    {
        LOGE("finishBinaryResponse(%d) failed",m_content_len);
        return false;
    }
    return true;
}



