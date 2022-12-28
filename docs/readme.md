# my_IOT

**my_IOT** is an IOT architecture (framework) that provides a somewhat general
approach to building simple *parameter driven* devices with ESP32's and monitoring and
controlling them.

This framework is device centric -- not network centric -- and starts by giving
you the ability to implement an ESP32 "device" and control it via the serial port, since
this is probably how you initially program and interact with your ESP32 (using the Arduino
IDE and serial monitor).

Secondly, if you want, with a compile flag, you can also control it over Wifi using
Telnet, with a program like Putty or any other common Telnet Client.

Thirdly, each **my_IOT device** presents its own Web User Interface (Webpage), that you can
access over Wifi, via a Web Browser, without the need for any other third party programs or
applications.

Finally, these devices can support [MQTT](https://en.wikipedia.org/wiki/MQTT) so they
can be integrated into existing IOT frameworks like HomeAssistant, or Mosquito and NodeRed,
so that you can monitor and control them in other ways, including via Siri and Alexa
voice interfaces.

This readme is broken up into several sections.

- Overview
- Installation
- Getting Started (Building the testDevice)
- WiFi Access
- Developer Strategies
- Implementation Notes
- Other

*note: this architecture presumes that you have a good understanding of **C++** and
how to derive from and extend existing **classes***


## Overview


A **my_IOT device** can be thought of as a program running on an ESP32 that
utilizes and manages a set of **values**.  The values can be monitored and/or
modified externally, via a number of different mechanisms, and can drive,
or be driven by, the program.

You implement your **my_IOT device** by deriving a class from the existing
**myIOTDevice** class, and providing a definition of the set of values that
are to be utilized and managed by your program.  You can *override* the
existing myIOTDevice **setup()** and **loop()** methods, and/or create
ESP32 **tasks** to implement the bulk of your device specific code.

As your program runs, it can use the **get** and **put** methods from
the base class to retrieve and/or set the values associated with the program.
In our simple *testDevice* example program we will define two *preferences*
(**ONBOARD_LED** and **DEMO_MODE**) which can be read and/or set via external UI's,
that alter the programs behavior, turning the ESP32 onboard LED on or off, and/or
entering a "mode" in which the LED flashes on and off by itself.

## Installation

### Get the Source Code

The installation starts by placing a copy of this entire repository into your Arduino-libraries folder.

That can be done usign **GIT**, or by downloading this repository as a **ZIP file** and placing the
contents of the zip file into your Arduino-libraries folder.  In either case you should end up
with a folder called **my_IOT** within your Arduino-libraries folder.


### High Level Defines

The installation of the my_IOT library depends a bit upon what you intend to do with it.

There are compile flags in the file **myIOTTypes.h** that can be set to 1 (one) or 0 (zero) to include
or exclude various pieces of functionality from the library.   *You may want to modify **myIOTTypes.h**
depending on your usage.*   The defaults are appropriate for building the included example **testDevice.ino**
sketch.

- **WITH_MQTT** default(0) - includes MQTT publication/subscription capabilities
- **WITH_TELNET** default(0) - includes a Telnet Serial Client
- **WITH_SD** default(0) - includes SD card and Logging to it
- **WITH_WS** default(1) - includes WebSockets, which is **required** for the my_IOT web user interface
- **WITH_NTP** default(1) - includes NTP (Network Time Protocol) to set the ESP32 clock when connected as a Wifi Station
- **WITH_BASIC_OTA** default(0) - includes Esperif's OTA capabilities, regardless of other settings
- **DEFAULT_DEVICE_WIFI** default(1) - determines whether or not my_IOT Wifi is turned on, or off, by default

These options and their behaviors will be more fully described elsewhere, but for now it is useful
to present them so that you can see how the Arduino Library dependencies work.

### Arduino Library Dependencies

Depending on the settings of the defines in **myIOTTypes.h**, various additional libraries **MUST BE INSTALLED**
into your Arduino IDE development environment.

- **WebSockets** by Markus Sattler - Version 2.3.6 (if **WITH_WS == 1**)
- **ESP Telnet** by Lennart Hennings - Version 1.2.2 (if **WITH_TELNET == 1**)
- **ArduinoJson** by Benoit Blanchon - Verson 6.18.5 (if **WITH_WS == 1**)
- **PubSubClient** by Nick O'Leary - Version 2.8.0 (if **WITH_MQTT == 1**)

So, in order to build the **testDevice.ino** you MUST install the **Websockets, ESP Telnet, and ArduinoJson**
libraries into your Arduino IDE environment.   The simplest way to do that is to use the *Library Manager*
in the Arduino IDE.











Values have fixed characteristics, determined by your implementation, that include

- a **value type** (boolean, integer, floating point, enum, string, etc)
- a **storage type** (memory only, persistent preference, published, subscribed to, etc)
- a rendering **style** that indicates how it will be displayed in the Web UI
- for ints, floats, and enums, a **range** indicating minimum and maximum allowable values

### Value Types

The architecture supports a number of different **value types** that programmers
are already familiar with, including *booleans, integers, floating point numbers,
single characters, strings, enumerated types, bitwise types* and *datetimes*.

```
#define VALUE_TYPE_BOOL    'B'        // a boolean (0 or 1)
#define VALUE_TYPE_CHAR    'C'        // a single character
#define VALUE_TYPE_STRING  'S'        // a string
#define VALUE_TYPE_INT     'I'        // a signed 32 bit integer
#define VALUE_TYPE_TIME    'T'        // time stored as 32 bit unsigned integer
#define VALUE_TYPE_FLOAT   'F'        // a float
#define VALUE_TYPE_ENUM    'E'        // enumerated integer
#define VALUE_TYPE_BENUM   'J'        // bitwise integer
#define VALUE_TYPE_COMMAND 'X'        // monadic (commands)
```

There is also the notion of a value being a "command".  A command does not have an actual value
associated with it, but presents itself as a button in the WebUI, or a single word that can be
typed into the serial UI that **does something**, calling a piece of code you write. Two stock,
pre-implemented commands included in this framework are **REBOOT** and **FACTORY_RESET**.

### Storage Types

When you define your values you also set their **storage type** which determines the
visibility and persistence of your values.   For instance, some values can be
merely stored in the ESP32's RAM memory at runtime, and have no persistency across
reboots.  Other values can be stored as *preferences* in EEPROM that persist across
reboots and drive your program's behavior.   A value can be **published** so that
the WebUI and/or MQTT can see it, and/or **subscribed** so that the program is
notified when it is changed externally by the WebUI and/or MQTT, or both.

```
#define VALUE_STORE_PROG      0x00      // only in ESP32 memory (or not anywhere at all)
#define VALUE_STORE_NVS       0x01      // stored/retrieved from NVS (EEPROM)
#define VALUE_STORE_WS        0x02      // broadcast to / received from WebSockets
#define VALUE_STORE_MQTT_PUB  0x04      // published to (the) MQTT broker
#define VALUE_STORE_MQTT_SUB  0x08      // subscribed to from (the) MQTT broker
```

These **storage types** can be combined bitwise, and there are some defines for
common combinations:

```
#define VALUE_STORE_PREF      (VALUE_STORE_NVS | VALUE_STORE_WS)
#define VALUE_STORE_TOPIC     (VALUE_STORE_MQTT_PUB | VALUE_STORE_MQTT_SUB | VALUE_STORE_WS)
#define VALUE_STORE_PUB       (VALUE_STORE_MQTT_PUB | VALUE_STORE_WS)
```

In this notion, a *preference* is something that is stored in NVS (non-volatile storage == EEPROM),
and which is nominally sent to and received from the WebSockets (WebUI) user interface.  The common
MQTT concepts of a *topic* (which is published and can be subscribed to) or a *publish-only* value
are encapsulated in the latter two of these combined bitwise defines.

### Value Styles

The rendering **style** determines other characteristics of the value including
things like whether or not it can be modified externally (VALUE_STYLE_READONLY),
whether, as a string, it should encrypted and generally hidden from view (VALUE_STYLE_PASSWORD),


Values

Those values can be monitored externally
so that you can

It can **publish** those values
so that they can be seen









## Motivation

This project represents my foray into the early world of IOT devices.

It can be built with MQTT capabilities in order to work with existing IOT frameworks
like HomeAssistant, Mosquito and NodeRed, but in the end I found those kinds of existing
solutions to be overly complicated to implement and use, even if they do support the whole
world of Alexa's, Siri's, and Phone Apps.  The existing architectures that I found focus
far more on the integration of items into the network than on developing simple,
easy to understand devices.



These my_IOT devices present a Webpage on your private local Wifi LAN and show up
automatically as SSDP Network devices on your Windows (or Mac) computer.  You just go
to your Network folder, click on the device's icon, and the webpage pops up.

That existing, well documented and understood functionality has already existed
for 10-20 years or more and does not require brain surgery or locking yourself into
the latest trend in order to create and develop a simple ESP32 device.

Only after that is accomplished does it make sense to then integrate that device
into a network of things, and there are a zillion ways, and more every day, to do that,
perhaps the most common, and simple at this time, being the use of the
MQTT protocol.  But at the point you want to consolidate multiuple devices, there is
an explosion of complexity and the focus switches entirely from the Device
to the Network and the architecture that is chosen to drive that.

Without MQTT or any other framework extensions, my_IOT devices are basically simple parameter
driven devices with a decent UI for modifing the parameters and monitoring the device.
The simple UI also automatically supports OTA (Over the Air Updates) as well as including
a full SD and SPIFFS File Manager so that you can upload and manage files on the device.
The devices also automatically support datetime stamped Logging to a text file on an SDCard
if one is present, with parameterized Debug and Logging levels.

Most other IOT frameworks don't even take advantage of the ESP32's ability to BE a network
device or have any mass storage of it's own.  Rather they devolve down to the MQTT level and
foist all of the UI and any other interactions with the device off onto the framework ...
some other device that you MUST manage and maintain if you want to interact with the device.

Sheesh.

I spent literally months learning how to install ESPHome and Home Assistant on rPi's,
learning the hard way that it is anything except "open source", even though it is.
ESPHome requires you to write your "device" in their terms, in a limited configuration
language.  And as for HA, sure, it's open source if you wanna compile a hugely complicated
program and manage a whole linux distro, but otherwise, you end up getting locked into the
HA distro and then being obligated to do all subsquent UI development in their framework.

Trying to install HA onto an existing Raspian distro was a horrible experience,
with dozens of tries, hugely complicated and fragile shell scripts, and the usual linux
instabilities.  So I switched Mosquito and Node Red ... pieces of software that
support MQTT and allow you to build a UI that you can at least fairly easily install, although
then, once again, you are immediately locked into doing all UI development in their terms,
and accepting any performance or other limitations of MQTT in the process.

I just did not like any of it.

I am just trying to make a simple ESP32 parameterized device with a decent UI !!!

So, yes, this framework supports MQTT and can exist and interact with existing IOT frameworks.
You can control a my_IOT device with your Alexa if you want.

Peronally, for the conglomeration of devices and user interface presentation, I chose an entirely
different approach, instead of MQTT, based on software I had already written, and somewhat perfected, over
the last 20 years, in Perl, including public internet facing HTTPS servers and private LAN
SSDP servers .. and that architecture does not limit itself to the MQTT bottleneck to commuicate
with devices. THAT project is a separate project and explained elsewhere.

THIS project is all about creating devices on ESP32's and simply monitoring and controlling them,
that CAN be, if chosen, integrated into an existing IOT.  But for many cases you just want to create
a controllable ESP32 device, and could care less if it's available on the public internet at large,
or integrated into your Alexa so you can talk to it.

## Building a myIOT Device




## Included external JS Source

- jquery-3.6.0.min.js
	https://jquery.com/download/
- bootstrap5.1.3.bundle.min.js
- bootstrap5.1.3.min.css
	https://getbootstrap.com/docs/5.1/getting-started/download/
- bootstrap-input-spinner.js Version 3.1.7 by Stefen Haack(modified)
	https://github.com/shaack/bootstrap-input-spinner
