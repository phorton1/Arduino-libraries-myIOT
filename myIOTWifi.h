//--------------------------
// myIOTWifi.h
//--------------------------

#pragma once

#include "myIOTTypes.h"


extern int ap_connection_count;
    // in wifiEventHandler.cpp


class myIOTWifi
{
    public:

        static void setup();
        static void loop();
        static const String &getIpAddress() { return m_ip_address; }

        static iotConnectStatus_t getConnectStatus()  { return m_connect_status; }
        static void connect(const String &sta_ssid, const String &sta_pass);
            // synonymous with begin() at this time, this is the entry point
            // to start wifi in either station or AP mode.
        static void disconnect();

    private:

        static iotConnectStatus_t m_connect_status;
        static String m_ip_address;
        static uint32_t m_stop_ap;
};


extern myIOTWifi my_iot_wifi;
