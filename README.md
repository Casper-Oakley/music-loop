# music-loop
A C++ portaudio extension for visualising alsa input.

## Messages

This program is intented to be used with a localhost rabbitMQ server, where it publishes every second 256 bins. These can be consumed by any other program, for whatever usage said program desires. Originally written to be used in conjunction with [Loopback Audio Visualiser](https://github.com/casper-oakley/loopback-audio-visualiser).

