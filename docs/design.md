# myIOT - Design

**[Home](readme.md)** --
**[Getting Started](getting_started.md)** --
**[Wifi](wifi.md)** --
**[Basics](basics.md)** --
**[How To](how_to.md)** --
**Design** --
**[Details](details.md)**

This page delves into the overall design of this myIOT project and it's API.

## A. myIOTDevice API

On the [basics](basics.md) page you have seen how you create a myIOT device by
deriving from the **myIOTDevice** base class, providing some overridden methods,
and defining your own table of **values**.

There is a visible, public pointer the the myIOTDevice base class, **my_iot_device**
that can be used by **any** code in the system to get at this API.

This section of this page will describe a little more about the myIOTDevice class methods
and members that you, as a developer, will be interested in.

Please see **myIOTDevice.h** for more details.

### 1. myIOTDevice methods called before instantiation

There are two static methods that are called in the derived implementation *before* the
instantiation of the myIOTDevice.    These basically set the device **name** and
**version number** so that they can be used in logging the instantiation process.

```
// the device type is set by the INO program, so that
// logging to logfile can begin at top of INO::setup()

static void setDeviceType(const char *device_type)
	{ _device_type = device_type; }
static void setDeviceVersion(const char *device_version)
	{ _device_version = String(device_version) + String(" ") + String(IOT_DEVICE_VERSION); }
```

In order to make it possible to also log details about the instantiation to an SDCard
based logfile, it is also possible to **begin** the SDCard before instantiation, if you
chose, from your INO file (if WITH_SD is defined to 1), by caling the static
**initSDCard()** method.

```
// likewise, the SD card is started early by the INO program
// for logging to the logfile

#if WITH_SD
	static bool hasSD() { return m_sd_started; }
	static bool initSDCard();
	static void showSDCard();
#else
	static bool hasSD() { return 0; }
#endif
```

If you chose not to begin the SDCard in your INO setup() method, then it will be
started (if WITH_SD is defined as 1, there is a reader hooked up to the ESP32, and
there is a valid FAT32 card in the reader) during the normal myIOTDevice setup()
method.

In addition, as we saw in the [basic](basics.md) description of how the system works,
there are two **protected** member methods that your derived class will call from it's
INO **setup()** method:

```
void addValues(const valDescriptor *desc, int num_values);
void setTabLayouts(valueIdType *dash, valueIdType *config=NULL, valueIdType *device=NULL)
```

**addValues()** lets you pass your table of valDescriptors to the base class, and
**setTabLayouts()** allows you to tell the system what should show up on the
*dashboard* and *config* tabs of the UI, and even, if you wish, the *device*
tab.  Usually setTabLayouts() is just called with two parameters, and you let
the base class decide what shows up on the device tab.


### 2. myIOTDevice virtual setup() and loop() methods for overriding in the derived class

The myIOTDevice has setup() and loop() methods that must be called appropriately by
your INO program in order for the device to work.  In some cases you will not need
to override these, and it is a bit of a stylistic decision if you do so, since you
call these from your INO setup() and loop() methods already.

```
virtual void setup();
virtual void loop();
```

But if you need to, for instance in order to access *private* members of your
derived class during setup() or loop(), then you may override these virtual methods
in your derived class.

**If you override these methods, you MUST call the base class methods from
your overriden versions**.


### 3. Value Accessors - getXXX "getters"

The myIOTDevice provides typesafe "getters" for **values** it, and you, have defined.

If you call one of these methods with an ID for a value of a different type,
the system will log an error and you will receive a 0, or empty string value.

There is also a method, **getAsString()** that will return any value in
the system as a string, regardless of it's type.

```
bool     getBool(valueIdType id);
char     getChar(valueIdType id);
int      getInt(valueIdType id);
float    getFloat(valueIdType id);
time_t   getTime(valueIdType id);
String   getString(valueIdType id);
uint32_t getEnum(valueIdType id);
uint32_t getBenum(valueIdType id);

String  getAsString(valueIdType id);
```

These methods allow you to access ANY of the values known by the myIOTDevice,
even though you do not have access to the class private or protected member
variables.

For example, you can retrieve an integer count of how many times the myIOTDevice
has been FACTORY_RESET with the following call:

```
int reset_count = my_iot_device->getInt(ID_RESET_COUNT);
```

### 4. Value Accessors - setXXX "setters"

There are methods for setting values that should generally be used in your implementation.

Once again, these methods are "typesafe" in that if you call a method with an ID for
an inappropriate type, the system will do *nothing* and report an error.

```
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
```

The last two calls, **setFromString()** allow you to set a value, regardless of it's type,
from a string (or const char star), and the system will do it's best to do a meaningful
conversion (i.e. allowing the string synonyms for enumerated types, converting floating
points in exponentional notation, and so on).

Each of these values has a third **valueStore from** parameter, which defaults to **VALUE_STORE_PROG**
to indicate **who** is setting the value, which can be useful in debugging, and which is important
to prevent *loops* when setting values.   For instance if a WebSocket sets a value (which will
call the set method with *VALUE_STORE_WS*) then, deep in the implementation, we can know NOT to
send the value back to that same WebSocket.

In general you *are* the **"program"** and so when you call these methods, you should just
let the C++ compiler pass in the default for the third parameter.

There is another "setter" that is used with VALUE_TYPE_COMMAND to "invoke" the command:

```
void invoke(valueIdType id, valueStore from=VALUE_STORE_PROG);
```

So, although there is a separate **reboot()** method on the myIOTDevice that you can call to
reboot the ESP32, you *could* reboot it by calling **my_iot_device->invoke(ID_REBOOT).**

### 5. Other possible public myIOTDevice API

Most of the rest of the myIOTDevice API should be considered private, and generally
not called by your application.   This is indicated in the comments.  Architecturally
I made the decision to use the myIOTDevice in a natural C++ manner, so as far as the
rest of the *library* is concerned, the remaining public members are easily accessible,
however, they are **not intended** for use by application level code.

One possible exception to that are the following methods, which *may* help you during
debugging, and/or to get a better idea of how the system is working, by allowing you
to directly get a **myIOTValue**, and/or to iterate over all of the values in the system,
and providing an overriden method that will be called when ANY value changes.

```
myIOTValue *findValueById(valueIdType id);
const iotValueList getValues()  { return m_values; }
virtual void onValueChanged(const myIOTValue *value, valueStore from) {}
```



## B. Logging

There are 6 levels of logging/debugging available to you as you develop your device.

- **LOGU** - *User* level logging
- **LOGE** - *Error* logging
- **LOGW** - *Warnings*
- **LOGI** - *Info*
- **LOGD** - *Debugging*
- **LOGV** - *Verbose Debugging*

These methods can be called with **sprintf** type formating, for displaying
hex numbers, floating point numbers, strings, and so on:

```
LOGD("this is a debug message with a string(%s) and an integer(%i)","blah",123);
LOGU("this is how you format a 4 digit hex number: %04X", 12345)
```

The idea is that there are things you almost always want to show the **user**,
in the logfiles and serial output, and that you usually want to show **errors**,
sometimes want to show **warnings**, may want to show general **information**, and
then there are two levels of **debugging** that you, as the developer only,
will probably be interested in.

What actually shows in the serial output, and logfiles, is determined by
the settings for LOG_LEVEL and DEBUG_LEVEL, which can be modified via
the Serial or WebUIs.

**Note** that there are boolean paremeter that allow you to see more
information in the serial output;

- **LOG_COLORS** (default=0) - if turned on, will cause the serial output to
send standard **ansi** color strings for each line of output, and different
colors will be shown (i.e. red for errors, yellow for warnings, white for
user level and green for debugging) in Putty
or other ansi compatible serial monitors*
- **LOG_DATE** (default=0) - will log the **date** to the output
- **LOG_TIME** (default=1) - will log the **time** to the output
- **LOG_MEM**  (default=0) - will log the current and lowest free **memory** to the output


*note2: the Arduino serial monitor does not support ansi colors*

*note3: the **default time zone** for the myIOT project is **EST**.  myIOT currently supports
a few timezones (mostly in the USA). If you need other timeszones you will need
to modify the source code to provide them.*

Please see **myIOTLog.h** and **cpp** for more details.

### 1. Log Indentation

You may have noticed in the [getting started](getting_started.md) page that
some of the serial output was *nested* according to it's **call level**.

The following (extremely quick, inline) methods can be called at the top
and bottoms of methods or blocks of code to cause the subsequent log calls
to be indented.

```
extern void proc_entry();
extern void proc_leave();
```

Please see the various implemenentation **cpp** files for examples on using
these two mthods.


### 2. Value Debugging Define

Please note that there is a define in **myIOTValue.cpp** that defaults to
one (1) to show values being set via "info" level (LOGI) calls.

In other words, by default, the serial output and logfiles show when any
values are set by the program or any UI.

```
#define DEBUG_VALUES   1
```

If you wish to turn that off, please set the #define to zero (0).


## Next

Next (and finally) I will present my motivation for creating this library
and provide additional detailed implementation notes for developers.

Next:  **[Details](details.md)**
