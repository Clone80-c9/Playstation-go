#include <android/input.h>
#include <android/log.h>
#include "psg_core_api.h"

#include <algorithm>
#include <cmath>
#include <mutex>

#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, "PSG_CORE", __VA_ARGS__)

PSGInputState g_input_state{};
std::mutex g_input_state_mutex;

namespace {

constexpr float kStickDeadzone = 0.15f;

float normalized_axis(float value) {
    if (!std::isfinite(value)) {
        return 0.0f;
    }
    const float normalized = std::clamp(value, -1.0f, 1.0f);
    return std::abs(normalized) < kStickDeadzone ? 0.0f : normalized;
}

float normalized_trigger(float value) {
    if (!std::isfinite(value)) {
        return 0.0f;
    }
    return std::clamp(value, 0.0f, 1.0f);
}

bool set_digital_button(int key_code, bool pressed) {
    switch (key_code) {
        case AKEYCODE_BUTTON_A:
            g_input_state.cross = pressed;
            return true;
        case AKEYCODE_BUTTON_B:
            g_input_state.circle = pressed;
            return true;
        case AKEYCODE_BUTTON_X:
            g_input_state.square = pressed;
            return true;
        case AKEYCODE_BUTTON_Y:
            g_input_state.triangle = pressed;
            return true;
        case AKEYCODE_BUTTON_L1:
            g_input_state.l1 = pressed;
            return true;
        case AKEYCODE_BUTTON_R1:
            g_input_state.r1 = pressed;
            return true;
        case AKEYCODE_BUTTON_L2:
            g_input_state.l2_digital = pressed;
            return true;
        case AKEYCODE_BUTTON_R2:
            g_input_state.r2_digital = pressed;
            return true;
        case AKEYCODE_BUTTON_THUMBL:
            g_input_state.l3 = pressed;
            return true;
        case AKEYCODE_BUTTON_THUMBR:
            g_input_state.r3 = pressed;
            return true;
        case AKEYCODE_BUTTON_START:
            g_input_state.options = pressed;
            return true;
        case AKEYCODE_BUTTON_SELECT:
            g_input_state.share = pressed;
            return true;
        default:
            return false;
    }
}

void set_virtual_button(int button_id, bool pressed) {
    switch (button_id) {
        case 0:
            g_input_state.cross = pressed;
            break;
        case 1:
            g_input_state.circle = pressed;
            break;
        case 2:
            g_input_state.square = pressed;
            break;
        case 3:
            g_input_state.triangle = pressed;
            break;
        case 4:
            g_input_state.l1 = pressed;
            break;
        case 5:
            g_input_state.r1 = pressed;
            break;
        case 6:
            g_input_state.l2_digital = pressed;
            break;
        case 7:
            g_input_state.r2_digital = pressed;
            break;
        case 8:
            g_input_state.up = pressed;
            break;
        case 9:
            g_input_state.down = pressed;
            break;
        case 10:
            g_input_state.left = pressed;
            break;
        case 11:
            g_input_state.right = pressed;
            break;
        case 12:
            g_input_state.options = pressed;
            break;
        case 13:
            g_input_state.share = pressed;
            break;
        case 14:
            g_input_state.l3 = pressed;
            break;
        case 15:
            g_input_state.r3 = pressed;
            break;
        default:
            break;
    }
}

}  // namespace

bool processKeyEvent(int keyCode, int action, int source) {
    if ((source & AINPUT_SOURCE_GAMEPAD) != AINPUT_SOURCE_GAMEPAD) {
        return false;
    }
    if (action != AKEY_EVENT_ACTION_DOWN && action != AKEY_EVENT_ACTION_UP) {
        return false;
    }
    std::lock_guard<std::mutex> lock(g_input_state_mutex);
    if (!set_digital_button(keyCode, action == AKEY_EVENT_ACTION_DOWN)) {
        return false;
    }
    g_input_state.connected = true;
    return true;
}

void processMotionEvent(float axisX, float axisY, float axisZ, float axisRZ,
                        float axisLT, float axisRT, float hatX, float hatY) {
    std::lock_guard<std::mutex> lock(g_input_state_mutex);
    g_input_state.left_stick_x = normalized_axis(axisX);
    g_input_state.left_stick_y = normalized_axis(axisY);
    g_input_state.right_stick_x = normalized_axis(axisZ);
    g_input_state.right_stick_y = normalized_axis(axisRZ);
    g_input_state.l2_analog = normalized_trigger(axisLT);
    g_input_state.r2_analog = normalized_trigger(axisRT);

    g_input_state.up = hatY < 0.0f;
    g_input_state.down = hatY > 0.0f;
    g_input_state.left = hatX < 0.0f;
    g_input_state.right = hatX > 0.0f;
    g_input_state.connected = true;
}

void processVirtualInput(int buttonId, bool pressed, float axisX, float axisY) {
    std::lock_guard<std::mutex> lock(g_input_state_mutex);
    if (buttonId >= 0 && buttonId <= 15) {
        set_virtual_button(buttonId, pressed);
        g_input_state.connected = true;
        return;
    }

    if (buttonId == 16) {
        g_input_state.left_stick_x = normalized_axis(axisX);
        g_input_state.left_stick_y = normalized_axis(axisY);
        g_input_state.connected = true;
        return;
    }
    if (buttonId == 17) {
        g_input_state.right_stick_x = normalized_axis(axisX);
        g_input_state.right_stick_y = normalized_axis(axisY);
        g_input_state.connected = true;
        return;
    }

    LOGE("Ignoring invalid virtual controller button ID: %d", buttonId);
}

PSGInputState getInputStateSnapshot() {
    std::lock_guard<std::mutex> lock(g_input_state_mutex);
    return g_input_state;
}
