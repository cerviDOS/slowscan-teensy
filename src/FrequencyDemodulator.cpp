#include <math.h>
#include <arduinoFFT.h>

#include "FrequencyDemodulator.h"


FrequencyDemodulator::FrequencyDemodulator(uint32_t sample_rate) :
    m_sample_rate(sample_rate)
{
    fft = ArduinoFFT<double>(nullptr,
                             nullptr,
                             BUFFER_SIZE,
                             m_sample_rate,
                             true);
}

void FrequencyDemodulator::analytic_signal(double real[BUFFER_SIZE], double imag[BUFFER_SIZE], bool window)
{
    fft.setArrays(real, imag, BUFFER_SIZE);

    if (window) {
        fft.windowing(FFTWindow::Blackman_Harris, FFTDirection::Forward);
    }

    fft.compute(FFTDirection::Forward);

    const uint16_t nyquist = BUFFER_SIZE / 2;
    // Zero out bins at 0 & Nyquist
    real[0] = 0;
    imag[0] = 0;

    // Multiply 1 to N/2 by 2
    for (int i = 0; i <= nyquist; i++) {
        real[i] *= 2;
        imag[i] *= 2;
    }
    // Zero out N/2 + 1 to N - 1
    for (int i = nyquist + 1; i < BUFFER_SIZE; i++) {
        real[i] = 0;
        imag[i] = 0;
    }

    fft.compute(FFTDirection::Reverse);
}

double FrequencyDemodulator::recover_phase(double I, double Q)
{
    return atan2(I, Q);
}

double FrequencyDemodulator::instantaneous_frequency(double real1, double imag1,
                                                     double real2, double imag2)
{
    double phase1 =
        (atan2(imag1, real1) / M_PI) * INT16_MAX;

    double phase2 =
        (atan2(imag2, real2) / M_PI) * INT16_MAX;

    int16_t phase_diff_int16 = (int) phase2 - phase1;

    double phase_diff = phase_diff_int16 / (double) INT16_MAX;

    return (phase_diff / 2) * m_sample_rate;
}

void FrequencyDemodulator::frequencies(const double waveform_data[BUFFER_SIZE], double frequency_data[BUFFER_SIZE-1])
{
    double real[BUFFER_SIZE];
    double imag[BUFFER_SIZE];
    for (int index = 0; index < BUFFER_SIZE; index++) {
        real[index] = waveform_data[index];
        imag[index] = 0;
    }

    analytic_signal(real, imag);

    for (int index = 0; index < BUFFER_SIZE-1; index++) {
        /*
        // TODO: encapsulate wrapping method in its own function
        double phase1 =
            (atan2(imag[index], real[index]) / M_PI) * INT16_MAX;

        double phase2 =
            (atan2(imag[index+1], real[index+1]) / M_PI) * INT16_MAX;

        int16_t phase_diff_int16 = (int) phase2 - phase1;

        double phase_diff = phase_diff_int16 / (double) INT16_MAX;

        double frequency = (phase_diff / 2) * m_sample_rate;

        frequency_data[index] = frequency;
        */

        frequency_data[index] = 
            instantaneous_frequency(real[index], imag[index],
                                    real[index+1], imag[index+1]);
    }
}
