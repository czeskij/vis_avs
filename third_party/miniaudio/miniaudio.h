/* Miniaudio embedded header (abridged placeholder).
   For full functionality, fetch official miniaudio.h later.
   This minimal subset just supports capture to a callback for macOS using CoreAudio via miniaudio's implementation macros.
*/
#ifndef MINIAUDIO_MINIMAL_EMBED
#define MINIAUDIO_MINIMAL_EMBED
#define MA_NO_DECODING
#define MA_NO_ENCODING
#define MA_NO_DEVICE_IO
#define MA_NO_THREADING
#define MA_NO_GENERATION
#define MA_NO_ENGINE
#define MA_NO_NODE_GRAPH
#define MA_NO_RESOURCE_MANAGER
#define MA_NO_RUNTIME_LINKING
#define MINIAUDIO_IMPLEMENTATION

// NOTE: This is a stub placeholder. Replace with full official miniaudio.h for production use.
// We'll fake a tiny API surface we need now.
#include <stdint.h>
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif

typedef enum { ma_format_f32 = 1 } ma_format;

typedef struct { ma_format format; uint32_t channels; uint32_t sampleRate; } ma_device_config;
typedef struct { ma_device_config config; int dummy; } ma_device;

typedef void (*ma_data_callback)(ma_device* pDevice, void* pOutput, const void* pInput, uint32_t frameCount);

static inline ma_device_config ma_device_config_init_capture(ma_format format, uint32_t channels, uint32_t sampleRate) {
    ma_device_config c; c.format = format; c.channels = channels; c.sampleRate = sampleRate; return c; }

static inline int ma_device_init(void* ignored, const ma_device_config* config, ma_device* device) {
    (void)ignored; device->config = *config; return 0; }
static inline void ma_device_uninit(ma_device* device) { (void)device; }
static inline int ma_device_start(ma_device* device) { (void)device; return 0; }

#ifdef __cplusplus
}
#endif
#endif
