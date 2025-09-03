// Oscillating star effect (relocated)
#pragma once
#include "effect_base.h"
#include <cstdint>
#include <vector>

struct OscStarParams {
    int arms = 5;
    float baseSpeed = 0.02f;
    float levelSpeed = 0.10f;
    float trailFade = 0.75f;
};

void render_oscstar(uint32_t* fb, int w, int h, const std::vector<float>& spec, float level, float time, const OscStarParams& p);

class OscStarEffect : public Effect {
public:
    OscStarParams params;
    const char* name() const override { return "OscStar"; }
    void render(FrameContext& ctx) override { render_oscstar(ctx.fb, ctx.width, ctx.height, ctx.spectrum ? *ctx.spectrum : std::vector<float> {}, ctx.audioLevel, (float)ctx.time, params); }
    void drawUI() override;
};
