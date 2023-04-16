//--------------------------------------------
// mySerial.h
//--------------------------------------------

#pragma once

#include "myIOTTypes.h"

#if WITH_TELNET
    #include <ESPTelnet.h>
#endif

#define SERIAL_TASK


class myIOTSerial
{
public:

    static void begin();

	#ifndef SERIAL_TASK
		static void loop();
	#endif

    #if WITH_TELNET
		// the myIOTSerial device is responsible for starting and
		// stopping the telnet server when getConnectStatus() changes
        static ESPTelnet telnet;
        static bool telnetConnected();
    #endif


private:

    static void loop_private();
    static void serialTask(void *param);

};
