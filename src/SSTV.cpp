#include <Arduino.h>

#include "sstv.h"

static const uint32_t MARTIN_M1_IMG_H = 256;
static const uint32_t MARTIN_M1_IMG_W = 320;

// Mode timings in milliseconds

// TODO: Also include ~30 millisecond pulse that occurs after VIS code

static const double MARTIN_M1_HSYNC_PULSE_MS = 4.862;
static const double MARTIN_M1_HSYNC_PORCH_MS = 0.572;
static const double MARTIN_M1_COLOR_SCAN_MS = 146.432;
static const double MARTIN_M1_USEC_PER_PIXEL = MARTIN_M1_COLOR_SCAN_MS / MARTIN_M1_IMG_W;

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
    static uint64_t hsync_start = 0;
    static uint64_t hsync_end = 0;
    static bool within_hsync_candidate = false;

    for (uint16_t index = start_index; index < frequency_count; index++) {
        double frequency = frequency_data[index];

        bool freq_within_hsync_bounds =
            is_within_tolerance(frequency,
                                MARTIN_M1_HSYNC_HZ,
                                FREQUENCY_TOLERANCE);

        if (freq_within_hsync_bounds && !within_hsync_candidate) {
            // ~1200 Hz detected AND not already inside an hsync candidate -> mark
            // sample as a new candidate
            hsync_start = m_sample_clock;
            within_hsync_candidate = true;

        } else if (within_hsync_candidate && !freq_within_hsync_bounds) {
            // Not ~1200 Hz AND inside an hsync candidate -> candidate ended,
            // mark end and calculate its duration
            hsync_end = m_sample_clock;
            within_hsync_candidate = false;

            if (validate_hsync_duration(hsync_start, hsync_end)) {
                m_current_state = DECODING;
                return index;
            }
        }

        m_sample_clock++;
    }

    return frequency_count;
}


uint16_t SSTV::decode_color_scan(double frequency_data[], uint16_t frequency_count, uint16_t start_index)
{
    // TODO: need to track current color scan, timing within color scan
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

    return false;
}

SSTV::RGB* SSTV::retrieve_scanline()
{
    if (!m_scanline_ready) {
        return nullptr;
    }

    return m_scanline;
}
