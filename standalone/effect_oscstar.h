#pragma once
#include <cstdint>
#include <vector>

struct OscStarParams {
    int arms = 5; // number of arms (2..12 reasonable)
    float baseSpeed = 0.02f; // base rotation delta per frame
    float levelSpeed = 0.10f; // added speed factor scaled by audio level
    float trailFade = 0.75f; // 0..1 fraction kept each frame
};

// Render oscillating star with configurable params.
void render_oscstar(uint32_t* fb, int w, int h, const std::vector<float>& spec, float level, float time, const OscStarParams& p);

#include "effect_base.h"
class OscStarEffect : public Effect {
public:
    OscStarParams params;
    const char* name() const override { return "OscStar"; }
    void render(FrameContext& ctx) override { render_oscstar(ctx.fb, ctx.width, ctx.height, ctx.spectrum ? *ctx.spectrum : std::vector<float> {}, ctx.audioLevel, (float)ctx.time, params); }
    void drawUI() override;
};
