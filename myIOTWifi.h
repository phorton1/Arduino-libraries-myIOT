//--------------------------
// myIOTWifi.h
//--------------------------

#pragma once

#include "myIOTTypes.h"


class myIOTWifi
{
    public:

        // myIOTWifi() {}
        // ~myIOTWifi() {}

        static void setup();
        static void loop();
        static const String &getIpAddress() { return m_ip_address; }

        static iotConnectStatus_t getConnectStatus()  { return m_connect_status; }
        static void connect(const String &sta_ssid, const String &sta_pass);
            // synonymous with begin() at this time, this is the entry point
            // to start wifi in either station or AP mode.

        static void disconnect();

        static void clearStopAP() { m_stop_ap = 0; }

        static void suppressAutoConnectSTA();

    private:

        static iotConnectStatus_t m_connect_status;
        static uint32_t m_stop_ap;
        static String m_ip_address;
        static bool m_suppress_auto_sta;
};

extern myIOTWifi my_iot_wifi;
