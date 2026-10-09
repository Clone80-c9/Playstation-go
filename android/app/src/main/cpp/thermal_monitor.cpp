#include <android/log.h>

#include <atomic>
#include <cctype>
#include <cmath>
#include <exception>
#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>

#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, "PSG_CORE", __VA_ARGS__)
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN, "PSG_CORE", __VA_ARGS__)

enum ThermalTier {
    THERMAL_NORMAL = 0,
    THERMAL_WARM = 1,
    THERMAL_HOT = 2,
    THERMAL_CRITICAL = 3,
};

std::atomic<float> g_cpu_temp{-1.0f};
std::atomic<int> g_current_thermal_tier{THERMAL_NORMAL};

namespace {

constexpr const char* kTemperaturePaths[] = {
    "/sys/class/thermal/thermal_zone0/temp",
    "/sys/class/thermal/thermal_zone1/temp",
    "/sys/devices/virtual/thermal/thermal_zone0/temp",
};

std::string uppercase(std::string value) {
    for (char& character : value) {
        character = static_cast<char>(
            std::toupper(static_cast<unsigned char>(character)));
    }
    return value;
}

}  // namespace

float readCPUTemperature() {
    for (const char* path : kTemperaturePaths) {
        try {
            std::error_code filesystem_error;
            if (!std::filesystem::exists(path, filesystem_error) ||
                filesystem_error ||
                !std::filesystem::is_regular_file(path, filesystem_error) ||
                filesystem_error) {
                continue;
            }

            std::ifstream temperature_file(path);
            if (!temperature_file.is_open()) {
                continue;
            }

            long temperature_millidegrees = 0;
            if (!(temperature_file >> temperature_millidegrees) ||
                temperature_file.bad() || temperature_millidegrees < 0) {
                LOGW("Invalid CPU temperature reading from %s", path);
                continue;
            }
            return static_cast<float>(temperature_millidegrees) / 1000.0f;
        } catch (const std::exception& exception) {
            LOGW("Unable to read CPU temperature from %s: %s", path,
                 exception.what());
        } catch (...) {
            LOGW("Unable to read CPU temperature from %s", path);
        }
    }
    LOGW("Unable to read CPU temperature from all configured thermal sensors");
    return -1.0f;
}

ThermalTier getThermalTier(float temp) {
    if (!std::isfinite(temp) || temp < 0.0f) {
        return THERMAL_NORMAL;
    }
    if (temp < 40.0f) {
        return THERMAL_NORMAL;
    }
    if (temp < 44.0f) {
        return THERMAL_WARM;
    }
    if (temp <= 48.0f) {
        return THERMAL_HOT;
    }
    return THERMAL_CRITICAL;
}

int checkAndUpdateThermal() {
    const float temperature = readCPUTemperature();
    g_cpu_temp.store(temperature);
    const ThermalTier tier = getThermalTier(temperature);
    g_current_thermal_tier.store(static_cast<int>(tier));
    return static_cast<int>(tier);
}

std::string readDeviceTier() {
    constexpr const char* kCpuInfoPath = "/proc/cpuinfo";
    try {
        std::error_code filesystem_error;
        if (!std::filesystem::exists(kCpuInfoPath, filesystem_error) ||
            filesystem_error ||
            !std::filesystem::is_regular_file(kCpuInfoPath, filesystem_error) ||
            filesystem_error) {
            LOGW("CPU information file is unavailable; using minimum supported tier");
            return "TIER_MID";
        }

        std::ifstream cpu_info(kCpuInfoPath);
        if (!cpu_info.is_open()) {
            LOGW("Unable to open CPU information; using minimum supported tier");
            return "TIER_MID";
        }

        std::string contents;
        std::string line;
        while (std::getline(cpu_info, line)) {
            contents.append(line);
            contents.push_back('\n');
        }
        if (cpu_info.bad()) {
            LOGW("Unable to read CPU information; using minimum supported tier");
            return "TIER_MID";
        }

        const std::string cpu = uppercase(contents);
        if (cpu.find("8 GEN 1") != std::string::npos ||
            cpu.find("8 GEN 2") != std::string::npos ||
            cpu.find("8 GEN 3") != std::string::npos ||
            cpu.find("SM8350") != std::string::npos ||
            cpu.find("SM8450") != std::string::npos ||
            cpu.find("SM8475") != std::string::npos ||
            cpu.find("SM8550") != std::string::npos) {
            return "TIER_FLAGSHIP";
        }
        if (cpu.find("SNAPDRAGON 860") != std::string::npos ||
            cpu.find("SD 860") != std::string::npos ||
            cpu.find("SD860") != std::string::npos) {
            return "TIER_MID";
        }
        if (cpu.find("SNAPDRAGON 865") != std::string::npos ||
            cpu.find("SNAPDRAGON 870") != std::string::npos ||
            cpu.find("SD 865") != std::string::npos ||
            cpu.find("SD865") != std::string::npos ||
            cpu.find("SD 870") != std::string::npos ||
            cpu.find("SD870") != std::string::npos ||
            cpu.find("SM8250") != std::string::npos) {
            return "TIER_HIGH";
        }
        if (cpu.find("SNAPDRAGON 855") != std::string::npos ||
            cpu.find("SD 855") != std::string::npos ||
            cpu.find("SD855") != std::string::npos ||
            cpu.find("SM8150") != std::string::npos ||
            cpu.find("SM8150-AC") != std::string::npos) {
            return "TIER_MID";
        }
        if (cpu.find("SNAPDRAGON 660") != std::string::npos ||
            cpu.find("SNAPDRAGON 675") != std::string::npos ||
            cpu.find("SNAPDRAGON 720") != std::string::npos ||
            cpu.find("SD 660") != std::string::npos ||
            cpu.find("SD660") != std::string::npos ||
            cpu.find("SDM660") != std::string::npos ||
            cpu.find("SD 675") != std::string::npos ||
            cpu.find("SD675") != std::string::npos ||
            cpu.find("SDM675") != std::string::npos ||
            cpu.find("SD 720") != std::string::npos ||
            cpu.find("SD720") != std::string::npos ||
            cpu.find("SM6150") != std::string::npos ||
            cpu.find("SM7150") != std::string::npos) {
            return "TIER_LOW";
        }
        LOGW("CPU model was not recognized; using minimum supported tier");
    } catch (const std::exception& exception) {
        LOGW("Unable to detect device tier: %s", exception.what());
    } catch (...) {
        LOGW("Unable to detect device tier");
    }

    return "TIER_MID";
}
