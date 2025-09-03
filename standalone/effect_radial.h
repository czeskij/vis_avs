#pragma once
#include "effect_base.h"
#include <algorithm>
#include <cmath>

struct RadialParams {
    float speed = 4.0f;
    float waveFreq = 10.0f;
};

class RadialEffect : public Effect {
public:
    RadialParams params;
    const char* name() const override { return "Radial Wave"; }
    void render(FrameContext& ctx) override
    {
        if (!ctx.fb)
            return;
        for (int y = 0; y < ctx.height; ++y) {
            float ny = (2.0f * y / ctx.height - 1.0f);
            for (int x = 0; x < ctx.width; ++x) {
                float nx = (2.0f * x / ctx.width - 1.0f);
                float r = std::sqrt(nx * nx + ny * ny);
                float angle = std::atan2(ny, nx);
                float normA = (angle + 3.14159265f) / (2 * 3.14159265f);
                size_t band = (ctx.spectrum && !ctx.spectrum->empty()) ? (size_t)(normA * (ctx.spectrum->size() - 1)) : 0;
                float specv = (ctx.spectrum && !ctx.spectrum->empty()) ? (*ctx.spectrum)[band] : 0.0f;
                float wave = 0.5f + 0.5f * std::sin(params.waveFreq * r - (float)ctx.time * params.speed + (ctx.audioLevel + specv) * 6.2831f);
                unsigned int base = (unsigned int)(wave * 255.0f) & 0xFF;
                unsigned int red = (unsigned int)(specv * 255.0f) & 0xFF;
                unsigned int colorA = (base) | (red << 16) | ((255 - base) << 8) | 0xFF000000;
                unsigned int colorB = ((int)(ctx.audioLevel * 255) & 0xFF) | 0x00202020 | 0xFF000000;
                // simple blend (simulate avs_portable_demo_pixel if available)
                unsigned int merged = (colorA & 0xFEFEFE) >> 1 | (colorB & 0xFEFEFE) >> 1;
                ctx.fb[y * ctx.width + x] = merged | 0xFF000000;
            }
        }
    }
    void drawUI() override;
};
