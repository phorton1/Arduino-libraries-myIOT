# my_IOT

**my_IOT** is an IOT architecture for ESP32's that provides a somewhat general
approach to building simple devices and monitoring and controlling them.

## Motivation

This project represents my foray into the early world of IOT devices.

It can be built with MQTT capabilities in order to work with existing IOT frameworks
like HomeAssistant, Mosquito and NodeRed, but in the end I found those kinds of existing
solutions to be overly complicated to implement and use, even if they do support the whole
world of Alexa's, Siri's, and Phone Apps.  The existing architectures that I found focus
far more on the integration of items into the network than on developing simple,
easy to understand devices.

This framework is device centric -- not network centric -- so you can simply design
and implement an ESP32 "device" and control it, first via the serial port, since
this is probably how you program and interact an ESP32, via the Arduino IDE during
initial development.  If you want, with a compile flag, you can control it via serial over
Telnet.

my_IOT devices themselves present a Webpage on your private local Wifi LAN and show up
automatically as SSDP Network devices on your Windows (or Mac) computer.  You just go
to your Network folder, click on the device's icon, and the webpage pops up.

That existing, well documented and understood functionality has already existed
for 10-20 years or more and does not require brain surgery or locking yourself into
the latest trend in order to create and develop a simple ESP32 device.

Only after that is accomplished does it make sense to then integrate that device
into a network of things, and there are a zillion ways, and more every day, to do that,
perhaps the most common, and simple at this time being the use of the
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

I am just trying to make a simple ESP32 parameterized device with a decent UI.

So, yes, this framework supports MQTT and can exist and interact with existing IOT frameworks.
You can control an my_IOT device with your Alexa if you want.

Peronally, for the conglomeration of devices and user interface presentation, I chose an entirely
different approach, instead of MQTT, based on software I had already written, and somewhat perfected, over
the last 20 years, in Perl, including public internet facing HTTPS servers and private LAN
SSDP servers .. and that architecture does not limit itself to the MQTT bottleneck to commuicate
with devices. THAT project is a separate project and explained elsewhere.

This project is all about creating devices on ESP32's and simply monitoring and controlling them,
that CAN be, if chosen, integrated into an existing IOT.  But for many cases you just want to create
a controllable ESP32 device, and could care less if it's available on the public internet at large,
or integrated into your Alexa so you can talk to it.

## Arduino Library Dependencies

- ESP Telnet by Lennart Hennings - Version 1.2.2 (optional)
- WebSockets by Markus Sattler - Version 2.3.6 (somewhat optional)
- ArduinoJson by Benoit Blanchon - Verson 6.18.5 (if using WebSockets - WITH_WS)
- PubSubClient by Nick O'Leary - Version 2.8.0 (if using MQTT - WITH_MQTT)

## Included external JS Source

- jquery-3.6.0.min.js
	https://jquery.com/download/
- bootstrap5.1.3.bundle.min.js
- bootstrap5.1.3.min.css
	https://getbootstrap.com/docs/5.1/getting-started/download/
- bootstrap-input-spinner.js Version 3.1.7 by Stefen Haack(modified)
	https://github.com/shaack/bootstrap-input-spinner
