#include <stdint.h>
#include <arduinoFFT.h>

#include "Array.h"


class FrequencyDemodulator {
public:
    static const uint16_t BUFFER_SIZE = 1024;

    FrequencyDemodulator(uint32_t sample_rate=44100);

    // Calculates the analytic signal of a waveform.
    // waveform_real[] will remain unchanged.
    // waveform_imag[] will contain waveform 90-degrees out of phase (in quadrature) with waveform_real[].
    void analytic_signal(double waveform_real[BUFFER_SIZE], double waveform_imag[BUFFER_SIZE], bool window = true);

    // Calculates the instantaneous frequencies of a waveform
    // and stores the result in frequency_data[].
    void inst_freq(const double waveform_data[BUFFER_SIZE], double frequency_data[BUFFER_SIZE-1]);

private:

    double recover_phase(double I, double Q);

    uint32_t m_sample_rate;


    ArduinoFFT<double> fft;
};
