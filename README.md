# SLOW_SCAN
slowscan-teensy is an SSTV decoder project for the Teensy 4.x.

<img width="1500" height="957" alt="mu-scan" src="https://github.com/user-attachments/assets/37d7bfba-863d-4ca4-a8a5-f91c49410bfc" />

The decoder currently only supports Martin M1, though more modes may be implemented in the future.

## Components
Teensy 4.x

[ILI9341 compatible display](https://www.adafruit.com/product/1480)

[Electret Microphone with Amplifier](https://www.adafruit.com/product/1713)

## Wiring

<img width="1500" height="1125" alt="Embedded Final Wiring Diagram" src="https://github.com/user-attachments/assets/d921f3d8-a60d-4883-b007-5f4179c48983" />


## Uploading Firmware

This project offers two build environments:

| Environment | Description |
|-------------|-------------|
| mic-in | Read audio input from the microphone attached to pin A2 |
| usb-in | Read audio input from the host device attached via USB |

Debug outputs may be enabled by editing debug.h:

| Flag | Description |
|------|-------------|
| DEBUG_WAVEFORM | Enables serial output of the waveform read in from the selected input source |
| DEBUG_FREQUENCY | Enables serial output of the frequencies computed from the audio input |
| DEBUG_DECODER_STATE | Enables debug messages for the decoder's state machine |
| DEBUG_DECODER_OUTPUT | Enables serial output of the RGB data detected by the decoder. This output can be interpreted into an image live by redirecting the serial monitor output to a file and passing the file name to image.py in the scripts folder. |

I prefer the PlatformIO CLI, so I use the command below for uploading the firmware:

```pio run -t upload -e {environment_name}```
