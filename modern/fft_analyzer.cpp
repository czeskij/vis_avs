#include "fft_analyzer.h"
#include <algorithm>
#include <cmath>
#include <cstring>

#if defined(__APPLE__)
#include <Accelerate/Accelerate.h>
#define AVS_USE_ACCELERATE 1
#else
#define AVS_USE_ACCELERATE 0
#endif

namespace {
static size_t nextPow2(size_t v)
{
    size_t p = 1;
    while (p < v)
        p <<= 1;
    return p;
}
}

struct AccelerateFFTState {
    FFTSetup setup = nullptr;
    int log2n = 0;
    std::vector<float> real;
    std::vector<float> imag;
};
static AccelerateFFTState g_accel;

FFTAnalyzer::FFTAnalyzer(size_t fftSize, size_t bands)
    : m_fftSize(nextPow2(fftSize))
    , m_bands(bands)
{
    m_ring.resize(m_fftSize * 4, 0.0f);
    m_window.resize(m_fftSize);
    for (size_t i = 0; i < m_fftSize; ++i) {
        m_window[i] = 0.5f * (1.f - std::cos(2.f * 3.1415926535f * i / (m_fftSize - 1)));
    }
    platform_init();
}

void FFTAnalyzer::platform_init()
{
#if AVS_USE_ACCELERATE
    int log2n = 0;
    while ((1u << log2n) < m_fftSize)
        ++log2n;
    g_accel.log2n = log2n;
    g_accel.setup = vDSP_create_fftsetup(log2n, kFFTRadix2);
    g_accel.real.resize(m_fftSize / 2);
    g_accel.imag.resize(m_fftSize / 2);
#endif
}

void FFTAnalyzer::push(const float* interleavedStereo, size_t frames)
{
    size_t w = m_writeIdx.load(std::memory_order_relaxed);
    size_t cap = m_ring.size();
    for (size_t i = 0; i < frames; ++i) {
        float L = interleavedStereo[i * 2];
        float R = interleavedStereo[i * 2 + 1];
        float mono = 0.5f * (L + R);
        m_ring[(w + i) % cap] = mono;
    }
    m_writeIdx.store((w + frames) % cap, std::memory_order_relaxed);
}

bool FFTAnalyzer::platform_fft(const float* time, std::vector<float>& mags)
{
    mags.assign(m_fftSize / 2, 0.0f);
#if AVS_USE_ACCELERATE
    if (!g_accel.setup)
        return false;
    for (size_t i = 0; i < m_fftSize / 2; ++i) {
        g_accel.real[i] = time[2 * i] * m_window[2 * i];
        g_accel.imag[i] = time[2 * i + 1] * m_window[2 * i + 1];
    }
    DSPSplitComplex sc { g_accel.real.data(), g_accel.imag.data() };
    vDSP_fft_zip(g_accel.setup, &sc, 1, g_accel.log2n, kFFTDirection_Forward);
    for (size_t i = 0; i < m_fftSize / 2; ++i) {
        float re = sc.realp[i];
        float im = sc.imagp[i];
        mags[i] = std::sqrt(re * re + im * im);
    }
    return true;
#else
    for (size_t k = 0; k < m_fftSize / 2; ++k) {
        double sumRe = 0, sumIm = 0;
        for (size_t n = 0; n < m_fftSize; ++n) {
            double w = 2 * M_PI * k * n / m_fftSize;
            double s = time[n] * m_window[n];
            sumRe += s * std::cos(w);
            sumIm -= s * std::sin(w);
        }
        mags[k] = std::sqrt(sumRe * sumRe + sumIm * sumIm);
    }
    return true;
#endif
}

bool FFTAnalyzer::compute(std::vector<float>& outBands)
{
    size_t w = m_writeIdx.load(std::memory_order_relaxed);
    if (w < m_fftSize)
        return false;
    std::vector<float> block(m_fftSize);
    size_t cap = m_ring.size();
    size_t start = (w + cap - m_fftSize) % cap;
    for (size_t i = 0; i < m_fftSize; ++i) {
        block[i] = m_ring[(start + i) % cap];
    }
    std::vector<float> mags;
    if (!platform_fft(block.data(), mags))
        return false;
    outBands.assign(m_bands, 0.0f);
    double minF = 1.0;
    double maxF = (double)(m_fftSize / 2 - 1);
    auto logMap = [&](double t) { return std::exp(std::log(minF) * (1.0 - t) + std::log(maxF) * t); };
    for (size_t b = 0; b < m_bands; ++b) {
        double t0 = (double)b / m_bands;
        double t1 = (double)(b + 1) / m_bands;
        double f0 = logMap(t0);
        double f1 = logMap(t1);
        if (f0 < 1.0)
            f0 = 1.0;
        if (f1 < f0 + 1.0)
            f1 = f0 + 1.0;
        size_t i0 = (size_t)f0;
        size_t i1 = (size_t)f1;
        if (i1 > mags.size())
            i1 = mags.size();
        if (i0 >= i1)
            continue;
        double acc = 0.0;
        for (size_t i = i0; i < i1; ++i)
            acc += mags[i];
        acc /= (double)(i1 - i0);
        outBands[b] = (float)acc;
    }
    float maxv = 0.f;
    for (float v : outBands)
        maxv = std::max(maxv, v);
    if (maxv > 0)
        for (float& v : outBands)
            v /= maxv;
    return true;
}
