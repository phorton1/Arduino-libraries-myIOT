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

The two projects I have implemented using this archecture so far are a
[**bilge pump alarm**](https://github.com/phorton1/Arduino-bilgeAlarm) for my boat, and a laser cut
[**wooden geared clock**](https://github.com/phorton1/Arduino-theClock) that uses the ESP32 to drive an electromagetic pendulum.

On the bilgeAlarm there are a number of values that can be seen, like "when did the bilge pump last run?",
along with a number of parameters that can be set, including the ability to turn the pump(s) on remotely, and/or
define conditions under which the (car-alarm!!!) alarm will sound .. like "if the pump runs more than 5 times in
an hour, more than 20 times in a day, or for more than 30 seconds, trigger an alarm!". In fact the bilgeAlarm,
in addition to keeping a logfile of all activity, keeps a history of every time it has run, and how long it ran
each time.

That information is useful when trying to determine if there are problems on the boat, and/or if they have been corrected.

On the clock I use the myIOT framework to allow me to view statistics on how well the clock is performing (how many milliseconds,
ahead or behind, has it run overall, and per tick) along with the ability to parameterize the controller, setting the duration,
and strength of the pulses sent to the electromagnet, and to modify the values for the PID controller that tries to keep the
clock running precisely.


In any event, you will begin by thinking about, and implementing the **values** used by, and presented by, your device.

Then you will add code to do things, either in the INO setup() and loop() methods, your derived setup() and loop() methods,
via ESP32 tasks and/or interrupts, or howeveer you would normally code your device.

**The best way to learn to create a myIOT device is to try it!!**

Next we will delve into the design of the myIOT library and the
main API's it presents.

Next:  **[Design](design.md)**
