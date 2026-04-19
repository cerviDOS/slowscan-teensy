#include <stdint.h>

class SSTV {
public:

    static const uint32_t MARTIN_M1_NUM_SCANLINES = 256;
    static const uint32_t MARTIN_M1_SCANLINE_WIDTH = 320;



public:
    typedef struct {
        uint8_t red;
        uint8_t green;
        uint8_t blue;
    } Pixel;

    SSTV(uint32_t sample_rate = 44100);

    // Analyzes the array of frequencies as a snippet of an SSTV signal.
    //
    // Assumes that the input frequencies are continuguous with those passed
    // during the last call to this function. A gap between frequencies will
    // produce incorrect timings.
    //
    // Returns true if a new scanline is available to be read with retrieve_scanline().
    // Returns false if (1) no scanlines have been completed, or (2) if retrieve_scanline()
    // has already been called and a new scanline is not yet ready.
    bool process_frequencies(double frequency_data[], uint16_t frequency_count);

    // TODO: Make location of contants more consistent. Only expose bare minimum,
    // like scanline width here? Declare extern?
    //static const uint32_t SCANLINE_WIDTH = 320;

    // If available, copies the most recently completed scanline data
    // into scanline_out.
    //
    // Otherwise, scanline_out will remain unchanged if called before
    // process_frequencies() returns true.
    void retrieve_scanline(Pixel scanline_out[MARTIN_M1_SCANLINE_WIDTH]);

private:
    uint8_t convert_frequency_to_intensity(double frequency);

    bool validate_hsync_duration(uint64_t start, uint64_t end);
    uint16_t detect_hsync(double frequency_data[], uint16_t frequency_count, uint16_t start_index);
    uint16_t decode_color_scan(double frequency_data[], uint16_t frequency_count, uint16_t start_index);

    uint32_t m_sample_rate;

    uint32_t m_num_scanlines_processed;

    // Need a timing mechanism, keeping track of duration between inputted samples
    uint64_t m_sample_clock; // keep track of number of samples processed?
    uint64_t m_last_hsync_start;
    uint64_t m_last_hsync_end;

    enum State {HSYNC_DETECTION, SCANLINE_DECODING};
    State m_current_state;

    // NOTE: maybe adding SAMPLES_PER_x as class members
    // is the way to go
    // e.g. SAMPLES_PER_PIXEL

    void swap_buffers();

    bool m_new_scanline_ready;

    Pixel* m_completed_scanline;
    Pixel* m_scanline_in_progress;

    // Swap when scanline is completed;
    // Pixel* temp = m_completed_scanline;
    // m_completed_scanline = m_scanline_in_progress;
    // m_scanline_in_progress = temp;
};

