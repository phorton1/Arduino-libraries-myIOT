//--------------------------
// myIOTWifi.cpp
//--------------------------

#include "myIOTWifi.h"
#include "myIOTDevice.h"
#include "myIOTLog.h"
#include <WiFi.h>
#include <esp_wifi_types.h>

// Basic security scheme.
//
// The idea is that in AP Mode they will ONLY be able to log on to a local wifi network,
// and generally be unable to do anything else.  In STA mode everyting is open HTTP,
// Websockets, OTA, etc.
//
// We keep track of the AP connection in WifiEventHandler.cpp
// If we cannot connect as a STA, we start an AP.
// If someone connects to that AP, we suppress auto-reconnect (and any stopping of the AP).
// Once STA is connected, and they disconnect from the AP, it should go into STA only mode
//



#define DEBUG_WIFI      1

#define AP_IP       "192.168.1.254"
#define AP_MASK     "255.255.255.0"

#define WIFI_CONNECT_TIMEOUT    15000
#define WIFI_DISCONNECT_TIMEOUT 10000
#define STOP_AP_TIME            10000

// reconnect will try 6 times every 30 seconds, 180 secs total
// then 15 times every 6 minutes, upto 1.5 hours
// and every 15 minutes thereafter

#define STA_RECONNECT_TIME0       30

#define STA_RECONNECT_TRIES1      6
#define STA_RECONNECT_TIME1       360

#define STA_RECONNECT_TRIES2      21
#define STA_RECONNECT_TIME2       900


myIOTWifi    my_iot_wifi;

iotConnectStatus_t myIOTWifi::m_connect_status = IOT_CONNECT_NONE;
String myIOTWifi::m_ip_address;

uint32_t myIOTWifi::m_stop_ap;

static uint32_t g_reconnect = 0;
static int g_reconnect_tries = 0;




void myIOTWifi::setup()
{
    LOGD("myIOTWifi::setup() started");
    proc_entry();

    #if DEBUG_WIFI
        extern void initWifiEventHandler();
        initWifiEventHandler();
        delay(250);
    #endif

    WiFi.setHostname(my_iot_device->getName().c_str());
    WiFi.persistent(false);
        // not sure what this does.
        // trying it to keep from losing AP password
        // seems to help

    if (my_iot_device->getBool(ID_WIFI))
    {
        connect(
            my_iot_device->getString(ID_STA_SSID),
            my_iot_device->getString(ID_STA_PASS));
    }

    proc_leave();
    LOGD("myIOTServer::setup() finished");
}


void myIOTWifi::disconnect()
{
    LOGD("myIOTWifi::disconnect()");
    iotConnectStatus_t old_status = m_connect_status;

    m_connect_status = IOT_CONNECT_NONE;
    m_stop_ap = 0;
    g_reconnect = 0;

    if (old_status & IOT_CONNECT_STA)
    {
        LOGI("disconnecting station");
        WiFi.disconnect();   // station only
    }
    if (old_status & IOT_CONNECT_AP)
    {
        LOGI("disconnecting Access Point");
        WiFi.softAPdisconnect(true);  // false == leave AP on?
        delay(400);
    }

    LOGI("turning off wifi");
    WiFi.mode(WIFI_OFF);
}




void myIOTWifi::connect(const String &sta_ssid/*=String()*/, const String &sta_pass/*=String()*/)
{
    LOGD("myIOTWifi::connect() started");
    proc_entry();

    // TRY STATION CONNECTION

    wifi_mode_t mode = WiFi.getMode();

    if (sta_ssid != "")
    {
        m_connect_status &= ~IOT_CONNECT_STA;
        if (mode != WIFI_AP_STA)
        {
            WiFi.mode(WIFI_STA);
                // Weird - this changes a variable called "g_log_level"
            delay(400);
        }

        if (WiFi.status() == WL_CONNECTED)
        {
            WiFi.disconnect();   // station only
            LOGD("Disconnecting station ...");
            uint32_t start = millis();
            while (WiFi.status() == WL_CONNECTED)
            {
                if (millis() > start + WIFI_DISCONNECT_TIMEOUT)
                {
                    LOGE("Could not discconnect station");
                    break;
                }
                delay(500);
            }
        }

        #if DEBUG_PASSWORDS
            LOGD("Connecting to (%s:%s)",sta_ssid.c_str(),sta_pass.c_str());
        #else
            LOGD("Connecting to %s",sta_ssid.c_str());
        #endif

        bool timeout = false;
        uint32_t start = millis();
        WiFi.begin(sta_ssid.c_str(),sta_pass.c_str());

        // delay(400);

        while (WiFi.status() != WL_CONNECTED)
        {
            delay(500);
            if (millis() > start + WIFI_CONNECT_TIMEOUT)
            {
                timeout = true;
                break;
            }
        }

        if (timeout)
        {
            LOGW("Could not connect to %s",sta_ssid.c_str());
        }
        else
        {
            g_reconnect_tries = 0;
            m_connect_status |= IOT_CONNECT_STA;
            m_ip_address = WiFi.localIP().toString();
            LOGI("Connected to %s with IPAddress=%s",sta_ssid.c_str(),m_ip_address.c_str());

            if (m_connect_status & IOT_CONNECT_AP)
            {
                LOGD("setting AP disconnect timer");
                m_stop_ap = millis();
            }
            else
            {
                my_iot_device->onConnectStation();
            }
        }
    }

    // CREATE ACCESS POINT
    // if there's no station (invariantly regardless if the AP has previouisly been created)

    if (!(m_connect_status & IOT_CONNECT_STA))
    //  && !(m_connect_status & IOT_CONNECT_AP))
    {
        WiFi.mode(WIFI_AP_STA);     // default mode == both on
        delay(400);

        // The access point is given the user defined name.
        // If they have two bilgeAlarms, they should differentiate
        // them via prefs.

        #if DEBUG_PASSWORD
            LOGD("starting AP %s:%s",
                my_iot_device->getName().c_str(),
                my_iot_device->getString(ID_AP_PASS).c_str());
        #else
            LOGD("starting AP %s",
                my_iot_device->getName().c_str());
        #endif

        #define CHANNEL_11   11

        if (WiFi.softAP(
            my_iot_device->getName().c_str(),
            my_iot_device->getString(ID_AP_PASS).c_str(),
            CHANNEL_11))
        {
            delay(250);
            my_iot_device->onDisconnectAP();

            IPAddress ip;
            IPAddress mask;
            ip.fromString(AP_IP);
            mask.fromString(AP_MASK);

            delay(250);
            WiFi.softAPConfig(ip, ip, mask);

            m_ip_address = AP_IP;
            LOGI("AP %s started with ap_ip=%s softAPIP()=%s",
                my_iot_device->getName().c_str(),
                ip.toString().c_str(),
                WiFi.softAPIP().toString().c_str());
            m_connect_status |= IOT_CONNECT_AP;

            delay(250);
            my_iot_device->onConnectAP();
        }
        else
        {
            LOGE("AP did not start");
        }
    }

    proc_leave();
    LOGD("myIOTWifi::connect() finished");
}




void myIOTWifi::loop()
{
    if (!ap_connection_count && g_reconnect)
    {
        uint32_t use_millis =
            g_reconnect_tries > STA_RECONNECT_TRIES2 ? STA_RECONNECT_TIME2 :
            g_reconnect_tries > STA_RECONNECT_TRIES1 ? STA_RECONNECT_TIME1 :
            STA_RECONNECT_TIME0;
        use_millis *= 1000;

        if (millis() - g_reconnect >= use_millis)
        {
           LOGI("autoReconnecting(%d) to station after %d ms ...",g_reconnect_tries,use_millis);
           m_stop_ap = 0;
           g_reconnect = 0;
           connect(
               my_iot_device->getString(ID_STA_SSID),
               my_iot_device->getString(ID_STA_PASS));
        }
    }
    else if (
        !ap_connection_count &&
        g_reconnect == 0 &&
        my_iot_device->getBool(ID_WIFI) &&
        my_iot_device->getString(ID_STA_SSID) != "" &&
        WiFi.status() != WL_CONNECTED)
    {
        m_stop_ap = 0;
        m_connect_status &= ~IOT_CONNECT_STA;
        g_reconnect = millis();
        g_reconnect_tries++;
        LOGI("Station connection lost.  Setting reconnect(%d) timer",g_reconnect_tries);
    }


    if (!ap_connection_count && m_stop_ap && millis() > m_stop_ap + STOP_AP_TIME)
    {
        m_stop_ap = 0;
        LOGI("Stopping Access Point");
        WiFi.softAPdisconnect(true);  // false == leave AP on?
        m_connect_status &= ~IOT_CONNECT_AP;
        delay(400);
        WiFi.mode(WIFI_STA);    // required to turn off the AP
        my_iot_device->onConnectStation();
    }

}
