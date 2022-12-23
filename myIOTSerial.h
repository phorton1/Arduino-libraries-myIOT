//--------------------------------------------
// mySerial.h
//--------------------------------------------

#pragma once

#include "myIOTTypes.h"

#if WITH_TELNET
    #include <ESPTelnet.h>
#endif


class myIOTSerial
{
public:

    static void begin();
    static void loop();

    #if WITH_TELNET
        static ESPTelnet telnet;
        static bool telnetConnected();
    #endif


private:

    static void loop_private();
    static void serialTask(void *param);

};



