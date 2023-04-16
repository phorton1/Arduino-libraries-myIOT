//--------------------------
// myIOTMQTT.h
//--------------------------
// NEEDS WORK

#pragma once

#include "myIOTTypes.h"

#if WITH_MQTT

#define MQTT_TASK


class myIOTMQTT
{
    public:

        myIOTMQTT();
        ~myIOTMQTT();

        void setup();

        #ifndef MQTT_TASK
            void loop();
        #endif

        void publishTopic(const char *name, String value, bool retain = false);


    protected:

        void MQTTConnect();
        static void MQTTConnectTask(void *is_task);
        static void MQTTCallback(char* ctopic, byte* cmsg, unsigned int len);

};


extern myIOTMQTT my_iot_mqtt;




#endif // WITH_MQTT