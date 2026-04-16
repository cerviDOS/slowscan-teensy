#include <stdint.h>

class SSTV {
public:
    typedef struct {
        uint8_t red;
        uint8_t green;
        uint8_t blue;
    } Pixel;

    SSTV(uint32_t sample_rate = 44100);

    // Returns true if a scanline is ready
    bool process_frequencies(double frequency_data[], uint16_t frequency_count);

    // TODO: Make location of contants more consistent. Only expose bare minimum,
    // like scanline width here? Declare extern?
    static const uint32_t SCANLINE_WIDTH = 320;
    void retrieve_scanline(Pixel scanline_out[SCANLINE_WIDTH]);
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

    enum State {HSYNC_DETECTION, DECODING};
    State m_current_state;

    // NOTE: maybe adding SAMPLES_PER_x as class members
    // is the way to go
    // e.g. SAMPLES_PER_PIXEL

    bool m_scanline_ready;
    Pixel m_scanline[SCANLINE_WIDTH];
};

