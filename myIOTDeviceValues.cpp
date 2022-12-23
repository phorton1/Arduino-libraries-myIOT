//--------------------------
// myIOTValue.h
//--------------------------

#pragma once

#include "myIOTDevice.h"
#include "myIOTLog.h"
#include <stdexcept>



void myIOTDevice::addValues(const valDescriptor *desc, int num_values)
{
    for (int i=0; i<num_values; i++)
    {
        myIOTValue *val = NULL;
        for (auto value:m_values)
        {
            if (!strcasecmp(desc->id,value->getId()))
            {
                val = value;
                break;
            }
        }
        if (val)
            val->assign(desc++);
        else
        {
            val = new myIOTValue(desc++);
            m_values.push_back(val);
        }
    }
}



myIOTValue *myIOTDevice::findValueById(valueIdType id)
{
    for (auto value:m_values)
    {
        if (!strcasecmp(id,value->getId()))
            return value;
    }

    throw String("could not findValueById()");
    return NULL;
}


bool myIOTDevice::getBool(valueIdType id)
{
    try
    {
        myIOTValue *value = findValueById(id);
        id = value->getId();
        return value->getBool();
    }
    catch (String e)
    {
        LOGE("error value(%s) %s",id,e.c_str());
    }
    return false;
}



char myIOTDevice::getChar(valueIdType id)
{
    try
    {
        myIOTValue *value = findValueById(id);
        id = value->getId();
        return value->getChar();
    }
    catch (String e)
    {
        LOGE("error value(%s) %s",id,e.c_str());
    }
    return 0;
}


int myIOTDevice::getInt(valueIdType id)
{
    try
    {
        myIOTValue *value = findValueById(id);
        id = value->getId();
        return value->getInt();
    }
    catch (String e)
    {
        LOGE("error value(%s) %s",id,e.c_str());
    }
    return 0;
}

float myIOTDevice::getFloat(valueIdType id)
{
    try
    {
        myIOTValue *value = findValueById(id);
        id = value->getId();
        return value->getFloat();
    }
    catch (String e)
    {
        LOGE("error value(%s) %s",id,e.c_str());
    }
    return 0.00;
}

time_t myIOTDevice::getTime(valueIdType id)
{
    try
    {
        myIOTValue *value = findValueById(id);
        id = value->getId();
        return value->getTime();
    }
    catch (String e)
    {
        LOGE("error value(%s) %s",id,e.c_str());
    }
    return 0;
}

String myIOTDevice::getString(valueIdType id)
{
    try
    {
        myIOTValue *value = findValueById(id);
        id = value->getId();
        return value->getString();
    }
    catch (String e)
    {
        LOGE("error value(%s) %s",id,e.c_str());
    }
    return "";
}

uint32_t myIOTDevice::getEnum(valueIdType id)
{
    try
    {
        myIOTValue *value = findValueById(id);
        id = value->getId();
        return value->getEnum();
    }
    catch (String e)
    {
        LOGE("error value(%s) %s",id,e.c_str());
    }
    return 0;
}

uint32_t myIOTDevice::getBenum(valueIdType id)
{
    try
    {
        myIOTValue *value = findValueById(id);
        id = value->getId();
        return value->getBenum();
    }
    catch (String e)
    {
        LOGE("error value(%s) %s",id,e.c_str());
    }
    return 0;
}



String myIOTDevice::getAsString(valueIdType id)
{
    try
    {
        myIOTValue *value = findValueById(id);
        id = value->getId();
        return value->getAsString();
    }
    catch (String e)
    {
        LOGE("error value(%s) %s",id,e.c_str());
    }
    return "";
}



void myIOTDevice::invoke(valueIdType id, valueStore from)
{
    try
    {
        myIOTValue *value = findValueById(id);
        id = value->getId();
        value->invoke();
    }
    catch (String e)
    {
        LOGE("error value(%s) %s",id,e.c_str());
    }
}


void myIOTDevice::setBool(valueIdType id, bool val, valueStore from)
{
    try
    {
        myIOTValue *value = findValueById(id);
        id = value->getId();
        value->setBool(val,from);
    }
    catch (String e)
    {
        LOGE("error value(%s) %s",id,e.c_str());
    }
}

void myIOTDevice::setChar(valueIdType id, char val, valueStore from)
{
    try
    {
        myIOTValue *value = findValueById(id);
        id = value->getId();
        value->setChar(val,from);
    }
    catch (String e)
    {
        LOGE("error value(%s) %s",id,e.c_str());
    }
}

void myIOTDevice::setInt(valueIdType id, int val, valueStore from)
{
    try
    {
        myIOTValue *value = findValueById(id);
        id = value->getId();
        value->setInt(val,from);
    }
    catch (String e)
    {
        LOGE("error value(%s) %s",id,e.c_str());
    }
}

void myIOTDevice::setFloat(valueIdType id, float val, valueStore from)
{
    try
    {
        myIOTValue *value = findValueById(id);
        id = value->getId();
        value->setFloat(val,from);
    }
    catch (String e)
    {
        LOGE("error value(%s) %s",id,e.c_str());
    }
}

void myIOTDevice::setTime(valueIdType id, time_t val, valueStore from)
{
    try
    {
        myIOTValue *value = findValueById(id);
        id = value->getId();
        value->setTime(val,from);
    }
    catch (String e)
    {
        LOGE("error value(%s) %s",id,e.c_str());
    }
}

void myIOTDevice::setString(valueIdType id, const char *val, valueStore from)
{
    try
    {
        myIOTValue *value = findValueById(id);
        id = value->getId();
        value->setString(val,from);
    }
    catch (String e)
    {
        LOGE("error value(%s) %s",id,e.c_str());
    }
}

void myIOTDevice::setEnum(valueIdType id, uint32_t val, valueStore from)
{
    try
    {
        myIOTValue *value = findValueById(id);
        id = value->getId();
        value->setEnum(val,from);
    }
    catch (String e)
    {
        LOGE("error value(%s) %s",id,e.c_str());
    }
}



void myIOTDevice::setBenum(valueIdType id, uint32_t val, valueStore from)
{
    try
    {
        myIOTValue *value = findValueById(id);
        id = value->getId();
        // LOGD("myIOTDevice::setBenum(%s,0x%04x) from(0x%02x)",id,val,from);
        value->setBenum(val,from);
    }
    catch (String e)
    {
        LOGE("error value(%s) %s",id,e.c_str());
    }
}


void myIOTDevice::setFromString(valueIdType id, const char *val, valueStore from)
{
    try
    {
        myIOTValue *value = findValueById(id);
        id = value->getId();
        value->setFromString(val,from);
    }
    catch (String e)
    {
        // LOGE("error value(%s) %s",id,e.c_str());
        String msg = "value(" + String(id) + ") " + e;
        LOGE("error %s",msg.c_str());

        // rethrow the error to myIOTSebSockets.cpp
        // so far this is the only method to use the "from" param ...

        if (from == VALUE_STORE_WS)
        {
            LOGD("rethrowing %s",msg.c_str());
            throw String("error " + msg);
        }

    }
}


#if WITH_WS
    String myIOTDevice::getAsWsSetCommand(valueIdType id)
    {
        String rslt = "";
        try
        {
            myIOTValue *value = findValueById(id);
            rslt = value->getAsWsSetCommand();
        }
        catch (String e)  {}    // ignore errors
        return rslt;
    }

#endif
