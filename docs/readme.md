# myIOT - Home

**Home** --
**[Getting Started](getting_started.md)** --
**[Wifi](wifi.md)** --
**[Basics](basics.md)** --
**[How To](how_to.md)** --
**[Design](design.md)** --
**[Details](details.md)**

**myIOT** is an IOT architecture (framework) that provides a somewhat general
approach to building simple *parameter driven* devices with ESP32's and monitoring and
controlling them.

This framework is **device centric** -- not network centric -- and starts by giving
you the ability to implement an ESP32 "device" and control it via the serial port, since
this is probably how you initially program and interact with your ESP32 (using the Arduino
IDE and serial monitor).

Secondly, if you want, with a compile flag, you can also control it over Wifi using
**Telnet**, with a program like Putty or any other common Telnet Client.

Thirdly, each *myIOT device* is automatically configured as an **SSDP** (Services and Search Discovery Protocol)
compatible device, so that it can be "found" on your home wifi network, and
each device presents its own **Web User Interface** (Webpage), that you can
access over Wifi, via a Web Browser, without the need for any other third
party programs orapplications.

Finally, these devices can support **[MQTT](https://en.wikipedia.org/wiki/MQTT)** so they
can be integrated into existing IOT frameworks like HomeAssistant, or Mosquito and NodeRed,
so that you can monitor and control them in other ways, including via Siri and Alexa
voice interfaces.

This readme is broken up into several sub-pages.

- **[Getting Started](getting_started.md)** - Installing the library and building the example program
- **[Wifi](wifi.md)** - Accessing the device via Wifi and the supplied WebUI
- **[Basics](basics.md)** - A basic description of Values and a breakdown of the testDevice.ino example program
- **[HowTo](how_to.md)** - A short tutorial on how to create your own myIOT device.
- **[Design](design.md)** - A review of the overall design of this project and it's API.
- **[Details](details.md)** - Motivation, Implementation details, and more in-depth documentation

*note: this architecture presumes that you have a good understanding of **C++** and
how to derive from and extend existing **classes** and are generally familiar with
the **Arduino IDE** devlopement environment and know how to manually add **libraries** to it.*

## Also See

I have built a number of devices based on this library.  They may be useful
for understanding more about how to use it.  Please see the
following repositories for more information:

- **[bilgeAlarm](https://github.com/phorton1/Arduino-bilgeAlarm)** - a device to monitor and control two bilge pumps on a sailboat that includes a car alarm if things go wrong
- **[theClock](https://github.com/phorton1/Arduino-theClock)** - my original laser cut wooden gear clock with a magnetic pendulum
- **[theClock3](https://github.com/phorton1/Arduino-theClock3)** - a *better*, **more accurate** electromagnetic wooden geared clock


## License

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License Version 3 as published by
the Free Software Foundation.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

Please see [LICENSE.TXT](https://github.com/phorton1/Arduino-libraries-myIOT/blob/master/LICENSE.TXT) for more information.

## Credits

This library uses a number of additional Arduino C++ Libraries as well as actually
including copies of several Javascript libraries.  Without the work done by the
people that provided those libraries, this project would not exist.  So I would
like to thank the following folks and efforts:

### Arduino and ESP32 teams, and the developer of the Arduino ESP32 filesystem uploader

- [**Arduino IDE Team**](https://www.arduino.cc/en/software)
- [**ESP32 Team**](https://docs.espressif.com/projects/arduino-esp32/en/latest/installing.html)
- [**Arduino ESP32 filesystem uploader**](https://github.com/me-no-dev/arduino-esp32fs-plugin) by **Hristo Gochkov**

### Arduino Libraries used by this Project

- **WebSockets** by Markus Sattler
- **ArduinoJson** by Benoit Blanchon
- **PubSubClient** by Nick O'Leary
- **ESP Telnet** by Lennart Hennings

### Open Source Javascript included in this project

- [jquery-3.6.0.min.js](https://jquery.com/download/)
- [bootstrap5.1.3.bundle.min.js and bootstrap5.1.3.min.css](https://getbootstrap.com/docs/5.1/getting-started/download/)
- [bootstrap-input-spinner.js](https://github.com/shaack/bootstrap-input-spinner) Version 3.1.7 by **Stefen Haack**


Next: **[Getting Started](getting_started.md)**