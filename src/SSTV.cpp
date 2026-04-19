#include <Arduino.h>

#include "sstv.h"

// Mode timings in milliseconds

// TODO: Also include ~30 millisecond pulse that occurs after VIS code
// TODO: change millisecond timings here for sample counting

static const double MARTIN_M1_HSYNC_PULSE_MS = 4.862;
static const double MARTIN_M1_HSYNC_PORCH_MS = 0.572;
static const double MARTIN_M1_COLOR_SCAN_MS = 146.432;
static const double MARTIN_M1_MS_PER_PIXEL = MARTIN_M1_COLOR_SCAN_MS / SSTV::MARTIN_M1_SCANLINE_WIDTH;

// Mode frequencies in Hz

static const uint32_t MARTIN_M1_HSYNC_HZ = 1200;
static const uint32_t MARTIN_M1_COLOR_LOW_HZ = 1500;
static const uint32_t MARTIN_M1_COLOR_HIGH_HZ = 2300;

// Tolerance

static const uint32_t FREQUENCY_TOLERANCE = 50;
static const double TIMING_TOLERANCE_MS = 0.25;

namespace {

bool is_within_tolerance(double value, double target, double tolerance)
{
    double upper_bound = target + tolerance;
    double lower_bound = target - tolerance;
    return (value >= lower_bound) && (value <= upper_bound);
}

}

SSTV::SSTV(uint32_t sample_rate) :
    m_sample_rate(sample_rate),
    m_num_scanlines_processed(0),
    m_sample_clock(0),
    m_last_hsync_start(0),
    m_last_hsync_end(0),
    m_current_state(State::HSYNC_DETECTION),
    m_new_scanline_ready(false),
    m_completed_scanline(new Pixel[MARTIN_M1_SCANLINE_WIDTH]),
    m_scanline_in_progress(new Pixel[MARTIN_M1_SCANLINE_WIDTH])
{}

bool SSTV::validate_hsync_duration(uint64_t hsync_start, uint64_t hsync_end)
{
    double pulse_duration_ms = ((hsync_end - hsync_start) / (double) m_sample_rate) * 1000;

    return is_within_tolerance(pulse_duration_ms,
                               MARTIN_M1_HSYNC_PULSE_MS,
                               TIMING_TOLERANCE_MS);
}

uint16_t SSTV::detect_hsync(double frequency_data[], uint16_t frequency_count, uint16_t start_index)
{
    static bool within_hsync_candidate = false;

    // Debug
    //Serial.print("hsync scanning beginning at offset: ");
    //Serial.print(start_index);
    //Serial.print("/");
    //Serial.println(frequency_count);

    for (uint16_t index = start_index; index < frequency_count; index++) {
        double frequency = frequency_data[index];

        bool freq_within_hsync_bounds =
            is_within_tolerance(frequency,
                                MARTIN_M1_HSYNC_HZ,
                                FREQUENCY_TOLERANCE);

        if (freq_within_hsync_bounds && !within_hsync_candidate) {
            // ~1200 Hz detected AND not already inside an hsync candidate -> mark
            // sample time as beginning of new candidate.
            m_last_hsync_start = m_sample_clock;
            within_hsync_candidate = true;

        } else if (within_hsync_candidate && !freq_within_hsync_bounds) {
            // Not ~1200 Hz AND inside an hsync candidate -> candidate ended,
            // mark end and calculate its duration
            m_last_hsync_end = m_sample_clock;
            within_hsync_candidate = false;

            if (validate_hsync_duration(m_last_hsync_start, m_last_hsync_end)) {
                // Candidate is indeed an hsync, begin decoding scanline
                m_current_state = SCANLINE_DECODING;
                
                // Debug
                //Serial.print("hsync terminated at index: ");
                //Serial.print(index);
                //Serial.print("/");
                //Serial.println(frequency_count);
                return index+1;
            }
        }
        m_sample_clock++;
    }

    return frequency_count;
}

// NOTE: Might be faster to have a LUT perform this conversion
uint8_t SSTV::convert_frequency_to_intensity(double frequency)
{
    double result =
        (frequency - MARTIN_M1_COLOR_LOW_HZ)
        / (MARTIN_M1_COLOR_HIGH_HZ - MARTIN_M1_COLOR_LOW_HZ)
        * 255;

    if (result > 255) {
        return 255;
    } else if (result < 0) {
        return 0;
    } else {
        return (uint8_t) result;
    }
}

// BUG: Testing shows that 435 ms of samples are read instead of 439 - 441,
//      likely a rounding error
//
// BUG: Occasionally "slips" and returns a bunch of garbage
//      scanlines at once
uint16_t SSTV::decode_color_scan(double frequency_data[], uint16_t frequency_count, uint16_t start_index)
{
    enum ColorScanState { GREEN = 1, BLUE = 2, RED = 3 };
    static ColorScanState current_scan = GREEN;

    static const uint16_t samples_per_pixel =
        (MARTIN_M1_MS_PER_PIXEL / 1000) * m_sample_rate;

    // x position of the pixel whose color channel is currently being read
    static uint16_t current_pixel = 0;

    static uint16_t samples_read = 0;
    static double frequency_sum = 0.0;


    // NOTE: Currently assumes that the color scan starts immediately after
    // the hsync ends. Likely the cause of the bug above.

    for (int index = start_index; index < frequency_count; index++) {
        frequency_sum += frequency_data[index];
        samples_read++;
        m_sample_clock++;

        // Stop early if the current pixel still
        // has frequencies to be read.
        if (samples_read != samples_per_pixel) {
            continue;
        }

        // Translate frequencies to the pixel's color channel intensity.
        double avg_frequecy = frequency_sum / samples_per_pixel;

        uint8_t color_intensity =
            convert_frequency_to_intensity(avg_frequecy);

        switch (current_scan) {
            case GREEN:
                m_scanline_in_progress[current_pixel].green = color_intensity;
                break;
            case BLUE:
                m_scanline_in_progress[current_pixel].blue = color_intensity;
                break;
            case RED:
                m_scanline_in_progress[current_pixel].red = color_intensity;
                break;
        }

        // Reset in preparation for new pixel.
        samples_read = 0;
        frequency_sum = 0;

        current_pixel++;

        if (current_pixel == 320) {

            // Color scan finished, move to next color channel
            // or return to hsync detection.
            current_pixel = 0;
            switch (current_scan) {
                case GREEN:
                    current_scan = BLUE;
                    break;
                case BLUE:
                    current_scan = RED;
                    break;
                case RED:

                    // Debug
                    //uint64_t now = m_sample_clock;

                    ///Serial.print("scanline terminated after: ");
                    //Serial.print(1000*(now - m_last_hsync_end)/m_sample_rate);
                    //Serial.println("ms");


                    // Swap double buffers

                    Pixel* temp = m_completed_scanline;
                    m_completed_scanline = m_scanline_in_progress;
                    m_scanline_in_progress = temp;

                    m_new_scanline_ready = true;

                    m_current_state = HSYNC_DETECTION;
                    current_scan = GREEN;

                    return index+1;
            }
        }
    }

    return frequency_count;
}

bool SSTV::process_frequencies(double frequency_data[], uint16_t frequency_count)
{
    uint16_t (SSTV::*processing_fn)(double[], uint16_t, uint16_t);

    uint16_t leftover_index = 0;
    do {

        switch(m_current_state) {
            default:
            case HSYNC_DETECTION:
                processing_fn = &SSTV::detect_hsync;
                break;
            case SCANLINE_DECODING:
                processing_fn = &SSTV::decode_color_scan;
                break;
        }

        leftover_index = (this->*processing_fn)(frequency_data, frequency_count, leftover_index);
    } while (leftover_index < frequency_count);

    return m_new_scanline_ready;
}

void SSTV::retrieve_scanline(Pixel scanline_out[MARTIN_M1_SCANLINE_WIDTH])
{
    if (!m_new_scanline_ready) {
        return;
    }

    for (int index = 0; index < MARTIN_M1_SCANLINE_WIDTH; index++) {
        scanline_out[index] = m_completed_scanline[index];
    }

    m_new_scanline_ready = false;
}
