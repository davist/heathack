Speak-IP

Speak out the Pi's IP address on the audio out.

To install, run ./install.sh from within this directory. This will install the if-up script so that speak-ip will run every time a network interface starts up.

Audio files

The default audio in mstts-audio/ was generated using the built-in text-to-speech in Windows.

The alternative files in voxeo-audio/ are from https://evolution.voxeo.com/tools/, released under the LGPL licence.

To change between them, comment/uncomment the AUDIO_DIR line in speak-ip.sh.

The voxeo files don't have very high quality audio as they were designed for telephony applications, but the speech quality is very professional and consistent.
