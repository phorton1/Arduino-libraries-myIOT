//--------------------------
// myIOTMQTT.cpp
//--------------------------

#include "myIOTMQTT.h"

#if WITH_MQTT

#include "myIOTDevice.h"
#include "myIOTLog.h"
#include <WiFi.h>
#include <PubSubClient.h>


#define MQQT_TASK_STACK   4096
    // MQQT must use a task if there is a likelyhood of the server
    // not being available (and ID_MQTT_IP is set) or else the system
    // becomes unresponsive (i.e. the WebUI js keep alive mechanism fails)
    // as the connect hangs the main thread.


myIOTMQTT my_iot_mqtt;
static WiFiClient wifi_client;
static PubSubClient mqtt_client(wifi_client);



myIOTMQTT::~myIOTMQTT()
{}


myIOTMQTT::myIOTMQTT()
{
    LOGD("myIOTMQTT::ctor()");
}


void myIOTMQTT::setup()
{
    LOGD("myIOTMQTT::setup() started");
    proc_entry();

    #ifdef MQTT_TASK
        LOGI("starting MQTT Task");
        xTaskCreate(MQTTConnectTask,
            "MQTTConnectTask",
            MQQT_TASK_STACK,
            (void *)1,  // is_task
            1,  	// priority
            NULL);
    #endif

    proc_leave();
    LOGD("myIOTMQTT::setup() finished");
}



void myIOTMQTT::publishTopic(const char *name, String value, bool retain /*=false*/)
{
    LOGD("myIOTMQTT::publishTopic(%s,%d)='%s'",name,retain,value.c_str());
    if (mqtt_client.connected())
    {
        // Topics are prefixed using the user-defined thing name, whatever that is
        String topic = my_iot_device->getName() + "/" + name;
        LOGD("    sending topic(%s,%d)='%s'",topic.c_str(),retain,value.c_str());
        if (!mqtt_client.publish(topic.c_str(),value.c_str(),retain))
            LOGE("could not publish topic(%s,%d)='%s'",topic.c_str(),retain,value.c_str());
    }
}


// static
void myIOTMQTT::MQTTCallback(char* ctopic, byte* cmsg, unsigned int len)
{
    String msg;
    String topic(ctopic);
    for (int i = 0; i < len; i++)
        msg += (char)cmsg[i];

    LOGD("MQTTCallback(%s,%s)",topic.c_str(),msg.c_str());
    proc_entry();

    // Topics are prefixed using the user-defined thing name, whatever that is
    // which is stripped off before calling setFromString()

    String prefix = my_iot_device->getName() + "/";
    if (topic.startsWith(prefix))
    {
        topic = topic.substring(prefix.length());
    }

    my_iot_device->setFromString(topic.c_str(),msg,VALUE_STORE_SUB);
    proc_leave();
}


// static
void myIOTMQTT::MQTTConnect()
{
    if (!mqtt_client.connected())
    {
        static int req_num = 0;

        String client_id(req_num++);
        String mqtt_user = my_iot_device->getString(ID_MQTT_USER);
        String mqtt_pass = my_iot_device->getString(ID_MQTT_PASS);

        LOGD("MQTTConnect(%s,%s,%s)",client_id.c_str(),mqtt_user.c_str(),mqtt_pass.c_str());
        proc_entry();

        if (mqtt_client.connect(client_id.c_str(),mqtt_user.c_str(),mqtt_pass.c_str()))
        {
            LOGI("MQTT Connected");
            mqtt_client.setCallback(MQTTCallback);
            for (auto value:my_iot_device->getValues())
            {
                valueStore store = value->getStore();
                if (store & (VALUE_STORE_SUB | VALUE_STORE_MQTT_PUB))
                {
                    if (store & VALUE_STORE_SUB)
                    {
                    // Topics are prefixed using the user-defined thing name, whatever that is
                        String topic = my_iot_device->getName() + "/";
                        topic += value->getId();
                        LOGD("    connect subscribing to topic(%s)",topic.c_str());
                        mqtt_client.subscribe(topic.c_str());
                    }

                    if (store & VALUE_STORE_MQTT_PUB &&
                        !(value->getStyle() & VALUE_STYLE_RETAIN))
                    {
                        String val = value->getAsString();
                        LOGD("    connect publishing topic(%s)='%s'",value->getId(),val.c_str());
                        publishTopic(value->getId(),val);
                    }
                }
            }
        }
        else
        {
            LOGW("MQTT Connect failed status code =%d",mqtt_client.state());
        }

        proc_leave();
    }
}



void myIOTMQTT::MQTTConnectTask(void *is_task)
    // if param is not NULL this is a real task,
    // otherwise it's a timeslice called from loop()
{
    if (is_task)
    {
        vTaskDelay(2000);
        LOGI("starting MQQTConnectTask loop on core(%d)",xPortGetCoreID());
    }

    bool first_time = true;
    while (first_time || is_task)
    {
        first_time = false;
        if (is_task)
            vTaskDelay(1);

        if (my_iot_device->getConnectStatus() == IOT_CONNECT_STA)
        {
            static uint32_t last_uptime = 0;
            if (millis() > last_uptime + 5000)
            {
                last_uptime = millis();
                String mqtt_ip = my_iot_device->getString(ID_MQTT_IP);
                int mqtt_port = my_iot_device->getInt(ID_MQTT_PORT);
                if (mqtt_ip != "" && mqtt_port)
                {
                    if (!mqtt_client.connected())
                    {
                        mqtt_client.setServer(mqtt_ip.c_str(), mqtt_port);
                        my_iot_mqtt.MQTTConnect();
                        last_uptime = millis();
                    }
                }
            }

            // if (mqtt_client.connected())
                mqtt_client.loop();
        }
    }
}


#ifndef MQTT_TASK
    void myIOTMQTT::loop()
    {
        MQTTConnectTask(NULL);
    }
#endif


#endif  // WITH_MQTT
