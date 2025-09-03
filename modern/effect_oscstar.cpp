#include "effect_oscstar.h"
#if AVS_IMGUI
#include <imgui.h>
#endif
#include <algorithm>
#include <cmath>

namespace {
struct StarState {
    double rot = 0.0;
};
static StarState g_state;

inline void drawLine(uint32_t* fb, int w, int h, int x0, int y0, int x1, int y1, uint32_t color)
{
    int dx = std::abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
    int dy = -std::abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;
    while (true) {
        if ((unsigned)x0 < (unsigned)w && (unsigned)y0 < (unsigned)h)
            fb[y0 * w + x0] = color;
        if (x0 == x1 && y0 == y1)
            break;
        int e2 = 2 * err;
        if (e2 >= dy) {
            err += dy;
            x0 += sx;
        }
        if (e2 <= dx) {
            err += dx;
            y0 += sy;
        }
    }
}

inline uint32_t packColor(float r, float g, float b)
{
    r = std::clamp(r, 0.f, 1.f);
    g = std::clamp(g, 0.f, 1.f);
    b = std::clamp(b, 0.f, 1.f);
    return 0xFF000000 | ((uint32_t)(b * 255) << 16) | ((uint32_t)(g * 255) << 8) | ((uint32_t)(r * 255));
}
}

void render_oscstar(uint32_t* fb, int w, int h, const std::vector<float>& spec, float level, float time, const OscStarParams& p)
{
    if (!fb || w <= 0 || h <= 0)
        return;
    int arms = std::max(2, std::min(24, p.arms));
    g_state.rot += p.baseSpeed + level * p.levelSpeed;
    if (g_state.rot > 6.28318530718)
        g_state.rot -= 6.28318530718;
    int segments = 64;
    if (!spec.empty())
        segments = (int)std::min<size_t>(spec.size(), 128);
    float radius = std::min(w, h) * 0.40f * (0.75f + 0.25f * std::sin(time * 0.5f + level * 3));
    for (int a = 0; a < arms; ++a) {
        double baseAng = g_state.rot + a * (2.0 * M_PI / arms);
        float prevx = (float)(w / 2);
        float prevy = (float)(h / 2);
        for (int s = 0; s < segments; ++s) {
            float t = (float)s / (segments - 1);
            float specv = spec.empty() ? 0.0f : spec[(size_t)((spec.size() - 1) * t)];
            float amp = (0.15f + specv * 0.85f) * radius;
            float wobble = 0.4f * specv * std::sin(time * 2 + s * 0.2f + a);
            double ang = baseAng + wobble;
            float x = (float)(w / 2 + std::cos(ang) * amp * t);
            float y = (float)(h / 2 + std::sin(ang) * amp * t);
            float hue = std::fmod((a / (float)arms) + t * 0.5f + level * 0.2f, 1.0f);
            float sat = 0.6f + 0.4f * specv;
            float val = 0.4f + 0.6f * t;
            float h6 = hue * 6.0f;
            int hi = (int)std::floor(h6);
            float f = h6 - hi;
            float p = val * (1 - sat);
            float q = val * (1 - sat * f);
            float qq = val * (1 - sat * (1 - f));
            float r, g, b;
            switch (hi % 6) {
            case 0:
                r = val;
                g = qq;
                b = p;
                break;
            case 1:
                r = q;
                g = val;
                b = p;
                break;
            case 2:
                r = p;
                g = val;
                b = qq;
                break;
            case 3:
                r = p;
                g = q;
                b = val;
                break;
            case 4:
                r = qq;
                g = p;
                b = val;
                break;
            default:
                r = val;
                g = p;
                b = q;
            }
            uint32_t col = packColor(r, g, b);
            drawLine(fb, w, h, (int)prevx, (int)prevy, (int)x, (int)y, col);
            prevx = x;
            prevy = y;
        }
    }
}

void OscStarEffect::drawUI()
{
#if AVS_IMGUI
    ImGui::SliderInt("Arms", &params.arms, 2, 24);
    ImGui::SliderFloat("Base Speed", &params.baseSpeed, 0.0f, 0.2f, "%.3f");
    ImGui::SliderFloat("Level Speed", &params.levelSpeed, 0.0f, 0.5f, "%.3f");
    ImGui::SliderFloat("Trail Fade", &params.trailFade, 0.0f, 0.98f, "%.2f");
#endif
}
