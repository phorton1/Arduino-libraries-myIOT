# myIOT - Home

**Home** --
**[Getting Started](getting_started.md)** --
**[Wifi](wifi.md)** --
**[Basics](basics.md)** --
**[Design](design.md)** --
**[Details](details.md)** --

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

This readme is broken up into several sub-pages.

- **[Getting Started](getting_started.md)** - Installing the library and building the example program
- **[Wifi](wifi.md)** - Accessing the device via Wifi and the supplied WebUI
- **[Basics](basics.md)** - A basic description of Values and a breakdown of the testDevice.ino example program
- **[Design](design.md)** - A basic description of Values and a breakdown of the testDevice.ino example program
- **[Details](details.md)** - Motivation, Implementation details, and more in-depth documentation

*note: this architecture presumes that you have a good understanding of **C++** and
how to derive from and extend existing **classes** and are generally familiar with
the **Arduino IDE** devlopement environment and know how to manually add **libraries** to it.*
