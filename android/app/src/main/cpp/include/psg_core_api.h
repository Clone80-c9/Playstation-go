#ifndef PSG_CORE_API_H
#define PSG_CORE_API_H

#include <android/native_window.h>

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define PSG_CORE_API_VERSION 1

typedef struct PSGInputState {
    bool cross;
    bool circle;
    bool square;
    bool triangle;
    bool l1;
    bool r1;
    bool l2_digital;
    bool r2_digital;
    float l2_analog;
    float r2_analog;
    bool up;
    bool down;
    bool left;
    bool right;
    bool l3;
    bool r3;
    bool options;
    bool share;
    bool ps_button;
    bool touchpad;
    float left_stick_x;
    float left_stick_y;
    float right_stick_x;
    float right_stick_y;
    bool connected;
    char controller_name[64];
} PSGInputState;

typedef struct PSGHostCallbacks {
    bool (*audio_init)(int32_t sample_rate, int32_t channel_count,
                       int32_t buffer_size_frames);
    bool (*audio_write)(const int16_t* samples, int32_t frames);
    bool (*audio_set_volume)(float volume);
    void (*audio_destroy)(void);
} PSGHostCallbacks;

uint32_t psg_core_api_version(void);
int psg_core_init(const char* rom_path, const char* console_type,
                  const char* bios_path, const char* settings_json,
                  const PSGHostCallbacks* host, ANativeWindow* window);
int psg_core_run(const PSGInputState* input_state);
void psg_core_shutdown(void);

int psg_core_save_state(const char* path);
int psg_core_load_state(const char* path);
int psg_core_apply_settings(const char* settings_json);

#ifdef __cplusplus
}
#endif

#endif
