//-------------------------------------------
// myIOTSerial.cpp
//--------------------------------------------
#include "myIOTSerial.h"
#include "myIOTDevice.h"
#include "myIOTLog.h"



#define KBD_TASK_STACK   4096


#define MAX_KBD 128
static int kbd_len = 0;
static char kbd_buf[MAX_KBD+1];

//--------------------------------
// telnet
//--------------------------------

#if WITH_TELNET
    static bool telnet_started = false;
    static bool telnet_connected = false;

    ESPTelnet myIOTSerial::telnet;


    bool myIOTSerial::telnetConnected()
    {
        return telnet_connected;
    }

    void onTelnetConnect(String ip)
    {
        LOGI("Telnet connected from %s",ip.c_str());
        telnet_connected = true;
        LOGU("%s(%s) %s %s",
            my_iot_device->getDeviceType(),
            my_iot_device->getName().c_str(),
            myIOTDevice::getVersion(),
            IOT_DEVICE_VERSION);
    }

    void onTelnetDisconnect(String ip)
    {
        telnet_connected = false;
        LOGI("Telnet %s disconnected",ip.c_str());
    }

    void onTelnetReconnect(String ip)
    {
        telnet_connected = true;
        LOGI("Telnet reconnected from %s",ip.c_str());
    }

    void onTelnetConnectionAttempt(String ip)
    {
        LOGI("Telnet %s tried to connect",ip.c_str());
    }

    void onTelnetReceived(String command)
    {
        // LOGI("Telnet Received(%d): '%s'",command.length(),command.c_str());
        String rslt = my_iot_device->handleCommand(command);
        if (rslt != "")
            LOGU(rslt.c_str());
    }


    void init_telnet()
    {
        myIOTSerial::telnet.onConnect(onTelnetConnect);
        myIOTSerial::telnet.onConnectionAttempt(onTelnetConnectionAttempt);
        myIOTSerial::telnet.onReconnect(onTelnetReconnect);
        myIOTSerial::telnet.onDisconnect(onTelnetDisconnect);
        myIOTSerial::telnet.onInputReceived(onTelnetReceived);
    }

    static void telnet_loop()
    {
        if (my_iot_device->getConnectStatus() == IOT_CONNECT_STA)
        {
            if (telnet_started)
            {
                myIOTSerial::telnet.loop();
            }
            else
            {
                LOGI("Starting telnet service");
                myIOTSerial::telnet.begin();
                telnet_started = true;
            }
        }
        else if (telnet_started)
        {
            LOGI("Stopping telnet service");
            myIOTSerial::telnet.stop();
            telnet_started = false;
        }
    }

#endif

//------------------------------------------
// myIOTSerial
//------------------------------------------

myIOTSerial  my_iot_serial;

void myIOTSerial::begin()
{
    #if WITH_TELNET
        init_telnet();
    #endif

    #ifdef SERIAL_TASK
        // Must run on ESP32_CORE_ARDUINO==1
        // Cannot run on ESP32_CORE_OTHER==0
        // see notes in bilgeAlarm.cpp lcdPrint()
        LOGI("starting serialTask pinned to core %d",ESP32_CORE_ARDUINO);
        xTaskCreatePinnedToCore(
            serialTask,
            "serialTask",
            KBD_TASK_STACK,
            NULL,
            1,  	// priority
            NULL,   // handle
            ESP32_CORE_ARDUINO);
    #endif



}


void myIOTSerial::serialTask(void *param)
{
    delay(1000);
    LOGI("starting serialTask loop on core(%d)",xPortGetCoreID());
    while (1)
    {
        vTaskDelay(1);
        loop_private();
    }
}




void myIOTSerial::loop_private()
{
    while (Serial.available())
    {
        char c = Serial.read();
        if (c == 0x0a)
        {
            kbd_buf[kbd_len++] = 0;
            String rslt = my_iot_device->handleCommand(String(kbd_buf));
            kbd_len = 0;
            if (rslt != "")
                LOGU(rslt.c_str());
        }
        else if (c == 0x08)
        {
            if (kbd_len)
                kbd_len--;
        }
        else if (c != 0x0d)
        {
            kbd_buf[kbd_len++] = c;
        }
    }

    #if WITH_TELNET
        telnet_loop();
    #endif
}



#ifndef SERIAL_TASK
    void myIOTSerial::loop()
    {
        loop_private();
        delay(1);
    }
#endif