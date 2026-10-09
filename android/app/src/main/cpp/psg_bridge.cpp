#include <jni.h>
#include <android/log.h>
#include <android/native_window.h>
#include <android/native_window_jni.h>

#include <dlfcn.h>

#include <atomic>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>
#include <utility>

#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, "PSG_CORE", __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, "PSG_CORE", __VA_ARGS__)
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN, "PSG_CORE", __VA_ARGS__)

extern std::atomic<float> g_cpu_temp;
int checkAndUpdateThermal();
std::string readDeviceTier();
bool processKeyEvent(int keyCode, int action, int source);
void processMotionEvent(float axisX, float axisY, float axisZ, float axisRZ,
                        float axisLT, float axisRT, float hatX, float hatY);
void processVirtualInput(int buttonId, bool pressed, float axisX, float axisY);

namespace {

using CoreInitFn = int (*)(const char*, const char*, const char*, const char*, ANativeWindow*);
using CoreRunFn = int (*)();
using CoreShutdownFn = void (*)();
using CoreStateFn = int (*)(const char*);
using CoreApplySettingsFn = int (*)(const char*);

ANativeWindow* g_window = nullptr;
void* g_core_handle = nullptr;
std::atomic<bool> g_emulation_running{false};
std::atomic<int> g_current_fps{0};
std::thread g_emulation_thread;
std::string g_current_console_type;
std::string g_current_rom_path;
std::string g_loaded_core_name;

std::string g_files_dir;
std::mutex g_state_mutex;
std::condition_variable g_thread_finished_condition;
bool g_thread_finished = true;
bool g_core_initialized = false;

template <typename T>
T get_core_symbol(const char* name) {
    if (g_core_handle == nullptr) {
        return nullptr;
    }

    dlerror();
    void* symbol = dlsym(g_core_handle, name);
    const char* error = dlerror();
    if (error != nullptr) {
        LOGE("Core symbol %s is unavailable: %s", name, error);
        return nullptr;
    }
    return reinterpret_cast<T>(symbol);
}

bool jstring_to_string(JNIEnv* env, jstring value, std::string* output) {
    if (env == nullptr || value == nullptr || output == nullptr) {
        return false;
    }

    const char* chars = env->GetStringUTFChars(value, nullptr);
    if (env->ExceptionCheck() == JNI_TRUE || chars == nullptr) {
        return false;
    }

    *output = chars;
    env->ReleaseStringUTFChars(value, chars);
    if (env->ExceptionCheck() == JNI_TRUE) {
        return false;
    }
    return true;
}

jstring make_java_string(JNIEnv* env, const std::string& value) {
    if (env == nullptr) {
        return nullptr;
    }
    jstring result = env->NewStringUTF(value.c_str());
    if (env->ExceptionCheck() == JNI_TRUE) {
        return nullptr;
    }
    return result;
}

std::string json_escape(const std::string& value) {
    std::ostringstream escaped;
    for (const unsigned char character : value) {
        switch (character) {
            case '"':
                escaped << "\\\"";
                break;
            case '\\':
                escaped << "\\\\";
                break;
            case '\b':
                escaped << "\\b";
                break;
            case '\f':
                escaped << "\\f";
                break;
            case '\n':
                escaped << "\\n";
                break;
            case '\r':
                escaped << "\\r";
                break;
            case '\t':
                escaped << "\\t";
                break;
            default:
                if (character < 0x20) {
                    escaped << "\\u00";
                    constexpr char digits[] = "0123456789abcdef";
                    escaped << digits[(character >> 4) & 0x0f]
                            << digits[character & 0x0f];
                } else {
                    escaped << static_cast<char>(character);
                }
        }
    }
    return escaped.str();
}

std::string game_id_from_path(const std::string& rom_path) {
    const std::filesystem::path path(rom_path);
    std::string game_id = path.stem().string();
    if (game_id.empty() || game_id == "." || game_id == "..") {
        return {};
    }
    return game_id;
}

std::filesystem::path state_path_for_slot(jint slot) {
    return std::filesystem::path(g_files_dir) / "psg" / "saves" /
           game_id_from_path(g_current_rom_path) /
           ("slot_" + std::to_string(slot) + ".state");
}

bool parse_setting_value(const std::string& json, const std::string& key,
                         std::string* value, bool* is_string) {
    if (value == nullptr || is_string == nullptr) {
        return false;
    }

    const std::string token = "\"" + key + "\"";
    const std::size_t key_position = json.find(token);
    if (key_position == std::string::npos) {
        return false;
    }

    const std::size_t colon = json.find(':', key_position + token.size());
    if (colon == std::string::npos) {
        return false;
    }

    std::size_t begin = json.find_first_not_of(" \t\r\n", colon + 1);
    if (begin == std::string::npos) {
        return false;
    }

    if (json[begin] == '"') {
        const std::size_t end = json.find('"', begin + 1);
        if (end == std::string::npos) {
            return false;
        }
        *value = json.substr(begin + 1, end - begin - 1);
        *is_string = true;
        return true;
    }

    const std::size_t end = json.find_first_of(",} \t\r\n", begin);
    *value = json.substr(begin, end == std::string::npos ? end : end - begin);
    *is_string = false;
    return !value->empty();
}

bool validate_settings(const std::string& json) {
    std::string resolution_scale;
    std::string fps_limit;
    std::string graphics_backend;
    std::string texture_filtering;
    bool is_string = false;

    if (!parse_setting_value(json, "resolution_scale", &resolution_scale, &is_string) ||
        is_string) {
        return false;
    }
    char* end = nullptr;
    const float scale = std::strtof(resolution_scale.c_str(), &end);
    if (end == resolution_scale.c_str() || *end != '\0' || scale <= 0.0f) {
        return false;
    }

    if (!parse_setting_value(json, "fps_limit", &fps_limit, &is_string) || is_string) {
        return false;
    }
    char* fps_end = nullptr;
    const long fps = std::strtol(fps_limit.c_str(), &fps_end, 10);
    if (fps_end == fps_limit.c_str() || *fps_end != '\0' || fps < 0) {
        return false;
    }

    if (!parse_setting_value(json, "graphics_backend", &graphics_backend, &is_string) ||
        !is_string || graphics_backend.empty()) {
        return false;
    }

    if (!parse_setting_value(json, "texture_filtering", &texture_filtering, &is_string)) {
        return false;
    }
    return is_string ? !texture_filtering.empty()
                     : texture_filtering == "true" || texture_filtering == "false";
}

bool state_functions_available(CoreStateFn* function) {
    if (function == nullptr) {
        return false;
    }
    *function = get_core_symbol<CoreStateFn>("psg_core_save_state");
    return *function != nullptr;
}

}  // namespace

extern "C" JNIEXPORT jboolean JNICALL
Java_com_psg_app_PSGEmulatorBridge_nativeInit(JNIEnv* env, jobject, jobject surface,
                                              jstring files_dir) {
    try {
        if (env == nullptr || surface == nullptr || files_dir == nullptr) {
            LOGE("nativeInit received a null argument");
            return JNI_FALSE;
        }

        std::string files_path;
        if (!jstring_to_string(env, files_dir, &files_path)) {
            LOGE("Unable to read filesDir from Java");
            return JNI_FALSE;
        }

        std::error_code filesystem_error;
        if (!std::filesystem::exists(files_path, filesystem_error) ||
            filesystem_error ||
            !std::filesystem::is_directory(files_path, filesystem_error) ||
            filesystem_error) {
            LOGE("App filesDir does not exist or is not a directory");
            return JNI_FALSE;
        }

        ANativeWindow* window = ANativeWindow_fromSurface(env, surface);
        if (env->ExceptionCheck() == JNI_TRUE) {
            if (window != nullptr) {
                ANativeWindow_release(window);
            }
            return JNI_FALSE;
        }
        if (window == nullptr) {
            LOGE("Unable to obtain native window from surface");
            return JNI_FALSE;
        }

        const int width = ANativeWindow_getWidth(window);
        const int height = ANativeWindow_getHeight(window);
        {
            std::lock_guard<std::mutex> lock(g_state_mutex);
            if (g_emulation_running.load()) {
                ANativeWindow_release(window);
                LOGE("Cannot replace the window while emulation is running");
                return JNI_FALSE;
            }
            if (g_window != nullptr) {
                ANativeWindow_release(g_window);
            }
            g_window = window;
            g_files_dir = std::move(files_path);
        }

        LOGI("Native window initialized: %dx%d", width, height);
        return JNI_TRUE;
    } catch (const std::exception& exception) {
        LOGE("nativeInit failed: %s", exception.what());
        return JNI_FALSE;
    } catch (...) {
        LOGE("nativeInit failed with an unknown error");
        return JNI_FALSE;
    }
}

extern "C" JNIEXPORT jstring JNICALL
Java_com_psg_app_PSGEmulatorBridge_nativeLoadCore(JNIEnv* env, jobject,
                                                  jstring core_path) {
    try {
        if (env == nullptr || core_path == nullptr) {
            return make_java_string(env, "ERROR: corePath is null");
        }

        std::string path;
        if (!jstring_to_string(env, core_path, &path)) {
            return make_java_string(env, "ERROR: unable to read corePath");
        }

        std::error_code filesystem_error;
        if (!std::filesystem::exists(path, filesystem_error) || filesystem_error) {
            return make_java_string(env, "ERROR: core file does not exist");
        }

        std::lock_guard<std::mutex> lock(g_state_mutex);
        if (g_emulation_running.load() || g_core_handle != nullptr) {
            return make_java_string(env, "ERROR: a core is already loaded or running");
        }

        dlerror();
        void* handle = dlopen(path.c_str(), RTLD_NOW | RTLD_LOCAL);
        if (handle == nullptr) {
            const char* error = dlerror();
            const std::string message = error == nullptr ? "unknown dlopen error" : error;
            LOGE("Unable to load core %s: %s", path.c_str(), message.c_str());
            return make_java_string(env, "ERROR: " + message);
        }

        void* previous_handle = g_core_handle;
        g_core_handle = handle;
        if (get_core_symbol<CoreInitFn>("psg_core_init") == nullptr) {
            g_core_handle = previous_handle;
            if (dlclose(handle) != 0) {
                const char* error = dlerror();
                LOGE("Unable to close core with missing entry point: %s",
                     error == nullptr ? "unknown dlclose error" : error);
            }
            return make_java_string(env, "ERROR: core entry point psg_core_init is missing");
        }

        g_loaded_core_name = std::filesystem::path(path).stem().string();
        LOGI("Core loaded: %s", path.c_str());
        return make_java_string(env, "OK");
    } catch (const std::exception& exception) {
        LOGE("nativeLoadCore failed: %s", exception.what());
        return make_java_string(env, std::string("ERROR: ") + exception.what());
    } catch (...) {
        LOGE("nativeLoadCore failed with an unknown error");
        return make_java_string(env, "ERROR: unknown native error");
    }
}

extern "C" JNIEXPORT jstring JNICALL
Java_com_psg_app_PSGEmulatorBridge_nativeLaunchGame(
    JNIEnv* env, jobject, jstring rom_path, jstring console_type,
    jstring bios_path, jstring settings_json) {
    try {
        if (env == nullptr || rom_path == nullptr || console_type == nullptr ||
            bios_path == nullptr || settings_json == nullptr) {
            return make_java_string(env,
                                    "{\"status\":\"error\",\"reason\":\"null argument\"}");
        }

        std::string rom;
        std::string console;
        std::string bios;
        std::string settings;
        if (!jstring_to_string(env, rom_path, &rom) ||
            !jstring_to_string(env, console_type, &console) ||
            !jstring_to_string(env, bios_path, &bios) ||
            !jstring_to_string(env, settings_json, &settings)) {
            return make_java_string(
                env, "{\"status\":\"error\",\"reason\":\"unable to read JNI strings\"}");
        }

        std::error_code filesystem_error;
        if (!std::filesystem::exists(rom, filesystem_error) || filesystem_error) {
            return make_java_string(env,
                                    "{\"status\":\"error\",\"reason\":\"ROM file does not exist\"}");
        }
        FILE* rom_file = std::fopen(rom.c_str(), "rb");
        if (rom_file == nullptr) {
            return make_java_string(env,
                                    "{\"status\":\"error\",\"reason\":\"unable to open ROM file\"}");
        }
        if (std::fclose(rom_file) != 0) {
            return make_java_string(
                env, "{\"status\":\"error\",\"reason\":\"unable to close ROM file\"}");
        }

        std::lock_guard<std::mutex> lock(g_state_mutex);
        if (g_window == nullptr) {
            return make_java_string(env,
                                    "{\"status\":\"error\",\"reason\":\"native window is not initialized\"}");
        }
        if (g_core_handle == nullptr) {
            return make_java_string(env,
                                    "{\"status\":\"error\",\"reason\":\"core is not loaded\"}");
        }
        if (g_emulation_running.load() || g_emulation_thread.joinable()) {
            return make_java_string(env,
                                    "{\"status\":\"error\",\"reason\":\"a game is already running\"}");
        }

        CoreInitFn core_init = get_core_symbol<CoreInitFn>("psg_core_init");
        CoreRunFn core_run = get_core_symbol<CoreRunFn>("psg_core_run");
        CoreShutdownFn core_shutdown =
            get_core_symbol<CoreShutdownFn>("psg_core_shutdown");
        if (core_init == nullptr || core_run == nullptr || core_shutdown == nullptr) {
            return make_java_string(
                env, "{\"status\":\"error\",\"reason\":\"required core entry point is missing\"}");
        }

        g_current_rom_path = rom;
        g_current_console_type = console;
        g_core_initialized = false;
        g_thread_finished = false;
        g_emulation_running.store(true);

        try {
            g_emulation_thread = std::thread(
                [core_init, core_run, rom, console, bios, settings]() {
                    try {
                        if (core_init(rom.c_str(), console.c_str(), bios.c_str(),
                                      settings.c_str(), g_window) != 0) {
                            LOGE("Core initialization failed");
                        } else {
                            {
                                std::lock_guard<std::mutex> initialized_lock(g_state_mutex);
                                g_core_initialized = true;
                            }
                            checkAndUpdateThermal();
                            auto next_thermal_check =
                                std::chrono::steady_clock::now() +
                                std::chrono::seconds(3);
                            while (g_emulation_running.load()) {
                                const int result = core_run();
                                if (result != 0) {
                                    LOGE("Core run loop returned error %d", result);
                                    break;
                                }
                                const auto now = std::chrono::steady_clock::now();
                                if (now >= next_thermal_check) {
                                    checkAndUpdateThermal();
                                    next_thermal_check =
                                        now + std::chrono::seconds(3);
                                }
                            }
                        }
                    } catch (const std::exception& exception) {
                        LOGE("Emulation thread failed: %s", exception.what());
                        g_emulation_running.store(false);
                    } catch (...) {
                        LOGE("Emulation thread failed with an unknown error");
                        g_emulation_running.store(false);
                    }
                    g_emulation_running.store(false);
                    {
                        std::lock_guard<std::mutex> finished_lock(g_state_mutex);
                        g_thread_finished = true;
                    }
                    g_thread_finished_condition.notify_all();
                });
        } catch (...) {
            g_emulation_running.store(false);
            g_thread_finished = true;
            return make_java_string(
                env, "{\"status\":\"error\",\"reason\":\"unable to create emulation thread\"}");
        }

        const std::string response =
            "{\"status\":\"launched\",\"console\":\"" + json_escape(console) +
            "\",\"core\":\"" + json_escape(g_loaded_core_name) + "\"}";
        return make_java_string(env, response);
    } catch (const std::exception& exception) {
        LOGE("nativeLaunchGame failed: %s", exception.what());
        return make_java_string(env, std::string("{\"status\":\"error\",\"reason\":\"") +
                                         json_escape(exception.what()) + "\"}");
    } catch (...) {
        LOGE("nativeLaunchGame failed with an unknown error");
        return make_java_string(
            env, "{\"status\":\"error\",\"reason\":\"unknown native error\"}");
    }
}

extern "C" JNIEXPORT jboolean JNICALL
Java_com_psg_app_PSGEmulatorBridge_nativeStopGame(JNIEnv* env, jobject) {
    try {
        if (env == nullptr) {
            return JNI_FALSE;
        }

        std::unique_lock<std::mutex> lock(g_state_mutex);
        g_emulation_running.store(false);
        if (g_emulation_thread.joinable()) {
            const bool finished = g_thread_finished_condition.wait_for(
                lock, std::chrono::seconds(3), [] { return g_thread_finished; });
            if (!finished) {
                LOGE("Emulation thread did not stop within 3 seconds");
                return JNI_FALSE;
            }
            std::thread thread_to_join = std::move(g_emulation_thread);
            lock.unlock();
            thread_to_join.join();
            lock.lock();
        }

        if (g_core_handle != nullptr) {
            if (g_core_initialized) {
                CoreShutdownFn core_shutdown =
                    get_core_symbol<CoreShutdownFn>("psg_core_shutdown");
                if (core_shutdown == nullptr) {
                    LOGE("Core does not export psg_core_shutdown");
                    return JNI_FALSE;
                }
                core_shutdown();
                g_core_initialized = false;
            }
            if (dlclose(g_core_handle) != 0) {
                const char* error = dlerror();
                LOGE("Unable to unload core: %s",
                     error == nullptr ? "unknown dlclose error" : error);
                return JNI_FALSE;
            }
            g_core_handle = nullptr;
        }

        if (g_window != nullptr) {
            ANativeWindow_release(g_window);
            g_window = nullptr;
        }
        g_current_console_type.clear();
        g_current_rom_path.clear();
        g_loaded_core_name.clear();
        LOGI("Emulation stopped cleanly");
        return JNI_TRUE;
    } catch (const std::exception& exception) {
        LOGE("nativeStopGame failed: %s", exception.what());
        return JNI_FALSE;
    } catch (...) {
        LOGE("nativeStopGame failed with an unknown error");
        return JNI_FALSE;
    }
}

extern "C" JNIEXPORT jboolean JNICALL
Java_com_psg_app_PSGEmulatorBridge_nativeSaveState(JNIEnv* env, jobject,
                                                    jint slot) {
    try {
        if (env == nullptr || slot < 0) {
            return JNI_FALSE;
        }
        std::lock_guard<std::mutex> lock(g_state_mutex);
        if (g_core_handle == nullptr || g_current_rom_path.empty() || g_files_dir.empty()) {
            LOGE("Cannot save state without a loaded game and app filesDir");
            return JNI_FALSE;
        }

        CoreStateFn save_state = nullptr;
        if (!state_functions_available(&save_state)) {
            LOGE("Core does not export psg_core_save_state");
            return JNI_FALSE;
        }

        const std::string game_id = game_id_from_path(g_current_rom_path);
        if (game_id.empty()) {
            LOGE("Unable to determine game ID for save state");
            return JNI_FALSE;
        }
        const std::filesystem::path state_path = state_path_for_slot(slot);
        const std::filesystem::path state_directory = state_path.parent_path();
        std::error_code filesystem_error;
        if (!std::filesystem::exists(state_directory, filesystem_error)) {
            if (filesystem_error ||
                !std::filesystem::create_directories(state_directory, filesystem_error) ||
                filesystem_error) {
                LOGE("Unable to create save-state directory");
                return JNI_FALSE;
            }
        }
        if (!std::filesystem::is_directory(state_directory, filesystem_error) ||
            filesystem_error) {
            LOGE("Save-state parent path is not a directory");
            return JNI_FALSE;
        }
        if (std::filesystem::exists(state_path, filesystem_error) && filesystem_error) {
            LOGE("Unable to inspect existing save-state path");
            return JNI_FALSE;
        }
        if (filesystem_error) {
            LOGE("Unable to inspect save-state path");
            return JNI_FALSE;
        }

        if (save_state(state_path.c_str()) != 0) {
            LOGE("Core failed to save state to %s", state_path.c_str());
            return JNI_FALSE;
        }
        filesystem_error.clear();
        if (!std::filesystem::exists(state_path, filesystem_error) ||
            filesystem_error ||
            !std::filesystem::is_regular_file(state_path, filesystem_error) ||
            filesystem_error) {
            LOGE("Core reported success but the save-state file is missing or invalid");
            return JNI_FALSE;
        }
        return JNI_TRUE;
    } catch (const std::exception& exception) {
        LOGE("nativeSaveState failed: %s", exception.what());
        return JNI_FALSE;
    } catch (...) {
        LOGE("nativeSaveState failed with an unknown error");
        return JNI_FALSE;
    }
}

extern "C" JNIEXPORT jboolean JNICALL
Java_com_psg_app_PSGEmulatorBridge_nativeLoadState(JNIEnv* env, jobject,
                                                    jint slot) {
    try {
        if (env == nullptr || slot < 0) {
            return JNI_FALSE;
        }
        std::lock_guard<std::mutex> lock(g_state_mutex);
        if (g_core_handle == nullptr || g_current_rom_path.empty() || g_files_dir.empty()) {
            LOGE("Cannot load state without a loaded game and app filesDir");
            return JNI_FALSE;
        }

        const std::filesystem::path state_path = state_path_for_slot(slot);
        std::error_code filesystem_error;
        if (!std::filesystem::exists(state_path, filesystem_error) ||
            filesystem_error ||
            !std::filesystem::is_regular_file(state_path, filesystem_error) ||
            filesystem_error) {
            LOGW("Save-state file is missing or invalid: %s", state_path.c_str());
            return JNI_FALSE;
        }

        CoreStateFn load_state =
            get_core_symbol<CoreStateFn>("psg_core_load_state");
        if (load_state == nullptr) {
            LOGE("Core does not export psg_core_load_state");
            return JNI_FALSE;
        }
        return load_state(state_path.c_str()) == 0 ? JNI_TRUE : JNI_FALSE;
    } catch (const std::exception& exception) {
        LOGE("nativeLoadState failed: %s", exception.what());
        return JNI_FALSE;
    } catch (...) {
        LOGE("nativeLoadState failed with an unknown error");
        return JNI_FALSE;
    }
}

extern "C" JNIEXPORT jstring JNICALL
Java_com_psg_app_PSGEmulatorBridge_nativeGetPerformanceStats(JNIEnv* env,
                                                             jobject) {
    try {
        if (env == nullptr) {
            return nullptr;
        }

        long total_kb = 0;
        long available_kb = -1;
        std::error_code filesystem_error;
        const std::filesystem::path meminfo_path("/proc/meminfo");
        if (!std::filesystem::exists(meminfo_path, filesystem_error) || filesystem_error) {
            LOGE("System memory information is unavailable");
            return make_java_string(
                env, "{\"status\":\"error\",\"reason\":\"memory information unavailable\"}");
        }

        std::ifstream meminfo(meminfo_path);
        if (!meminfo.is_open()) {
            LOGE("Unable to open system memory information");
            return make_java_string(
                env, "{\"status\":\"error\",\"reason\":\"unable to read memory information\"}");
        }
        std::string line;
        while (std::getline(meminfo, line)) {
            std::istringstream line_stream(line);
            std::string label;
            long value = 0;
            if (!(line_stream >> label >> value)) {
                continue;
            }
            if (label == "MemTotal:") {
                total_kb = value;
            } else if (label == "MemAvailable:") {
                available_kb = value;
            }
        }
        if (meminfo.bad() || total_kb <= 0 || available_kb < 0 ||
            available_kb > total_kb) {
            LOGE("Memory information in /proc/meminfo is incomplete or invalid");
            return make_java_string(
                env, "{\"status\":\"error\",\"reason\":\"invalid memory information\"}");
        }

        const long used_mb = (total_kb - available_kb) / 1024;
        const float cpu_temperature = g_cpu_temp.load();
        if (!std::isfinite(cpu_temperature)) {
            LOGE("CPU temperature is not a finite value");
            return make_java_string(
                env, "{\"status\":\"error\",\"reason\":\"invalid CPU temperature\"}");
        }
        const bool throttling = cpu_temperature >= 80.0f;
        std::ostringstream response;
        response << "{\"fps\":" << g_current_fps.load()
                 << ",\"cpu_temp\":" << cpu_temperature
                 << ",\"ram_used_mb\":" << used_mb
                 << ",\"throttling\":" << (throttling ? "true" : "false") << "}";
        return make_java_string(env, response.str());
    } catch (const std::exception& exception) {
        LOGE("nativeGetPerformanceStats failed: %s", exception.what());
        return make_java_string(env, std::string("{\"status\":\"error\",\"reason\":\"") +
                                         json_escape(exception.what()) + "\"}");
    } catch (...) {
        LOGE("nativeGetPerformanceStats failed with an unknown error");
        return make_java_string(
            env, "{\"status\":\"error\",\"reason\":\"unknown native error\"}");
    }
}

extern "C" JNIEXPORT jboolean JNICALL
Java_com_psg_app_PSGEmulatorBridge_nativeApplySettings(JNIEnv* env, jobject,
                                                       jstring settings_json) {
    try {
        if (env == nullptr || settings_json == nullptr) {
            return JNI_FALSE;
        }

        std::string settings;
        if (!jstring_to_string(env, settings_json, &settings) ||
            !validate_settings(settings)) {
            LOGE("Settings JSON is invalid or missing required values");
            return JNI_FALSE;
        }

        std::lock_guard<std::mutex> lock(g_state_mutex);
        if (!g_emulation_running.load() || g_core_handle == nullptr) {
            LOGE("Cannot apply settings when no core is running");
            return JNI_FALSE;
        }
        CoreApplySettingsFn apply_settings =
            get_core_symbol<CoreApplySettingsFn>("psg_core_apply_settings");
        if (apply_settings == nullptr) {
            LOGE("Core does not export psg_core_apply_settings");
            return JNI_FALSE;
        }
        if (apply_settings(settings.c_str()) != 0) {
            LOGE("Core rejected runtime settings");
            return JNI_FALSE;
        }
        return JNI_TRUE;
    } catch (const std::exception& exception) {
        LOGE("nativeApplySettings failed: %s", exception.what());
        return JNI_FALSE;
    } catch (...) {
        LOGE("nativeApplySettings failed with an unknown error");
        return JNI_FALSE;
    }
}

extern "C" JNIEXPORT jstring JNICALL
Java_com_psg_app_PSGEmulatorBridge_nativeGetDeviceTier(JNIEnv* env, jobject) {
    try {
        if (env == nullptr) {
            return nullptr;
        }
        const std::string tier = readDeviceTier();
        return make_java_string(env, tier);
    } catch (const std::exception& exception) {
        LOGE("nativeGetDeviceTier failed: %s", exception.what());
        return make_java_string(env, "TIER_MID");
    } catch (...) {
        LOGE("nativeGetDeviceTier failed with an unknown error");
        return make_java_string(env, "TIER_MID");
    }
}

extern "C" JNIEXPORT jboolean JNICALL
Java_com_psg_app_PSGEmulatorBridge_nativeProcessKeyEvent(
    JNIEnv* env, jobject, jint key_code, jint action, jint source) {
    try {
        if (env == nullptr) {
            return JNI_FALSE;
        }
        if (processKeyEvent(key_code, action, source)) {
            return JNI_TRUE;
        }
        return JNI_FALSE;
    } catch (const std::exception& exception) {
        LOGE("nativeProcessKeyEvent failed: %s", exception.what());
        return JNI_FALSE;
    } catch (...) {
        LOGE("nativeProcessKeyEvent failed with an unknown error");
        return JNI_FALSE;
    }
}

extern "C" JNIEXPORT jboolean JNICALL
Java_com_psg_app_PSGEmulatorBridge_nativeProcessMotionEvent(
    JNIEnv* env, jobject, jfloat axis_x, jfloat axis_y, jfloat axis_z,
    jfloat axis_rz, jfloat axis_lt, jfloat axis_rt, jfloat hat_x,
    jfloat hat_y) {
    try {
        if (env == nullptr) {
            return JNI_FALSE;
        }
        processMotionEvent(axis_x, axis_y, axis_z, axis_rz, axis_lt, axis_rt,
                           hat_x, hat_y);
        return JNI_TRUE;
    } catch (const std::exception& exception) {
        LOGE("nativeProcessMotionEvent failed: %s", exception.what());
        return JNI_FALSE;
    } catch (...) {
        LOGE("nativeProcessMotionEvent failed with an unknown error");
        return JNI_FALSE;
    }
}

extern "C" JNIEXPORT jboolean JNICALL
Java_com_psg_app_PSGEmulatorBridge_nativeProcessVirtualInput(
    JNIEnv* env, jobject, jint button_id, jboolean pressed, jfloat axis_x,
    jfloat axis_y) {
    try {
        if (env == nullptr || button_id < 0 || button_id > 17) {
            LOGE("nativeProcessVirtualInput received an invalid argument");
            return JNI_FALSE;
        }
        processVirtualInput(button_id, pressed == JNI_TRUE, axis_x, axis_y);
        return JNI_TRUE;
    } catch (const std::exception& exception) {
        LOGE("nativeProcessVirtualInput failed: %s", exception.what());
        return JNI_FALSE;
    } catch (...) {
        LOGE("nativeProcessVirtualInput failed with an unknown error");
        return JNI_FALSE;
    }
}
