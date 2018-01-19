This little program turns my ICOM-7300 ham radio transciever on or off from the
Linux PC it is connected to via the USB. It implements a few of the commands
described in the full user manual. In addition to on and off it can be used to
read Vd (the battery voltage) and the S-Meter.  This little program makes it
possible for me to remotely access the Linux PC, turn on my radio, play with
fldigi/flrig, check the battery, and turn off the radio.

Before using this turn off "Menu", "Set", "Connetors", "USB SEND" on the radio.
Apparently with Linux it is not possible to open a serial port without
momentarily activating DTR and RTS.
https://stackoverflow.com/questions/5090451/how-to-open-serial-port-in-linux-without-changing-any-pin/21753723
If "USB SEND" is anything other than off a short transmit control will be sent
from the PC anytime this little program is used. (Use "Config", "Setup",
"Tranciever", "PTT via CAT", "PTT via CAT" on flrig). It is okay to use this
little program to turn the radio on from off even if "USB SEND" is not off
because the RTS and DTR lines are back to not activated long before the radio
boots up.

If you use this little program you may want to change the baud rate, or make it
selectable through arguments. I have my radio set at 115200 baud because I want
to play with reading the waterfall over the serial/USB if I suddenly end up
with spare time.
