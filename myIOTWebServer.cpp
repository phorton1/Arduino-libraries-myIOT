//--------------------------
// myIOTWebServer.cpp
//--------------------------

#include "myIOTWebServer.h"
#include "myIOTLog.h"

myIOTWebServer *myiot_web_server;


myIOTWebServer::myIOTWebServer(int port) :
    WebServer(port)
{
    myiot_web_server = this;
}


bool myIOTWebServer::startBinaryResponse(const char* mime_type, int content_length)
{
    m_content_len = content_length;
    m_content_written = 0;

    String header;
    // "binary/octet-stream"
    _prepareHeader(header, 200, mime_type, content_length);
    int header_len = header.length();
    int bytes = _currentClientWrite(header.c_str(), header_len);
    if (bytes != header_len)
    {
        LOGE("startBinaryResponse sent(%d/%d) content_len(%d)",bytes,header_len,content_length);
        return false;
    }
    return true;
}

bool myIOTWebServer::writeBinaryData(const char *data, int len)
{
    if (len<=0 || m_content_written + len > m_content_len)
    {
        LOGE("writeBinaryData(%d) attempted at byte(%d) of (%d)",len,m_content_written,m_content_len);
        return false;
    }
    int bytes = _currentClientWrite(data, len);
    if (bytes != len)
    {
        LOGE("writeBinaryData(%d) failed at byte(%d) of (%d)",len,m_content_written,m_content_len);
        return false;
    }

    return true;
}

bool myIOTWebServer::finishBinaryResponse()
{
    int bytes = _currentClientWrite("\r\n", 2);
    if (bytes != 2)
    {
        LOGE("finishBinaryResponse(%d) failed",m_content_len);
        return false;
    }
    return true;
}



