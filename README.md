# ESP32-Foxhunt-Rabbit

Software for building a foxhunt rabbit using the Espressif ESP32U line of chips.

# audio_call.h information

For both the Rabbit_Web_UI and Rabbit_Controller, a file called audio_call.h is
required for this to run. Obviously, I'm not going to upload my club's audio
file, because it is only legal for club members to use it.

To create your own file, the following website has the information you need:
https://circuitdigest.com/microcontroller-projects/esp32-based-audio-player

Notes
-----

I modified XT_DAC_Audio and the audio_call.h instructions to place the large
file in PROGMEM. The author of the blog states that the ESP32 memory space is
small, but that's not true. They simply aren't putting the audio in the right
chunks. Constify the WavData and set audio in audio_call.h to:

		const unsigned char audio[######] PROGMEM

# Library Requirements

Both the Rabbit_Web_UI and Rabbit_Controller require the following libraries:

Libraries
---------

XT_DAC_Audio:
https://www.xtronical.com/the-dacaudio-library-download-and-installation/

ESP32-Morse-Code:
https://github.com/basepr1me/ESP32-Morse-Code

# Rabbit_Web_UI

The Rabbit_Web_UI is a stand-alone ESP32, which can be controlled via a web
browser. It requires two additional libraries:

Libraries
---------

AsyncTCP:
https://github.com/me-no-dev/AsyncTCP

ESPAsyncWebServer:
https://github.com/me-no-dev/ESPAsyncWebServer

Notes
-----

To turn enable hunting to start when turning on the rabbit, set hunting = 1.

Also, note the notes about the following in the ino file:

		#define CONFIG_ESP32_WIFI_AMPDU_TX_ENABLED 0
		#define CONFIG_ESP32_WIFI_AMPDU_RX_ENABLED 0

# Rabbit_Controller and Rabbit_Remote

The Rabbit_Controller and Rabbit_Remote use two ESP32s, one acting as the rabbit
and one acting as the remote control, which works with the Windows
Rabbit_Manager software. The ESP32s connect via LR mode and it is advisable to
have a large, WiFi yagi on the remote. With this setup, line-of-sight works to
approximately 1/8 of a mile in my testing, with a +3 dBi antenna on the rabbit.
Espressif specs state this should work at .6 miles, but I believe that would
require two directional antennas, which would make the rabbit much easier to
find in a foxhunt. However, an 1/8 mile is plenty to hide from hunters.

# Rabbit_Manager

This is the Windows software used to control the rabbit via the Rabbit_Remote
controller. Rabbit_Remote acts as a repeater to control the rabbit.
Rabbit_Remote also pings the rabbit every 10 seconds to ensure Rabbit_Manager
knows that the two ESP32s are still connected.

# Additional Notes

This style is OpenBSD style and may not look right in the Arduino IDE, since
I don't use it. See the [rabbit.pdf](rabbit.pdf) for an example schematic to get
your rabbit started. I used a cheap Baofeng UV-5R as the transmitter, using the
earpiece to connect to the ESP32 and a specially designed ribbon cable for
controlling power from the radio with the battery installed. The schematic and
this respository are subject to large changes.

Author
------

[Tracey Emery](https://github.com/basepr1me/)

If you like this software, consider [donating](https://k7tle.com/?donate=1).

See the [License](LICENSE.md) file for more information.
