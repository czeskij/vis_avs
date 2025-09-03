// ...existing code moved from standalone/avs_runner.cpp...
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
#if __has_include(<filesystem>)
#include <filesystem>
#define AVS_HAVE_FILESYSTEM 1
#else
#define AVS_HAVE_FILESYSTEM 0
#endif
static std::atomic<float> g_level { 0.0f };
static FFTAnalyzer g_fft(2048, 64);
static std::vector<float> g_spec;
static OscStarParams g_starParams;
#if AVS_SDL2
struct AudioCapture {
    SDL_AudioDeviceID dev = 0;
    std::atomic<bool> ok { false };
    static void callback(void*, Uint8*, int);
    void init(const char* deviceName = nullptr);
    ~AudioCapture();
};
void AudioCapture::callback(void* userdata, Uint8* stream, int lenBytes)
{
    if (!userdata)
        return;
    int samples = lenBytes / sizeof(float);
    float* f = (float*)stream;
    g_fft.push(f, samples / 2);
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
void AudioCapture::init(const char* deviceName)
{
    if (SDL_InitSubSystem(SDL_INIT_AUDIO) != 0) {
        printf("SDL audio init failed: %s\n", SDL_GetError());
        return;
    }
    SDL_AudioSpec want {}, have {};
    want.freq = 48000;
    want.format = AUDIO_F32SYS;
    want.channels = 2;
    want.samples = 1024;
    want.callback = callback;
    want.userdata = this;
    dev = SDL_OpenAudioDevice(deviceName, 1, &want, &have, 0);
    if (!dev) {
        printf("Audio capture open failed: %s\n", SDL_GetError());
        return;
    }
    SDL_PauseAudioDevice(dev, 0);
    ok.store(true);
    printf("Audio capture started: %d Hz, channels=%d\n", have.freq, have.channels);
}
AudioCapture::~AudioCapture()
{
    if (dev)
        SDL_CloseAudioDevice(dev);
}
#endif
int main(int argc, char** argv)
{
    printf("AVS portable runner starting (%s mode)\n", AVS_SDL2 ? "SDL2" : "headless");
    const char* requestedDevice = nullptr;
    bool listDevices = false;
    for (int i = 1; i < argc; ++i) {
        if (!std::strcmp(argv[i], "--list-devices"))
            listDevices = true;
        else if (!std::strcmp(argv[i], "--device") && i + 1 < argc)
            requestedDevice = argv[++i];
    }
    const int W = 640, H = 360;
    std::vector<unsigned int> fb(W * H, 0x00000000);
#if AVS_SDL2
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS | SDL_INIT_TIMER) != 0) {
        printf("SDL init failed: %s\n", SDL_GetError());
        return 1;
    }
    AudioCapture audio;
    if (listDevices) {
        int n = SDL_GetNumAudioDevices(SDL_TRUE);
        printf("Capture devices (%d):\n", n);
        for (int i = 0; i < n; ++i)
            printf("  [%d] %s\n", i, SDL_GetAudioDeviceName(i, SDL_TRUE));
        if (!requestedDevice)
            return 0;
    }
    if (requestedDevice) {
        int n = SDL_GetNumAudioDevices(SDL_TRUE);
        int useIndex = -1;
        char* endp = nullptr;
        long asNum = strtol(requestedDevice, &endp, 10);
        if (endp && *endp == '\0' && asNum >= 0 && asNum < n)
            useIndex = (int)asNum;
        if (useIndex < 0) {
            for (int i = 0; i < n; ++i) {
                const char* nm = SDL_GetAudioDeviceName(i, SDL_TRUE);
                if (nm && std::strstr(nm, requestedDevice)) {
                    useIndex = i;
                    break;
                }
            }
        }
        if (useIndex >= 0)
            audio.init(SDL_GetAudioDeviceName(useIndex, SDL_TRUE));
        else
            audio.init(nullptr);
    } else
        audio.init(nullptr);
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
#if AVS_IMGUI
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();
    ImGui_ImplSDL2_InitForSDLRenderer(win, ren);
    ImGui_ImplSDLRenderer2_Init(ren);
#endif
#endif
    uint64_t frame = 0;
    auto lastPrint = std::chrono::steady_clock::now();
    bool running = true;
    bool showOverlay = true;
    std::string presetPath = "presets/oscstar.json";
    if (auto p = load_oscstar_preset(presetPath))
        g_starParams = *p;
    std::vector<std::unique_ptr<Effect>> chain;
    int selectedIndex = 0;
    auto radial = std::make_unique<RadialEffect>();
    auto osc = std::make_unique<OscStarEffect>();
    osc->params = g_starParams;
    chain.push_back(std::move(radial));
    chain.push_back(std::move(osc));
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
                auto k = e.key.keysym.sym;
                if (k == SDLK_ESCAPE)
                    running = false;
                if (k == SDLK_UP && selectedIndex > 0)
                    selectedIndex--;
                if (k == SDLK_DOWN && selectedIndex + 1 < (int)chain.size())
                    selectedIndex++;
            }
        }
#endif
        float lvl = g_level.load(std::memory_order_relaxed);
        float t = (float)frame * 0.016f;
        g_fft.compute(g_spec);
        FrameContext fctx { fb.data(), W, H, lvl, &g_spec, (double)t, frame };
        for (auto& eff : chain) {
            if (eff->enabled)
                eff->render(fctx);
        }
        avs_portable_tick();
        auto now = std::chrono::steady_clock::now();
        if (now - lastPrint > std::chrono::seconds(1)) {
            printf("frame %llu lvl=%.3f fb0=%08X\n", (unsigned long long)frame, (double)lvl, fb[0]);
            lastPrint = now;
        }
#if AVS_SDL2
        void* pixels = nullptr;
        int pitch = 0;
        if (SDL_LockTexture(tex, nullptr, &pixels, &pitch) == 0) {
            for (int y = 0; y < H; ++y)
                std::memcpy((uint8_t*)pixels + y * pitch, &fb[y * W], W * sizeof(uint32_t));
            SDL_UnlockTexture(tex);
        }
        SDL_RenderClear(ren);
        SDL_RenderCopy(ren, tex, nullptr, nullptr);
#if AVS_IMGUI
        ImGui_ImplSDLRenderer2_NewFrame();
        ImGui_ImplSDL2_NewFrame();
        ImGui::NewFrame();
        if (ImGui::Begin("Chain")) {
            for (size_t i = 0; i < chain.size(); ++i) {
                ImGui::PushID((int)i);
                bool en = chain[i]->enabled;
                if (ImGui::Checkbox("##en", &en))
                    chain[i]->enabled = en;
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
        if (selectedIndex >= 0 && selectedIndex < (int)chain.size())
            if (ImGui::Begin("Inspector")) {
                chain[selectedIndex]->drawUI();
                ImGui::End();
            }
        if (ImGui::Begin("Audio")) {
            ImGui::Text("Level %.3f", lvl);
            if (!g_spec.empty())
                ImGui::PlotLines("Spectrum", g_spec.data(), (int)g_spec.size(), 0, nullptr, 0.0f, 1.0f, ImVec2(0, 60));
            ImGui::End();
        }
        if (ImGui::Begin("Stats")) {
            ImGui::Text("Frame %llu", (unsigned long long)frame);
            ImGui::End();
        }
        ImGui::Render();
        ImGui_ImplSDLRenderer2_RenderDrawData(ImGui::GetDrawData(), ren);
#endif
        SDL_RenderPresent(ren);
#else
        std::this_thread::sleep_for(std::chrono::milliseconds(16));
#endif
        ++frame;
    }
#if AVS_SDL2
    SDL_Quit();
#endif
    return 0;
}
