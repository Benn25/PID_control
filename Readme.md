# Abstract

This is the code for a simple device that can control the temperature inside a hot chamber, ideally for a 3D printer.
It works with an arduino (any type should work).

the outputs are:
- 1pin for the temp control (basically the control pin for a SSR)
- 3pins for a addressible LED strip (if you want to add light)

the inputs are
- POWER (5V + GND)
- Temp sensor
- potentiometer


## You will need the following:

-Arduino (UNO, NANO, Pro Micro...)

-OLED display SSD1306_128X32 IIC

-10K potentiometer

-100K temp sensor + 100K resistor

-a blank PCB

-1 PCB switch

Optional:

-addressible LED strip (led ws2812b is perfect)

-the enclosure files I made (and 3D print it). It fits a Ardiono pro micro.

-the following libraries : PID, Fastled and U8glib
