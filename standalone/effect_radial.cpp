#include "effect_radial.h"
#if AVS_IMGUI
#include <imgui.h>
#endif

void RadialEffect::drawUI()
{
#if AVS_IMGUI
    ImGui::SliderFloat("Speed", &params.speed, 0.1f, 10.0f, "%.2f");
    ImGui::SliderFloat("Wave Freq", &params.waveFreq, 1.0f, 30.0f, "%.1f");
#endif
}
