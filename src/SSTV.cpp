#include <Arduino.h>

#include "sstv.h"

static const uint32_t MARTIN_M1_IMG_H = 256;
static const uint32_t MARTIN_M1_IMG_W = 320;

// Mode timings in milliseconds

// TODO: Also include ~30 millisecond pulse that occurs after VIS code

static const double MARTIN_M1_HSYNC_PULSE_MS = 4.862;
static const double MARTIN_M1_HSYNC_PORCH_MS = 0.572;
static const double MARTIN_M1_COLOR_SCAN_MS = 146.432;
static const double MARTIN_M1_PIXEL_MS = MARTIN_M1_COLOR_SCAN_MS / MARTIN_M1_IMG_W;

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
    m_scanline_ready(false) {}

bool SSTV::validate_hsync_duration(uint64_t hsync_start, uint64_t hsync_end)
{
    double pulse_duration_ms = ((hsync_end - hsync_start) / (double) m_sample_rate) * 1000;

    return is_within_tolerance(pulse_duration_ms,
                               MARTIN_M1_HSYNC_PULSE_MS,
                               TIMING_TOLERANCE_MS);
}

uint16_t SSTV::detect_hsync(double frequency_data[], uint16_t frequency_count, uint16_t start_index)
{
    // iterate through frequencies
    //  if frequnecy is within tolerance to be hsync, begin timing it
    //  if, upon ending, the duration of the hsync is approx the expected 5ms
    //  return and switch modes
    //
    //  how to handle remainder of array being non-hsync data?
    //  how to handle running out of data while still in hsync?

    // Static since horizontal sync can continue after
    // reaching the end of the data currently available
    //static uint64_t hsync_start = 0;
    //static uint64_t hsync_end = 0;
    static bool within_hsync_candidate = false;

    //Serial.print("hsync scanning beginning at offset: ");
    //Serial.print(start_index);
    ///Serial.print("/");
    //Serial.println(frequency_count);

    for (uint16_t index = start_index; index < frequency_count; index++) {
        double frequency = frequency_data[index];

        bool freq_within_hsync_bounds =
            is_within_tolerance(frequency,
                                MARTIN_M1_HSYNC_HZ,
                                FREQUENCY_TOLERANCE);

        if (freq_within_hsync_bounds && !within_hsync_candidate) {
            // ~1200 Hz detected AND not already inside an hsync candidate -> mark
            // sample as a new candidate
            m_last_hsync_start = m_sample_clock;
            within_hsync_candidate = true;

        } else if (within_hsync_candidate && !freq_within_hsync_bounds) {
            // Not ~1200 Hz AND inside an hsync candidate -> candidate ended,
            // mark end and calculate its duration
            m_last_hsync_end = m_sample_clock;
            within_hsync_candidate = false;

            if (validate_hsync_duration(m_last_hsync_start, m_last_hsync_end)) {
                m_current_state = DECODING;

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
    // first revision
    //  keep track of the sample when the method was first called
    //      use as basis for timing throughout the color scan
    //
    //  if elapsed ms   < 147 -> Green
    //                  > 147 && < 294 -> Blue
    //                  > 294 && < 441 -> Red
    //                  > 441 -> Return to hsync scan

    // color scan can roll over at any point during processing
    // loop condition that loops until predicted end of
    // color scan
    // for ( index; index < frequency_count && index < COLOR_SCAN_END; index++) {}
    //
    // if color scan starts at sample 1000

    enum ColorScanState { GREEN = 1, BLUE = 2, RED = 3 };
    static ColorScanState current_scan = GREEN;

    static const uint16_t samples_per_pixel =
        (MARTIN_M1_PIXEL_MS / 1000) * m_sample_rate;

    static uint16_t current_pixel = 0;

    static uint16_t samples_read = 0;
    static double frequency_sum = 0.0;

    // Scanline is now being edited
    m_scanline_ready = false;

    // Assume that color scan starts immediately after hsync ends
    // How to initialize this upon a new scanline?
    //  Check if m_last_hsync_end == m_sample_clock?


    // TODO: complicated system, document this
    for (int index = start_index; index < frequency_count; index++) {
        m_sample_clock++;
        // TODO: keep track of which pixel these samples correspond to,
        // then average them all out
        frequency_sum += frequency_data[index];

        // Maybe switch to next pixel end?
        // Go back to landmark system?
        //
        // m_sample_clock == pixel_end_sample
        samples_read++;

        if (samples_read == samples_per_pixel) {

            //Serial.print("\nread samples for pixel: ");
            //Serial.print(current_pixel);
            //Serial.print(" for scan: ");
            //Serial.println(current_scan);

            double avg_frequecy = frequency_sum / samples_per_pixel;

            uint8_t color_intensity =
                convert_frequency_to_intensity(avg_frequecy);
            
            switch (current_scan) {
                case GREEN:
                    m_scanline[current_pixel].green = color_intensity;
                    break;
                case BLUE:
                    m_scanline[current_pixel].blue = color_intensity;
                    break;
                case RED:
                    m_scanline[current_pixel].red = color_intensity;
                    break;
            }


            samples_read = 0;
            frequency_sum = 0;

            current_pixel++;

            if (current_pixel == 320) {

                current_pixel = 0;
                switch (current_scan) {
                    case GREEN:
                        current_scan = BLUE;
                        break;
                    case BLUE:
                        current_scan = RED;
                        break;
                    case RED:

                        uint64_t now = m_sample_clock;

                        ///Serial.print("scanline terminated after: ");
                        //Serial.print(1000*(now - m_last_hsync_end)/m_sample_rate);
                        //Serial.println("ms");

                        current_scan = GREEN;

                        m_scanline_ready = true;
                        m_current_state = HSYNC_DETECTION;

                        // TODO: verify that this doesn't cause
                        // bounds error
                        return index+1;
                }

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
            case DECODING:
                processing_fn = &SSTV::decode_color_scan;
                break;
        }

        leftover_index = (this->*processing_fn)(frequency_data, frequency_count, leftover_index);
    } while (leftover_index < frequency_count);

    return m_scanline_ready;
}

void SSTV::retrieve_scanline(Pixel scanline_out[SCANLINE_WIDTH])
{
    if (!m_scanline_ready) {
        return;
    }

    for (int index = 0; index < SCANLINE_WIDTH; index++) {
        scanline_out[index] = m_scanline[index];
    }
}
