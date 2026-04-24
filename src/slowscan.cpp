#include <Audio.h>
#include <SPI.h>
#include <Adafruit_ILI9341.h>

#include "FrequencyDemodulator.h"
#include "SSTV.h"

#include "debug.h"

// Audio Capture
static const int PACKET_SIZE = 128; // size of array returned by readBuffer()
AudioRecordQueue queue;

#if USB_IN
AudioInputAnalog analog_in;
AudioConnection analog2queue(analog_in, queue);
#elif MIC_IN
AudioInputAnalog analog_in(A2);
AudioFilterBiquad filter;
AudioConnection analog2filter(analog_in, filter);
AudioConnection filter2queue(filter, queue);
#endif

// Frequency Demodulation
static const int BUFFER_SIZE = FrequencyDemodulator::BUFFER_SIZE;
static const int BUFFER_HALF = BUFFER_SIZE / 2;
static const int TRIM_OFFSET= BUFFER_SIZE / 4;
FrequencyDemodulator freq;

// SSTV Decoding
static const int SCANLINE_WIDTH = SSTV::MARTIN_M1_SCANLINE_WIDTH;
SSTV sstv;

// TFT Display
static const int TFT_DC = 21;
static const int TFT_CS = 10;
static const int TFT_HEIGHT = 240;
static const int TFT_WIDTH = 320;

Adafruit_ILI9341 display(TFT_CS, TFT_DC);

// Util Functions
uint16_t rgb565(uint8_t red, uint8_t green, uint8_t blue);

void setup()
{
    display.begin();
    display.setRotation(1);

    display.fillScreen(ILI9341_BLACK);

    AudioMemory(256);

#if MIC_IN
    // TODO: Test how this or a bandpass filter affect decoding
    filter.setLowpass(0, 2400);
    filter.setHighpass(1, 1000);
#endif
    queue.begin();

    Serial.begin(9600);
}

void loop()
{
    static int wf_index = 0;
    static double waveform_real[BUFFER_SIZE];
    static double frequencies[BUFFER_SIZE-1];

    static SSTV::Pixel scanline_data[320];

    if (!queue.available()) {
        return;
    }

    int16_t* buffer = queue.readBuffer();

    // Read samples from audio queue until the WF buffer is full.
    // No spillover logic needed if the buffer size is a power of 2.
    for (int i = 0; i < PACKET_SIZE && wf_index < BUFFER_SIZE; i++) {
        waveform_real[wf_index] = (double) buffer[i] / INT16_MAX;
        wf_index++;
    }

    queue.freeBuffer();

    if (wf_index != BUFFER_SIZE) {
        return; // Buffer not yet full, return early.
    }

#if DEBUG_WAVEFORM
    for (int index = 0; index < BUFFER_SIZE; index++) {
        Serial.printf("%lf\n", waveform_real[index]);
    }
#endif

    freq.frequencies(waveform_real, frequencies);

    // Extract stable frequencies from the center of the frequency buffer.
    double trimmed_frequencies[BUFFER_HALF];
    for (int index = 0; index < BUFFER_HALF; index++) {
        trimmed_frequencies[index] = frequencies[index+TRIM_OFFSET];
    }

#if DEBUG_FREQUENCY
    for (int index = 0; index < BUFFER_HALF; index++) {
        Serial.printf("%lf\n",trimmed_frequencies[index]);
    }
#endif

    bool scanline_ready = sstv.process_frequencies(trimmed_frequencies,
                                                   BUFFER_HALF);

    // NOTE: Currently halts entirely once 240 scanlines are read.
    // Maybe add a button for reset?
    static int y_pos = 0;
    if (scanline_ready && y_pos < TFT_HEIGHT) {

        sstv.retrieve_scanline(scanline_data);

        for (int x_pos = 0; x_pos < SCANLINE_WIDTH; x_pos++) {
            SSTV::Pixel pixel = scanline_data[x_pos];

            // Convert color data to packed 16-bit color.
            uint16_t tft_color = rgb565(pixel.red, pixel.green, pixel.blue);

            display.drawPixel(x_pos, y_pos, tft_color);

#if DEBUG_DECODER_OUTPUT
            Serial.printf("%d,%d,%d\n", pixel.red, pixel.green, pixel.blue);
#endif

        }

        y_pos++;
    }

    // Move latter half of waveform data to beginning.
    // 50% overlap means no data is lost since already processed samples
    // will be trimmed in the next run while samples previously trimmed
    // from the end will now be stable.
    for (int index = 0; index < BUFFER_HALF; index++) {
        waveform_real[index] = waveform_real[index + BUFFER_HALF];
    }
    wf_index = BUFFER_HALF;
}

uint16_t rgb565(uint8_t red, uint8_t green, uint8_t blue)
{
    uint16_t hi_color = (red >> 3) << 11;
    hi_color |= (green >> 2) << 5;
    hi_color |= (blue >> 3);

    return hi_color;
}

