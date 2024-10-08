//--------------------------
// myIOTHTTP.cpp
//--------------------------
// The HTTP Server is responsible for starting and stopping
// the underlying SSDP Server (in station mode) and DNS server
// (in AP mode) via calls to onConnect and onDisconnect AP/Station

#include "myIOTHTTP.h"
#include "myIOTDevice.h"
#include "myIOTWifi.h"     // for suppressAutoConnectSTA()
#include "myIOTLog.h"
#include "myIOTWebServer.h"

#include <SD.h>
#include <SPIFFS.h>
#include <DNSServer.h>
#if WITH_SSDP
    #include <ESP32SSDP.h>
#endif
#include <StreamString.h>
#include <ArduinoJson.h>
#include <Update.h>
#include <esp_ota_ops.h>


#define MY_HTTP_PORT    80
#define MY_DNS_PORT     53

// #define USE_302_REDIRECT
// If this is defined, instead of sending the PAGE_CAPTIVE html,
// we will use 302 redirects.  However, the 302 approach did not seem to
// work as well (iPad) as the html approach.  Neither appears to work
// on Android 12 (my new Galaxy S21), so in those cases the user must
// explicitly hit a page (i.e. 192.168.1.254/captive)

#define CAPTIVE_PORTAL_HTML  "/captive.html"

static const char PAGE_404[] =
    "<HTML>\n<HEAD>\n<title>Redirecting...</title> \n</HEAD>\n<BODY>\n<CENTER>Unknown page : $QUERY$<BR>\nYou will be "
    "redirected...\n<BR><BR>\nif not redirected, <a href='http://$STA_ADDRESS$'>click here</a>\n<BR><BR>\n<PROGRESS name='prg' "
    "id='prg'></PROGRESS>\n\n<script>\nvar i = 0; \nvar x = document.getElementById(\"prg\"); \nx.max=5; \nvar "
    "interval=setInterval(function(){\ni=i+1; \nvar x = document.getElementById(\"prg\"); \nx.value=i; \nif (i>5) "
    "\n{\nclearInterval(interval);\nwindow.location.href='http://$STA_ADDRESS$/';\n}\n},1000);\n</script>\n</CENTER>\n</BODY>\n</HTML>\n\n";
#ifndef USE_302_REDIRECT
    static const char PAGE_CAPTIVE[] =
        "<HTML>\n<HEAD>\n<title>Captive Portal $COUNT$</title> \n</HEAD>\n<BODY>\n<CENTER>Captive Portal page : $QUERY$<BR>\nYou will be "
        "redirected...\n<BR><BR>\nif not redirected, <a href='http://$AP_ADDRESS$/captive'>click here</a>\n<BR><BR>\n<PROGRESS name='prg' "
        "id='prg'></PROGRESS>\n\n<script>\nvar i = 0; \nvar x = document.getElementById(\"prg\"); \nx.max=5; \nvar "
        "interval=setInterval(function(){\ni=i+1; \nvar x = document.getElementById(\"prg\"); \nx.value=i; \nif (i>2) "
        "\n{\nclearInterval(interval);\nwindow.location.href='http://$AP_ADDRESS$/captive';\n}\n},1000);\n</script>\n</CENTER>\n</BODY>\n</HTML>\n\n";
#endif

myIOTWebServer web_server(MY_HTTP_PORT);
DNSServer dns_server;

int myIOTHTTP::m_upload_pct = 0;
int myIOTHTTP::m_num_upload_files = 0;
File myIOTHTTP::m_upload_file;
bool myIOTHTTP::m_in_upload = 0;
uint32_t myIOTHTTP::m_upload_total = 0;
uint32_t myIOTHTTP::m_upload_bytes = 0;
String myIOTHTTP::m_upload_filename;
String myIOTHTTP::m_upload_request_id;

myIOTHTTP my_iot_http;


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
    } else if (file_name.endsWith(".md")) {
        return "text/plain";    // markdown";
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
    LOGI("myIOTHTTP::onConnectStation()");
    proc_entry();

    web_server.begin();
    LOGI("STA HTTP started on port 80");

    // changes to the _device_ssdp boolean require a reboot

    #if WITH_SSDP
        if (my_iot_device->getBool(ID_SSDP))
        {
            LOGI("Starting SSDP");
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
        }
    #endif  // WITH_SSDP

    #if WITH_NTP

        String ntpServer = my_iot_device->getString(ID_NTP_SERVER);    // "pool.ntp.org";
        uint32_t tz_enum = my_iot_device->getEnum(ID_TIMEZONE);
        const char *tz_string = tzString(static_cast<IOT_TIMEZONE>(tz_enum));
        LOGI("Connecting to ntpServer=%s TZ(%d)=%s",ntpServer.c_str(),tz_enum,tz_string);

        // setting ESP32 timezone from https://randomnerdtutorials.com/esp32-ntp-timezones-daylight-saving/

        // try up to 5 times to connect.  The time can be crucial for
        // many applications.

        bool ok = 0;
        int count = 0;

        while (!ok && count++ < 5)
        {
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
                LOGU("NTP try(%d) connected: %04d-%02d-%02d-%02d:%02d:%02d  DST(%d)",
                    count,
                    timeinfo.tm_year +1900,
                    timeinfo.tm_mon + 1,
                    timeinfo.tm_mday,
                    timeinfo.tm_hour,
                    timeinfo.tm_min,
                    timeinfo.tm_sec,
                    timeinfo.tm_isdst);
                ok = 1;
            }
            else
            {
                LOGE("NTP try(%d) failed!!",count);
            }
        }

        if (!ok)
        {
            LOGE("COULD NOT START NTP!!!");
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
            "<modelURL>%s</modelURL>"
            "<manufacturer>prhSystems</manufacturer>"
            "<manufacturerURL>https://github.com/phorton1/Arduino-libraries-myIOT</manufacturerURL>"
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
            myIOTDevice::getVersion(),                              // Model number
            myIOTDevice::getDeviceUrl(),                            // Device Url
            my_iot_device->getUUID().c_str());                      // uuid

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
    if (!my_iot_device->showDebug(path))
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
        // my_iot_device->clearStopAP();
            // we want to stop wifi from turning off the AP once
            // we have started a session, even if it *might* prefer
            // to stop the AP and (re)connect to a station otherwise.
            // Once a captive portal session is started, we will
            // not go to station mode until succesfull WebUI call
            // thru myIOTDevice() to connect() OR a reboot of
            // the device, or some long amount of time.


        File file = SPIFFS.open(CAPTIVE_PORTAL_HTML, FILE_READ);
        web_server.streamFile(file, "text/html");
        file.close();
    }
    else    // should not get here
    {
        LOGE("ERROR CAPTIVE PORTAL HTML FILE %s DOES NOT EXIST!!",CAPTIVE_PORTAL_HTML);
    }
}


void myIOTHTTP::handle_request()
{
    String path = web_server.urlDecode(web_server.uri());

    bool is_myiot_request = 0;
        // request uses fake /myIOT/ prefix which means that the file is
        // denormalized as the 'data' common submodule and (1) uploaded to the
        // local ESP32's spiffs file system at /, or (2) in the myIOTServer's
        // myIOT directory, which is likewise a clone of the the common 'data'
        // submodule
    bool is_extra_spiffs = 0;
        // request uses fake /spiffs/ prefix which, in the myIOTServer, indicates
        // that it should be served from the given ESP32's spiffs file system, given
        // a UUID parameter.


    if (path == "/")
    {
        // LOGD("changing path to index.html");
        path = "/index.html";
    }
    else if (path.startsWith("/spiffs/"))
    {
        path.replace("/spiffs/","/");
        is_extra_spiffs = 1;
    }
    

    // 2024-03-01 data directory as submodule
    //
    // The CSS and JS referenced from index.html are prefixed with
    // /myIOT/ to allow the Perl myIOTServer to place the 'data'
    // submodule in a different location in the folder tree than
    // the default 'site' folder.
    //
    // On the ESP32 we merely remove the /myIOT/ and serve it
    // from the '/' root of the flat SPIFFS

    else if (path.startsWith("/myIOT/"))
    {
        path.replace("/myIOT/","/");
        is_myiot_request = 1;
    }

    debugRequest("handle_request");

    // If someobne is connected in AP Mode (Captive Portal)
    // all html requests (except /captive) return a 302 redirect

    if (ap_connection_count)
    {
        #ifdef USE_302_REDIRECT   // redirect using 302
            //how to do a redirect, next two lines
            LOGI("Sending 302 redirect to /captive");
            web_server.sendHeader("Location", String("/captive"), true);
            web_server.send ( 302, "text/plain", "");
        #else
            LOGI("Sending PAGE_CAPTIVE redirect page");
            web_server.send(200, "text/html", doReplacements(PAGE_CAPTIVE));
        #endif
        return;
    }

    // otherwise, if AP is active, return 404 not found

    else if  (my_iot_device->getConnectStatus() & IOT_CONNECT_AP)
    {
        LOGI("AP_MODE returning page not found: %s",path.c_str());
        web_server.send(404, "text/html", doReplacements(PAGE_404));
    }

    //----------------------------------------------------
    // normal behavior from here down
    //----------------------------------------------------
    // custom links
    // could be expanded to have a mime_type and use a buffer of some sort

    else if (path.startsWith("/custom/"))
    {
        path.replace("/custom/","");
        const char *mime_type = "text/html";

        String text = my_iot_device->onCustomLink(path,&mime_type);
        if (text == "")
        {
            LOGE("custom link(%s) not found",path.c_str());
            web_server.send(404, "text/html", doReplacements(PAGE_404));
        }
        else if (text != RESPONSE_HANDLED)
        {
            web_server.send(200, mime_type, text.c_str());
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
    // in STATION mode, and includes not found error

    else if (SPIFFS.exists(path))
    {
        String cache_arg = myiot_web_server->arg("cache");
        LOGI("serving SPIFFS file: %s  cache=%s",path.c_str(),cache_arg.c_str());
        File file = SPIFFS.open(path, FILE_READ);
        if (file)
        {
            #if 1
                if (cache_arg.equals("1"))
                    web_server.sendHeader("Cache-Control","max-age=31536000, immutable",false);
            #endif
            
            web_server.streamFile(file, getContentType(path));
            file.close();
            return;
        }
    }

    LOGE("page not found: %s",path.c_str());
    web_server.send(404, "text/html", doReplacements(PAGE_404));
}




// Kludgy and Weird
//
// This method is set to handle POST requests to /spiffs_files or /sdcard_file.
// The way it "works" in the WebServer is that this method will be called numerous
//      times as the incoming HTTP request is being parsed, where the WebServer
//      implementation has decided that for a POST request of a "file", each file
//      within multi-part HTTP request, and each buffer within those, is treated as
//      essentially, a separate request (a separate call to this method).
//      orchestrated by the unweildy "HTTPUpload" object.
// So, wheras there is a single POST from the Browser, we get multiple pseudo
//      "requests" to upload files within the POST, and, as mentioned, within
//      that, separate requests to handle each Part of the request.
// In order to manage this complexity, I had to add a slew of header parameters
//      to the POST request, including a unique ID for each POST request, so
//      that I can identify separate POSTS from ongoing requests within a single
//      POST, as well as a "filename_size" parameter so that I could tell the
//      size of each (multipart) file which is not part of the normal "chunked"
//      POST request.
// Perhaps the weirdest part is that, although this HTTPUpload thing appears
//      to be knowledgable about "Aborted" requests, it does not, apparently
//      act on that information.  So, even after the WebServer itself sets
//      the "upload state" to UPLOAD_FILE_ABORTED, or if I do as a result of
//      an error (i.e. the file wont fit on the SPIFFS), the brain-dead WebServer
//      keeps calling this method until it has parsed the entire POST request,
//      and I could not find a way to stop it, including even by closing
//      the client socket.
// So, in order to at least prevent getting dozens or hundreds of superflouse
//      error messages, I had to carefully manage the global m_in_upload boolean
//      and not report subsequent errors after I reset it to false (within
//      the same request_id).
// Other weirdnesses of this approach, is that for each UPLOAD_FILE_START
//      you MUST send a 200 or 404 response.  MAYBE a 404 stops the parser,
//      but if you don't send the 200, it definitely doesn't work.
//
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
    if (web_server.method() == HTTP_GET)
    {
        LOGD("handle_fileUpload(%s) returning 200 on GET request",what);
        web_server.send(200);
        return;
    }

    bool failed = false;
    HTTPUpload& upload = web_server.upload();

    // verbose debugging will show how this is called after failures
    // until the WebServer parser is good and ready to stop:
    // LOGD("handle_fileUpload() status=%d",upload.status);

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
                if (arg_name.endsWith("_size"))
                {
                    m_upload_total += web_server.arg(i).toInt();
                }
            }

            LOGU("NEW UPLOAD_REQUEST(%s) num_files(%d) total_bytes(%d)",m_upload_request_id,m_num_upload_files,m_upload_total);
            sendUploadProgress(1);
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

        // here, for example, we DONT report the error if we have
        // closed the output file and turned m_in_upload off

        else if (m_in_upload)
        {
            LOGE("Upload error - file lost");
            failed = true;
        }
    }
    else if (upload.status == UPLOAD_FILE_END)
    {
        // and here, we only continue to report END and continue
        // if m_in_upload

        if (m_in_upload)
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

        }   // only do anything if m_in_upload for sanity
    }

    else if (upload.status == UPLOAD_FILE_ABORTED)
    {
        // we may report ABORTED ONE TIME from Browser/WebServer

        if (m_in_upload)
            LOGW("%s UPLOAD_FILE_ABORTED!!!",what);
        failed = true;
    }

    if (failed)
    {
        // This method keeps getting called even after the first UPLOAD_FILE_ABORTED
        // The following code avoids LOGGING subsquent behaviors after m_in_upload is
        // set to zero.

        if (m_in_upload)
        {
            LOGW("Cancelling %s upload",what);

            if (1)
            {
                 web_server.send(404, "text/html", String("Server Upload Failed"));
            }
            else
            {
                upload = web_server.upload();
                upload.status = UPLOAD_FILE_ABORTED;
                errno = ECONNABORTED;
                web_server.client().stop();
            }
            
            sendUploadProgress(1,true,true);
            delay(100);
            if (m_upload_file)
                m_upload_file.close();
            if (SPIFFS.exists(m_upload_filename))
                SPIFFS.remove(m_upload_filename);

            m_in_upload = false;

            #if WITH_WS
                my_iot_device->onFileSystemChanged(sdcard);
            #endif
        }

        // The filename is the only thing that *might* be set
        // when we get here with "failed".

        m_upload_filename = "";
    }

}   // handle_fileUpload()





void myIOTHTTP::handle_OTA()
    // I'm pretty sure handle_OTA() has the same flaws,
    // but I didn't mess with this since there is only
    // ONE file involved.
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
