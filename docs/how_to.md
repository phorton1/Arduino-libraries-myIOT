# myIOT - How To

**[Home](readme.md)** --
**[Getting Started](getting_started.md)** --
**[Wifi](wifi.md)** --
**[Basics](basics.md)** --
**How To** --
**[Design](design.md)** --
**[Details](details.md)**

This page describes the steps for you to create your own myIOT device.

Probably the simplest approach is to copy the testDevice.ino sketch to a new Arduino folder,
and modify it from there.

- copy the testDevice subfolder to your Arduino sketch folder
- rename the folder to whatever you want (i.e. "myDevice")
- rename the ino file appropriately (i.e. "myDevice.ino")
- edit the ino file and replace all occurances of "testDevice" with your name "myDevice"
- edit the ino file and change other constants and variables (i.e. "TEST_DEVICE" and "test_device") appropriately.
- compile and upload it, and upload the data directory as you did in the [Getting Started](getting_started.md) for the testDevice.

At that point you are ready to start designing and adding the values for your particular device.

I envision that an ESP32 is typically used to implement something more than a simple lightbulb on/off switch.

A weather station, for instance, might have floating point values for the temperature and humidity, wind speed, and direction,
and might be configurable to send emails at some period of time, or upon some condition (it's REALLY WINDY!!). Or a home security
system might have things like passwords and overrides, and you might be interested in keeping a log of all activity on an SD card.


In any event, you will begin by thinking about, and implementing the **values** used by, and presented by, your device.

Then you will add code to do things, either in the INO setup() and loop() methods, your derived setup() and loop() methods,
via ESP32 tasks and/or interrupts, or howeveer you would normally code your device.

**The best way to learn to create a myIOT device is to try it!!**

Next we will delve into the design of the myIOT library and the
main API's it presents.

Next:  **[Design](design.md)**
