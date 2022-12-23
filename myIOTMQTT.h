//--------------------------
// myIOTMQTT.h
//--------------------------

#pragma once

#include "myIOTTypes.h"

#if WITH_MQTT

#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>

class myIOTMQTT
{
    public:

        myIOTMQTT();
        ~myIOTMQTT();

        void setup();
        void loop();

        void publishTopic(const char *name, String value, bool retain = false);


    protected:

        static myIOTMQTT *m_this;

        WiFiClient m_wifi_client;
        PubSubClient m_mqtt_client;

        void MQTTConnect();
        static void MQTTConnectTask(void *is_task);
        static void MQTTCallback(char* ctopic, byte* cmsg, unsigned int len);

};



#endif // WITH_MQTT