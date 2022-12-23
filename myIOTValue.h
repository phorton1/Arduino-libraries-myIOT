//--------------------------
// myIOTValue.h
//--------------------------

#pragma once

#include "myIOTTypes.h"
#include <Preferences.h>


class myIOTValue
{
    public:

        ~myIOTValue() {}

        myIOTValue(const valDescriptor *desc)  { m_desc = desc; }
        void assign(const valDescriptor *desc) { m_desc = desc; }

        void    init();
            // values will be set to their
            // (a) default values from the descriptor
            // (b) the nvs value if it exists
        static void initPrefs();
            // called from factoryReset(), restart the m_preferences member
            // so we can write the RESET_COUNT for next boot

        const   valueIdType getId()    const { return m_desc->id; }
        const   valueType   getType()  const { return m_desc->type; }
        const   valueStore  getStore() const { return m_desc->store; }
        const   valueStyle  getStyle() const { return m_desc->style; }

        void     invoke();
        bool     getBool();
        char     getChar();
        int      getInt();
        float    getFloat();
        time_t   getTime();
        String   getString();
        uint32_t getEnum();
        uint32_t getBenum();

        String  getAsString();
            // Does NOT work (returns "") for values that are not in memory,
            // i.e. VALUE_STORE_PROG with no val_ptr

        void    setBool(bool val, valueStore from=VALUE_STORE_PROG);
        void    setChar(char val, valueStore from=VALUE_STORE_PROG);
        void    setInt(int val, valueStore from=VALUE_STORE_PROG);
        void    setFloat(float val, valueStore from=VALUE_STORE_PROG);
        void    setTime(time_t val, valueStore from=VALUE_STORE_PROG);
        void    setString(const char *val, valueStore from=VALUE_STORE_PROG);
        void    setString(const String &val, valueStore from=VALUE_STORE_PROG)   { setString(val.c_str(),from); }
        void    setEnum(uint32_t val, valueStore from=VALUE_STORE_PROG);
        void    setBenum(uint32_t val, valueStore from=VALUE_STORE_PROG);

        void    setFromString(const char *val, valueStore from=VALUE_STORE_PROG);
        void    setFromString(const String &val, valueStore from=VALUE_STORE_PROG)  { setFromString(val.c_str(),from); }

        #if WITH_WS
            String  getAsJson();
                // Note that the "value" of items not in memrory (VALUE_STORE_PROG
                // and no val_ptr) will have "empty" values in the json.
            String  getAsWsSetCommand(const char *val_ptr = NULL);
                // if NULL, getAsString() will be called which
                // WONT work for values that are VALUE_STORE_PROG and have no val_ptr
                // like the current test implementation of DEVICE_VOLTS and
                // DEVICE_AMPS, which DO work because VALUE_STORE_PROG with no val_ptrs
                // act as "pure broadcasts" and WS and MQTT are 'notified' via
                // the publish() at the end of the setBool() with the val_ptr param.
        #endif

        bool getIntRange(int *min, int *max);
            // For bools, ints, and enums returns true and sets min/max values
            // chars not currently supported.
            // returns false otherwise.
        String getIntAsString(int val);
        // For bools, ints, and enums returns the string representation
        // of the value (including "off", enum strings, or number)
        // Returns "" otherwise,
        // DOES NOT DO ANY RANGE CHECKING or handle the gap in VALUE_STYLE_OFF_ZERO!!
        // May crash if presented with enum values out of range!!

    private:

        static bool m_prefs_inited;
        static Preferences m_preferences;

        const valDescriptor *m_desc;

        void checkType(valueType type);
        void checkNumber(const char *val, bool allow_neg=true);
        void checkRequired(const char *val);
        void checkReadonly(valueStore from);

        void publish(String value, valueStore from=VALUE_STORE_PROG);

};  // class myIOTValue


typedef std::vector<myIOTValue *> iotValueList;


typedef void (*invokeFxn)();
typedef void (*boolChangeFxn)  (const myIOTValue *value, bool val);
typedef void (*charChangeFxn)  (const myIOTValue *value, char val);
typedef void (*intChangeFxn)   (const myIOTValue *value, int val);
typedef void (*floatChangeFxn) (const myIOTValue *value, float val);
typedef void (*timeChangeFxn)  (const myIOTValue *value, time_t val);
typedef void (*stringChangeFxn)(const myIOTValue *value, const char *val);
typedef void (*enumChangeFxn)  (const myIOTValue *value, uint32_t val);
typedef void (*benumChangeFxn) (const myIOTValue *value, uint32_t val);
    // called BEFORE the change is committed to memory, etc
    // client can throw a String() to stop the setting of memory, prefs, mqtt, etc
