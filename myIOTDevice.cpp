//--------------------------
// myIOTDevice.cpp
//--------------------------

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

#define DEFAULT_DEVICE_SSDP  1

// compile options for logging

#ifndef DEFAULT_LOG_COLORS
    #define DEFAULT_LOG_COLORS  0
#endif
#ifndef DEFAULT_LOG_DATE
    #define DEFAULT_LOG_DATE  1
#endif
#ifndef DEFAULT_LOG_TIME
    #define DEFAULT_LOG_TIME  1
#endif
#ifndef DEFAULT_LOG_MEM
    #define DEFAULT_LOG_MEM  0
#endif

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
    ID_WIFI,
    ID_SSDP,
    ID_DEVICE_IP,
#if WITH_NTP
    ID_TIMEZONE,
    ID_NTP_SERVER,
#endif
    ID_AP_PASS,
    ID_STA_SSID,
    ID_STA_PASS,

    ID_DEGREE_TYPE,
#if WITH_SD
    ID_RESTART_SD,
#endif
    ID_DEBUG_LEVEL,
    ID_LOG_LEVEL,
    ID_LOG_COLORS,
    ID_LOG_DATE,
    ID_LOG_TIME,
    ID_LOG_MEM,

#if WITH_AUTO_REBOOT
    ID_AUTO_REBOOT,
#endif
    ID_FACTORY_RESET,
    ID_LAST_BOOT,
    ID_UPTIME,
    ID_DEVICE_TYPE,
    ID_DEVICE_VERSION,
    ID_DEVICE_UUID,
    ID_DEVICE_URL,
    ID_DEVICE_BOOTING,

#if WITH_MQTT
    ID_MQTT_IP,
    ID_MQTT_PORT,
    ID_MQTT_USER,
    ID_MQTT_PASS,
#endif

    0,
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

static enumValue temperatureAllowed[] = {
    "Centigrade",
    "Farenheit",
    0};


// in order of presentation via UI 'values' command

const valDescriptor myIOTDevice::m_base_descriptors[] =
{
    { ID_REBOOT,        VALUE_TYPE_COMMAND,    VALUE_STORE_SUB,       VALUE_STYLE_VERIFY,     NULL,                       (void *) reboot },
    { ID_FACTORY_RESET, VALUE_TYPE_COMMAND,    VALUE_STORE_SUB,       VALUE_STYLE_VERIFY,     NULL,                       (void *) factoryReset },
    { ID_VALUES,        VALUE_TYPE_COMMAND,    VALUE_STORE_PROG,      VALUE_STYLE_VERIFY,     NULL,                       (void *) showValues },
    { ID_PARAMS,        VALUE_TYPE_COMMAND,    VALUE_STORE_PROG,      VALUE_STYLE_VERIFY,     NULL,                       (void *) showAllParameters },
#if WITH_WS
    { ID_JSON,          VALUE_TYPE_COMMAND,    VALUE_STORE_PROG,      VALUE_STYLE_VERIFY,     NULL,                       (void *) showJson },
#endif

    { ID_DEVICE_NAME,   VALUE_TYPE_STRING,     VALUE_STORE_PREF,      VALUE_STYLE_REQUIRED,   NULL,                       NULL,   "myIotDevice" },
    { ID_DEVICE_TYPE,   VALUE_TYPE_STRING,     VALUE_STORE_PUB,       VALUE_STYLE_READONLY,   (void *) &_device_type,     },
    { ID_DEVICE_VERSION,VALUE_TYPE_STRING,     VALUE_STORE_PUB,       VALUE_STYLE_READONLY,   (void *) &_device_version,  },
    { ID_DEVICE_UUID,   VALUE_TYPE_STRING,     VALUE_STORE_PUB,       VALUE_STYLE_READONLY,   (void *) &_device_uuid,     },
    { ID_DEVICE_URL,    VALUE_TYPE_STRING,     VALUE_STORE_PUB,       VALUE_STYLE_READONLY,   (void *) &_device_url,      },
    { ID_DEVICE_IP,     VALUE_TYPE_STRING,     VALUE_STORE_PUB,       VALUE_STYLE_READONLY,   (void *) &_device_ip,       },
    { ID_DEVICE_BOOTING,VALUE_TYPE_BOOL,       VALUE_STORE_PUB,       VALUE_STYLE_READONLY,   (void *) &_device_booting,  },

    { ID_DEBUG_LEVEL,   VALUE_TYPE_ENUM,       VALUE_STORE_PREF,      VALUE_STYLE_NONE,       (void *) &iot_debug_level,  NULL,   { .enum_range = { LOG_LEVEL_DEBUG, logAllowed }} },
    { ID_LOG_LEVEL,     VALUE_TYPE_ENUM,       VALUE_STORE_PREF,      VALUE_STYLE_NONE,       (void *) &iot_log_level,    NULL,   { .enum_range = { LOG_LEVEL_NONE, logAllowed }} },
    { ID_LOG_COLORS,    VALUE_TYPE_BOOL,       VALUE_STORE_PREF,      VALUE_STYLE_NONE,       (void *) &_log_colors,      NULL,   { .int_range = { DEFAULT_LOG_COLORS }} },
    { ID_LOG_DATE,      VALUE_TYPE_BOOL,       VALUE_STORE_PREF,      VALUE_STYLE_NONE,       (void *) &_log_date,        NULL,   { .int_range = { DEFAULT_LOG_DATE }} },
    { ID_LOG_TIME,      VALUE_TYPE_BOOL,       VALUE_STORE_PREF,      VALUE_STYLE_NONE,       (void *) &_log_time,        NULL,   { .int_range = { DEFAULT_LOG_TIME }} },
    { ID_LOG_MEM,       VALUE_TYPE_BOOL,       VALUE_STORE_PREF,      VALUE_STYLE_NONE,       (void *) &_log_mem,         NULL,   { .int_range = { DEFAULT_LOG_MEM }} },

    { ID_PLOT_DATA,     VALUE_TYPE_BOOL,       VALUE_STORE_PUB,       VALUE_STYLE_NONE,       (void *) &_plot_data,       NULL,   },

    { ID_WIFI,          VALUE_TYPE_BOOL,       VALUE_STORE_PREF,      VALUE_STYLE_NONE,       (void *) &_device_wifi,     (void *) onChangeWifi, { .int_range = { DEFAULT_DEVICE_WIFI }} },
    { ID_AP_PASS,       VALUE_TYPE_STRING,     VALUE_STORE_PREF,      VALUE_STYLE_PASSWORD,   NULL,                       NULL,   DEFAULT_AP_PASSWORD },
    { ID_STA_SSID,      VALUE_TYPE_STRING,     VALUE_STORE_PREF,      VALUE_STYLE_NONE,       NULL,                       NULL,   "" },
    { ID_STA_PASS,      VALUE_TYPE_STRING,     VALUE_STORE_PREF,      VALUE_STYLE_PASSWORD,   NULL,                       NULL,   "" },
    { ID_SSDP,          VALUE_TYPE_BOOL,       VALUE_STORE_PREF,      VALUE_STYLE_NONE,       (void *) &_device_ssdp,     NULL,                  { .int_range = { DEFAULT_DEVICE_SSDP }} },
#if WITH_NTP
    { ID_TIMEZONE,      VALUE_TYPE_ENUM,       VALUE_STORE_PREF,      VALUE_STYLE_NONE,       (void *) &_device_tz,       (void *)onChangeTZ,   { .enum_range = { IOT_TZ_EST, tzAllowed }} },
    { ID_NTP_SERVER,    VALUE_TYPE_STRING,     VALUE_STORE_PREF,      VALUE_STYLE_NONE,       (void *) &_ntp_server,      NULL,   DEFAULT_NTP_SERVER },
#endif

    { ID_DEGREE_TYPE,   VALUE_TYPE_ENUM,       VALUE_STORE_PREF,      VALUE_STYLE_NONE,       (void *) &_degree_type,     NULL, { .enum_range = { 1, temperatureAllowed }} },
    { ID_LAST_BOOT,     VALUE_TYPE_TIME,       VALUE_STORE_PUB,       VALUE_STYLE_READONLY,   (void *) &_device_last_boot, },
    { ID_UPTIME,        VALUE_TYPE_INT,        VALUE_STORE_PUB,       VALUE_STYLE_HIST_TIME,  (void *) &_device_uptime,   NULL, { .int_range = { 0, DEVICE_MIN_INT, DEVICE_MAX_INT}}  },
    { ID_RESET_COUNT,   VALUE_TYPE_INT,        VALUE_STORE_PREF,      VALUE_STYLE_NONE,       NULL,                       NULL,  { .int_range = { 0, 0, DEVICE_MAX_INT}}  },

#if WITH_SD
    { ID_RESTART_SD,    VALUE_TYPE_COMMAND,    VALUE_STORE_SUB,       VALUE_STYLE_VERIFY,     NULL,                       (void *) restartSDCard },
#endif

#if WITH_AUTO_REBOOT
    { ID_AUTO_REBOOT,   VALUE_TYPE_INT,        VALUE_STORE_PREF,      VALUE_STYLE_OFF_ZERO,  (void *) &_auto_reboot,      NULL, { .int_range = { 0, 0, 1000}}  },
#endif

#if WITH_MQTT
    { ID_MQTT_IP,       VALUE_TYPE_STRING,     VALUE_STORE_PREF,      VALUE_STYLE_NONE,       },
    { ID_MQTT_PORT,     VALUE_TYPE_INT,        VALUE_STORE_PREF,      VALUE_STYLE_NONE,       NULL,                       NULL,   { .int_range = { 1883, 1, 65535 }} },
    { ID_MQTT_USER,     VALUE_TYPE_STRING,     VALUE_STORE_PREF,      VALUE_STYLE_NONE,       NULL,                       NULL,   "myIOTClient" },
    { ID_MQTT_PASS,     VALUE_TYPE_STRING,     VALUE_STORE_PREF,      VALUE_STYLE_PASSWORD,   NULL,                       NULL,   "1234" },
#endif



};

#define NUM_BASE_VALUES (sizeof(m_base_descriptors)/sizeof(valDescriptor))

// device tooltips
// in pairs with a null terminator

static const char *device_tooltips[] = {
    ID_VALUES           ,   "From Serial or Telnet monitors, shows a list of all of the current <i>parameter</i> <b>values</b>",

    ID_DEVICE_NAME      ,   "<i>User Modifiable</i> <b>name</b> of the device that will be shown in the <i>WebUI</i>, as the <i>Access Point</i> name, and in </i>SSDP</i> (Service Search and Discovery)",
    ID_DEVICE_TYPE      ,   "The <b>type</b> of the device as determined by the implementor",
    ID_DEVICE_VERSION   ,   "The <b>version number</b> of the device as determined by the implementor",
    ID_DEVICE_UUID      ,   "A <b>unique identifier</b> for this device.  The last 12 characters of this are the <i>MAC Address</i> of the device",
    ID_DEVICE_URL       ,   "The <b>url</b> of a webpage for this device.",
    ID_DEVICE_IP        ,   "The most recent Wifi <b>IP address</b> of the device. Assigned by the WiFi router in <i>Station mode</i> or hard-wired in <i>Access Point</i> mode.",
    ID_DEVICE_BOOTING   ,   "A value that indicates that the device is in the process of <b>rebooting</b>",

    ID_FACTORY_RESET    ,   "Performs a Factory Reset of the device, restoring all of the <i>parameters</i> to their initial values and rebooting",
    ID_RESET_COUNT      ,   "The number of times the <b>Factory Reset</b> command has been issued on this device",
    ID_REBOOT           ,   "Reboots the device.",
    ID_LAST_BOOT        ,   "The <b>time</b> at which the device was last rebooted.",
    ID_UPTIME           ,   "LAST_BOOT value as integer seconds since Jan 1, 1970.  Displayed as he number of <i>hours, minutes, and seconds</i> since the device was last rebooted in the WebUI",

#if WITH_AUTO_REBOOT
    ID_AUTO_REBOOT      ,   "How often, in <b>hours</b> to automatically <b>reboot the device</b>.",
#endif

    ID_AP_PASS          ,   "The <i>encrypted</i> <b>Password</b> for the <i>Access Point</i> when in AP mode",
    ID_STA_SSID         ,   "The <b>SSID</b> (name) of the WiFi network the device will attempt to connect to as a <i>Station</i>.  Setting this to <b>blank</b> will force the device into <i>AP</i> (Access Point) mode at the next <b>reboot</b>",
    ID_STA_PASS         ,   "The <i>encrypted</i> <b>Password</b> for connecting in <i>STA</i> (Station) mode",
    ID_WIFI             ,   "Turns the device's <b>Wifi</b> on and off",
    ID_SSDP             ,   "Turns <b>SSDP</b> (Service Search and Discovery Protocol) on and off.  SSDP allows a device attached to Wifi in <i>Station mode</i> to be found by other devices on the LAN (Local Area Network). Examples include the <b>Network tab</b> in <i>Windows Explorer</i> on a <b>Windows</b> computer",

    ID_DEGREE_TYPE      ,   "Sets the default display of DS18B20 temperature values (VALUE_STYLE_TEMPERATURE)",

#if WITH_NTP
    ID_TIMEZONE         ,   "Sets the <b>timezone</b> for the RTC (Real Time Clock) when connected to WiFi in <i>Station mode</i>. There is a very limited set of timezones currently implemented.",
    ID_NTP_SERVER       ,   "Specifies the NTP (Network Time Protocol) <b>Server</b> that will be used when connected to Wifi as a <i>Station</i>",
#endif

    ID_DEBUG_LEVEL      ,   "Sets the amount of detail that will be shown in the <i>Serial</i> and <i>Telnet</i> output.",
    ID_LOG_LEVEL        ,   "Sets the amount of detail that will be shown in the <i>Logfile</i> output. <b>Note</b> that a logfile is only created if the device is built with an <b>SD Card</b> on which to store it!!",
    ID_LOG_COLORS       ,   "Sends standard <b>ansi color codes</b> to the <i>Serial and Telnet</i> output to highlight <i>errors, warnings,</i> etc",
    ID_LOG_DATE         ,   "Shows the <b>date</b> in Logfile and Serial output",
    ID_LOG_TIME         ,   "Shows the current <b>time</b>, including <i>milliseconds</i> in Logfile and Serial output",
    ID_LOG_MEM          ,   "Shows the <i>current</i> and <i>least</i> <b>memory available</b>, in <i>KB</i>, on the ESP32, in Logfile and Serial output",
    0 };


//------------------------
// vars
//------------------------

String myIOTDevice::_device_uuid;
String myIOTDevice::_device_type = IOT_DEVICE;
String myIOTDevice::_device_version = IOT_DEVICE_VERSION;
String myIOTDevice::_device_url = DEFAULT_DEVICE_URL;
bool myIOTDevice::_device_wifi = DEFAULT_DEVICE_WIFI;
bool myIOTDevice::_device_ssdp = DEFAULT_DEVICE_SSDP;
String myIOTDevice::_device_ip;

const myIOTWidget_t *myIOTDevice::_device_widget = NULL;
const char *myIOTDevice::m_plot_legend = NULL;


#if WITH_NTP
    IOT_TIMEZONE myIOTDevice::_device_tz;
    String myIOTDevice::_ntp_server;
#endif

time_t myIOTDevice::_device_last_boot;
int    myIOTDevice::_device_uptime;
bool   myIOTDevice::_device_booting;
int    myIOTDevice::_degree_type;

bool myIOTDevice::_log_colors = DEFAULT_LOG_COLORS;
bool myIOTDevice::_log_date = DEFAULT_LOG_DATE;
bool myIOTDevice::_log_time = DEFAULT_LOG_TIME;
bool myIOTDevice::_log_mem = DEFAULT_LOG_MEM;
bool myIOTDevice::_plot_data = 0;


#if WITH_AUTO_REBOOT
    int myIOTDevice::_auto_reboot;
#endif

// member variables (not parameters)

#if WITH_SD
    bool   myIOTDevice::m_sd_started;
#endif

RTC_NOINIT_ATTR int myIOTDevice::m_boot_count;

valueIdType *myIOTDevice::m_dash_items = NULL;
valueIdType *myIOTDevice::m_config_items = NULL;
valueIdType *myIOTDevice::m_device_items = device_items;
String      myIOTDevice::m_disabled_classes = "";


const char  **g_derived_tooltips = NULL;
const char  **g_extra_text = NULL;

myIOTDevice *my_iot_device;


// certain most important current methods

void myIOTDevice::addDerivedToolTips(const char **derived_tooltips, const char **extra_text)
{
    g_derived_tooltips = derived_tooltips;
    g_extra_text = extra_text;
}

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
    void myIOTDevice::restartSDCard()
    {
        initSDCard();
        showSDCard();
    }

    void myIOTDevice::initSDCard()
    {
        m_sd_started = SD.begin();
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
    // init SPIFFS and get Values from NVS

    if (!SPIFFS.begin())
        LOGE("Could not initialize SPIFFS");
    for (auto value:m_values)
        value->init();

    // set timezone in case we are soft-rebooting so
    // correct time will show as soon as possible,
    // esp in logfile

    #if WITH_NTP
        extern const char *tzString(IOT_TIMEZONE tz);
        uint32_t tz_enum = my_iot_device->getEnum(ID_TIMEZONE);
        const char *tz_string = tzString(static_cast<IOT_TIMEZONE>(tz_enum));
        setenv("TZ",tz_string,1);
        tzset();
    #endif
    
    // show banner AFTER SD card inited so it shows in logfile

    #if WITH_SD
        initSDCard();
    #endif

    LOGU("-------------------------------------------------------");
    LOGU("myIOTDevice::setup(%s) started",IOT_DEVICE_VERSION);
    LOGU("-------------------------------------------------------");

    proc_entry();

    LOGD("iot_log_level=%d == %s",iot_log_level,getAsString(ID_LOG_LEVEL).c_str());
    LOGD("iot_debug_level=%d == %s",iot_debug_level,getAsString(ID_DEBUG_LEVEL).c_str());

    LOGD("DEVICE_NAME:    %s",getName().c_str());
    LOGD("DEVICE_TYPE:    %s",_device_type.c_str());
    LOGD("DEVICE_VERSION: %s",_device_version.c_str());
    LOGD("DEVICE_UUID:    %s",_device_uuid.c_str());
    LOGD("DEVICE_WIFI:    %d",_device_wifi);

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
    #if WITH_SD
        showSDCard();
    #endif

    //----------------------------
    // finish setting up
    //----------------------------
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
    // ID_WIFI except for the user event of turning it on or off.

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
    LOGD("myIOTDevice::setup() completed LAST_BOOT=%s",getAsString(ID_LAST_BOOT).c_str());

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
            Serial.print(val);
            Serial.print("\r\n");

            #if WITH_TELNET
                if (myIOTSerial::telnetConnected())
                {
                    static char tel_buf[80];
                    sprintf(tel_buf,"%-15s = ",value->getId());
                    myIOTSerial::telnet.print(tel_buf);
                    myIOTSerial::telnet.print(val);
                    myIOTSerial::telnet.print("\r\n");
                }
            #endif
        }
    }
}


//----------------------
// showAllParameters()
//----------------------
// Outputs HTML table for inclusion in documentation

const char *findToolTip(const char *id)
{
    const char **ptr = device_tooltips;
    while (ptr && *ptr)
    {
        const char *left = *ptr++;
        if (!*ptr)
            return NULL;
        const char *right = *ptr++;
        if (!strcmp(id,left))
            return right;
    }
    ptr = g_derived_tooltips;
    while (ptr && *ptr)
    {
        const char *left = *ptr++;
        if (!*ptr)
            return NULL;
        const char *right = *ptr++;
        if (!strcmp(id,left))
            return right;
    }
    return NULL;
}

const char *findExtraText(const char *id)
{
    const char **ptr = g_extra_text;
    while (ptr && *ptr)
    {
        const char *left = *ptr++;
        if (!*ptr)
            return NULL;
        const char *right = *ptr++;
        if (!strcmp(id,left))
            return right;
    }
    return NULL;
}


const char *getTypeAsString(myIOTValue *value)
{
    switch (value->getType())
    {
        case VALUE_TYPE_COMMAND : return "COMMAND";     // 'X'        // monadic (commands)
        case VALUE_TYPE_BOOL    : return "BOOL";        // 'B'        // a boolean (0 or 1)
        case VALUE_TYPE_CHAR    : return "CHAR";        // 'C'        // a single character
        case VALUE_TYPE_STRING  : return "STRING";      // 'S'        // a string
        case VALUE_TYPE_INT     : return "INT";         // 'I'        // a signed 32 bit integer
        case VALUE_TYPE_TIME    : return "TIME";        // 'T'        // time stored as 32 bit unsigned integer
        case VALUE_TYPE_FLOAT   : return "FLOAT";       // 'F'        // a float
        case VALUE_TYPE_ENUM    : return "ENUM";        // 'E'        // enumerated integer
        case VALUE_TYPE_BENUM   : return "BENUM";       // 'J'        // bitwise integer
    }
    return "UNKNOWN";
}


void addParamFloat(bool strip_zeros, String &descrip, float f)
{
    static char float_buf[12];
    sprintf(float_buf,"%0.3f",f);
    if (strip_zeros)
    {
        char *index = strstr(float_buf,".000");
        if (index) *index = 0;
    }
    descrip += float_buf;
}


void myIOTDevice::showAllParameters()
    // This function is intended for my own use to generate a table of
    // parameters for the device to be included in the documentation.
    // It ONLY lists parameters for which tooltips are found.
{
    LOGU("PARAMS");
    for (auto value:my_iot_device->m_values)
    {
        const char *tip = findToolTip(value->getId());
        const char *extra = findExtraText(value->getId());
        // if (tip)
        {
            String descrip;
            if (tip) descrip = tip;
            if (extra)
            {
                if (tip)
                    descrip += " ";
                descrip += extra;
            }
            valueType typ = value->getType();
            valueStyle style = value->getStyle();
            valueStore store = value->getStore();
            const valDescriptor *desc = value->getDesc();

            // EXPAND THE DESCRIP (tip) to include extra lines
            // readonly just displays the hardwired value

            if (style & VALUE_STYLE_READONLY)
            {
                descrip += "\n   <br><i>Readonly</i>";
            }

            // append RANGES (allowed values) and DEFAULTS by TYPE

            else if (typ != VALUE_TYPE_COMMAND)
            {
                if (typ == VALUE_TYPE_ENUM || typ == VALUE_TYPE_BENUM)
                {
                    descrip += "\n   <br><i>allowed</i> : ";

                    uint32_t def = desc->int_range.default_value;

                    int num = 0;
                    bool started = 0;
                    enumValue *p = desc->enum_range.allowed;
                    enumValue default_value = *p;
                    while (*p)
                    {
                        if (num == def)
                            default_value = *p;
                        if (started)
                            descrip += ", ";
                        descrip += "<b>";
                        descrip += String(num++);
                        descrip += "</b>";
                        descrip += "=";
                        descrip += *p;
                        p++;
                        started = 1;
                    }

                    descrip += "\n   <br><i>default</i> : ";
                    descrip += "<b>";
                    descrip += String(def);
                    descrip += "</b>";
                    if (typ == VALUE_TYPE_ENUM)
                    {
                        descrip += "=";
                        descrip += default_value;
                    }
                }
                else if (typ == VALUE_TYPE_STRING)
                {
                    if (style & VALUE_STYLE_REQUIRED)
                        descrip += "\n   <br><b>Required</b> (must not be blank)";
                    const char *def = desc->default_value;
                    if (def && *def)
                    {
                        descrip += "\n   <br><i>default</i> : ";
                        descrip += def;
                    }
                }
                else if (typ == VALUE_TYPE_BOOL)
                {
                    int def = desc->int_range.default_value;
                    descrip += "\n   <br><i>default</i> : ";
                    descrip += def ? "<b>1</b>=on" : "<b>0</b>=off";
                }
                else if (typ == VALUE_TYPE_INT)
                {
                    int def = desc->int_range.default_value;
                    int mn = desc->int_range.int_min;
                    int mx = desc->int_range.int_max;
                    descrip += "\n   <br><i>default</i> : ";
                    descrip += "<b>";
                    descrip += String(def);
                    descrip += "</b>";

                    if (!def && style & VALUE_STYLE_OFF_ZERO)
                        descrip += "=off";

                    if ((style & VALUE_STYLE_OFF_ZERO && !mn) || (mn && mn != DEVICE_MIN_INT))
                    {
                        descrip += "&nbsp;&nbsp;&nbsp;<i>min</i> : ";
                        descrip += String(mn);
                        if (!mn && style & VALUE_STYLE_OFF_ZERO)
                            descrip += "=off";
                    }
                    if (mx && mx != DEVICE_MAX_INT)
                    {
                        descrip += "&nbsp;&nbsp;&nbsp;<i>max</i> : ";
                        descrip += String(mx);
                    }
                }
                else if (typ == VALUE_TYPE_FLOAT)
                {
                    float def = desc->float_range.default_value;
                    float mn = desc->float_range.float_min;
                    float mx = desc->float_range.float_max;

                    descrip += "\n   <br><i>default</i> : ";
                    descrip += "<b>";
                    addParamFloat(0,descrip,def);
                    descrip += "</b>";

                    if (mn && mn != 0.00)
                    {
                        descrip += "&nbsp;&nbsp;&nbsp;<i>min</i> : ";
                        addParamFloat(1,descrip,mn);
                    }
                    descrip += "&nbsp;&nbsp;&nbsp;<i>max</i> : ";
                    addParamFloat(1,descrip,mx);
                }

                // note any Memory Only values

                if (!(store & VALUE_STORE_NVS))
                    descrip += "\n   <br><i>Memory Only</i>";
            }

            if (descrip.startsWith("\n   <br>"))
                descrip = descrip.substring(8);

            // BUILD THE LINE

            String rslt = "<tr><td valign='top'><b>";
            rslt += value->getId();
            rslt += "</b></td><td valign='top'>";
            rslt += getTypeAsString(value);
            rslt += "</td><td valign='top'>";
            rslt += descrip;
            rslt += "</td></tr>";

            // output the line
            // THIS NO LONGER WORKS

            Serial.print(rslt);
            Serial.print("\r\n");
            #if WITH_TELNET
                if (myIOTSerial::telnetConnected())
                {
                    myIOTSerial::telnet.print(rslt);
                    myIOTSerial::telnet.print("\r\n");
                }
            #endif
        }
    }
}




//-----------------------------
// valueListJson()
//-----------------------------

void myIOTDevice::disableClass(const char *class_name, bool disabled)
{
    bool send_it = 0;
    LOGD("disableClass(%s,%d)",class_name,disabled);
    String id = String(".") + String(class_name);
    int idx = m_disabled_classes.indexOf(id);
    bool exists = idx != -1;
    if (disabled)
    {
        if (!exists)
        {
            if (m_disabled_classes.length())
                m_disabled_classes += String(",");
            m_disabled_classes += id;
            send_it = 1;
            LOGD("DISABLED(%s) = %s",class_name,m_disabled_classes.c_str());
        }
    }
    else if (exists)
    {
        int remove_len = id.length();
        int s_len = m_disabled_classes.length();
        if (s_len == remove_len)    // no commas
        {
            m_disabled_classes = "";
            remove_len = 0;
        }
        else if (idx > 0)           // remove leading comma
        {
            remove_len++;
            idx--;
        }
        else                        // remove trailng comma on 0th item
        {
            remove_len++;
        }

        if (remove_len)
        {
            m_disabled_classes.remove(idx,remove_len);
        }

        send_it = 1;
        LOGD("ENABLED(%s) = %s",class_name,m_disabled_classes.c_str());
    }

    #if WITH_WS
        if (send_it)
        {
            String json = String("{\"disabled_classes\":\"") + m_disabled_classes + "\"}";
            wsBroadcast(json.c_str());
        }
    #endif
}



#if WITH_WS

    static void addToolTips(String &rslt)
    {
        rslt += ",\n";
        rslt += "\"tooltips\":{\n";

        const char **ptr = device_tooltips;

        bool started = false;
        while (ptr && *ptr)
        {
            const char *left = *ptr++;
            const char *right = *ptr++;
            if (started)
                rslt += ",\n";
            rslt += "\"";
            rslt += left;
            rslt += "\":\"";
            rslt += right;
            rslt += "\"";
            started = true;
        }
        ptr = g_derived_tooltips;
        while (ptr && *ptr)
        {
            const char *left = *ptr++;
            const char *right = *ptr++;
            if (started)
                rslt += ",\n";
            rslt += "\"";
            rslt += left;
            rslt += "\":\"";
            rslt += right;
            rslt += "\"";
            started = true;
        }
        rslt += "}\n";
    }


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
        rslt += ",\n\"disabled_classes:\":\"" + m_disabled_classes + "\"\n";

        addToolTips(rslt);

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



//-----------------------------------------
// plot support
//-----------------------------------------

// static
void myIOTDevice::setPlotLegend(const char *comma_list)
{
    m_plot_legend = comma_list;
    #if WITH_WS
        String json = "{\"plot_legend\":\"";
        json += comma_list;
        json += "\"}";
        my_iot_device->wsBroadcast(json.c_str());
    #endif
}


//------------------------------------------
// old misc
//------------------------------------------

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

    #if WITH_AUTO_REBOOT
        // check auto reboot every 30 seconds
        // There is some weird behavior here on the bilgeAlarm.
        // Perhaps something to do with bilgeAlarm "since_last_run" ?? It jumped back 14 seconds!
        // causing an AUTO_REBOOT, as if it sets LAST_BOOT time and THEN NTP adjusted the clock
        // backwards a few seconds a few seconds later.

        uint32_t now = time(NULL);
        static uint32_t last_boot_check = 0;

        if (_auto_reboot &&
            now - last_boot_check > 30)
        {
            uint32_t hours = now - _device_last_boot;
            hours = hours / 3600;
            if (hours >= _auto_reboot &&
                okToAutoReboot())
            {
                LOGI("AUTO REBOOTING! last(%d) now(%d) hours(%d) _auto_reboot(%d)",_device_last_boot,now,hours,_auto_reboot);
                reboot();
            }
        }
    #endif
}
