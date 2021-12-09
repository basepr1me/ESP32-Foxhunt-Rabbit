# Club_Rabbit
ESP32 software for building a Foxhunt rabbit.

# Library Requirements
XT_DAC_Audio: https://www.xtronical.com/the-dacaudio-library-download-and-installation/

Morse: https://github.com/basepr1me/Morse

AsyncTCP: https://github.com/me-no-dev/AsyncTCP

ESPAsyncWebServer: https://github.com/me-no-dev/ESPAsyncWebServer

# audio_call.h information
A file called audio_call.h is required for this to run. Obviously, I'm not going
to upload my club's audio file, because it is only legal for club members to
use it.

To create your own file, the following website has the information you need: https://circuitdigest.com/microcontroller-projects/esp32-based-audio-player

However, I modified XT_DAC_Audio and the audio_call.h instructions to place the
large file in PROGMEM. The author of the blog states that the ESP32 memory space
is small, but that's not true. They simply aren't putting the audio in the right
chunks. Constify the WavData and set audio in audio_call.h to:

const unsigned char audio[######] PROGMEM

I'm not certain the XT_DAC_Audio writer knows this either.

# WIP
This is in a WIP state and currently only transmits audio. Sending Morse code is not yet completed.

This style is OpenBSD style and may not look right in the Arduino IDE, since I don't use it.
