# Burst v1.1

This code is made for Befaco's Burst module. A trigger burst generator with exponential/logaritmic distribution and probability
Find further information [here](https://www.befaco.org/en/burst/)


## Dependencies:

[RotaryEncoder](https://github.com/0xPIT/encoder/tree/arduino)


## Uploading the firmware


1. Get an ICSP Programmer 
We normally use an USBasp programmer like this one https://www.ebay.com/itm/USBASP-USB-ISP-Programmer-for-Atmel-AVR-ATMega328-ATMega32U4-Arduino-/322662323277

2. Download Arduino IDE
Go to the official Arduino website https://www.arduino.cc/en/Main/Software and download the latest version of Arduino IDE for your operating system and install it.

3. Download the firmware 
clone or download thisrepository. Go to https://github.com/Befaco/burst and press "clone or download" to download the repository to your computer. Extract the zip file and copy "firmware" folder to your sketchbook folder. 

4. Install the libraries
Download the libraries  (Rotary encoder) and install them following the instructions from this link https://www.arduino.cc/en/Guide/Libraries

5. Connecting the module
Connect the programmer to the ICSP conector in the back of the module. Pay special attention to the pinout when you plug the module. You can check this link for more information about ICSP https://www.arduino.cc/en/Tutorial/ArduinoISP

6. Upgrading
Press "Open" and search the file "BURST.ino" located on the "BURST" folder. Go to "Sketch" and hit "Upload using programmer".

If everything goes well you should see a "Done uploading" message in a few seconds. If something happens during the upgrade, check your Arduino settings following the instructions of this link https://www.arduino.cc/en/Guide/Troubleshooting


## Credits.

Coded by Eloi Flowers (Winter Modular) and Jeremy Bernstein
