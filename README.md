# GarageLEDDriver
Simple LED dimmer turning on from inputs

12V power supply powering 12V LED Strips through MOSFETs. 

A 12V motion detector is used as a trigger (input has a voltage dividor to arduino input).

Also a 120V relay is used to interface to the 120V lights already on the ceiling.  If motion is detected or the preinstalled lights are turned on, this program will fade the lights on in 1 seconds

Uses PWM library to alter standard arduino library from 600hz to 120hz increasing the resolution from 8bit to ~15.1bits or 33196 duty cycles.
