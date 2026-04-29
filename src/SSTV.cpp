#include <Arduino.h>

#include "sstv.h"
#include "debug.h"

// TODO:
// - Include ~30 millisecond pulse that occurs after VIS code
// - Change millisecond timings to sample timings
// - Add fallback if hsync isn't detected ~5ms after scanline ends.
// Requires tracking when the previous color_scan_ended, new field + check in detect_hsync?

// Hsync detection tolerances
//
// These values can be redefined with build flags to ease
// the tolerances a bit.
//
// The microphone seems to play nice with a frequency
// tolerance of 150-200 and timing tolerance of 2.0.
#ifndef HSYNC_FREQUENCY_TOLERANCE
#define HSYNC_FREQUENCY_TOLERANCE 50
#endif

#ifndef HSYNC_TIMING_TOLERANCE_MS
#define HSYNC_TIMING_TOLERANCE_MS 0.5
#endif

// Mode timings in milliseconds
static const double MARTIN_M1_HSYNC_PULSE_MS = 4.862;
static const double MARTIN_M1_HSYNC_PORCH_MS = 0.572;
static const double MARTIN_M1_COLOR_SCAN_MS = 146.432;
static const double MARTIN_M1_COLOR_SEPARATOR_MS = 0.572;
static const double MARTIN_M1_MS_PER_PIXEL = 0.4576;

// Mode frequencies in Hz
static const uint32_t MARTIN_M1_HSYNC_HZ = 1200;
static const uint32_t MARTIN_M1_COLOR_LOW_HZ = 1500;
static const uint32_t MARTIN_M1_COLOR_HIGH_HZ = 2300;

SSTV::SSTV(uint32_t sample_rate) :
    m_sample_rate(sample_rate),
    m_sample_clock(0),
    m_last_hsync_start(0),
    m_last_hsync_end(0),
    m_color_scan_start(0),
    m_current_state(State::HSYNC_DETECTION),
    m_num_samples_to_wait(0),
    m_new_scanline_ready(false),
    m_completed_scanline(new Pixel[MARTIN_M1_SCANLINE_WIDTH]),
    m_scanline_in_progress(new Pixel[MARTIN_M1_SCANLINE_WIDTH])
{}


bool SSTV::is_within_tolerance(double value, double target, double tolerance)
{
    double upper_bound = target + tolerance;
    double lower_bound = target - tolerance;
    return (value >= lower_bound) && (value <= upper_bound);
}

bool SSTV::validate_hsync_duration(uint64_t hsync_start, uint64_t hsync_end)
{
    double pulse_duration_ms = ((hsync_end - hsync_start) / (double) m_sample_rate) * 1000;

    bool is_hsync = is_within_tolerance(pulse_duration_ms,
                                        MARTIN_M1_HSYNC_PULSE_MS,
                                        HSYNC_TIMING_TOLERANCE_MS);

#if DEBUG_DECODER_STATE
    Serial.print("[HSYNC] H-sync candidate duration: ");
    Serial.print(pulse_duration_ms);
    Serial.println(" ms");
#endif

    return is_hsync;
}

uint16_t SSTV::detect_hsync(double frequency_data[], uint16_t frequency_count, uint16_t start_index)
{
    static bool within_hsync_candidate = false;

    for (uint16_t index = start_index; index < frequency_count; index++) {
        double frequency = frequency_data[index];

        bool freq_within_hsync_bounds =
            is_within_tolerance(frequency,
                                MARTIN_M1_HSYNC_HZ,
                                HSYNC_FREQUENCY_TOLERANCE);

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
#if DEBUG_DECODER_STATE
                Serial.print("\n[HSYNC] Found valid hsync, duration: ");
                Serial.print( 1000.0 * (m_last_hsync_end - m_last_hsync_start) / m_sample_rate);
                Serial.println("ms\n");
#endif

                // Candidate is indeed an hsync, begin decoding scanline
                m_current_state = SCANLINE_DECODING;
                m_color_scan_start = m_sample_clock;

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

uint16_t SSTV::decode_color_scan(double frequency_data[], uint16_t frequency_count, uint16_t start_index)
{

    static const uint16_t samples_per_pixel =
        (MARTIN_M1_MS_PER_PIXEL / 1000) * m_sample_rate;

    enum ColorScanState { GREEN = 1, BLUE = 2, RED = 3 };
    static ColorScanState current_scan = GREEN;

    static uint16_t current_pixel = 0;

    static uint16_t samples_read = 0;
    static double frequency_sum = 0.0;

    for (int index = start_index; index < frequency_count; index++) {
        m_sample_clock++;

        samples_read++;

        frequency_sum += frequency_data[index];

        // Stop early if the current pixel still has frequencies to be read.
        if (samples_read != samples_per_pixel) {
            continue;
        }
        // Translate frequencies to the pixel's color channel intensity.
        double avg_frequecy = frequency_sum / samples_read;

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

        // Reset in preparation for new pixel
        samples_read = 0;
        frequency_sum = 0;

        if (current_pixel++ == 320) {
            current_pixel = 0;

#if DEBUG_DECODER_STATE
            uint64_t now = m_sample_clock;

            Serial.print("[CSAN] Finished channel after: ");
            Serial.print(1000.0 *(now - m_last_hsync_end)/m_sample_rate);
            Serial.println(" ms");
#endif

            // Color scan finished, move to next color channel
            // or return to hsync detection.
            switch (current_scan) {
                case GREEN:
                    current_scan = BLUE;
                    break;
                case BLUE:
                    current_scan = RED;
                    break;
                case RED:

#if DEBUG_DECODER_STATE
                Serial.print("[CSAN] Finished scanline after: ");
                Serial.print(1000.0 * (now - m_last_hsync_end) /m_sample_rate);
                Serial.println(" ms\n");
#endif

                // Swap buffers
                Pixel* temp = m_completed_scanline;
                m_completed_scanline = m_scanline_in_progress;
                m_scanline_in_progress = temp;

                m_new_scanline_ready = true;

                m_current_state = HSYNC_DETECTION;
                current_scan = GREEN;

                //m_last_scanline_end = m_sample_clock;

                return index+1;
            }

            // Current timing system introduces rounding errors, so
            // wait until the next color scan *should* begin
            uint32_t samples_per_scan = (MARTIN_M1_COLOR_SCAN_MS / 1000) * m_sample_rate;
            uint32_t curr_scan_duration = m_sample_clock - m_color_scan_start;

            m_num_samples_to_wait = samples_per_scan - curr_scan_duration;

            m_color_scan_start = m_sample_clock + m_num_samples_to_wait;

            m_current_state = WAITING;

            return index+1;
        }
    }

    return frequency_count;
}

uint16_t SSTV::wait(double frequency_data[], uint16_t frequency_count, uint16_t start_index)
{
    static uint16_t samples_elapsed = 0;

#if DEBUG_DECODER_STATE
    Serial.print("[WAIT] Waiting ");
    Serial.print(m_num_samples_to_wait);
    Serial.print(" samples / ");
    Serial.print(1000.0 * ((float) m_num_samples_to_wait / m_sample_rate));
    Serial.println(" ms");

#endif

    for (int index = start_index; index < frequency_count; index++) {
        // Don't update the sample clock here since this function
        // is used to throw away samples instead of processing them

        if (samples_elapsed++ == m_num_samples_to_wait) {
            samples_elapsed = 0;
            m_num_samples_to_wait = 0;

            // TODO:
            // Change to m_state_after_wait at some point.
            // More book keeping but should be useful for decoding other modes.
            m_current_state = SCANLINE_DECODING;

            return index+1;
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
            case WAITING:
                processing_fn = &SSTV::wait;
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

    for (uint16_t index = 0; index < MARTIN_M1_SCANLINE_WIDTH; index++) {
        scanline_out[index] = m_completed_scanline[index];
    }

    m_new_scanline_ready = false;
}
