//--------------------------
// myIOTValue.h
//--------------------------

#pragma once

#include "myIOTValue.h"
#include "myIOTDevice.h"
#include "myIOTLog.h"
#include <string>
#include <vector>
#include <stdlib.h>


#define FIXED_FLOAT_PRECISION 3

#ifndef DEBUG_VALUES
    #define DEBUG_VALUES   1
#endif


#define LOG_VALUE_CHANGE(...)  if (m_desc->style & VALUE_STYLE_VERBOSE) LOGV(__VA_ARGS__); else LOGD(__VA_ARGS__)



bool myIOTValue::m_prefs_inited = false;
Preferences myIOTValue::m_preferences;


static uint32_t getEnumMax(enumValue *ptr)
{
    if (!*ptr)
        throw String("no allowed defined for enum");
    uint32_t i = 0;
    while (*ptr)
    {
        i++;
        ptr++;
    }
    return i-1;
}


void myIOTValue::initPrefs()
{
    m_preferences.clear();
}


void myIOTValue::init()
{
    if (!m_prefs_inited)
    {
        m_prefs_inited = true;
        m_preferences.begin("");
    }

    #if DEBUG_VALUES > 1
        LOGD("init(%s) type(%c) store(0x%02x) style(0x%04x) val(0x%08x) fxn(0x%08x)",
             m_desc->id,
             m_desc->type,
             m_desc->store,
             m_desc->style,
             m_desc->val_ptr,
             m_desc->fxn_ptr);
    #endif

    valueIdType id = m_desc->id;
    valueStore store = m_desc->store;
    valueType type = m_desc->type;
    switch (type)
    {
        case VALUE_TYPE_BOOL:
            {
                bool *ptr = (bool *) m_desc->val_ptr;
                if (ptr)
                {
                    bool val = m_desc->int_range.default_value;
                    if (store & VALUE_STORE_NVS)
                        val = m_preferences.getUChar(id,val);
                    *ptr = val;
                }
                break;
            }
        case VALUE_TYPE_CHAR:
            {
                char *ptr = (char *) m_desc->val_ptr;
                if (ptr)
                {
                    char val = m_desc->default_value[0];
                    if (store & VALUE_STORE_NVS)
                        val = m_preferences.getChar(id,val);
                    *ptr = val;
                }
                break;
            }
        case VALUE_TYPE_INT:
             {
                int *ptr = (int *) m_desc->val_ptr;
                if (ptr)
                {
                    int val = m_desc->int_range.default_value;
                    if (store & VALUE_STORE_NVS)
                        val = m_preferences.getInt(id,val);
                    *ptr = val;
                }
                break;
            }
        case VALUE_TYPE_FLOAT:
            {
                float *ptr = (float *) m_desc->val_ptr;
                if (ptr)
                {
                    float val = m_desc->float_range.default_value;
                    if (store & VALUE_STORE_NVS)
                        val = m_preferences.getFloat(id,val);
                    *ptr = val;
                }
                break;
            }
        case VALUE_TYPE_TIME:
            {
                time_t *ptr = (time_t *) m_desc->val_ptr;
                if (ptr)
                {
                    time_t val = 0;
                    if (store & VALUE_STORE_NVS)
                        val = m_preferences.getUInt(id,val);
                    *ptr = val;
                }
                break;
            }
        case VALUE_TYPE_STRING:
            {
                String *ptr = (String *) m_desc->val_ptr;
                if (ptr && m_desc->default_value)
                {
                    String val = m_desc->default_value;
                    if (store & VALUE_STORE_NVS)
                        val = m_preferences.getString(id,val);
                    *ptr = val;
                }
                break;
            }
        case VALUE_TYPE_ENUM:
             {
                uint32_t *ptr = (uint32_t *) m_desc->val_ptr;
                if (ptr)
                {
                    uint32_t val = m_desc->int_range.default_value;
                    if (store & VALUE_STORE_NVS)
                        val = m_preferences.getUInt(id,val);
                    *ptr = val;
                }
                break;
            }
        case VALUE_TYPE_BENUM:
             {
                uint32_t *ptr = (uint32_t *) m_desc->val_ptr;
                if (ptr)
                {
                    uint32_t val = 0;
                    if (store & VALUE_STORE_NVS)
                        val = m_preferences.getUInt(id,val);
                    *ptr = val;
                }
                break;
            }
        case VALUE_TYPE_COMMAND:
            break;
        default:
            LOGE("ERROR - myIOTValue::init(%s) - unknown valueType %02x=='%c'",id,type,type>=' '?type:' ');
            break;
    }
}


void myIOTValue::checkReadonly(valueStore from)
{
    // the program IS allowed to change readonly values
    if (from != VALUE_STORE_PROG &&
        m_desc->style & VALUE_STYLE_READONLY)
        throw String("attempt to modify READONLY value");

}

void myIOTValue::checkType(valueType type)
{
    if (type != m_desc->type)
        throw String("value_type(" + String(m_desc->type) +  ") is not function_call_type(" + String(type) +  ")");
}

void myIOTValue::checkRequired(const char *val)
{
    if ((m_desc->style & VALUE_STYLE_REQUIRED) && !*val)
        throw String("empty value with VALUE_STYLE_REQUIRED");
}



void myIOTValue::invoke()
{
    checkType(VALUE_TYPE_COMMAND);
    invokeFxn fxn = (invokeFxn) m_desc->fxn_ptr;
    if (!fxn)
        throw String("invoke has no function pointer");
    else
        fxn();
}



bool myIOTValue::getBool()
{
    checkType(VALUE_TYPE_BOOL);
    bool *ptr = (bool *) m_desc->val_ptr;
    bool def = m_desc->int_range.default_value;
    bool val =
        ptr ? *ptr :
        m_desc->store & VALUE_STORE_NVS ? m_preferences.getUChar(m_desc->id,def) :
        def;
    return val;
}


char myIOTValue::getChar()
{
    checkType(VALUE_TYPE_CHAR);
    char *ptr = (char *) m_desc->val_ptr;
    char def = m_desc->default_value[0];
    char val =
        ptr ? *ptr :
        m_desc->store & VALUE_STORE_NVS ? m_preferences.getChar(m_desc->id,def) :
        def;
    return val;
}



int myIOTValue::getInt()
{
    checkType(VALUE_TYPE_INT);
    int *ptr = (int *) m_desc->val_ptr;
    int def = m_desc->int_range.default_value;
    int val =
        ptr ? *ptr :
        m_desc->store & VALUE_STORE_NVS ? m_preferences.getInt(m_desc->id,def) :
        def;
    return val;
}


float myIOTValue::getFloat()
{
    checkType(VALUE_TYPE_FLOAT);
    float *ptr = (float *) m_desc->val_ptr;
    float def = m_desc->float_range.default_value;
    float val =
        ptr ? *ptr :
        m_desc->store & VALUE_STORE_NVS ? m_preferences.getFloat(m_desc->id,def) :
        def;
    return val;
}


time_t myIOTValue::getTime()
{
    checkType(VALUE_TYPE_TIME);
    time_t *ptr = (time_t *) m_desc->val_ptr;
    time_t def = 0;
    time_t val =
        ptr ? *ptr :
        m_desc->store & VALUE_STORE_NVS ? m_preferences.getUInt(m_desc->id,def) :
        def;
    return val;
}


String myIOTValue::getString()
{
    checkType(VALUE_TYPE_STRING);
    String *ptr = (String *) m_desc->val_ptr;
    String def = m_desc->default_value;
    String val =
        ptr ? *ptr :
        m_desc->store & VALUE_STORE_NVS ? m_preferences.getString(m_desc->id,def) :
        def;
    return val;
}


uint32_t myIOTValue::getEnum()
{
    checkType(VALUE_TYPE_ENUM);
    uint32_t *ptr = (uint32_t *) m_desc->val_ptr;
    uint32_t def = m_desc->enum_range.default_value;
    uint32_t val =
        ptr ? *ptr :
        m_desc->store & VALUE_STORE_NVS ? m_preferences.getInt(m_desc->id,def) :
        def;
    return val;
}



uint32_t myIOTValue::getBenum()
{
    checkType(VALUE_TYPE_BENUM);
    uint32_t *ptr = (uint32_t *) m_desc->val_ptr;
    uint32_t def = 0;
    uint32_t val =
        ptr ? *ptr :
        m_desc->store & VALUE_STORE_NVS ? m_preferences.getUInt(m_desc->id,def) :
        def;
    return val;
}




String  myIOTValue::getAsString()
{
    String rslt = "undefined";
    switch (m_desc->type)
    {
        case VALUE_TYPE_BOOL   : rslt = String(getBool()); break;
        case VALUE_TYPE_CHAR   : rslt = String(getChar()); break;
        case VALUE_TYPE_INT    :
        {
            int val = getInt();
            if (!val && (m_desc->style & VALUE_STYLE_OFF_ZERO))
                rslt = "off";
            else
                rslt = String(getInt());
            break;
        }
        case VALUE_TYPE_FLOAT  : rslt = String(getFloat(),FIXED_FLOAT_PRECISION); break;
        case VALUE_TYPE_TIME   : rslt = getTime() ? timeToString(getTime()) : String(""); break;
        case VALUE_TYPE_STRING : rslt = getString();  break;
        case VALUE_TYPE_ENUM   :
        {
            uint32_t val = getEnum();
            enumValue *ptr = m_desc->enum_range.allowed;
            int max = getEnumMax(ptr);
            if (val > max)
                throw String("enum(" + String(val) + "out of range(0.." + String(max) + ")");
            rslt = ptr[val];
            break;
        }
        case VALUE_TYPE_BENUM  :
        {
            uint32_t val = getBenum();
            int mask = 1;
            rslt = "";
            enumValue *eptr = m_desc->enum_range.allowed;
            while (*eptr)
            {
                if (val & mask)
                {
                    if (rslt != "") rslt += " - ";
                    rslt += *eptr;
                }
                mask <<= 1;
                eptr++;
            }
            break;
        }
    }
    if (!DEBUG_PASSWORDS && (m_desc->style & VALUE_STYLE_PASSWORD))
        rslt = "********";
    return rslt;
}



void myIOTValue::setBool(bool val, valueStore from)
{
    #if DEBUG_VALUES
        LOG_VALUE_CHANGE("setBool(%s,%d) from(0x%02x)",m_desc->id,val,from);
    #endif

    checkReadonly(from);
    checkType(VALUE_TYPE_BOOL);

    boolChangeFxn fxn = (boolChangeFxn) m_desc->fxn_ptr;
    if (fxn) fxn(this,val);

    bool *ptr = (bool *) m_desc->val_ptr;
    if (ptr) *ptr = val;
;
    if (m_desc->store & VALUE_STORE_NVS)
        if (m_preferences.putUChar(m_desc->id,val) != sizeof(unsigned char))
            throw String("Could not putUChar in NVS");

    publish(String(val),from);
}


void myIOTValue::setChar(char val, valueStore from)
{
    #if DEBUG_VALUES
        LOG_VALUE_CHANGE("setChar(%s,%c) from(0x%02x)",m_desc->id,val,from);
    #endif

    checkReadonly(from);
    checkType(VALUE_TYPE_CHAR);

    charChangeFxn fxn = (charChangeFxn) m_desc->fxn_ptr;
    if (fxn) fxn(this,val);

    char *ptr = (char *) m_desc->val_ptr;
    if (ptr) *ptr = val;

    if (m_desc->store & VALUE_STORE_NVS)
        if (m_preferences.putChar(m_desc->id,val) != sizeof(char))
            throw String("Could not putChar in NVS");

    publish(String(val),from);
}


void myIOTValue::setInt(int val, valueStore from)
{
    #if DEBUG_VALUES
        LOG_VALUE_CHANGE("setInt(%s,%d) from(0x%02x)",m_desc->id,val,from);
    #endif

    checkReadonly(from);
    checkType(VALUE_TYPE_INT);

    if (m_desc->style & VALUE_STYLE_OFF_ZERO && val==0)
    {
        // allowed regardles of min
    }
    else
    {
        int min = m_desc->int_range.int_min;
        int max = m_desc->int_range.int_max;
        if (val<min || val>max)
            throw String("integer(" + String(val) + ") is out of range " + String(min) + ".." + String(max));
    }

    intChangeFxn fxn = (intChangeFxn) m_desc->fxn_ptr;
    if (fxn) fxn(this,val);

    int *ptr = (int *) m_desc->val_ptr;
    if (ptr) *ptr = val;

    if (m_desc->store & VALUE_STORE_NVS)
        if (m_preferences.putInt(m_desc->id,val) != sizeof(int))
            throw String("Could not putInt in NVS");

    publish(String(val),from);
}


void myIOTValue::setFloat(float val, valueStore from)
{
    #if DEBUG_VALUES
        if (strcmp(m_desc->id,ID_DEVICE_VOLTS) &&
            strcmp(m_desc->id,ID_DEVICE_AMPS))
            LOG_VALUE_CHANGE("setFloat(%s,%0.3f) from(0x%02x)",m_desc->id,val,from);
    #endif

    checkReadonly(from);
    checkType(VALUE_TYPE_FLOAT);

    float min = m_desc->float_range.float_min;
    float max = m_desc->float_range.float_max;
    if (val<min || val>max)
        throw String("float(" + String(val,3) + ") is out of range " + String(min,3) + ".." + String(max,3));

    floatChangeFxn fxn = (floatChangeFxn) m_desc->fxn_ptr;
    if (fxn) fxn(this,val);

    float *ptr = (float *) m_desc->val_ptr;
    if (ptr) *ptr = val;

    if (m_desc->store & VALUE_STORE_NVS)
        if (m_preferences.putFloat(m_desc->id,val) != sizeof(float))
            throw String("Could not putFloat in NVS");

    publish(String(val,FIXED_FLOAT_PRECISION),from);
}


void myIOTValue::setTime(time_t val, valueStore from)
{
    #if DEBUG_VALUES
        LOG_VALUE_CHANGE("setTime(%s,%d=%s) from(0x%02x)",m_desc->id,val,timeToString(val).c_str(),from);
    #endif

    checkReadonly(from);
    checkType(VALUE_TYPE_TIME);

    timeChangeFxn fxn = (timeChangeFxn) m_desc->fxn_ptr;
    if (fxn) fxn(this,val);

    time_t *ptr = (time_t *) m_desc->val_ptr;
    if (ptr) *ptr = val;

    if (m_desc->store & VALUE_STORE_NVS)
        if (m_preferences.putUInt(m_desc->id,val) != sizeof(time_t))
            throw String("Could not putTime in NVS");

    publish(val?timeToString(val):String(""),from);
}



void myIOTValue::setString(const char *val, valueStore from)
{
    if (!val) val = "";

    #if DEBUG_VALUES
        const char *show_val = !DEBUG_PASSWORDS && (m_desc->style & VALUE_STYLE_PASSWORD) ? "********" : val;
        LOG_VALUE_CHANGE("setString(%s,%s) from(0x%02x)",m_desc->id,show_val,from);
    #endif

    checkReadonly(from);
    checkType(VALUE_TYPE_STRING);
    checkRequired(val);

    stringChangeFxn fxn = (stringChangeFxn) m_desc->fxn_ptr;
    if (fxn) fxn(this,val);

    String *ptr = (String *) m_desc->val_ptr;
    if (ptr) *ptr = val;

    if (m_desc->store & VALUE_STORE_NVS)
        if (m_preferences.putString(m_desc->id,val) != strlen(val))
            throw String("Could not putString in NVS");

    publish(String(val),from);
}



void myIOTValue::setEnum(uint32_t val, valueStore from)
{
    #if DEBUG_VALUES
        LOG_VALUE_CHANGE("setEnum(%s,0x%04x) from(0x%02x)",m_desc->id,val,from);
    #endif

    checkReadonly(from);
    checkType(VALUE_TYPE_ENUM);

    uint32_t max = getEnumMax(m_desc->enum_range.allowed);
    if (val<0 || val>max)
        throw String("enum(" + String(val) + ") is out of range 0.." + String(max));

    enumChangeFxn fxn = (enumChangeFxn) m_desc->fxn_ptr;
    if (fxn) fxn(this,val);

    uint32_t *ptr = (uint32_t *) m_desc->val_ptr;
    if (ptr) *ptr = val;

    if (m_desc->store & VALUE_STORE_NVS)
        if (m_preferences.putUInt(m_desc->id,val) != sizeof(uint32_t))
            throw String("Could not putEnum in NVS");

    // note for MQTT: enums get published as strings

    publish(String(m_desc->enum_range.allowed[val]),from);
}




void myIOTValue::setBenum(uint32_t val, valueStore from)
{
    #if DEBUG_VALUES
        LOG_VALUE_CHANGE("setBenum(%s,0x%04x) from(0x%02x)",m_desc->id,val,from);
    #endif

    checkReadonly(from);
    checkType(VALUE_TYPE_BENUM);

    benumChangeFxn fxn = (benumChangeFxn) m_desc->fxn_ptr;
    if (fxn) fxn(this,val);

    uint32_t *ptr = (uint32_t *) m_desc->val_ptr;
    if (ptr) *ptr = val;

    if (m_desc->store & VALUE_STORE_NVS)
        if (m_preferences.putUInt(m_desc->id,val) != sizeof(int))
            throw String("Could not putUInt in NVS");

    publish(getAsString(),from);
}



void myIOTValue::checkNumber(const char *ptr, bool allow_neg /*=true*/)
{
    bool is_float = m_desc->type == VALUE_TYPE_FLOAT;

    bool dp = false;
    bool started = false;
    while (*ptr)
    {
        char c = *ptr++;
        if (c == '-')
        {
            if (!allow_neg)
                throw String("minus sign not allowed");
            if (started)
                throw String("minus sign not at front of number");
        }
        else if (c == '.')
        {
            if (dp)
                throw String("number has more than one decimal point");
            if (is_float)
                dp = true;
            else
                throw String("illegal decimal point in integer");
        }
        else if (c < '0' || c > '9')
        {
            String msg = "illegal character(";
            msg += c;
            msg += ") in number";
            throw msg;
        }
        started = true;
    }
}


void myIOTValue::setFromString(const char *ptr, valueStore from)
{
    if (!ptr) ptr = "";

    #if DEBUG_VALUES
        const char *show_val = !DEBUG_PASSWORDS && (m_desc->style & VALUE_STYLE_PASSWORD) ? "********" : ptr;
        LOG_VALUE_CHANGE("setFromString(%s,%s) from(0x%02x)",m_desc->id,show_val,from);
    #endif

    int len = strlen(ptr);
    switch (m_desc->type)
    {
        case VALUE_TYPE_BOOL :
        {
            bool val = false;
            if (len>1)
                throw String("bool length(" + String(ptr) + ") > 1");
            if (len==0)
                LOGW("setting bool %s to false from empty value",m_desc->id);
            else if (*ptr == '1')
                val = true;
            else if (*ptr != '0')
                throw String("illegal value(" + String(ptr) + ") for bool");
            setBool(val,from);
            break;
        }
        case VALUE_TYPE_CHAR :
        {
            char val = 0;
            if (len>1) throw String("char length>1 '" + String(ptr) + "'");
            if (len==0)
                LOGW("setting char %s to 0 from empty value",m_desc->id);
            else
                val = *ptr;
            setChar(val,from);
            break;
        }
        case VALUE_TYPE_INT :
        {
            int val = 0;
            if (len==0)
                LOGW("setting int %s to 0 from empty value",m_desc->id);
            else if (!strcmp(ptr,"off"))
                LOGW("setting int %s to 0 from 'off'",m_desc->id);
            else
            {
                checkNumber(ptr);
                val = atoi(ptr);
            }
            setInt(val,from);
            break;
        }
        case VALUE_TYPE_FLOAT :
        {
            float val = 0.00;
            if (len==0)
                LOGW("setting float %s to 0.00 from empty value",m_desc->id);
            else
            {
                checkNumber(ptr);
                val = atof(ptr);
            }
            setFloat(val,from);
            break;
        }
        case VALUE_TYPE_TIME :
        {
            time_t val = 0;
            if (len==0)
                LOGW("setting time %s to 0 from empty value",m_desc->id);
            else
            {
                checkNumber(ptr,false);
                val = strtoul(ptr, NULL, 0);
            }
            setTime(val,from);
            break;
        }
        case VALUE_TYPE_STRING :
        {
            setString(ptr,from);
            break;
        }
        case VALUE_TYPE_ENUM :
        {
            if (len==0)
            {
                LOGW("setting enum %s to 0 from empty value",m_desc->id);
                setEnum(0);
            }
            else
            {
                bool is_number = true;
                const char *p = ptr;
                while (*p)
                {
                    if (*p < '0' || *p > '9')
                    {
                        is_number = false;
                        break;
                    }
                    p++;
                }
                if (is_number)
                {
                    uint32_t val = strtoul(ptr, NULL, 0);
                    setEnum(val,from);
                }
                else
                {
                    uint32_t num = 0;
                    bool found = 0;
                    uint32_t val = 0;
                    enumValue *p = m_desc->enum_range.allowed;
                    while (*p)
                    {
                        if (!strcasecmp(ptr,*p))
                        {
                            found = 1;
                            val = num;
                            break;
                        }
                        num++;
                        *p++;
                    }
                    if (!found)
                        throw String("illegal enum value(" + String(ptr) + ")");
                    setEnum(val,from);
                }
            }
            break;
        }
        case VALUE_TYPE_BENUM :
        {
            uint32_t val = 0;
            if (len==0)
                LOGW("setting benum %s to 0 from empty value",m_desc->id);
            else
            {
                checkNumber(ptr,false);
                val = strtoul(ptr, NULL, 0);
            }
            setBenum(val,from);
            break;
        }
        default:
            throw String("unsupported type(" + String(m_desc->type) + ") in setFromString()");
            break;
    }
}



void myIOTValue::publish(String val, valueStore from /*=VALUE_STORE_PROG*/)
{
    // LOGD("publish(%s) from(0x%02x)",m_desc->id,from);

    my_iot_device->onValueChanged(this,from);

    #if WITH_WS
        if (m_desc->store & VALUE_STORE_WS)
        {
            String cmd = getAsWsSetCommand(val.c_str());
            my_iot_device->wsBroadcast(cmd.c_str());
        }
    #endif
    #if WITH_MQTT
        // prevent recursion if it CAME from MQTT
        if (from != VALUE_STORE_MQTT_SUB &&
            m_desc->store & VALUE_STORE_MQTT_PUB)
        {
            bool retain = m_desc->style & VALUE_STYLE_RETAIN;
            my_iot_device->publishTopic(m_desc->id,val,retain);
        }
    #endif
}


#if WITH_WS

    String myIOTValue::getAsWsSetCommand(const char *val_ptr /*= NULL*/)
    {
        bool is_off_int = false;
        String str_val = val_ptr ? val_ptr : getAsString();
        if (VALUE_TYPE_INT && (m_desc->style & VALUE_STYLE_OFF_ZERO))
        {
            if (str_val == "off" || str_val == "0")
            {
                is_off_int = true;
                str_val = "off";
            }
        }

        String command = "{\"set\":\"";
        command += m_desc->id;
        command +=  "\",";
        command += "\"value\":";
        bool quoted =
            m_desc->type == VALUE_TYPE_STRING ||
            m_desc->type == VALUE_TYPE_CHAR ||
            m_desc->type == VALUE_TYPE_TIME ||
            m_desc->type == VALUE_TYPE_ENUM ||
            m_desc->type == VALUE_TYPE_BENUM ||
            is_off_int;
        if (quoted) command += "\"";
        command += str_val;
        if (quoted) command += "\"";
        command += "}";
        return command;
    }

    static String nameValuePair(bool quoted, const char *name, String value, bool comma=true)
    {
        String rslt = "\"";
        rslt += name;
        rslt += "\":";
        if (quoted) rslt += "\"";
        rslt += value;
        if (quoted) rslt += "\"";
        if (comma) rslt += ",";
        return rslt;
    }

    String  myIOTValue::getAsJson()
    {
        bool has_value = m_desc->type != VALUE_TYPE_COMMAND;
        String str_value = has_value ? getAsString() : "";

        char type = m_desc->type;
        bool quoted =
            type == VALUE_TYPE_STRING ||
            type == VALUE_TYPE_CHAR ||
            type == VALUE_TYPE_TIME ||
            type == VALUE_TYPE_ENUM ||
            type == VALUE_TYPE_BENUM ||
            m_desc->type == VALUE_TYPE_INT && (m_desc->style & VALUE_STYLE_OFF_ZERO) && str_value == "off";
        bool has_range =
           type == VALUE_TYPE_INT ||
           type == VALUE_TYPE_FLOAT;

        String rslt = "{";
        rslt += nameValuePair(true,"id",String(m_desc->id));
        rslt += nameValuePair(true,"type",String(type));
        rslt += nameValuePair(false,"store",String(m_desc->store));
        rslt += nameValuePair(false,"style",String(m_desc->style),has_range || has_value);

        if (type == VALUE_TYPE_INT)
        {
            rslt += nameValuePair(false,"min",String(m_desc->int_range.int_min));
            rslt += nameValuePair(false,"max",String(m_desc->int_range.int_max),has_value);
        }
        else if (type == VALUE_TYPE_FLOAT)
        {
            rslt += nameValuePair(false,"min",String(m_desc->float_range.float_min,FIXED_FLOAT_PRECISION));
            rslt += nameValuePair(false,"max",String(m_desc->float_range.float_max,FIXED_FLOAT_PRECISION),has_value);
        }
        else if (type == VALUE_TYPE_ENUM)
        {
            String allowed = "";
            enumValue *ptr = m_desc->enum_range.allowed;
            while (*ptr)
            {
                if (allowed != "") allowed += ",";
                allowed += *ptr;
                ptr++;
            }
            rslt += nameValuePair(true,"allowed",allowed,has_value);
        }

        if (has_value)
            rslt += nameValuePair(quoted,"value",str_value,false);

        rslt += "}";
        return rslt;
    }
#endif



// Support for LCD_UI (i.e. bilgeAlarm)

bool myIOTValue::getIntRange(int *min, int *max)
    // For bools, ints, and enums returns true and sets min/max values
    // chars not currently supported.
    // returns false otherwise.
{
    if (m_desc->type == VALUE_TYPE_BOOL)
    {
        *min = 0;
        *max = 1;
        return true;
    }
    else if (m_desc->type == VALUE_TYPE_INT)
    {
        *min = m_desc->int_range.int_min;
        *max = m_desc->int_range.int_max;
        return true;
    }
    else if (m_desc->type == VALUE_TYPE_ENUM)
    {
        *min = 0;
        *max = getEnumMax(m_desc->enum_range.allowed);
        return true;
    }
    else
        return false;
}



String myIOTValue::getIntAsString(int val)
    // For bools, ints, and enums returns the string representation
    // of the value (including "off", enum strings, or number)
    // Returns "" otherwise,
    // DOES NOT DO ANY RANGE CHECKING or handle the gap in VALUE_STYLE_OFF_ZERO!!
    // May crash if presented with enum values out of range!!
{
    String rslt = "";
    switch (m_desc->type)
    {
        case VALUE_TYPE_BOOL   : rslt = String(val); break;
        case VALUE_TYPE_INT    :
        {
            if (!val && (m_desc->style & VALUE_STYLE_OFF_ZERO))
                rslt = "off";
            else
                rslt = String(val);
            break;
        }
        case VALUE_TYPE_ENUM   :
        {
            enumValue *ptr = m_desc->enum_range.allowed;
            rslt = ptr[val];
            break;
        }
    }
    return rslt;
}
