//--------------------------
// myIOTHTTP.cpp
//--------------------------

#include "myIOTHTTP.h"
#include "myIOTDevice.h"
#include "myIOTWifi.h"     // for suppressAutoConnectSTA()
#include "myIOTLog.h"
#include <WebServer.h>

#include <SD.h>
#include <SPIFFS.h>
#include <DNSServer.h>
#include <ESP32SSDP.h>
#include <StreamString.h>
#include <ArduinoJson.h>
#include <Update.h>
#include <esp_ota_ops.h>


#define MY_HTTP_PORT    80
#define MY_DNS_PORT     53



#define CAPTIVE_PORTAL_HTML  "/captive.html"

static const char PAGE_404[] =
    "<HTML>\n<HEAD>\n<title>Redirecting...</title> \n</HEAD>\n<BODY>\n<CENTER>Unknown page : $QUERY$<BR>\nYou will be "
    "redirected...\n<BR><BR>\nif not redirected, <a href='http://$STA_ADDRESS$'>click here</a>\n<BR><BR>\n<PROGRESS name='prg' "
    "id='prg'></PROGRESS>\n\n<script>\nvar i = 0; \nvar x = document.getElementById(\"prg\"); \nx.max=5; \nvar "
    "interval=setInterval(function(){\ni=i+1; \nvar x = document.getElementById(\"prg\"); \nx.value=i; \nif (i>5) "
    "\n{\nclearInterval(interval);\nwindow.location.href='http://$STA_ADDRESS$/';\n}\n},1000);\n</script>\n</CENTER>\n</BODY>\n</HTML>\n\n";
static const char PAGE_CAPTIVE[] =
    "<HTML>\n<HEAD>\n<title>Captive Portal $COUNT$</title> \n</HEAD>\n<BODY>\n<CENTER>Captive Portal page : $QUERY$<BR>\nYou will be "
    "redirected...\n<BR><BR>\nif not redirected, <a href='http://$AP_ADDRESS$/captive'>click here</a>\n<BR><BR>\n<PROGRESS name='prg' "
    "id='prg'></PROGRESS>\n\n<script>\nvar i = 0; \nvar x = document.getElementById(\"prg\"); \nx.max=5; \nvar "
    "interval=setInterval(function(){\ni=i+1; \nvar x = document.getElementById(\"prg\"); \nx.value=i; \nif (i>2) "
    "\n{\nclearInterval(interval);\nwindow.location.href='http://$AP_ADDRESS$/captive';\n}\n},1000);\n</script>\n</CENTER>\n</BODY>\n</HTML>\n\n";


WebServer web_server(MY_HTTP_PORT);
DNSServer dns_server;

int myIOTHTTP::m_upload_pct = 0;
int myIOTHTTP::m_num_upload_files = 0;
File myIOTHTTP::m_upload_file;
bool myIOTHTTP::m_in_upload = 0;
uint32_t myIOTHTTP::m_upload_total = 0;
uint32_t myIOTHTTP::m_upload_bytes = 0;
String myIOTHTTP::m_upload_filename;
String myIOTHTTP::m_upload_request_id;


//-------------------------------------------------------
// utilities
//-------------------------------------------------------

// #define GMT_TIME_ZONE  -5
// TimeZones listed in C:\Users\Patrick\AppData\Local\Arduino15\packages\esp8266\hardware\esp8266\3.0.1\cores\esp8266\TZ.h
// https://github.com/nayarsystems/posix_tz_db/blob/master/zones.csv
//
// I have only implemented the following time zone constants.
// The issue is that I don't want to send a huge list of timezones to all clients.
//
// EST - Eastern without Daylight Savings     Panama        "EST5"
// EDT - Eastern with Daylight Savings        New York      "EST5EDT,M3.2.0,M11.1.0"
// CDT - Central with Daylight Savings        Chicago       "CST6CDT,M3.2.0,M11.1.0"
// MST - Mountain without Daylight Savings    Pheonix       "MST7"
// MDT - Mountain with Daylight Savings       Denver        "MST7MDT,M3.2.0,M11.1.0"
// PDT - Pacific with Daylight Savings        Los Angeles   "PST8PDT,M3.2.0,M11.1.0"

const char *tzString(IOT_TIMEZONE tz)
{
    switch (tz)
    {
        case IOT_TZ_EST : return("EST5");
        case IOT_TZ_EDT : return("EST5EDT,M3.2.0,M11.1.0");
        case IOT_TZ_CDT : return("CST6CDT,M3.2.0,M11.1.0");
        case IOT_TZ_MST : return("MST7");
        case IOT_TZ_MDT : return("MST7MDT,M3.2.0,M11.1.0");
        case IOT_TZ_PDT : return("PST8PDT,M3.2.0,M11.1.0");
    }
    return "";
}




String myIOTHTTP::getContentType(String filename)
{
    String file_name = filename;
    file_name.toLowerCase();
    if (filename.endsWith(".htm")) {
        return "text/html";
    } else if (file_name.endsWith(".html")) {
        return "text/html";
    } else if (file_name.endsWith(".css")) {
        return "text/css";
    } else if (file_name.endsWith(".js")) {
        return "application/javascript";
    } else if (file_name.endsWith(".png")) {
        return "image/png";
    } else if (file_name.endsWith(".gif")) {
        return "image/gif";
    } else if (file_name.endsWith(".jpeg")) {
        return "image/jpeg";
    } else if (file_name.endsWith(".svg")) {
        return "image//svg+xml";
    } else if (file_name.endsWith(".jpg")) {
        return "image/jpeg";
    } else if (file_name.endsWith(".ico")) {
        return "image/x-icon";
    } else if (file_name.endsWith(".xml")) {
        return "text/xml";
    } else if (file_name.endsWith(".pdf")) {
        return "application/x-pdf";
    } else if (file_name.endsWith(".zip")) {
        return "application/x-zip";
    } else if (file_name.endsWith(".gz")) {
        return "application/x-gzip";
    } else if (file_name.endsWith(".txt")) {
        return "text/plain";
    }
    return "application/octet-stream";
}




//---------------------------------------------
// myIOTServer lifecycle
//---------------------------------------------

myIOTHTTP::~myIOTHTTP()
{}

myIOTHTTP::myIOTHTTP()
{}



void myIOTHTTP::setup()
{
    LOGD("myIOTHTTP::setup() started");
    proc_entry();

    web_server.onNotFound(handle_request);
    web_server.on("/captive", HTTP_GET, handle_captive);
    web_server.on("/description.xml", HTTP_GET, handle_SSDP);
    web_server.on("/spiffs_files", HTTP_POST, handle_SPIFFS, handle_SPIFFS);
#if WITH_SD
    web_server.on("/sdcard_files", HTTP_POST, handle_SDCARD, handle_SDCARD);
#endif
    web_server.on("/ota_files", HTTP_POST, handle_OTA, handle_OTA);

    // web_server.begin();
    // LOGI("HTTP started on port 80");

    proc_leave();
    LOGD("myIOTHTTP::setup() finished");
}

void myIOTHTTP::handle_SPIFFS() { handle_fileUpload(0,SPIFFS); }

#if WITH_SD
    void myIOTHTTP::handle_SDCARD() { handle_fileUpload(1,SD); }
#endif



void myIOTHTTP::onConnectStation()
{
    LOGD("myIOTHTTP::onConnectStation()");
    proc_entry();

    web_server.begin();
    LOGI("STA HTTP started on port 80");

    LOGD("Starting SSDP");
    SSDP.end();
    SSDP.setSchemaURL("description.xml");
    SSDP.setHTTPPort(MY_HTTP_PORT);
    SSDP.setURL("/");

    // The "device type" as far as SSDP is concerned is
    // "urn:myIOTDevice" for all of myIOTDevices.
    // The "urn:" may not be necessary.

    SSDP.setDeviceType("urn:myIOTDevice");

    // On the other hand, we encode our "device type" (i.e. "bilgeAlarm")
    // into the SSDP Server Name field, so that my cilents (myIOTServer)
    // can tell the type of the device without getting the description.xml.

    SSDP.setServerName("myIOTDevice");
    SSDP.setModelName(my_iot_device->getDeviceType());
    SSDP.setModelNumber(myIOTDevice::getVersion());

    LOGD("SSDP Server=%s UUID=%s",my_iot_device->getDeviceType(),my_iot_device->getUUID().c_str());

    SSDP.setUUID(my_iot_device->getUUID().c_str(),false);
        // the false is important, the default parameter (rootonly) is true
        // in which case luc's library will ALSO concatenate the MAC bytes.
        // Since he has no accessor, and I need it in the XML below, I
        // have my own version of getUniqueId().

    SSDP.setName(my_iot_device->getName().c_str());
        // This is the "friendly name" in the SSDP xml

    SSDP.begin();

    #if WITH_NTP

        String ntpServer = my_iot_device->getString(ID_NTP_SERVER);    // "pool.ntp.org";
        uint32_t tz_enum = my_iot_device->getEnum(ID_DEVICE_TZ);
        const char *tz_string = tzString(static_cast<IOT_TIMEZONE>(tz_enum));
        LOGI("Connecting to ntpServer=%s TZ(%d)=%s",ntpServer.c_str(),tz_enum,tz_string);
        // setting ESP32 timezone from https://randomnerdtutorials.com/esp32-ntp-timezones-daylight-saving/
        configTime(0, 0, ntpServer.c_str());
        setenv("TZ", tz_string, 1);
        tzset();

        // old code with #define GMT_TIME_ZONE -5
        // const long  gmtOffset_sec = GMT_TIME_ZONE * 60 * 60;
        // const int   daylightOffset_sec = 0;
        // configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);

        // validate ntp configuration

        struct tm timeinfo;
        if (getLocalTime(&timeinfo))
        {
            LOGD("   NTP connected: %04d-%02d-%02d-%02d:%02d:%02d  DST(%d)",
                timeinfo.tm_year +1900,
                timeinfo.tm_mon + 1,
                timeinfo.tm_mday,
                timeinfo.tm_hour,
                timeinfo.tm_min,
                timeinfo.tm_sec,
                timeinfo.tm_isdst);
        }
        else
        {
            LOGD("   NTP not connected");
        }

    #endif  // WITH_NTP

    proc_leave();
}

void myIOTHTTP::onDisconnectAP()
{
    LOGD("myIOTHTTP::onDisconnectAP()");
    dns_server.stop();

}
void myIOTHTTP::onConnectAP()
{
    LOGD("myIOTHTTP::onConnectAP()");

    web_server.begin();
    LOGI("AP HTTP started on port 80");

    dns_server.start(MY_DNS_PORT, "*", WiFi.softAPIP());
    LOGD("    Captive Portal Started");
}



//-------------------------------------------------------
// SSDP
//-------------------------------------------------------


void myIOTHTTP::handle_SSDP()
{
    StreamString sschema;

    // Note that we do not use the SSDP class schema() method which would
    // return the same string basically.

    if (sschema.reserve(1024))
    {
        String templ =
            "<?xml version=\"1.0\"?>"
            "<root xmlns=\"urn:schemas-upnp-org:device-1-0\">"
            "<specVersion>"
            "<major>1</major>"
            "<minor>0</minor>"
            "</specVersion>"
            "<URLBase>http://%s:%u/</URLBase>"
            "<device>"
            "<deviceType>urn:myIOTDevice</deviceType>"  // agrees with the deviceType sent in the SSDP packets
            "<friendlyName>%s</friendlyName>"
            "<presentationURL>/</presentationURL>"
            "<serialNumber>%s</serialNumber>"
            "<modelName>%s</modelName>"                 // whereas this agrees with the Server and is OUR device type name
            "<modelNumber>%s</modelNumber>"
            "<modelURL>https://github.com/phorton1</modelURL>"
            "<manufacturer>prhSystems</manufacturer>"
            "<manufacturerURL>http://phorton.com</manufacturerURL>"
            "<UDN>uuid:%s</UDN>"
            "</device>"
            "</root>\r\n"
            "\r\n";

        sschema.printf(templ.c_str(),
            WiFi.localIP().toString().c_str(),                      // ip
            MY_HTTP_PORT,                                           // port
            my_iot_device->getName().c_str(),                       // friendlyName
            String((uint16_t)(ESP.getEfuseMac() >> 32)).c_str(),    // serial number
            my_iot_device->getDeviceType(),                         // Model name
            myIOTDevice::getVersion(),                            // Model number
            my_iot_device->getUUID().c_str());                        // uuid

        web_server.send(200, "text/xml", (String)sschema);
    }
    else
    {
        web_server.send(500);
    }
}


//-------------------------------------------------------
// HTTP Server
//-------------------------------------------------------


void myIOTHTTP::debugRequest(const char* what)
{
    String path = web_server.urlDecode(web_server.uri());
    if (path == "/favicon.ico")
        return;
    LOGD("%s %d(%s)",what, web_server.method(),path.c_str());
    proc_entry();
    if (web_server.args())
    {
        for (int i=0; i<web_server.args(); i++)
        {
            LOGD("ARG[%d] %s=%s",i,web_server.argName(i).c_str(),web_server.arg(i).c_str());
        }
    }
    proc_leave();
}


String myIOTHTTP::doReplacements(const char *page)
{
    String content = page;
    String ap_addr = WiFi.softAPIP().toString();
    String sta_addr = WiFi.localIP().toString();
    if (MY_HTTP_PORT != 80)
    {
        ap_addr += ":";
        ap_addr += String(MY_HTTP_PORT);
        sta_addr += ":";
        sta_addr += String(MY_HTTP_PORT);
    }

    static int count = 0;
    content.replace("$AP_ADDRESS$", ap_addr);
    content.replace("$STA_ADDRESS$", sta_addr);
    content.replace("$QUERY$", web_server.uri());
    content.replace("$COUNT$",String(count++));
    return content;
}


void myIOTHTTP::handle_captive()
    // while connected in WS_CAPTIVE mode the only things
    // that the user can access are /captive and the redirect
    // pages from handle_request()
{
    if (SPIFFS.exists(CAPTIVE_PORTAL_HTML))
    {
        my_iot_device->clearStopAP();
        File file = SPIFFS.open(CAPTIVE_PORTAL_HTML, FILE_READ);
        web_server.streamFile(file, "text/html");
        file.close();
    }
    else    // should not get here
    {
        web_server.send(200, "text/html", doReplacements(PAGE_CAPTIVE));
    }
}


void myIOTHTTP::handle_request()
{
    String path = web_server.urlDecode(web_server.uri());

    if (path == "/")
    {
        // LOGD("changing path to index.html");
        path = "/index.html";
    }
    else if (path.startsWith("/spiffs/"))
        path.replace("/spiffs/","/");


    debugRequest("handle_request");

    // in AP Mode (Captive Portal) all html requests (except /captive)
    // return the redirect page, and if it is hit once, we inhibit auto
    // reconnect to the station ...

    if (my_iot_device->getConnectStatus() & IOT_CONNECT_AP)
    {
        myIOTWifi::suppressAutoConnectSTA();
        web_server.send(200, "text/html", doReplacements(PAGE_CAPTIVE));
    }

    // custom links
    // could be expanded to have a mime_type and use a buffer of some sort

    else if (path.startsWith("/custom/"))
    {
        path.replace("/custom/","");
        String text = my_iot_device->onCustomLink(path);
        if (text == "")
        {
            LOGE("custom link(%s) not found",path.c_str());
            web_server.send(404, "text/html", doReplacements(PAGE_404));
        }
        else
        {
            web_server.send(200, "text/html", text.c_str());
        }
        return;
    }

    // sdcard

    else if (path.startsWith("/sdcard/"))
    {
        if (myIOTDevice::hasSD())
        {
            path.replace("/sdcard/","/");
            LOGI("serving SDCARD file: %s",path.c_str());
            File file = SD.open(path, FILE_READ);
            if (file)
            {
                web_server.streamFile(file, getContentType(path));
                file.close();
                return;
            }
        }
    }

    // The normal IOTDevice WebUI can only be accessed
    // in WS_STATION mode, and includes not found error

    else if (SPIFFS.exists(path))
    {
        LOGI("serving SPIFFS file: %s",path.c_str());
        File file = SPIFFS.open(path, FILE_READ);
        if (file)
        {
            web_server.streamFile(file, getContentType(path));
            file.close();
            return;
        }
    }

    LOGE("page not found: %s",path.c_str());
    web_server.send(404, "text/html", doReplacements(PAGE_404));
}




// Kludgy
// the 200 response MUST be sent out for each request from the web,
// but in order to do so WE must keep track of the number of files and
// hence, the request number.
// ALSO SPIFFS FILENAMES ARE LIMITED to 31 characters!!!!

void myIOTHTTP::handle_fileUpload(bool sdcard, fs::FS the_fs)
{
    const char *what = sdcard ? "SDCARD" : "SPIFFS";

    if (my_iot_device->getConnectStatus() & IOT_CONNECT_AP)
    {
        LOGE("Somebody trying %s upload from an AP!!",what);
        web_server.send(404);
    }

    // debugRequest("handle_SPIFFS()");
    if (web_server.method() == HTTP_GET)    // 1
    {
        LOGD("handle_fileUpload(%s) returning 200 on GET request",what);
        web_server.send(200);
        return;
    }

    bool failed = false;
    HTTPUpload& upload = web_server.upload();

    if (upload.status == UPLOAD_FILE_START)
    {
        int num_files = web_server.arg("num_files").toInt();
        String request_id = web_server.arg("file_request_id");
        LOGI("upload UPLOAD_FILE_START %s num_files=%d request_id=%s",upload.filename.c_str(),num_files,request_id.c_str());

        m_upload_filename = upload.filename;
        if (m_upload_filename[0] != '/')
        {
            m_upload_filename = "/";
            m_upload_filename += upload.filename;
        }

        if (m_upload_request_id != request_id)
        {
            if (m_in_upload)
            {
                LOGW("attempt to upload file while m_in_upload");
                web_server.send(404, "text/html", String("An upload is already in progress"));
                return;
            }

            m_in_upload = 1;

            m_upload_pct = -1;
            m_upload_total = 0;
            m_upload_bytes = 0;
            m_upload_request_id = request_id;
            m_num_upload_files = num_files;
            for (int i=0; i<web_server.args(); i++)
            {
                String arg_name = web_server.argName(i);
                if (arg_name.endsWith("_size") >= 0)
                {
                    m_upload_total += web_server.arg(i).toInt();
                }
            }

            LOGU("NEW UPLOAD_REQUEST(%s) num_files(%d) total_bytes(%d)",m_upload_request_id,m_num_upload_files,m_upload_total);
            sendUploadProgress(1);
        }

        if (sdcard)
        {
        }
        if (the_fs.exists(m_upload_filename))
        {
            LOGD("Removing old %s %s",what,m_upload_filename.c_str());
            the_fs.remove(m_upload_filename);
        }
        if (m_upload_file)
        {
            LOGD("closing abandoned m_upload_file");
            m_upload_file.close();
        }
        String size_arg_name = upload.filename + "_size";
        if (web_server.hasArg(size_arg_name))
        {
            uint64_t filesize  = web_server.arg(size_arg_name).toInt();
            uint64_t freespace = sdcard ?
                SD.totalBytes() - SD.usedBytes() :
                SPIFFS.totalBytes() - SPIFFS.usedBytes();

            LOGD("Filesize from 'S' = %d",filesize);
            if (filesize > freespace)
            {
                LOGE("Upload %s error - not enough free space",what);
                failed = true;
            }
        }
        if (!failed)
        {
            LOGD("opening %s %s",what,m_upload_filename.c_str());
            m_upload_file = the_fs.open(m_upload_filename, FILE_WRITE);
            if (!m_upload_file)
            {
                LOGE("Upload error - could not open %s %s for writing",what,m_upload_filename.c_str());
                failed = true;
            }
        }
    }
    else if (upload.status == UPLOAD_FILE_WRITE)
    {
        uint32_t bytes = upload.currentSize;
        // LOGI("upload UPLOAD_FILE_WRITE %d",bytes);

        if (m_upload_file)
        {
            if (bytes != m_upload_file.write(upload.buf, bytes))
            {
                LOGE("Upload error - write failed");
                failed = true;
            }
            else
            {
                m_upload_bytes += bytes;
                sendUploadProgress(1);
            }
        }
        else
        {
            LOGE("Upload error - file lost");
            failed = true;
        }
    }
    else if (upload.status == UPLOAD_FILE_END)
    {
        LOGI("upload UPLOAD_FILE_END %d",upload.currentSize);
        if (m_upload_file)
        {
            m_upload_file.close();

            String size_arg_name = upload.filename + "_size";
            if (web_server.hasArg(size_arg_name))
            {
                uint32_t arg_filesize = web_server.arg(size_arg_name).toInt();
                m_upload_file = the_fs.open(m_upload_filename, FILE_READ);
                if (!m_upload_file)
                {
                    LOGE("Could not open %s file for size check",what);
                    failed = true;
                }
                else
                {
                    uint32_t actual_filesize = m_upload_file.size();
                    m_upload_file.close();
                    if (actual_filesize != arg_filesize)
                    {
                        LOGE("%s File verify failed expected(%d) actual(%d)",what,arg_filesize,actual_filesize);
                        failed = true;
                    }
                }
            }

            if (!failed)
            {
                m_num_upload_files--;
                LOGI("File %s upload(%s) succeeded %d files remaining",what,m_upload_filename.c_str(),m_num_upload_files);

                // OK ... the 200 response MUST be sent out for each request from the web,
                // but in order to do so WE must keep track of the number of files and
                // hence, the request number.

                if (m_num_upload_files <= 0)
                {
                    LOGU("%s upload finished",what);
                    web_server.send(200);
                    sendUploadProgress(1,true);
                    m_upload_filename = "";
                    m_in_upload = false;

                    #if WITH_WS
                        my_iot_device->onFileSystemChanged(sdcard);
                    #endif
                }
            }

        }
        else if (m_upload_filename != "")   // UPLOAD_FILE_END appears to be called one extra time with the file already closed by the above snippet
        {
            LOGW("Upload error - %s file disappeared",what);
            failed = true;
        }
    }
    else if (upload.status == UPLOAD_FILE_ABORTED)
    {
        LOGW("%s UPLOAD_FILE_ABORTED!!!",what);
        failed = true;
    }

    if (failed)
    {
        LOGW("Cancelling %s upload",what);

        upload = web_server.upload();
        upload.status = UPLOAD_FILE_ABORTED;
        errno = ECONNABORTED;
        web_server.client().stop();
        sendUploadProgress(1,true,true);
        delay(100);
        if (m_upload_file)
            m_upload_file.close();
        if (SPIFFS.exists(m_upload_filename))
            SPIFFS.remove(m_upload_filename);
        m_upload_filename = "";
        m_in_upload = false;

        #if WITH_WS
            my_iot_device->onFileSystemChanged(sdcard);
        #endif

    }
}   // handle_fileUpload()





void myIOTHTTP::handle_OTA()
{
    if (my_iot_device->getConnectStatus() & IOT_CONNECT_AP)
    {
        LOGE("Somebody trying OTA upload from an AP!!");
        web_server.send(404);
    }

    // debugRequest("handle_SPIFFS()");
    if (web_server.method() == HTTP_GET)    // 1
    {
        LOGD("handleOTA returning 404 on GET request");
        web_server.send(404, "text/html", doReplacements(PAGE_404));
        return;
    }

    bool failed = false;
    HTTPUpload& upload = web_server.upload();

    if (upload.status == UPLOAD_FILE_START)
    {
        LOGI("upload OTA_FILE_START %s",upload.filename.c_str());

        if (m_in_upload)
        {
            LOGW("attempt to upload file while m_in_upload");
            web_server.send(404, "text/html", String("An upload is already in progress"));
            return;
        }
        m_in_upload = true;

        if (m_upload_request_id == web_server.arg("file_request_id"))
        {
            LOGE("duplicate request id in handleOTA");
            web_server.send(404, "text/html", String("request id already used"));
            m_in_upload = false;
            return;
        }

        m_upload_pct = -1;
        m_upload_bytes = 0;
        m_upload_request_id = web_server.arg("file_request_id");
        m_num_upload_files = 1;
        String arg_name = upload.filename + "_size";
        String arg = web_server.arg(arg_name);

        if (arg == "")
        {
            LOGE("no size argument for OTA %s",upload.filename.c_str());
            web_server.send(404, "text/html", String("OTA no size argument"));
            m_in_upload = false;
            return;
        }
        m_upload_total = arg.toInt();
        if (!m_upload_total)
        {
            LOGE("zero size argument for OTA %s",upload.filename.c_str());
            web_server.send(404, "text/html", String("OTA zero size argument"));
            m_in_upload = false;
            return;
        }

        uint32_t available = 0;
        if (esp_ota_get_running_partition())
        {
            const esp_partition_t* partition = esp_ota_get_next_update_partition(NULL);
            if (partition)
            {
                available = partition->size;
            }
        }
        if (available <= m_upload_total)
        {
            LOGE("OTA available(%d) less than OTA size(%d)",available,m_upload_total);
            web_server.send(404, "text/html", String("OTA not enough room"));
            m_in_upload = false;
            return;
        }


        if (!Update.begin())
        {
            LOGW("ESP32 Update.begin() call failed");
            web_server.send(404, "text/html", String("request id already used"));
            m_in_upload = false;
            return;
        }

        LOGI("OTA upload_request(%s) total_bytes(%d)",m_upload_request_id,m_upload_total);
        m_upload_filename = upload.filename;
        sendUploadProgress(5);

    }
    else if (upload.status == UPLOAD_FILE_WRITE)
    {
        uint32_t bytes = upload.currentSize;
        // LOGI("upload OTA_FILE_WRITE %d",bytes);
        if (Update.write(upload.buf, bytes) != bytes)
        {
            failed = 1;
            LOGE("Update write failed");
        }
        m_upload_bytes += bytes;
        sendUploadProgress(5);
    }
    else if (upload.status == UPLOAD_FILE_END)
    {
        LOGI("upload OTA_FILE_END %d",upload.currentSize);

        // prh could verify size written here ...

        if (Update.end(true))
        {
            sendUploadProgress(5,true);
            my_iot_device->invoke(ID_REBOOT);
        }
        else
        {
            LOGE("Update.end(true) call failed");
            failed = 1;
        }
    }
    else if (upload.status == UPLOAD_FILE_ABORTED)
    {
        LOGW("UPLOAD_OTA_ABORTED!!!");
        failed = true;
    }

    if (failed)
    {
        LOGW("Cancelling upload");

        upload = web_server.upload();
        upload.status = UPLOAD_FILE_ABORTED;
        errno = ECONNABORTED;
        web_server.client().stop();
        sendUploadProgress(5,true,true);
        delay(100);
        m_in_upload = false;
    }
}   // handle_OTA()





void myIOTHTTP::sendUploadProgress(int every, bool finished /*=false*/, bool error/*=false*/)
{
    float fpct = m_upload_total ? ((float)m_upload_bytes)/((float)m_upload_total)*100 : 100;
    int ipct = fpct;
    ipct = (ipct/every) * every;
    fpct = ipct;

    if (ipct >= 100 && !finished)
        ipct = 99;
    if (finished || error)
        ipct = 100;

    // LOGD("uploadProgress(%s) pct=%d",m_upload_filename.c_str(),ipct);

    if (m_upload_pct != ipct || finished || error)
    {
        m_upload_pct = ipct;
        StaticJsonDocument<255> doc;
        doc["upload_progress"] = ipct;
        doc["upload_filename"] = m_upload_filename;
        doc["upload_error"] = error;

        String json_string;
        serializeJson(doc, json_string);

        // prh issue: the upload is started by HTTP, but progress is reported by WS
        // and we don't know which ws number to use!!!
        // We would need a WS handler for the client to "reserve" the upload "channel",
        // and the actual upload would have to wait until the reseved channel reply.
        //
        // This would substantially change the logic of upload handling, obviating
        // the need for a unique id, but would have issues if, for instance, a client
        // became disconnected just after reserving the channel (so it would need to
        // automatically timeout if an upload was not started in a certain amount of
        // time).
        //
        // In a similar vein, client WS calls that "fail" because there is no websocket
        // do not currently report an error or do anything (i.e. delete a file when
        // there is no websocket open)

        #if WITH_WS
            my_iot_device->wsBroadcast(json_string.c_str());
        #endif

        Serial.printf("uploadProgress(%d,%s) %s\r",ipct,m_upload_filename.c_str(),error?"ERROR":"");
    }
}




//------------------------------------------------
// loop()
//------------------------------------------------

void myIOTHTTP::loop()
{
    web_server.handleClient();

    if (my_iot_device->getConnectStatus() & IOT_CONNECT_AP)
        dns_server.processNextRequest();
}
