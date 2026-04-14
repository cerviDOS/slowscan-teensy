#include <Audio.h>
#include "FrequencyDemodulator.h"
#include "SSTV.h"

static const int PACKET_SIZE = 128;

AudioInputUSB usb_in;
AudioRecordQueue queue;
AudioConnection usb2queue(usb_in, queue);

static const int BUFFER_SIZE = FrequencyDemodulator::BUFFER_SIZE;
static const int BUFFER_HALF = BUFFER_SIZE / 2;
static const int TRIM_OFFSET= BUFFER_SIZE / 4;
FrequencyDemodulator freq;

SSTV sstv;

void setup()
{
    AudioMemory(256);
    queue.begin();

    Serial.begin(9600);
}

void loop()
{
    static int wf_index = 0;
    static double waveform_real[BUFFER_SIZE];
    static double frequencies[BUFFER_SIZE-1];

    if (queue.available()) {
        int16_t* buffer = queue.readBuffer();

        for (int i = 0; i < PACKET_SIZE && wf_index < BUFFER_SIZE; i++) {
            waveform_real[wf_index] = (double) buffer[i] / INT16_MAX;
            wf_index++;
        }

        if (wf_index == BUFFER_SIZE) {
            double trimmed_frequencies[BUFFER_HALF];

            freq.inst_freq(waveform_real, frequencies);
            // Extract stable frequencies from the center of the frequency buffer
            for (int index = 0; index < BUFFER_HALF; index++) {
                //Serial.println(frequencies[index+TRIM_OFFSET]);
                trimmed_frequencies[index] = frequencies[index+TRIM_OFFSET];
            }
            
            sstv.process_frequencies(trimmed_frequencies, BUFFER_HALF);

            // Move latter half of waveform data to beginning.
            // 50% overlap means no data is lost since already processed frequencies
            // will be trimmed in the next run while frequencies previously trimmed 
            // from the end will now be stable.
            for (int index = 0; index < BUFFER_HALF; index++) {
                waveform_real[index] = waveform_real[index + BUFFER_HALF];
            }
            wf_index = BUFFER_HALF;
        }

        queue.freeBuffer();
    }
}
