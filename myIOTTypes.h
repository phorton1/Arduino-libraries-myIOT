//--------------------------
// myIOTTypes.h
//--------------------------

#pragma once

#include <Arduino.h>
#include <WiFi.h>



#define DEBUG_PASSWORDS  0

#define DEVICE_MAX_INT   2147483647L
#define DEVICE_MIN_INT   -2147483648L

// default settings are for my generic IOT device (bilgeAlarm)
// I can modify this at compile time via /base/bat/setup_platform.pm


#ifndef WITH_WS
    #define WITH_WS     1
#endif

#ifndef WITH_NTP
    #define WITH_NTP    1
#endif

#ifndef WITH_MQTT
    #define WITH_MQTT   0
#endif

#ifndef WITH_TELNET
    #define WITH_TELNET 1
#endif

#ifndef WITH_SD
    #define WITH_SD     0
#endif

#ifndef WITH_AUTO_REBOOT
    #define WITH_AUTO_REBOOT  0
#endif

#ifndef DEFAULT_DEVICE_WIFI
    #define DEFAULT_DEVICE_WIFI 1
#endif


#define ESP32_CORE_ARDUINO     1        // the core on which setup() and loop() run
#define ESP32_CORE_OTHER       0        // the unused core which is default for xCreateTask

// IMPORTANT NOTE:  THE LCD ONLY WORKS FROM THE SAME CORE IT WAS INITED ON.
// THE FOLLOWING TASKS MUST BE STARTED WITH xTaskCreatePinnedToCore(1)
// as the lcd is initialized on ESP32_CORE_ARDUINO
//
//      main bilgeAlarm::stateTask()
//      myIOTSerial::serialTask()
//
// It appears to be ok if the WebSocket task runs on Core 0 as it somehow calls back
// on core 1 in it's webSocketEvent, even though the loop() itself is running on core 0.



//----------------------------------------
// myIOTDevice pins
//----------------------------------------

#if WITH_SD      // reminder
    // #define PIN_SD_CS        5
    // #define PIN_SPI_MOSI     23
    // #define PIN_SPI_CLK      18
    // #define PIN_SPI_MISO     23
#endif

#if WITH_POWER
    #define PIN_MAIN_VOLTAGE   35       // 3.3V == 18.5V
    #define PIN_MAIN_CURRENT   34       // Divide 0..5V to 0..2.5V   1024 = 0 Amps  2048=5A   0=-5A
#endif



//----------------------------------------------
// Value Descriptors
//----------------------------------------------

#define VALUE_TYPE_COMMAND 'X'        // monadic (commands)
#define VALUE_TYPE_BOOL    'B'        // a boolean (0 or 1)
#define VALUE_TYPE_CHAR    'C'        // a single character
#define VALUE_TYPE_STRING  'S'        // a string
#define VALUE_TYPE_INT     'I'        // a signed 32 bit integer
#define VALUE_TYPE_TIME    'T'        // time stored as 32 bit unsigned integer
    // times are generally readonly by the UI
    // they are set and retrieved as uint32_t's
    // for TIME_SINCE, the UI can add the clock ticks
#define VALUE_TYPE_FLOAT   'F'        // a float
#define VALUE_TYPE_ENUM    'E'        // enumerated integer
#define VALUE_TYPE_BENUM   'J'        // bitwise integer
    // BENUM value strings represent leftmost bits first and are implicitly 'packed'
    // so values for 1,2,4,8,16,32 etc are presented.  Currently implemented as debug
    // feature to output bitwise machine states, they are not generally UI inputs,
    // but if they were, the input would be a decimal integer, unless a parser for
    // the OR, NOT, etc operators was implemented.  The range is essentially unlimited
    // and all bit values? may be used.  They have an implicit default value of 0.


#define VALUE_STORE_PROG      0x00      // only in ESP32 memory (or not anywhere at all)
#define VALUE_STORE_NVS       0x01      // stored/retrieved from NVS (EEPROM)
#define VALUE_STORE_WS        0x02      // broadcast to / received from WebSockets
#define VALUE_STORE_MQTT_PUB  0x04      // published to (the) MQTT broker
#define VALUE_STORE_SUB       0x08      // subscribed from (the) MQTT broker
#define VALUE_STORE_SERIAL    0x10      // recieved from serial port

#define VALUE_STORE_PREF      (VALUE_STORE_NVS | VALUE_STORE_WS)
#define VALUE_STORE_TOPIC     (VALUE_STORE_MQTT_PUB | VALUE_STORE_SUB | VALUE_STORE_WS)
#define VALUE_STORE_PUB       (VALUE_STORE_MQTT_PUB | VALUE_STORE_WS)
    // publish only topic, also to WS - used for items only stored in RAM or RTC memory


#define VALUE_STYLE_NONE        0x0000      // no special styling
#define VALUE_STYLE_READONLY    0x0001      // Value may not be modified except by PROG
#define VALUE_STYLE_REQUIRED    0x0002      // String item may not be blank
#define VALUE_STYLE_PASSWORD    0x0004      // displayed as '********', protected in debugging, etc. Gets "retype" dialog in UI
    // by convention, for hiding during debugging, password elements should be named with "_PASS" in them,
    // and the global define DEBUG_PASSWORDS implemented to ensure they are not, or are
    // displayed in LOGN calls.
#define VALUE_STYLE_TIME_SINCE  0x0008      // UI shows '23 minutes ago' in addition to the time string
#define VALUE_STYLE_VERIFY      0x0010      // UI command buttons will display a confirm dialog
#define VALUE_STYLE_LONG        0x0020      // UI will show a long (rather than default 15ish) String Input Control
#define VALUE_STYLE_OFF_ZERO    0x0040      // Allows 0 below min and displays it as "OFF"
#define VALUE_STYLE_RETAIN      0x0100      // MQTT if published, will be "retained"
    // CAREFUL with the use of MQTT retained messages!!
    // They can only be cleared on the rpi with:
    //      sudo service mosquitto stop
    //      sudo rm /var/lib/mosquitto/mosquitto.db
    //      sudo service mosquitto start
#define VALUE_STYLE_HIST_TIME    (VALUE_STYLE_READONLY | VALUE_STYLE_TIME_SINCE)

typedef const char *valueIdType;
typedef char        valueType;
typedef uint8_t     valueStore;
typedef uint16_t    valueStyle;
typedef const char *enumValue;


typedef struct {

    valueIdType id;
    valueType   type;
    valueStore  store;
    valueStyle  style;

    void *val_ptr;      // pointer to actual value in memory
    void *fxn_ptr;      // onChange function to be called (command)

    union {
        const char *default_value;
        struct {
            const int default_value;
            const int int_min;
            const int int_max;
        } int_range;
        struct {
            const float default_value;
            const float float_min;
            const float float_max;
        } float_range;
        struct {
            const uint32_t  default_value;
            enumValue       *allowed;
        } enum_range;
    };

} valDescriptor;


//----------------------------------------------
// myIOTDevice
//----------------------------------------------


#define IOT_CONNECT_NONE   0x00
#define IOT_CONNECT_STA    0x01
#define IOT_CONNECT_AP     0x02
#define IOT_CONNECT_ALL    0x03

typedef uint8_t iotConnectStatus_t;
