//--------------------------
// myIOTWifi.cpp
//--------------------------
// There is an issue that if the STA_SSID is set, but the Wifi network is not available,
// so you try to use the AP mode to reconnect, that in the middle of that it will try
// reconnecting to the STA, thus breaking the AP connection (channel number difference?).
// THE WHOLE DEVICE APPEARS TO CRASH.
//
// This is different than setting the STA_SSID to blank, which then allows plenty of
// time to connect to the AP and during which the device appears to run ok.
//
// The conflict also presents itself as a re-entrancy to connect(), which leads
// to weird debugging output.
//
// I am thinking the proper solution is to give better (complete) control to the user
// over the AP mode, including allowing the device to be used as an access point.
//
// This is a fairly major change to the way I've done the Captive Portal since the
// very beginning and leads to an architectural review of the entire networking
// capabililty of the device, including possibly revisiting MQTT (with an ESP32
// providing the LAN and server) or things like ESP_NOW (for a completely local
// system based entirely on ESP32's)
//
// At a minimum we are going to implement the notion that if we are in (fallback to)
// AP mode, and anybody "hits" the captive portal page, that we will stop trying to
// connect to the previous STATION at least until a reboot.  This at least has the
// effect that if the auto-reconnect kills the AP during a first connection to it,
// due to the fact that auto-reconnect happens fapidly, that subsequent to the first
// AP connection, a second AP connection will remain persistent and usable.
//
// This barely gets us through.   We can now usually connect to the AP at least
// by the second try and get the device back onto the LAN, but then the rPi server
// (who is also trying over and over to reconnect to it) finds it BEFORE the AP mode
// has been turned off, resulting (currently) in IT getting "illegal commands" for
// things like the spiffs list, and values, which it then basically never retries,
// so to it, the bilgeAlarm looks like it's stuck "rebooting", until I reboot the
// rpi server ... sheesh.
//
// Once again, a cleanup of this whole mess is in order and I think it revolves
// over having more explicit control of the AP/STA modes (via the device screen
// UI if present) and/or being able to "use" the device while in AP mode (so
// as to set/allow "station" mode).
//
// It is almost as if it should come in AP mode fully ready to go (with the possible
// exception of forcing the user to change the AP password) and THEN have a UI to
// allow them to connect to a station (CONNECT STA button) which, in turn, also sets
// the mode to STA_AP or something.
//
// It more or less CANNOT be allowed to fail commands when connected via station
// mode or else I have to change the rPi architecture/timing too ....
//
// Maybe widen the time window for wifi-retries to allow a reasonable amount of time
// to get to AP mode?   There is always still the problem that you could hit it when
// it just startin

#include "myIOTWifi.h"
#include "myIOTDevice.h"
#include "myIOTLog.h"
#include <WiFi.h>
#include <esp_wifi_types.h>
#include <time.h>

// Basic security scheme.
//
// The idea is that in AP Mode they will ONLY be able to log on to a local wifi network,
// and generally be unable to do anything else.  In STA mode everyting is open HTTP,
// Websockets, OTA, etc.


#define DEBUG_WIFI      1

#define WIFI_CONNECT_TIMEOUT    15000
#define WIFI_DISCONNECT_TIMEOUT 10000
#define STOP_AP_TIME            10000
#define AP_RECONNECT_TIME       10000

#define AP_IP       "192.168.1.254"
#define AP_MASK     "255.255.255.0"


iotConnectStatus_t myIOTWifi::m_connect_status = IOT_CONNECT_NONE;
uint32_t myIOTWifi::m_stop_ap;
bool myIOTWifi::m_suppress_auto_sta;
String myIOTWifi::m_ip_address;

static uint32_t g_reconnect = 0;


void myIOTWifi::suppressAutoConnectSTA()
{
    LOGI("SUPPRESSING STA AUTO RECONNECT IN AP MODE");
    m_suppress_auto_sta = true;
}



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

    if (my_iot_device->getBool(ID_DEVICE_WIFI))
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
    // if there's no station and the AP has not been created

    if (!(m_connect_status & IOT_CONNECT_STA) &&
        !(m_connect_status & IOT_CONNECT_AP))
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
    if (!m_suppress_auto_sta && g_reconnect && millis() > g_reconnect + AP_RECONNECT_TIME)
    {
        LOGI("autoReconnecting to station...");
        m_stop_ap = 0;
        g_reconnect = 0;
        connect(
            my_iot_device->getString(ID_STA_SSID),
            my_iot_device->getString(ID_STA_PASS));
    }
    else if (
        g_reconnect == 0 &&
        my_iot_device->getBool(ID_DEVICE_WIFI) &&
        my_iot_device->getString(ID_STA_SSID) != "" &&
        WiFi.status() != WL_CONNECTED)
    {
        LOGI("Station connection lost.  Setting reconnect timer");
        m_stop_ap = 0;
        m_connect_status &= ~IOT_CONNECT_STA;
        g_reconnect = millis();
    }


    if (m_stop_ap && millis() > m_stop_ap + STOP_AP_TIME)
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
