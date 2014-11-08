Sound Activated Wonder Lantern
==============
Wonder Lantern for Arduino (Flora) and Adafruit NeoPixel LEDs.

Project Homepage: http://stuff.3r13.nl/2014/11/wonder-lantern/

This script is based off of Adafruit's Ampli-tie project:
https://github.com/adafruit/LED-Ampli-Tie-with-Flora

Hardware requirements:
 - Most Arduino or Arduino-compatible boards (ATmega 328P or better).
 - Adafruit Electret Microphone Amplifier (ID: 1063)
 - Adafruit Flora RGB Smart Pixels (ID: 1260)
   OR
 - Adafruit NeoPixel Digital LED strip (ID: 1138)
 - Optional: battery for portable use (else power through USB or adapter)
Software requirements:
 - Adafruit NeoPixel library

Connections:
 - 3.3V to mic amp +
 - GND to mic amp -
 - Analog pin to microphone output (configurable below)
 - Digital pin to LED data input (configurable below)
 See notes in setup() regarding 5V vs. 3.3V boards - there may be an
 extra connection to make and one line of code to enable or disable.

Written by Adafruit Industries.  Distributed under the BSD license.
Rewritten by Eriestuff, to add twinkle effect.
This paragraph must be included in any redistribution.

