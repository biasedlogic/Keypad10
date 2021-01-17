# Keypad10
A simple external keypad based on Arduino Micro Pro / ATMega32u4, emulates HID Keyboard and is intended to use with Open Broadcaster Studio (OBS).

![Photo of the keypad](https://github.com/biasedlogic/Keypad10/blob/main/PXL_20201229_113510824.jpg?raw=true)

# What does it do?

It emulates an external keyboard with keys not found normally on keyboards: F13-F22. The idea was to bind OBS macros/hotkeys to keys that cannot possibly be used by any real - even if totally obscure - software running on the PC. So this keypad does not need any special driver, it emulates HID Keyboard, plug and play, and then you have 10 extra keys that you know won't be already bound to something.

The keypad has freely configurable key options:

1. There's your ordinary simple button. You press it, it sends a key pressed event, you release it, it sends key released event. It can be used for gaming as well as for any other purpose. The key LED lights up when you press it and goes dark as soon as you release it.
2. There'e a radio-button-group mode, where some keys e.g. switch scenes or weapons. While they still act on keyboard level just as any key would, their LED stays on until you press another key from the same group, so that you know which option/scene/weapon you selected last.
3. Then there's a toggle key. You press it once, it sends e.g. F13 keypress followed immediately by key release and its LED lights up. You press it again, it will send CTRL-F13 (if it was the F13 key) and release it immediately, also extinguishing its LED. So you can bind the two key combinations to an action activating something and then deactivating it and have the LED indicate present state reliably. You can e.g. start/stop streaming or recording that way. While OBS can use same key as a toggle between start/stop you can't tell blindly whether it started or stopped on the last hotkey press. This way you are sure.

The code allows for up as many switches and LEDs as you want, as long as you don't run out of pins. The reference hardware is anr Arduino Pro Micro and the limit is at 11 buttons with 12 LEDs (11 + 3 + 4 = 18 pins), since the code reads buttons directly (1 button = 1 pin) and only LED output is multiplexed. 

Theoretically you could raise that number to 20/20 by multiplexing both, keys and LEDs, but that will need changes in code and I didn't need it.

Default configuration is: first four keys (in my pack that's a single top key and a first row of three) are toggles, then the next 6 keys are single radio group for scene switching.

# Components

For my implementation I used:
* 1x Arduino Pro Micro (ATMega 32u4) board
* 10x Cherry MX switches of your choice (LED option) or compatible mechanically
* 12x 3mm LEDs in color of your choice
* 3 universal screws 9mm long, 3.5mm diameter, flat head
* Some wire, solder, etc.

Arduino code is provided.
STL files for a housing to hold 10 MX switches and an Arduino board as well.

# Update - RGB version

![Photo of the RGB version](https://github.com/biasedlogic/Keypad10/blob/main/Keypad10rgb/DSC04994.JPG?raw=true)

This is using RGB LEDs WS2812B-MINI under the keys. Schematic is in the directory with the code (keypad10rgb), the code is documented with some comments and can be easily modified for a different pinning and/or keycount. 
I have designed a slightly different housing and a PCB to mount it all, if there's interest I can provide the PCBs.
