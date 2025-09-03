// Minimal portable core subset for early macOS/Linux standalone build.
// Provides stub globals and a tiny init to allow using r_defs.h blend helpers.

#include "../../platform_shim.h"
#include "r_defs.h"
#include <cstdio>

// Global variables expected by legacy headers
char g_path[1024] = { 0 };
unsigned char g_blendtable[256][256];
int g_reset_vars_on_recompile = 0;
int g_line_blend_mode = 0; // 0 = copy

// MMX-related constants (unused in NO_MMX path but must exist)
unsigned int const mmx_blend4_revn[2] = { 0, 0 };
int const mmx_blend4_zero = 0;
int const mmx_blendadj_mask[2] = { 0, 0 };

static struct BlendTableInitializer {
    BlendTableInitializer()
    {
        for (int x = 0; x < 256; ++x) {
            for (int w = 0; w < 256; ++w) {
                // Simple linear scale used by many original routines:
                // result approximates (x * w) / 255
                g_blendtable[x][w] = (unsigned char)((x * w + 127) / 255);
            }
        }
    }
} s_blendInit;

// Tiny demo function exercising blend helpers (used for unit smoke if needed)
extern "C" unsigned int avs_portable_demo_pixel(unsigned int a, unsigned int b, int mode)
{
    g_line_blend_mode = mode & 0xFF; // restrict
    BLEND_LINE(reinterpret_cast<int*>(&a), b);
    return a;
}

// Placeholder no-op update step; future: hook audio + render loop
extern "C" void avs_portable_tick()
{
    static unsigned counter = 0;
    if ((counter++ % 3000000u) == 0u) {
        unsigned int demo = avs_portable_demo_pixel(0x10203040, 0x01020304, 1);
        (void)demo; // suppress unused in optimized builds
    }
}
