//--------------------------
// myIOTWebSockets.cpp
//--------------------------
// There was a fairly large problem that if client drops websocket connection,
// the device grinds to a near halt trying to send to the socket.
// Most browsers / OS's will close the socket, I think, so perhaps
// it was just from the rPi server.  I both updated my version of
// the arduinoWebsockets library and implemented a My::IOT::WSLocal::stop()
// method on the rPi, and that seems to have helped.


#include "myIOTWebSockets.h"

#if WITH_WS

#include "myIOTDevice.h"
#include "myIOTLog.h"

#include <ArduinoJson.h>
#include <SPIFFS.h>
#include <SD.h>
#include <FS.h>


#define WS_PORT 81
    // http port plus one
#define DEBUG_CAPTIVE_WS   0
    // If 1, the "captive_message" will contain debugging information
    // that can be shown by setting the javascript debug_captive=1.
    // If this define is 0, only the minimum information will be sent
    // to the AP client.

#define DEBUG_PING 0



WebSocketsServer myIOTWebSockets::m_web_sockets(WS_PORT);

static int connect_count = 0;


myIOTWebSockets::~myIOTWebSockets()
{}

myIOTWebSockets::myIOTWebSockets()
{}

// static
void myIOTWebSockets::begin()
{
    LOGI("myIOTWebSockets begin()");
    m_web_sockets.begin();
}

void myIOTWebSockets::setup()
{
    LOGD("myIOTWebSockets::setup() started");
    proc_entry();

    m_web_sockets.onEvent(webSocketEvent);
    if (my_iot_device->getBool(ID_DEVICE_WIFI))
        begin();

    #ifdef WS_WITH_TASK
        // Must run on ESP32_CORE_ARDUINO==1
        // Cannot run on ESP32_CORE_OTHER==0
        // see notes in bilgeAlarm.cpp lcdPrint()
        LOGI("starting webSocketTask pinned to core %d",ESP32_CORE_ARDUINO);
        xTaskCreatePinnedToCore(
            webSocketTask,
            "webSocketTask",
            8192,   // noticed crashes with multiple sockets open
            NULL,
            1,  	// priority
            NULL,   // handle
            ESP32_CORE_ARDUINO);
    #endif

    proc_leave();
    LOGD("myIOTWebSockets::setup() finished");
}


#ifdef WS_WITH_TASK

    #define DEBUG_WS_TASK_STACK 1

    void myIOTWebSockets::webSocketTask(void *param)
    {
        delay(1300);
        LOGI("Starting webSocketTask loop on core(%d)",xPortGetCoreID());
        #ifdef DEBUG_WS_TASK_STACK
            UBaseType_t saved = uxTaskGetStackHighWaterMark(NULL);
            LOGD("Starting webSocketTask stack=%d",saved);
        #endif
        while (1)
        {
            vTaskDelay(1);
            m_web_sockets.loop();
            #ifdef DEBUG_WS_TASK_STACK
                UBaseType_t high = uxTaskGetStackHighWaterMark(NULL);
                if (saved != high)
                {
                    saved = high;
                    LOGD("webSocketTask stack=%d",saved);
                }
            #endif
        }
    }
#endif


void myIOTWebSockets::loop()
{
#ifndef WS_WITH_TASK
    m_web_sockets.loop();
#endif
}

void myIOTWebSockets::broadcast(const char *msg)
{
    m_web_sockets.broadcastTXT(msg);
}

void myIOTWebSockets::onFileSystemChanged(bool sdcard)
{
    broadcast(fileDirectoryJson(sdcard).c_str());
}

void myIOTWebSockets::sendTXT(int num, const char *msg)
{
    m_web_sockets.sendTXT(num,msg);
}



static String myJson(const char *id, bool quoted, String value, bool comma_after)
{
    String rslt = "\"";
    rslt += id;
    rslt += "\":";
    if (quoted) rslt += "\"";
    rslt += value;
    if (quoted) rslt += "\"";
    if (comma_after) rslt += ",";
    return rslt;
}


String uint64ToString(uint64_t input) {
  String result = "";
  uint8_t base = 10;

  do {
    char c = input % base;
    input /= base;

    if (c < 10)
      c +='0';
    else
      c += 'A' - 10;
    result = c + result;
  } while (input);
  return result;
}



String myIOTWebSockets::fileDirectoryJson(bool sdcard)
    // only doing a flat listing of outer directory of SD card at this time (no navigation)
{
    String json_string = "{";

    json_string += "\"";
    json_string += sdcard ? "sdcard_list" : "spiffs_list";
    json_string += "\":{";

    uint64_t total = sdcard ? SD.totalBytes() : SPIFFS.totalBytes();
    uint64_t used = sdcard ? SD.usedBytes() : SPIFFS.usedBytes();
    json_string += myJson("sdcard",false,String(sdcard),true);
    json_string += myJson("total",false,uint64ToString(total),true);
    json_string += myJson("used",false,uint64ToString(used),true);
    json_string += "\n\"files\":[";

    bool started = false;
    File dir = sdcard ? SD.open("/") : SPIFFS.open("/");
    File entry = dir.openNextFile();
    while (entry)
    {
        if (!sdcard || !entry.isDirectory())
        {
            json_string += started ? ",\n" : "\n";
            json_string += "{";
            json_string += myJson("name",true,String(&entry.name()[1]),true);
            json_string += myJson("size",false,String(entry.size()),false);
            json_string += "}";
            started = true;
        }
        entry = dir.openNextFile();
    }
    dir.close();
    json_string += "]}}\n";
    return json_string;
}


void myIOTWebSockets::onDeleteFile(int num, String filename)
{
    bool sdcard = filename.startsWith("/sdcard/");
    if (sdcard)
        filename.replace("/sdcard/","/");
    else
        filename.replace("/spiffs/","/");
    const char *what = sdcard?"SDCARD ":"SPIFFS ";

    // using this reference apparently mungs the SDCARD
    //      fs::FS &the_fs = SPIFFS;
    //      if (sdcard) the_fs = SD;
    // also I cannot simply use an inline conditional
    // wihout complicated casts
    //      fs:FS &fs = sdcard ? SD : SPIFFS;
    // if (!the_fs.exists(filename) ||
    //     !the_fs.remove(filename))

    LOGU("DELETING %s%s",what,filename.c_str());

    bool ok = sdcard ?
        (SD.exists(filename) && SD.remove(filename)) :
        (SPIFFS.exists(filename) && SPIFFS.remove(filename));
    if (!ok)
    {
        String rslt = "{\"error\":\"";
        rslt += "Could not delete ";
        rslt += what;
        rslt += filename;
        rslt += "\"}";
        sendTXT(num,rslt.c_str());
    }

    onFileSystemChanged(sdcard);
}


String myIOTWebSockets::deviceInfoJson()
{
    DynamicJsonDocument doc(1024);

    wifi_mode_t mode = WiFi.getMode();

    doc["uuid"] = my_iot_device->getUUID();
    doc["device_type"] = my_iot_device->getDeviceType();
    doc["device_name"] = my_iot_device->getName();
    doc["version"] = myIOTDevice::getVersion();
    doc["iot_version"] = IOT_DEVICE_VERSION;
    doc["uptime"] = millis()/1000;
    String ap_pass = my_iot_device->getString(ID_AP_PASS);
    doc["iot_setup"] =  ap_pass != "" && ap_pass != DEFAULT_AP_PASSWORD ? 1 : 0;
    doc["has_sd"] =
        #if WITH_SD
            String(my_iot_device->hasSD());
        #else
            "0";
        #endif


#if DEBUG_CAPTIVE_WS
    // information added only for sake of debuging the captive Portal

    doc["wifi_mode"] =
        mode == WIFI_MODE_AP ? "WIFI_AP" :
        mode == WIFI_MODE_STA ? "WIFI_STA" :
        mode == WIFI_MODE_APSTA ? "WIFI_AP_STA" : "";

    iotConnectStatus_t status = my_iot_device->getConnectStatus();

    doc["iot_status"] =
        status == (IOT_CONNECT_AP | IOT_CONNECT_STA) ? "IOT_AP_STA" :
        status == IOT_CONNECT_STA ? "IOT_STA" :
        status == IOT_CONNECT_AP ? "IOT_AP" : "";

    doc["ap_ssid"] = mode == WIFI_MODE_AP || mode == WIFI_MODE_APSTA ?
        WiFi.softAPSSID() : "";
    doc["ap_ip"] = mode == WIFI_MODE_AP || mode == WIFI_MODE_APSTA ?
        WiFi.softAPIP().toString() : "";

    doc["sta_ssid"] = mode == WIFI_MODE_STA || mode == WIFI_MODE_APSTA ?
        WiFi.SSID() : "";
    doc["sta_ip"] = mode == WIFI_MODE_STA || mode == WIFI_MODE_APSTA ?
        WiFi.localIP().toString() : "";
    doc["sta_gateway"] = mode == WIFI_MODE_STA || mode == WIFI_MODE_APSTA ?
        WiFi.gatewayIP().toString() : "";

#endif

    String json_string;
    serializeJson(doc, json_string);
    return json_string;

}


void myIOTWebSockets::webSocketEvent(uint8_t num, WStype_t ws_type, uint8_t *data, size_t len)
{
    switch (ws_type)
    {
        case WStype_DISCONNECTED:
            LOGI("WS[%u] Disconnected!", num);
            connect_count--;
            break;
        case WStype_CONNECTED:
            {
                IPAddress ip = m_web_sockets.remoteIP(num);
                LOGI("WS[%u] Connected from %s url: %s", num, ip.toString().c_str(), data);
                connect_count++;
            }
            break;
        case WStype_TEXT:
            #if 1
                if (!strstr((const char *)data,"ping") &&
                    !strstr((const char *)data,"device_info") &&
                    (DEBUG_PASSWORDS || (
                        !strstr((const char *)data,"_PASS") &&
                        !strstr((const char *)data,"psk"))) )
                {
                    LOGD("");
                    LOGD("WS[%u] text: %s", num, data);
                }
            #endif

            StaticJsonDocument<255> in_doc;
            DeserializationError err = deserializeJson(in_doc, data);
            if (err == DeserializationError::Ok)
            {
                String cmd = in_doc["cmd"];
                if (cmd != "")
                {
                    if (DEBUG_PING ||
                       (cmd != "ping" &&
                        cmd != "device_info" &&
                        (DEBUG_PASSWORDS || (
                        !strstr((const char *)data,"_PASS") &&
                        !strstr((const char *)data,"psk")))))

                        LOGD("got command=%s",cmd.c_str());

                    if (cmd == "device_info")
                    {
                        sendTXT(num,deviceInfoJson().c_str());
                    }
                    else if (cmd == "join_network")
                    {
                        String ssid = (const char *) in_doc["ssid"];
                        String pass = (const char *) in_doc["psk"];

                        #if DEBUG_PASSWORD
                            LOGD("join_network(%s,%s)",ssid.c_str(),pass.c_str());
                        #else
                            LOGD("join_network(%s)",ssid.c_str());
                        #endif

                        my_iot_device->connect(ssid,pass);

                        if (my_iot_device->getConnectStatus() & IOT_CONNECT_STA)
                        {
                            my_iot_device->setString(ID_STA_SSID,ssid);
                            my_iot_device->setString(ID_STA_PASS,pass);
                            sendTXT(num,"{\"join_network\":\"OK\"}");
                            LOGD("Sent OK reply");
                        }
                        else
                        {
                            sendTXT(num,"{\"join_network\":\"FAIL\"}");
                            LOGD("Sent FAIL reply");
                        }
                    }
                    else if (cmd == "set_ap_pass")
                    {
                        String pass = (const char *) in_doc["psk"];
                        if (pass != "")
                        {
                            #if DEBUG_PASSWORDS
                                LOGD("Setting AP Password to %s",pass.c_str());
                            #else
                                LOGD("Setting AP Password");
                            #endif
                            my_iot_device->setString(ID_AP_PASS,pass);
                        }
                    }
                    else if (my_iot_device->getConnectStatus() != IOT_CONNECT_STA)
                    {
                        LOGE("illegal command in AP Mode: %s",cmd.c_str());
                    }

                    //--------------------------------------------------
                    // below here is ONLY in available in station mode
                    //--------------------------------------------------

                    else if (cmd == "ping")
                    {
                        #if DEBUG_PING
                            LOGD("WS(%d) got ping, sending pong",num);
                        #endif
                        sendTXT(num,"{\"pong\":1}");
                    }
                    else if (cmd == "invoke")
                    {
                        my_iot_device->invoke((const char *) in_doc["id"]);
                    }
                    else if (cmd == "set")
                    {
                        try
                        {
                            my_iot_device->setFromString(
                                (const char *) in_doc["id"],
                                (const char *) in_doc["value"],
                                VALUE_STORE_WS);
                        }
                        catch (String e)
                        {
                            LOGD("websockt caught %s",e.c_str());
                            String msg = "{\"error\":\"" + e + "\"}";
                            sendTXT(num,msg.c_str());

                            // if we send an error, we also resend the correct value

                            msg = my_iot_device->getAsWsSetCommand(in_doc["id"]);
                            if (msg)
                                sendTXT(num,msg.c_str());
                        }
                    }
                    else if (cmd == "spiffs_list")
                    {
                        sendTXT(num,fileDirectoryJson(0).c_str());
                    }
                    else if (cmd == "sdcard_list")
                    {
                        if (my_iot_device->hasSD())
                            sendTXT(num,fileDirectoryJson(1).c_str());
                        // else
                        //     LOGD("note: request for sdcard_list without my_iot_device->hasSD()");
                    }
                    else if (cmd == "delete_file")
                    {
                        onDeleteFile(num,in_doc["filename"]);
                    }
                    else if (cmd == "value_list")
                    {
                        sendTXT(num,my_iot_device->valueListJson().c_str());
                    }
                    else
                    {
                        LOGE("unknown WS command: %s",cmd.c_str());
                    }
                }
                else
                {
                    LOGE("WS could not find 'cmd' in ws request");
                }
            }
            else
            {
                LOGE("WS deserializeJson() failed with code %s",err.f_str());
            }

            break;
    }
}


#endif  // WITH_WS
