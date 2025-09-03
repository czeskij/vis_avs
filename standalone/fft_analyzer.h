// Simple audio FFT analyzer: gathers recent samples and produces log-spaced magnitude bands.
#pragma once
#include <atomic>
#include <cstddef>
#include <vector>

struct FFTAnalyzer {
    explicit FFTAnalyzer(size_t fftSize = 2048, size_t bands = 64);
    void push(const float* interleavedStereo, size_t frames); // expects stereo
    bool compute(std::vector<float>& outBands); // returns false if insufficient data yet
private:
    size_t m_fftSize;
    size_t m_bands;
    size_t m_channels = 2;
    std::vector<float> m_ring; // mono mixed
    std::atomic<size_t> m_writeIdx { 0 };
    std::vector<float> m_window;
    // FFT buffers/platform specific
    void platform_init();
    bool platform_fft(const float* time, std::vector<float>& mags);
};
