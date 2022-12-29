//-----------------------------------
// testDevice.ino
//-----------------------------------

#include <myIOTDevice.h>
#include <myIOTLog.h>

//------------------------
// testDevice definition
//------------------------

#define TEST_DEVICE             "testDevice"
#define TEST_DEVICE_VERSION     "t1.0"

#define PIN_ONBOARD_LED     2

#define ID_ONBOARD_LED      "ONBOARD_LED"
#define ID_DEMO_MODE        "DEMO_MODE"


// what shows up on the "dashboard" UI tab

static valueIdType dash_items[] = {
    ID_ONBOARD_LED,
    ID_DEMO_MODE,
    ID_REBOOT,
    0
};

// what shows up on the "config" UI tab

static valueIdType config_items[] = {
    ID_DEMO_MODE,
    0
};


class testDevice : public myIOTDevice
{
public:

    testDevice();
    ~testDevice() {}

    virtual void setup() override
    {
        LOGD("testDevice::setup(%s)",getVersion());
        pinMode(PIN_ONBOARD_LED,OUTPUT);
        myIOTDevice::setup();
    }

    // using baseClass loop() method

private:

    static const valDescriptor m_test_values[];

    static bool _ONBOARD_LED;
    static bool _DEMO_MODE;

    static void onLed(const myIOTValue *desc, bool val)
    {
        String id = desc->getId();
        LOGD("onLed(%s,%d)",id.c_str(),val);
        digitalWrite(PIN_ONBOARD_LED,val);
    }

};


// value descriptors for testDevice

const valDescriptor testDevice::m_test_values[] =
{
    { ID_DEVICE_NAME,      VALUE_TYPE_STRING,   VALUE_STORE_PREF,     VALUE_STYLE_REQUIRED,   NULL,   NULL,   TEST_DEVICE },
        // override base class element
    { ID_ONBOARD_LED,      VALUE_TYPE_BOOL,     VALUE_STORE_TOPIC,    VALUE_STYLE_NONE,       (void *) &_ONBOARD_LED,    (void *) onLed, },
    { ID_DEMO_MODE,        VALUE_TYPE_BOOL,     VALUE_STORE_PREF,     VALUE_STYLE_NONE,       (void *) &_DEMO_MODE,       NULL,          },
};

#define NUM_DEVICE_VALUES (sizeof(m_test_values)/sizeof(valDescriptor))

// not inline cuz ctor must know descriptors, which must know members

testDevice::testDevice()
{
    addValues(m_test_values,NUM_DEVICE_VALUES);
    setTabLayouts(dash_items,config_items);
}

// static member data

bool testDevice::_ONBOARD_LED;
bool testDevice::_DEMO_MODE;



//--------------------------------
// main
//--------------------------------

testDevice *test_device;


void setup()
{
    Serial.begin(115200);
    delay(1000);

    testDevice::setDeviceType(TEST_DEVICE);
    testDevice::setDeviceVersion(TEST_DEVICE_VERSION);

    LOGU("");
    LOGU("");
    LOGU("testDevice.ino setup() started on core(%d)",xPortGetCoreID());

    test_device = new testDevice();
    test_device->setup();

    LOGU("testDevice.ino setup() finished");
}



void loop()
{
    test_device->loop();

    if (test_device->getBool(ID_DEMO_MODE))
    {
        uint32_t now = millis();
        static uint32_t toggle_led = 0;
        if (now > toggle_led + 2000)
        {
            toggle_led = now;
            bool led_state = test_device->getBool(ID_ONBOARD_LED);
            test_device->setBool(ID_ONBOARD_LED,!led_state);
        }
    }
}