#include <stdint.h>

class SSTV {
public:
    typedef struct {
        uint8_t red;
        uint8_t green;
        uint8_t blue;
    } RGB;

    SSTV(uint32_t sample_rate = 44100);

    bool process_frequencies(double frequency_data[], uint16_t frequency_count);

    RGB* retrieve_scanline();
private:

    bool validate_hsync_duration(uint64_t start, uint64_t end);
    uint16_t detect_hsync(double frequency_data[], uint16_t frequency_count, uint16_t start_index);
    uint16_t decode_color_scan(double frequency_data[], uint16_t frequency_count, uint16_t start_index);

    uint32_t m_sample_rate;

    uint32_t m_num_scanlines_processed;

    // Need a timing mechanism, keeping track of duration between inputted samples
    uint64_t m_sample_clock; // keep track of number of samples processed?
    uint64_t m_last_hsync_begin;
    uint64_t m_last_hsync_end;

    enum State {HSYNC_DETECTION, DECODING};
    State m_current_state;


    // Make this an array?
    bool m_scanline_ready;
    static const uint32_t M_SCANLINE_WIDTH = 320;
    RGB m_scanline[M_SCANLINE_WIDTH];
};

