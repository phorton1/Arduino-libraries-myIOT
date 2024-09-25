//--------------------------
// myIOTDevice.h
//--------------------------
// You will extend this class to provide your own members and functionality
// The base class handles the Captive Portal, Network Credentials, SSDP etc.

#pragma once

#include "myIOTValue.h"


#define IOT_DEVICE          "myIOTDevice"
#define IOT_DEVICE_VERSION  ("iot1.10e" MY_IOT_ESP32_CORE_STR)
    // note that the version has MY_IOT_ESP32_CORE (1 or 3) appended after an "e"
    
#define DEFAULT_DEVICE_URL  "https://github.com/phorton1-Arduino-libraries-myIOT"
#define DEFAULT_AP_PASSWORD   "11111111"


// IDS
// commands

#define ID_REBOOT         "REBOOT"
#define ID_FACTORY_RESET  "FACTORY_RESET"
#define ID_VALUES         "VALUES"
#define ID_PARAMS         "PARAMS"
#define ID_JSON           "JSON"

// things with the word DEVICE in them

#define ID_DEVICE_NAME    "DEVICE_NAME"
#define ID_DEVICE_TYPE    "DEVICE_TYPE"
#define ID_DEVICE_VERSION "DEVICE_VERSION"
#define ID_DEVICE_UUID    "DEVICE_UUID"
#define ID_DEVICE_URL     "DEVICE_URL"
#define ID_DEVICE_IP      "DEVICE_IP"
#define ID_DEVICE_BOOTING "DEVICE_BOOTING"

// debug stuff

#define ID_DEBUG_LEVEL    "DEBUG_LEVEL"
#define ID_LOG_LEVEL      "LOG_LEVEL"
#define ID_LOG_COLORS     "LOG_COLORS"
#define ID_LOG_DATE       "LOG_DATE"
#define ID_LOG_TIME       "LOG_TIME"
#define ID_LOG_MEM        "LOG_MEM"

// plotter

#define ID_PLOT_DATA      "PLOT_DATA"

// wifi stuff

#define ID_WIFI           "WIFI"
#define ID_AP_PASS        "AP_PASS"
#define ID_STA_SSID       "STA_SSID"
#define ID_STA_PASS       "STA_PASS"
#define ID_SSDP           "SSDP"

#if WITH_NTP
    #define ID_TIMEZONE      "TIMEZONE"
    #define ID_NTP_SERVER    "NTP_SERVER"
#endif

// miscellaneous stuff

#define ID_LAST_BOOT      "LAST_BOOT"
#define ID_UPTIME         "UPTIME"
#define ID_RESET_COUNT    "RESET_COUNT"

#if WITH_AUTO_REBOOT
    #define ID_AUTO_REBOOT    "AUTO_REBOOT"
#endif

#ifdef WITH_MQTT
    #define ID_MQTT_IP        "MQTT_IP"
    #define ID_MQTT_PORT      "MQTT_PORT"
    #define ID_MQTT_USER      "MQTT_USER"
    #define ID_MQTT_PASS      "MQTT_PASS"
#endif


// timezone addition

typedef enum
{
    IOT_TZ_EST = 0,     // EST - Eastern without Daylight Savings     Panama        "EST5"
    IOT_TZ_EDT,         // EDT - Eastern with Daylight Savings        New York      "EST5EDT,M3.2.0,M11.1.0"
    IOT_TZ_CDT,         // CDT - Central with Daylight Savings        Chicago       "CST6CDT,M3.2.0,M11.1.0"
    IOT_TZ_MST,         // MST - Mountain without Daylight Savings    Pheonix       "MST7"
    IOT_TZ_MDT,         // MDT - Mountain with Daylight Savings       Denver        "MST7MDT,M3.2.0,M11.1.0"
    IOT_TZ_PDT,         // PDT - Pacific with Daylight Savings        Los Angeles   "PST8PDT,M3.2.0,M11.1.0"

}   IOT_TIMEZONE;

extern const char *tzString(IOT_TIMEZONE tz);
    // in myIOTHTTP.cpp


typedef struct {
    const char *name;               // every widget has a name
    const char *dependencies;       // a comma delimited list of fully qualified CSS and JS filenames to load for the widget
    const char *onActivate;         // a JS function to be called when the tab is activated
    const char *onInactivate;         // a JS function to be called when the tab is activated
    const String *html;                // the HTML for the widget
}   myIOTWidget_t;


class myIOTDevice
{
    public:

        myIOTDevice();
        ~myIOTDevice();

        // the device type is set by the INO program, so that
        // logging to logfile can begin at top of INO::setup()

        static void setDeviceType(const char *device_type)
            { _device_type = device_type; }
        static void setDeviceVersion(const char *device_version)
            { _device_version = String(device_version) + String(" ") + String(IOT_DEVICE_VERSION); }
        static void setDeviceUrl(const char *device_url)
            { _device_url = device_url; }

        // likewise, the SD card is started early by the INO program
        // for logging to the logfile

        #if WITH_SD
            static bool hasSD() { return m_sd_started; }
            static bool initSDCard();
            static void showSDCard();
        #else
            static bool hasSD() { return 0; }
        #endif

        virtual void setup();
        virtual void loop();

        String getUUID()   { return _device_uuid; }
            // it's public

        static const char *getDeviceType()  { return _device_type.c_str(); }
            // Derived classes set the _device_type member in their ctor
            // like the "bilgeAlarm" or whatever.  This will be used for the "Model"
            // in SSDP messages, which along with getVersion() is presented in the
            // SSDP "Server" field.

        static const char *getVersion()  { return _device_version.c_str(); }
            // Derived classes set the _device_version member in their ctor.
            // This is used as the "Model Number" in SSDP, and also shows up in the
            // SSDP broadcast messages.  Note that the myIOTDevice layer knows the
            // constant IOT_DEVICE_VERSION and can report both of them.

        static const char *getDeviceUrl()   { return _device_url.c_str(); }
            // also optionally set by derived class

        String getName()   { return getString(ID_DEVICE_NAME);  }
            // This is a user-defined name for the device, and can be anything.
            // It is usually initialized to the same as getDeviceType() via value descriptors.

        static int getBootCount()      { return m_boot_count; }
            // stored in RTC memory, only valid for clients after setup()

        // Widget & Plot

        static void setDeviceWidget(const myIOTWidget_t *the_widget)
            { _device_widget = the_widget; }
        static const myIOTWidget_t *getDeviceWidget()   { return _device_widget; }
            // also optionally set by derived class

        virtual bool hasPlot()    { return false; }
            // optionally overriden by derived class,
            // in which case when _plot_data is true,
            // the client sends the json for an array
            // of numbers as the "plot_data"
        static void setPlotLegend(const char *comma_list);
            // Called by the derived class to provide a list
            // of field (column) names for the plot series.
            // Does two things: immediately sends a WS broadcast
            // to all connected webUI's with the list, and saves
            // the list so that it will be sent with any device_info
            // requests to new webUI's that come on line.
            // In the former case, on broadcast, any existing plots
            // will be restarted inasmuch as this implies that the
            // semantics, as well as the number of columns, for the
            // plot have changed.
        static const char *getPlotLegend()  { return m_plot_legend; }
            // called by myIOTWebSocket when sending device info,
            // if returns non-null, will include a "plot_legend" member.

        static bool   _plot_data;
            // true if plotting should take place

        
        // Values

        bool     getBool(valueIdType id);
        char     getChar(valueIdType id);
        int      getInt(valueIdType id);
        float    getFloat(valueIdType id);
        time_t   getTime(valueIdType id);
        String   getString(valueIdType id);
        uint32_t getEnum(valueIdType id);
        uint32_t getBenum(valueIdType id);

        String  getAsString(valueIdType id);

        void    invoke(valueIdType id, valueStore from=VALUE_STORE_PROG);

        void    setBool(valueIdType id, bool val, valueStore from=VALUE_STORE_PROG);
        void    setChar(valueIdType id, char val, valueStore from=VALUE_STORE_PROG);
        void    setInt(valueIdType id, int val, valueStore from=VALUE_STORE_PROG);
        void    setFloat(valueIdType id, float val, valueStore from=VALUE_STORE_PROG);
        void    setTime(valueIdType id, time_t val, valueStore from=VALUE_STORE_PROG);
        void    setString(valueIdType id, const char *val, valueStore from=VALUE_STORE_PROG);
        void    setString(valueIdType id, const String &val, valueStore from=VALUE_STORE_PROG)         { setString(id, val.c_str(), from); }
        void    setEnum(valueIdType id, uint32_t val, valueStore from=VALUE_STORE_PROG);
        void    setBenum(valueIdType id, uint32_t val, valueStore from=VALUE_STORE_PROG);

        void    setFromString(valueIdType id, const char *val, valueStore from=VALUE_STORE_PROG);
        void    setFromString(valueIdType id, const String &val, valueStore from=VALUE_STORE_PROG)     { setFromString(id, val.c_str(), from); }

        void    clearValueById(valueIdType id);     // necessary when values change type

        myIOTValue *findValueById(valueIdType id);
        const iotValueList getValues()  { return m_values; }
        virtual void onValueChanged(const myIOTValue *value, valueStore from) {}
        void    disableClass(const char *class_name, bool disabled);

        // WiFi (internal api)

        iotConnectStatus_t getConnectStatus();
        void connect(const String &sta_ssid, const String &sta_pass);

        void onConnectStation();
        void onDisconnectAP();
        void onConnectAP();
            // methods called by WiFi so that this object can coordinate
            // and stop/restart other object based on wifiState, passed
            // through to myIOTHTTP


        // WebSockets (internal api)

        #if WITH_WS
            void wsBroadcast(const char *msg);
            void onFileSystemChanged(bool sdcard);
            String valueListJson();
            String getAsWsSetCommand(valueIdType id);
        #endif


        // MQTT (internal api)

        #if WITH_MQTT
            void publishTopic(const char *topic, String msg, bool retain = false);
                // pass through to m_mqtt_client for use in multiple myIOTDevice source files.
        #endif

        // Miscalleneous internal api's

        String handleCommand(const String &command, valueStore from = VALUE_STORE_PROG);
            // called from myIOTSerial.cpp

        void getLogBooleans(bool *colors, bool *dte, bool *tm, bool *mem)
        {
            *colors = _log_colors;
            *dte = _log_date;
            *tm = _log_time;
            *mem = _log_mem;
        }

        // miscellaneous called internally, virtualized methods for derived class

        virtual String onCustomLink(const String &path, const char **mime_type) { return ""; }
            // Web requests to /custom/blah will call this with path='blah'
            // and the address of a pointer to a const char * mime_type.
            // The default mime_type is "text/html".
            // Client may return a String that will be sent with a HTTP 200 success code,
            // and may determine the mime_type by setting *mime_type to a different pointer.
            // If an empty string is returned, the server will return a 404 not found.
        virtual void showIncSetupProgress() {}
            // called three times by Setup()

        virtual void onInitRTCMemory()
            // DERIVED CLASS MUST CALL BASE CLASS METHOD
            // onInitRTC memory checks if the machine has ever been booted,
            // and is only called on a POWERON_RESET when the RESET_COUNT
            // preference is zero.  The RESET_COUNT keeps track of the number
            // of FACTORY_RESETS performed.  Voila, we have persistent non-volatile
            // RAM that does not suffer from likely wearout, since we rarely do
            // a FACTORY_RESET.
            //
        {
            m_boot_count = 0;
        }


        // commands available to clients (i.e. LCD UI)

        static void reboot();
        static void factoryReset();


    protected:

        void addValues(const valDescriptor *desc, int num_values);
        void setTabLayouts(valueIdType *dash, valueIdType *config=NULL, valueIdType *device=NULL)
        {
            m_dash_items = dash;
            m_config_items = config;
            if (device != NULL) m_device_items = device;
        }
        void addDerivedToolTips(const char **derived_tooltips, const char **extra_text=NULL);


        static void onChangeWifi(const myIOTValue *desc, bool val);
        static bool _device_wifi;
            // _device_wifi can be changed, and onChangeWifi(NULL,false/true)
            // called to temporarily turn wifi on or off without actually
            // changing the preference

        #if WITH_AUTO_REBOOT
            virtual bool okToAutoReboot()   { return 1; }
        #endif

    private:

        // descriptors

        static const valDescriptor m_base_descriptors[];
        static valueIdType *m_dash_items;
        static valueIdType *m_config_items;
        static valueIdType *m_device_items;

        // values
        // leading underscore indicates pointed to values

        iotValueList  m_values;
        static String m_disabled_classes;
        static String _device_uuid;
        static String _device_type;
        static String _device_version;
        static String _device_url;
        static String _device_ip;
        static bool   _device_ssdp;

        static const myIOTWidget_t *_device_widget;
        static const char *m_plot_legend;

        #if WITH_NTP
            static IOT_TIMEZONE _device_tz;
            static String _ntp_server;
        #endif

        static time_t _device_last_boot;
        static int    _device_uptime;
        static bool   _device_booting;
        static bool   _log_colors;
        static bool   _log_date;
        static bool   _log_time;
        static bool   _log_mem;

        #if WITH_AUTO_REBOOT
            static int _auto_reboot;
        #endif


        // working vars

        #if WITH_SD
            static bool m_sd_started;
        #endif

        static RTC_NOINIT_ATTR int m_boot_count;


        //---------------------------
        // methods
        //---------------------------

        void initRTCMemory();
        void handleKeyboard();
        static void keyboardTask(void *param);
        static void showValues();
        static void showAllParameters();


        #if WITH_WS
            static void showJson();
        #endif

        #if WITH_NTP
            static void onChangeTZ(const myIOTValue *desc, uint32_t val);
        #endif

};  // class myIOTDevice



extern myIOTDevice *my_iot_device;
