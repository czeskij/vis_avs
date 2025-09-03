// Effect system base definitions (relocated from standalone version)
#pragma once
#include <cstdint>
#include <string>
#include <vector>

struct FrameContext {
    uint32_t* fb = nullptr;
    int width = 0;
    int height = 0;
    float audioLevel = 0.0f;
    const std::vector<float>* spectrum = nullptr; // may be null
    double time = 0.0;
    uint64_t frameIndex = 0;
};

class Effect {
public:
    bool enabled = true;
    virtual ~Effect() = default;
    virtual const char* name() const = 0;
    virtual void render(FrameContext&) = 0; // modify framebuffer in-place
    virtual void drawUI() { /* optional */ }
};
