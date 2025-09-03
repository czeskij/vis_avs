// Basic standalone runner: creates a pixel buffer, captures audio (SDL2),
// and mutates pixels using blend helpers from the legacy core subset.

#include "../avs/vis_avs/r_defs.h"
#include "../platform_shim.h"
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <thread>
#include <vector>

#if AVS_SDL2
#include <SDL.h>
#if AVS_IMGUI
#include <backends/imgui_impl_sdl2.h>
#include <backends/imgui_impl_sdlrenderer2.h>
#include <imgui.h>
#endif
#endif

extern "C" unsigned int avs_portable_demo_pixel(unsigned int a, unsigned int b, int mode);
extern "C" void avs_portable_tick();

#include "effect_base.h"
#include "effect_oscstar.h"
#include "effect_radial.h"
#include "fft_analyzer.h"
#include "preset_io.h"
#if AVS_SDL2
#include <SDL.h>
#endif
#if __has_include(<filesystem>)
#include <filesystem>
#define AVS_HAVE_FILESYSTEM 1
#else
#define AVS_HAVE_FILESYSTEM 0
#endif

static std::atomic<float> g_level { 0.0f };
static FFTAnalyzer g_fft(2048, 64);
static std::vector<float> g_spec; // 64 bands 0..1
static OscStarParams g_starParams; // legacy global (will be superseded by effect instance)

#if AVS_SDL2
struct AudioCapture {
    SDL_AudioDeviceID dev = 0;
    std::atomic<bool> ok { false };
    static void callback(void* userdata, Uint8* stream, int lenBytes)
    {
        if (!userdata)
            return;
        AudioCapture* self = static_cast<AudioCapture*>(userdata);
        int samples = lenBytes / sizeof(float);
        float* f = reinterpret_cast<float*>(stream);
        // Push to FFT analyzer (interleaved stereo)
        g_fft.push(f, samples / 2); // since samples = frames*channels (channels=2)
        // RMS level
        double sum = 0.0;
        int frames = samples / 2;
        for (int i = 0; i < frames; ++i) {
            double L = f[i * 2];
            double R = f[i * 2 + 1];
            double m = 0.5 * (L + R);
            sum += m * m;
        }
        if (frames > 0) {
            double rms = std::sqrt(sum / frames);
            float prev = g_level.load(std::memory_order_relaxed);
            float newv = (float)(prev * 0.85 + rms * 0.15);
            if (newv > 1)
                newv = 1;
            g_level.store(newv, std::memory_order_relaxed);
        }
    }
    void init(const char* deviceName = nullptr)
    {
        if (SDL_InitSubSystem(SDL_INIT_AUDIO) != 0) {
            printf("SDL audio init failed: %s\n", SDL_GetError());
            return;
        }
        SDL_AudioSpec want {};
        SDL_AudioSpec have {};
        want.freq = 48000;
        want.format = AUDIO_F32SYS;
        want.channels = 2;
        want.samples = 1024;
        want.callback = callback;
        want.userdata = this;
        dev = SDL_OpenAudioDevice(deviceName, 1 /* capture */, &want, &have, 0);
        if (!dev) {
            printf("Audio capture open failed: %s\n", SDL_GetError());
            return;
        }
        SDL_PauseAudioDevice(dev, 0); // start
        ok.store(true);
        printf("Audio capture started: %d Hz, channels=%d\n", have.freq, have.channels);
    }
    ~AudioCapture()
    {
        if (dev)
            SDL_CloseAudioDevice(dev);
    }
};
#endif

int main(int argc, char** argv)
{
    printf("AVS portable runner starting (%s mode)\n", AVS_SDL2 ? "SDL2" : "headless");

    const char* requestedDevice = nullptr;
    bool listDevices = false;
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--list-devices") == 0) {
            listDevices = true;
        } else if (std::strcmp(argv[i], "--device") == 0 && i + 1 < argc) {
            requestedDevice = argv[++i];
        } else if (std::strcmp(argv[i], "--help") == 0) {
            printf("Options:\n  --list-devices       List capture devices\n  --device <name|#>    Use capture device by name substring or index\n");
            return 0;
        }
    }

#if AVS_SDL2
    // Ensure audio subsystem ready for enumeration before possible window init.
    if (SDL_InitSubSystem(SDL_INIT_AUDIO) != 0) {
        printf("SDL audio init failed early: %s\n", SDL_GetError());
    }
#endif

    const int W = 640;
    const int H = 360;
    std::vector<unsigned int> fb(W * H, 0x00000000);

#if AVS_SDL2
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS | SDL_INIT_TIMER) != 0) {
        printf("SDL init failed: %s\n", SDL_GetError());
        return 1;
    }

    AudioCapture audio;
    if (listDevices) {
        int n = SDL_GetNumAudioDevices(SDL_TRUE); // capture=1
        printf("Capture devices (%d):\n", n);
        for (int i = 0; i < n; ++i) {
            printf("  [%d] %s\n", i, SDL_GetAudioDeviceName(i, SDL_TRUE));
        }
        // If only listing (no device requested) exit before opening any device
        if (!requestedDevice) {
            SDL_QuitSubSystem(SDL_INIT_AUDIO);
            // Continue to create window so user can still see visualization? choose to exit.
            return 0;
        }
    }
    if (requestedDevice) {
        // Allow numeric index or substring match
        int n = SDL_GetNumAudioDevices(SDL_TRUE);
        int useIndex = -1;
        char* endp = nullptr;
        long asNum = strtol(requestedDevice, &endp, 10);
        if (endp && *endp == '\0' && asNum >= 0 && asNum < n) {
            useIndex = (int)asNum;
        }
        if (useIndex < 0) {
            for (int i = 0; i < n; ++i) {
                const char* nm = SDL_GetAudioDeviceName(i, SDL_TRUE);
                if (nm && std::strstr(nm, requestedDevice)) {
                    useIndex = i;
                    break;
                }
            }
        }
        if (useIndex >= 0) {
            const char* devName = SDL_GetAudioDeviceName(useIndex, SDL_TRUE);
            printf("Opening capture device [%d]: %s\n", useIndex, devName ? devName : "<null>");
            audio.init(devName);
        } else {
            printf("Requested device '%s' not found, falling back to default.\n", requestedDevice);
            audio.init(nullptr);
        }
    } else {
        audio.init(nullptr);
    }

    SDL_Window* win = SDL_CreateWindow("AVS Portable", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, W, H, SDL_WINDOW_RESIZABLE);
    if (!win) {
        printf("SDL window fail: %s\n", SDL_GetError());
        return 1;
    }
    SDL_Renderer* ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!ren) {
        printf("SDL renderer fail: %s\n", SDL_GetError());
        return 1;
    }
    SDL_Texture* tex = SDL_CreateTexture(ren, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, W, H);
#endif

    uint64_t frame = 0;
    auto lastPrint = std::chrono::steady_clock::now();
    bool running = true;
    bool useOscStar = false;
    bool showOverlay = true; // legacy overlay (disabled automatically if ImGui active)
    std::string presetPath = "presets/oscstar.json";
    // attempt load existing preset
    if (auto p = load_oscstar_preset(presetPath))
        g_starParams = *p;
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--oscstar") == 0)
            useOscStar = true;
    }

    // ImGui init
#if AVS_SDL2 && AVS_IMGUI
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    (void)io;
    ImGui::StyleColorsDark();
    ImGui_ImplSDL2_InitForSDLRenderer(win, ren);
    ImGui_ImplSDLRenderer2_Init(ren);
#endif

    // Build effect chain (once)
    std::vector<std::unique_ptr<Effect>> chain;
    int selectedIndex = 0;
    auto oscStar = std::make_unique<OscStarEffect>();
    oscStar->params = g_starParams; // load any preset
    auto radial = std::make_unique<RadialEffect>();
    chain.push_back(std::move(radial));
    chain.push_back(std::move(oscStar));

    while (running) {
#if AVS_SDL2
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT)
                running = false;
#if AVS_IMGUI
            ImGui_ImplSDL2_ProcessEvent(&e);
#endif
            if (e.type == SDL_KEYDOWN) {
                SDL_Keycode k = e.key.keysym.sym;
                if (k == SDLK_ESCAPE)
                    running = false;
                if (k == SDLK_TAB)
                    showOverlay = !showOverlay;
                // keyboard chain select basic shortcuts
                if (k == SDLK_UP) {
                    if (selectedIndex > 0)
                        selectedIndex--;
                }
                if (k == SDLK_DOWN) {
                    if (selectedIndex + 1 < (int)chain.size())
                        selectedIndex++;
                }
                if (useOscStar) {
                    switch (k) {
                    case SDLK_a:
                        g_starParams.arms = (g_starParams.arms > 2 ? g_starParams.arms - 1 : 2);
                        break;
                    case SDLK_s:
                        g_starParams.arms = (g_starParams.arms < 24 ? g_starParams.arms + 1 : 24);
                        break;
                    case SDLK_z:
                        g_starParams.baseSpeed = (g_starParams.baseSpeed > 0.005f ? g_starParams.baseSpeed - 0.005f : 0.0f);
                        break;
                    case SDLK_x:
                        g_starParams.baseSpeed += 0.005f;
                        break;
                    case SDLK_c:
                        g_starParams.levelSpeed = (g_starParams.levelSpeed > 0.01f ? g_starParams.levelSpeed - 0.01f : 0.0f);
                        break;
                    case SDLK_v:
                        g_starParams.levelSpeed += 0.01f;
                        break;
                    case SDLK_f:
                        g_starParams.trailFade = (g_starParams.trailFade > 0.02f ? g_starParams.trailFade - 0.02f : 0.0f);
                        break;
                    case SDLK_g:
                        g_starParams.trailFade = (g_starParams.trailFade < 0.98f ? g_starParams.trailFade + 0.02f : 0.98f);
                        break;
                    case SDLK_p: {
// ensure directory (optional)
#if AVS_HAVE_FILESYSTEM
                        try {
                            std::filesystem::create_directories("presets");
                        } catch (...) {
                        }
#endif
                        if (save_oscstar_preset(presetPath, g_starParams))
                            printf("Saved preset to %s\n", presetPath.c_str());
                    } break;
                    case SDLK_l:
                        if (auto p = load_oscstar_preset(presetPath)) {
                            g_starParams = *p;
                            printf("Loaded preset from %s\n", presetPath.c_str());
                        }
                        break;
                    }
                }
            }
        }
#endif
        float lvl = g_level.load(std::memory_order_relaxed);
#if !AVS_SDL2
        // Headless fallback synthetic level
        lvl = 0.5f + 0.5f * std::sin(frame * 0.02f);
        g_level.store(lvl);
#endif
        float t = (float)frame * 0.016f;
        if (g_fft.compute(g_spec)) {
            // g_spec normalized 0..1
        }
        // Render chain
        FrameContext fctx { fb.data(), W, H, lvl, &g_spec, t, frame };
        for (auto& eff : chain) {
            if (!eff->enabled)
                continue;
            eff->render(fctx);
        }

        avs_portable_tick();

        auto now = std::chrono::steady_clock::now();
        if (now - lastPrint > std::chrono::seconds(1)) {
            printf("frame %llu lvl=%.3f band0=%.2f fb0=%08X\n", (unsigned long long)frame, (double)lvl, g_spec.empty() ? 0.0f : g_spec[0], fb[0]);
            lastPrint = now;
        }

#if AVS_SDL2
        void* pixels = nullptr;
        int pitch = 0;
        if (SDL_LockTexture(tex, nullptr, &pixels, &pitch) == 0) {
            for (int y = 0; y < H; ++y) {
                std::memcpy((uint8_t*)pixels + y * pitch, &fb[y * W], W * sizeof(uint32_t));
            }
            SDL_UnlockTexture(tex);
        }
        SDL_RenderClear(ren);
        SDL_RenderCopy(ren, tex, nullptr, nullptr);

#if AVS_SDL2 && AVS_IMGUI
        // Start a new ImGui frame
        ImGui_ImplSDLRenderer2_NewFrame();
        ImGui_ImplSDL2_NewFrame();
        ImGui::NewFrame();
        // (Dockspace disabled: compiled ImGui without docking; panels float.)
        // Chain panel
        if (ImGui::Begin("Chain")) {
            for (size_t i = 0; i < chain.size(); ++i) {
                ImGui::PushID((int)i);
                bool enabled = chain[i]->enabled;
                if (ImGui::Checkbox("##en", &enabled))
                    chain[i]->enabled = enabled;
                ImGui::SameLine();
                if (ImGui::Selectable(chain[i]->name(), selectedIndex == (int)i))
                    selectedIndex = (int)i;
                ImGui::SameLine();
                if (ImGui::SmallButton("Up") && i > 0) {
                    std::swap(chain[i - 1], chain[i]);
                    if ((int)i == selectedIndex)
                        selectedIndex--;
                }
                ImGui::SameLine();
                if (ImGui::SmallButton("Dn") && i + 1 < chain.size()) {
                    std::swap(chain[i + 1], chain[i]);
                    if ((int)i == selectedIndex)
                        selectedIndex++;
                }
                ImGui::PopID();
            }
        }
        ImGui::End();
        // Inspector for selected effect
        if (selectedIndex >= 0 && selectedIndex < (int)chain.size()) {
            if (ImGui::Begin("Inspector")) {
                chain[selectedIndex]->drawUI();
            }
            ImGui::End();
        }
        // Audio & Stats windows remain
        if (ImGui::Begin("Audio")) {
            float lvlv = g_level.load(std::memory_order_relaxed);
            ImGui::Text("Level %.3f", lvlv);
            if (!g_spec.empty())
                ImGui::PlotLines("Spectrum", g_spec.data(), (int)g_spec.size(), 0, nullptr, 0.0f, 1.0f, ImVec2(0, 60));
            ImGui::End();
        }
        if (ImGui::Begin("Stats")) {
            ImGui::Text("Frame %llu", (unsigned long long)frame);
            ImGui::Text("FPS (approx) TBD");
            ImGui::End();
        }
        ImGui::Render();
        ImGui_ImplSDLRenderer2_RenderDrawData(ImGui::GetDrawData(), ren);
#else
        if (showOverlay) {
            // Very primitive text overlay using SDL primitives: we will draw a tiny 4x6 pixel font inline
            auto putpix = [&](int x, int y, uint32_t col) { if(x>=0&&x<W&&y>=0&&y<H){ // modify texture already copied? simpler: queue a point draw
                SDL_SetRenderDrawColor(ren,(col)&0xFF,(col>>8)&0xFF,(col>>16)&0xFF,255); SDL_RenderDrawPoint(ren,x,y);
            } };
            static const unsigned char font4x6[][6] = {
                // digits+letters subset (ASCII 32..32+47 for minimal overlay)
                { 0, 0, 0, 0, 0, 0 }, // space
            };
            // We'll just print via SDL_RenderDrawLine for simplicity instead of full font: show parameters in console-like lines
            // Draw translucent backdrop
            SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);
            SDL_SetRenderDrawColor(ren, 0, 0, 0, 160);
            SDL_Rect rct { 8, 8, 260, 120 };
            SDL_RenderFillRect(ren, &rct);
            auto drawText = [&](int ox, int oy, const char* txt) { int x=ox,y=oy; while(*txt){ char ch=*txt++; if(ch=='\n'){ y+=12; x=ox; continue;} // simple line
                // crude: draw character as rectangle pattern using bits from a tiny hex pattern fallback
                unsigned mask[8]; for(int i=0;i<8;++i) mask[i]=0; // currently blank -> just simple placeholder bars
                SDL_SetRenderDrawColor(ren, 200,200,200,255); SDL_RenderDrawLine(ren,x,y+8,x+6,y+8); // underline only
                x+=8;
            } };
            char buf[256];
            snprintf(buf, sizeof(buf), "OSCSTAR PARAMETERS\nArms: %d (A-/S+)\nBaseSpeed: %.3f (Z-/X+)\nLevelSpeed: %.3f (C-/V+)\nTrailFade: %.2f (F-/G+)\nSave: P  Load: L  Hide: TAB", g_starParams.arms, g_starParams.baseSpeed, g_starParams.levelSpeed, g_starParams.trailFade);
            drawText(16, 16, buf);
        }
#endif
        SDL_RenderPresent(ren);
#else
        std::this_thread::sleep_for(std::chrono::milliseconds(16));
#endif
        ++frame;
    }

#if AVS_SDL2
    SDL_DestroyTexture(tex);
    SDL_DestroyRenderer(ren);
    SDL_DestroyWindow(win);
    SDL_Quit();
#endif
    return 0;
}
