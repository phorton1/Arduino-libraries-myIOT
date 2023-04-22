//--------------------------
// myIOTDevice.cpp
//--------------------------
// memory
//      68%/12% at compile time
//          248 start ino setup
//          216 SD Card
//          187 | 172 Connected THX37
//          184 | 157 starting keyboard task
//          155..144 | 130..112  webUI reloads etc (steady state?)
// prh - could have a memory watchdog that reboots it every so often


#include "myIOTDevice.h"
#include "myIOTLog.h"
#include "myIOTSerial.h"
#include "myIOTWifi.h"
#include "myIOTHTTP.h"
#include <SPIFFS.h>
#include <FS.h>
#include <WiFi.h>
#include <esp_partition.h>
#include <rom/rtc.h>

#if WITH_WS
    #include "myIOTWebSockets.h"
#endif

#if WITH_MQTT
    #include "myIOTMQTT.h"
#endif

#if WITH_SD
    #include <SD.h>
#endif



#if WITH_NTP
    #define DEFAULT_NTP_SERVER  "pool.ntp.org"
#endif


// reminder
// PIN_SDCARD_CS = 5

#define BASE_UUID "38323636-4558-4dda-9188-"
    // We use a somewhat random UUID with the full MAC address
    // of the chip (6 bytes = 12 characters) postpended

#define REBOOT_TIME_DELAY  5000
    // after command we still dispatch info

//-----------------------------------
// what shows up in the "system" UI
//-----------------------------------

static valueIdType device_items[] = {
    ID_REBOOT,
    ID_DEVICE_NAME,
    ID_DEBUG_LEVEL,
    ID_LOG_LEVEL,
    ID_DEVICE_WIFI,
    ID_DEVICE_IP,
#if WITH_NTP
    ID_DEVICE_TZ,
    ID_NTP_SERVER,
#endif
    ID_AP_PASS,
    ID_STA_SSID,
    ID_STA_PASS,
#if WITH_MQTT
    ID_MQTT_IP,
    ID_MQTT_PORT,
    ID_MQTT_USER,
    ID_MQTT_PASS,
#endif

    ID_FACTORY_RESET,
    ID_LAST_BOOT,
    ID_UPTIME,
    ID_DEVICE_TYPE,
    ID_DEVICE_VERSION,
    ID_DEVICE_UUID,
    ID_DEVICE_BOOTING,
    0
};


static enumValue logAllowed[] = {
    "NONE",
    "USER",
    "ERROR",
    "WARNING",
    "INFO",
    "DEBUG",
    "VERBOSE",
    0};

static enumValue tzAllowed[] = {
    "EST - Panama",
    "EDT - New York",
    "CDT - Chicago",
    "MST - Phoenix",
    "MDT - Denver",
    "PDT - Los Angeles",
    0};



const valDescriptor myIOTDevice::m_base_descriptors[] =
{
    { ID_REBOOT,        VALUE_TYPE_COMMAND,    VALUE_STORE_MQTT_SUB,  VALUE_STYLE_VERIFY,     NULL,                       (void *) reboot },
    { ID_FACTORY_RESET, VALUE_TYPE_COMMAND,    VALUE_STORE_MQTT_SUB,  VALUE_STYLE_VERIFY,     NULL,                       (void *) factoryReset },
    { ID_VALUES,        VALUE_TYPE_COMMAND,    VALUE_STORE_PROG,      VALUE_STYLE_VERIFY,     NULL,                       (void *) showValues },
#if WITH_WS
    { ID_JSON,          VALUE_TYPE_COMMAND,    VALUE_STORE_PROG,      VALUE_STYLE_VERIFY,     NULL,                       (void *) showJson },
#endif

    { ID_LAST_BOOT,     VALUE_TYPE_TIME,       VALUE_STORE_PUB,       VALUE_STYLE_READONLY,   (void *) &_device_last_boot, },
    { ID_UPTIME,        VALUE_TYPE_INT,        VALUE_STORE_PUB,       VALUE_STYLE_HIST_TIME,  (void *) &_device_uptime,   NULL, { .int_range = { 0, -DEVICE_MAX_INT-1, DEVICE_MAX_INT}}  },

    { ID_DEVICE_BOOTING,VALUE_TYPE_BOOL,       VALUE_STORE_PUB,       VALUE_STYLE_READONLY,   (void *) &_device_booting,  },
    { ID_RESET_COUNT,   VALUE_TYPE_INT,        VALUE_STORE_PREF,      VALUE_STYLE_NONE,       NULL,                       NULL,  { .int_range = { 0, 0, DEVICE_MAX_INT}}  },

    { ID_DEVICE_UUID,   VALUE_TYPE_STRING,     VALUE_STORE_PUB,       VALUE_STYLE_READONLY,   (void *) &_device_uuid,     },
    { ID_DEVICE_TYPE,   VALUE_TYPE_STRING,     VALUE_STORE_PUB,       VALUE_STYLE_READONLY,   (void *) &_device_type,     },
    { ID_DEVICE_VERSION,VALUE_TYPE_STRING,     VALUE_STORE_PUB,       VALUE_STYLE_READONLY,   (void *) &_device_version,  },
    { ID_DEVICE_NAME,   VALUE_TYPE_STRING,     VALUE_STORE_PREF,      VALUE_STYLE_REQUIRED,   NULL,                       NULL,   "myIotDevice" },
    { ID_DEVICE_WIFI,   VALUE_TYPE_BOOL,       VALUE_STORE_PREF,      VALUE_STYLE_NONE,       (void *) &_device_wifi,     (void *) onChangeWifi, { .int_range = { DEFAULT_DEVICE_WIFI }} },
    { ID_DEVICE_IP,     VALUE_TYPE_STRING,     VALUE_STORE_PUB,       VALUE_STYLE_READONLY,   (void *) &_device_ip,       },

#if WITH_NTP
    { ID_DEVICE_TZ,     VALUE_TYPE_ENUM,       VALUE_STORE_PREF,      VALUE_STYLE_NONE,       (void *) &_device_tz,       (void *)onChangeTZ,   { .enum_range = { IOT_TZ_EST, tzAllowed }} },
    { ID_NTP_SERVER,   VALUE_TYPE_STRING,      VALUE_STORE_PREF,      VALUE_STYLE_NONE,       (void *) &_ntp_server,      NULL,   DEFAULT_NTP_SERVER },
#endif

    { ID_DEBUG_LEVEL,   VALUE_TYPE_ENUM,       VALUE_STORE_PREF,      VALUE_STYLE_NONE,       (void *) &iot_debug_level,  NULL,   { .enum_range = { LOG_LEVEL_DEBUG, logAllowed }} },
    { ID_LOG_LEVEL,     VALUE_TYPE_ENUM,       VALUE_STORE_PREF,      VALUE_STYLE_NONE,       (void *) &iot_log_level,    NULL,   { .enum_range = { LOG_LEVEL_NONE, logAllowed }} },
    { ID_AP_PASS,       VALUE_TYPE_STRING,     VALUE_STORE_PREF,      VALUE_STYLE_PASSWORD,   NULL,                       NULL,   DEFAULT_AP_PASSWORD },
    { ID_STA_SSID,      VALUE_TYPE_STRING,     VALUE_STORE_PREF,      VALUE_STYLE_NONE,       NULL,                       NULL,   "" },
    { ID_STA_PASS,      VALUE_TYPE_STRING,     VALUE_STORE_PREF,      VALUE_STYLE_PASSWORD,   NULL,                       NULL,   "" },
#if WITH_MQTT
    { ID_MQTT_IP,       VALUE_TYPE_STRING,     VALUE_STORE_PREF,      VALUE_STYLE_NONE,       },
    { ID_MQTT_PORT,     VALUE_TYPE_INT,        VALUE_STORE_PREF,      VALUE_STYLE_NONE,       NULL,                       NULL,   { .int_range = { 1883, 1, 65535 }} },
    { ID_MQTT_USER,     VALUE_TYPE_STRING,     VALUE_STORE_PREF,      VALUE_STYLE_NONE,       NULL,                       NULL,   "myIOTClient" },
    { ID_MQTT_PASS,     VALUE_TYPE_STRING,     VALUE_STORE_PREF,      VALUE_STYLE_PASSWORD,   NULL,                       NULL,   "1234" },
#endif
};

#define NUM_BASE_VALUES (sizeof(m_base_descriptors)/sizeof(valDescriptor))


//------------------------
// vars
//------------------------

RTC_NOINIT_ATTR int myIOTDevice::m_boot_count;

String myIOTDevice::_device_uuid;
String myIOTDevice::_device_type = IOT_DEVICE;
String myIOTDevice::_device_version = IOT_DEVICE_VERSION;

bool myIOTDevice::_device_wifi = DEFAULT_DEVICE_WIFI;

String myIOTDevice::_device_ip;

#if WITH_NTP
    IOT_TIMEZONE myIOTDevice::_device_tz;
    String myIOTDevice::_ntp_server;
#endif

time_t myIOTDevice::_device_last_boot;
int    myIOTDevice::_device_uptime;
bool   myIOTDevice::_device_booting;


#if WITH_SD
    bool   myIOTDevice::m_sd_started;
#endif

valueIdType *myIOTDevice::m_dash_items = NULL;
valueIdType *myIOTDevice::m_config_items = NULL;
valueIdType *myIOTDevice::m_device_items = device_items;

myIOTDevice *my_iot_device;



// static
void myIOTDevice::onChangeWifi(const myIOTValue *desc, bool val)
{
    LOGI("   onChangeWifi(%d)",val);
    if (val)
    {
        my_iot_wifi.connect(
            my_iot_device->getString(ID_STA_SSID),
            my_iot_device->getString(ID_STA_PASS));
        #if WITH_WS
            my_web_sockets.begin();
        #endif
    }
    else
    {
        #if WITH_WS
            my_web_sockets.end();
        #endif
        my_iot_wifi.disconnect();
    }
}



#if WITH_NTP
    // static
    void myIOTDevice::onChangeTZ(const myIOTValue *desc, uint32_t val)
    {
        const char *tz_string = tzString(static_cast<IOT_TIMEZONE>(val));
        LOGI("   onChangeTZ(%d)=%s",val,tz_string);
        // setting ESP32 timezone from https://randomnerdtutorials.com/esp32-ntp-timezones-daylight-saving/
        setenv("TZ", tz_string, 1);
        tzset();
    }
#endif



//----------------------------------------
// myIOTDevice
//----------------------------------------

myIOTDevice::myIOTDevice()
{
    _device_uuid = BASE_UUID;

    uint64_t mac = ESP.getEfuseMac();
    uint8_t  *p = (uint8_t *) &mac;
    for (int i=0; i<6; i++)
    {
        char buf[3];
        sprintf(buf,"%02x",p[i]);
        _device_uuid += buf;
    }

    my_iot_device = this;

    #if WITH_SD
        m_sd_started = false;
    #endif

    addValues(m_base_descriptors,NUM_BASE_VALUES);
}


myIOTDevice::~myIOTDevice()
{
}


static void showPartitions()
{
    LOGD("Partitions");
    proc_entry();

    for (int i=0; i<2; i++)
    {
        esp_partition_iterator_t iter = esp_partition_find(
            static_cast<esp_partition_type_t>(i),
            // ESP_PARTITION_TYPE_APP, // 0
            // ESP_PARTITION_TYPE_DATA, // 1
            // static_cast<esp_partition_type_t>( ESP_PARTITION_TYPE_DATA | ESP_PARTITION_TYPE_APP ),
            // (esp_partition_type_t)0xff,
            ESP_PARTITION_SUBTYPE_ANY,
            NULL);

        while (iter)
        {
            const esp_partition_t *p = esp_partition_get(iter);
            LOGD("PARTITION: %s type(%02x) subtype(%02x)  address(%08x) size(%x=%d) enc(%d)",
                p->label,
                p->type,
                p->subtype,
                p->address,
                p->size,
                p->size,
                p->encrypted);
            iter = esp_partition_next(iter);
        }

        esp_partition_iterator_release(iter);
    }
    proc_leave();
}



static void showSPIFFS()
{
    LOGD("SPIFFS listing");
    proc_entry();
    File dir = SPIFFS.open("/");
    File entry = dir.openNextFile();
    while (entry)
    {
        LOGD("%-8d %s",entry.size(),entry.name());
        entry = dir.openNextFile();
    }
    dir.close();
    uint32_t total = SPIFFS.totalBytes();
    uint32_t used  = SPIFFS.usedBytes();
    LOGD("total(%d) used(%d)",total,used);
    proc_leave();
}


#if WITH_SD
    bool myIOTDevice::initSDCard()
    {
        if (!m_sd_started)
            m_sd_started = SD.begin();
        return m_sd_started;
    }

    void myIOTDevice::showSDCard()
    {
        delay(1000);
        if (!m_sd_started)
        {
            LOGI("SD Card Not Present");
        }
        else
        {
            uint8_t cardType = SD.cardType();
            LOGI("SD Card Type: %s",
                cardType == CARD_NONE ? "NONE" :
                cardType == CARD_MMC  ? "MMC" :
                cardType == CARD_SD   ? "SDSC" :
                cardType == CARD_SDHC ? "SDHC" :
                "UNKNOWN");
            uint64_t cardSize = SD.cardSize() / (1024 * 1024);
            proc_entry();
            LOGI("SD Card Size: %lluMB", cardSize);
            LOGI("Total space:  %lluMB", (SD.totalBytes()+1024*1024-1) / (1024 * 1024));
            LOGI("Used space:   %lluMB", (SD.usedBytes()+1024*1024-1) / (1024 * 1024));
            bool sd_started = cardSize > 0;

            if (sd_started)
            {
                LOGD("Directory Listing");
                proc_entry();
                File dir = SD.open("/");
                File entry = dir.openNextFile();
                while (entry)
                {
                    LOGD("%-8d %s",entry.size(),entry.name());
                    entry = dir.openNextFile();
                }
                dir.close();
                proc_leave();
            }
            proc_leave();
        }
    }
#endif




void myIOTDevice::setup()
{
    LOGD("myIOTDevice::setup(%s) started",IOT_DEVICE_VERSION);
    proc_entry();

    if (!SPIFFS.begin())
        LOGE("Could not initialize SPIFFS");
    for (auto value:m_values)
        value->init();

    LOGI("iot_log_level=%d == %s",iot_log_level,getAsString(ID_LOG_LEVEL).c_str());
    LOGI("iot_debug_level=%d == %s",iot_debug_level,getAsString(ID_DEBUG_LEVEL).c_str());

    LOGU("DEVICE_NAME:    %s",getName().c_str());
    LOGU("DEVICE_TYPE:    %s",_device_type.c_str());
    LOGU("DEVICE_VERSION: %s",_device_version.c_str());
    LOGU("DEVICE_UUID:    %s",_device_uuid.c_str());
    LOGU("DEVICE_WIFI:    %d",_device_wifi);

    #if 0
        // value failure tests
        getBool("blah");
        getBool(ID_DEVICE_NAME);
        setInt(ID_DEBUG_LEVEL,100);
        invoke(ID_DEVICE_NAME);
    #endif

    //----------------------------
    // static debug methods
    //----------------------------

    //  Serial.print(valueListJson());
    showPartitions();
    showSPIFFS();

    //----------------------------
    // finish setting up
    //----------------------------

    #if WITH_SD
        initSDCard();
        showSDCard();
    #endif

    // We (must) reinitialize the RTC memory if either
    // RTCWDT_RTC_RESET or POWERON_RESET, or else it is random.
    // We do this AFTER the SD Carfd is started to facilitate
    // clients (i.e. bilgeAlarm) that want to use that to store
    // and restore persistent data.
    //
    // Here is a table showing empirical reboot reason testing
    //
    // NO CONSOLE:
    //      compile with "hard_reset":  RTCWDT_RTC_RESET
    //      pressing the reset button:  RTCWDT_RTC_RESET
    //      powering up:                RTCWDT_RTC_RESET
    //      reboot from menu:           SW_CPU_RESET
    //      OTA from device page:       SW_CPU_RESET
    // CONSOLE:
    //      start console:              POWERON_RESET
    //      compile with "hard_reset":  POWERON_RESET
    //      reboot from menu:           SW_CPU_RESET
    //      OTA from device page:       SW_CPU_RESET
    //
    // note that RTC memory is retained during my OTA

    int reason = rtc_get_reset_reason(0);
        // '0' is the core to check
    if (reason == RTCWDT_RTC_RESET ||
        reason == POWERON_RESET)
        onInitRTCMemory();
    LOGW("RESET_COUNT=%d BOOT_COUNT=%d",getInt(ID_RESET_COUNT),m_boot_count++);

    showIncSetupProgress();         // 2

    // I would like to change the system so that we consistently
    // allow setup() and associated tasks run invariantly,
    // eliminate unused loops() where there are tasks (with defines)
    // and use a consistent onConnectStatusChanged() method, where there
    // is a new mode IOT_CONNECT_OFF and IOT_CONNECT_ON is equal
    // to the current IOT_CONNECT_NONE, and IOT_CONNECT_OFF encapsulates
    // ID_DEVICE_WIFI except for the user event of turning it on or off.

    my_iot_wifi.setup();

        // currently contains a call to connect() if WIFI=1
        // would become my_iot_wifi.begin() if WIFI=1 and
        // my_iot_wifi.end() if WIFI=0 AFTER all setup()
        // methods and tasks are running.
        //
        // Then onChangeWifi() could just call begin() or
        // end() appropriately, and my_iot_wifi, in turn,
        // would call back with iotDevice:::onConnectStatusChanged()
        // which would then propogate downwards through all
        // objects.
        //
        // The loops() and tasks could then do nothing when
        //
        //
        // HOWEVER it is probably better to turn off the services
        // in a specific order BEFORE actually turnning the wifi
        // off. Each sub device would then have to be responsible.
        //
        // simple:  my_iot_wifi calls onConnectStatusChanged()
        // BEFORE disconnecting and AFTER connecting.


    showIncSetupProgress();         // 3 = last internal bilgeAlarm LED

    my_iot_http.setup();

    #if WITH_WS
        my_web_sockets.setup();
            // invariantly starts a task, but only
            // currently calls underlying begin() if WIFI==1
    #endif

    #if WITH_MQTT
        my_iot_mqtt.setup();
            // long unused implementation of mqtt invariantly starts a task
            // and always tries to connect to an mqtt server if !connected and
            // status==IOT_CONNECT_STA but has no 'disconnect' method if wifi
            // changes,
    #endif

    myIOTSerial::begin();
        // invariantly starts a task, but only
        // currently calls telnet.begin() if WIFI==1

    // everything after wifi setup is encapsulated in the
    // led's clearing at the top of loop() or in some later
    // derived class task.
    // showIncSetupProgress();         // 3

    // Finished

    // SET THE LAST_BOOT and UPTIME values
    // If they were not, but later connect to NTP, it will show 40 years.

    setTime(ID_LAST_BOOT,time(NULL));
    setInt(ID_UPTIME,_device_last_boot);

    proc_leave();
    LOGD("myIOTDevice::setup() completed");

}   // myIOTDevice::setup();



//--------------------------------------------------------
// pass thrus
//--------------------------------------------------------

void myIOTDevice::connect(const String &sta_ssid, const String &sta_pass)
{
    my_iot_wifi.connect(sta_ssid, sta_pass);
}

iotConnectStatus_t myIOTDevice::getConnectStatus()
{
    return my_iot_wifi.getConnectStatus();
}




void myIOTDevice::clearStopAP()
{
    my_iot_wifi.clearStopAP();
}

void myIOTDevice::onConnectStation()
{
    _device_ip = myIOTWifi::getIpAddress();
    my_iot_http.onConnectStation();
}

void myIOTDevice::onDisconnectAP()
{
    my_iot_http.onDisconnectAP();
}

void myIOTDevice::onConnectAP()
{
    _device_ip = myIOTWifi::getIpAddress();
    my_iot_http.onConnectAP();
}




#if WITH_WS
    void myIOTDevice::wsBroadcast(const char *msg)
    {
        my_web_sockets.broadcast(msg);
    }

    void myIOTDevice::onFileSystemChanged(bool sdcard)
    {
        my_web_sockets.onFileSystemChanged(sdcard);
    }
#endif

#if WITH_MQTT
    void myIOTDevice::publishTopic(const char *topic, String msg, bool retain /*=false*/)
    {
        my_iot_mqtt.publishTopic(topic,msg,retain);
    }
#endif


//--------------------------------------------------------
// commands
//--------------------------------------------------------

void myIOTDevice::reboot()
{
    LOGU("Rebooting ...");
    my_iot_device->setBool(ID_DEVICE_BOOTING,1);
    vTaskDelay(1500 / portTICK_PERIOD_MS);
    ESP.restart();
    while (1) {}
}

void myIOTDevice::factoryReset()
{
    LOGU("myIOTDevice::factoryReset()");
    int reset_count = my_iot_device->getInt(ID_RESET_COUNT);
    myIOTValue::initPrefs();
    my_iot_device->setInt(ID_RESET_COUNT,reset_count+1);
    reboot();
}


void myIOTDevice::showValues()
{
    LOGU("VALUES");
    for (auto value:my_iot_device->m_values)
    {
        if (value->getType() != VALUE_TYPE_COMMAND)
        {
            String val = value->getAsString();
            Serial.printf("%-15s = ",value->getId());
            Serial.println(val);

        #if WITH_TELNET
			if (myIOTSerial::telnetConnected())
            {
                static char tel_buf[80];
                sprintf(tel_buf,"%-15s = ",value->getId());
				myIOTSerial::telnet.print(tel_buf);
				myIOTSerial::telnet.println(val);
            }
		#endif

        }
    }
}



#if WITH_WS

    static String addIdList(const char *name, valueIdType *ptr)
    {
        String rslt = "";
        if (ptr && *ptr)
        {
            rslt += ",\n";
            rslt += "\"";
            rslt += name;
            rslt += "\":[";
            bool started = false;
            while (*ptr)
            {
                rslt += started ? ",\n" : "\n";
                started = true;
                rslt += "\"";
                rslt += *ptr++;
                rslt += "\"";
            }
            rslt += "]\n";
        }
        return rslt;
    }


    String myIOTDevice::valueListJson()
    {
        bool started = false;
        String rslt = "{\"values\":{";
        for (auto value:m_values)
        {
            if (started) rslt += ",";
            rslt += "\n";
            rslt += "\"";
            rslt += value->getId();
            rslt += "\":";
            rslt += value->getAsJson();
            started = true;
        }
        rslt += "}";

        rslt += addIdList("dash_items",m_dash_items);
        rslt += addIdList("config_items",m_config_items);
        rslt += addIdList("device_items",m_device_items);

        rslt += "}\n";
        return rslt;
    }


    void myIOTDevice::showJson()
    {
        LOGU("JSON");
        String val = my_iot_device->valueListJson();
        Serial.print(val);
        #if WITH_TELNET
			if (myIOTSerial::telnetConnected())
            {
				myIOTSerial::telnet.print(val);
            }
		#endif

    }
#endif


//--------------------------------------------
// handleCommand
//--------------------------------------------

String myIOTDevice::handleCommand(const String &raw_command, valueStore from /*= VALUE_STORE_PROG */)
{
    String command = "";
    for (int i=0; i<raw_command.length(); i++)
        if (raw_command[i] > ' ')
            command += raw_command[i];
    if (command == "")
        return "ok";

    if (DEBUG_PASSWORDS || command.indexOf("_PASS")<0)
        LOGD("handleCommand(%s)",command.c_str());

    int delim = command.indexOf('=');
    bool dyadic = delim >= 0;
    String left = dyadic ? command.substring(0,delim) : command;
    String right = dyadic ? command.substring(delim+1) : "";

    try
    {
        myIOTValue *value = findValueById(left.c_str());
        left = value->getId();
        valueType type = value->getType();

        if (type == VALUE_TYPE_COMMAND)
        {
            if (dyadic)
            {
                throw String("unexpected equals sign in command");
            }
            else
            {
                value->invoke();
                return "ok";
            }
        }

        if (dyadic)
        {
            value->setFromString(right,VALUE_STORE_SERIAL);
        }

        right = value->getAsString();
            // note that times are generally readonly and it is not expected
            // that they will be seen in handleCommand.

        String op = dyadic ? " <= " : " = ";
        return left + op + right;
    }
    catch (String e)
    {
        LOGE("error value(%s) %s",left.c_str(),e.c_str());
    }

    return "";

}   // myIOTDevice::handleCommand()





// #define PIN_POWER_SENSE  25
//
// static void enterSleepMode()
//     // go into deep sleep mode, keeping the rtc
//     // set the ULP to wake up on restoration of power
// {
//
// }

//--------------------------------------------
// loop
//--------------------------------------------

void myIOTDevice::loop()
{
    if (_device_wifi)
    {
        my_iot_wifi.loop();     // required
        my_iot_http.loop();     // required

        #if WITH_WS
        #ifndef WS_TASK
            my_web_sockets.loop();      // has a task
        #endif
        #endif

        #if WITH_MQTT
        #ifndef MQTT_TASK
            my_iot_mqtt.loop();         // has a task
        #endif
        #endif
    }

    #ifndef SERIAL_TASK
        myIOTSerial::loop();        // has a task
    #endif
}
