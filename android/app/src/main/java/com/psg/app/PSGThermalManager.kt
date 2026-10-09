package com.psg.app

import android.content.Context
import android.os.Build
import android.os.HardwarePropertiesManager
import android.util.Log
import kotlinx.coroutines.CancellationException
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.Job
import kotlinx.coroutines.delay
import kotlinx.coroutines.isActive
import kotlinx.coroutines.launch
import java.io.File
import java.util.Locale

enum class ThermalTier {
    NORMAL,
    WARM,
    HOT,
    CRITICAL
}

class PSGThermalManager(context: Context) {
    private val appContext = context.applicationContext
    private var monitorJob: Job? = null

    var onThermalTierChanged: ((ThermalTier) -> Unit)? = null
    var onThermalStateChanged: ((ThermalTier, Float) -> Unit)? = null

    @Synchronized
    fun startMonitoring(scope: CoroutineScope) {
        monitorJob?.cancel()
        monitorJob = scope.launch(Dispatchers.IO) {
            var lastTier: ThermalTier? = null
            while (isActive) {
                val temperature = readTemperature()
                val tier = getThermalTier(temperature)
                if (tier != lastTier) {
                    lastTier = tier
                    try {
                        onThermalTierChanged?.invoke(tier)
                        onThermalStateChanged?.invoke(tier, temperature)
                    } catch (exception: CancellationException) {
                        throw exception
                    } catch (exception: Exception) {
                        Log.e(TAG, "Thermal-tier callback failed", exception)
                    }
                }
                delay(MONITOR_INTERVAL_MS)
            }
        }
    }

    fun readTemperature(): Float {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.N) {
            try {
                val manager = appContext.getSystemService(
                    Context.HARDWARE_PROPERTIES_SERVICE
                ) as? HardwarePropertiesManager
                if (manager == null) {
                    Log.w(TAG, "HardwarePropertiesManager is unavailable")
                } else {
                    val temperatures = manager.getDeviceTemperatures(
                        HardwarePropertiesManager.DEVICE_TEMPERATURE_CPU,
                        HardwarePropertiesManager.TEMPERATURE_CURRENT
                    )
                    val validTemperatures = temperatures.filter { it.isFinite() }
                    if (validTemperatures.isNotEmpty()) {
                        return validTemperatures.average().toFloat()
                    }
                    Log.w(TAG, "HardwarePropertiesManager returned no valid CPU temperatures")
                }
            } catch (exception: Exception) {
                Log.w(TAG, "Unable to read CPU temperature from HardwarePropertiesManager", exception)
            }
        }

        readTemperatureFile(CPU_TEMPERATURE_ZONE_0)?.let { return it }
        readTemperatureFile(CPU_TEMPERATURE_ZONE_1)?.let { return it }

        Log.w(TAG, "CPU temperature sensors unavailable; using safe default 35.0 C")
        return SAFE_TEMPERATURE_CELSIUS
    }

    fun getThermalTier(temp: Float): ThermalTier = when {
        !temp.isFinite() || temp < 40.0f -> ThermalTier.NORMAL
        temp < 44.0f -> ThermalTier.WARM
        temp <= 48.0f -> ThermalTier.HOT
        else -> ThermalTier.CRITICAL
    }

    fun getRecommendedSettings(
        tier: ThermalTier,
        deviceTier: String,
        consoleType: String
    ): EmulatorSettings {
        if (tier == ThermalTier.CRITICAL) {
            return pauseSettings()
        }

        val console = consoleType.trim().lowercase(Locale.ROOT)
        val normalizedDeviceTier = deviceTier.trim().uppercase(Locale.ROOT)
        if (tier == ThermalTier.NORMAL) {
            when (normalizedDeviceTier) {
                "TIER_HIGH" -> return when (console) {
                    "ps1", "ps2", "ps3" -> settings(60, 2, "vulkan")
                    "ps4" -> settings(30, 1, "vulkan")
                    else -> conservativeSettings()
                }
                "TIER_FLAGSHIP" -> return when (console) {
                    "ps1", "ps2", "ps3" -> settings(60, 3, "vulkan")
                    "ps4" -> settings(60, 2, "vulkan")
                    else -> conservativeSettings()
                }
                else -> return midTierSettings(ThermalTier.NORMAL, console)
            }
        }

        return midTierSettings(tier, console)
    }

    @Synchronized
    fun stopMonitoring() {
        monitorJob?.cancel()
        monitorJob = null
    }

    private fun readTemperatureFile(path: String): Float? {
        val file = File(path)
        return try {
            if (!file.exists() || !file.isFile) {
                return null
            }
            val rawTemperature = file.readText().trim().toFloatOrNull()
            if (rawTemperature == null || !rawTemperature.isFinite() || rawTemperature < 0.0f) {
                Log.w(TAG, "Invalid CPU temperature reading from $path")
                null
            } else {
                rawTemperature / 1000.0f
            }
        } catch (exception: Exception) {
            Log.w(TAG, "Unable to read CPU temperature from $path", exception)
            null
        }
    }

    private fun midTierSettings(tier: ThermalTier, console: String): EmulatorSettings {
        return when (tier) {
            ThermalTier.NORMAL -> when (console) {
                "ps1", "ps2" -> settings(60, 2, "opengl")
                "ps3", "ps4" -> settings(30, 1, "opengl")
                else -> conservativeSettings()
            }
            ThermalTier.WARM -> when (console) {
                "ps1", "ps2" -> settings(60, 1, "opengl")
                "ps3" -> settings(30, 1, "opengl")
                "ps4" -> settings(25, 1, "opengl")
                else -> conservativeSettings()
            }
            ThermalTier.HOT -> when (console) {
                "ps1", "ps2" -> settings(30, 1, "opengl")
                "ps3" -> settings(20, 1, "opengl")
                "ps4" -> pauseSettings()
                else -> conservativeSettings()
            }
            ThermalTier.CRITICAL -> pauseSettings()
        }
    }

    private fun settings(fps: Int, resolution: Int, backend: String) =
        EmulatorSettings(
            resolutionScale = resolution,
            fpsLimit = fps,
            graphicsBackend = backend
        )

    private fun pauseSettings() = EmulatorSettings(fpsLimit = 0)

    private fun conservativeSettings() = settings(30, 1, "opengl")

    companion object {
        private const val TAG = "PSGThermalManager"
        private const val MONITOR_INTERVAL_MS = 3_000L
        private const val SAFE_TEMPERATURE_CELSIUS = 35.0f
        private const val CPU_TEMPERATURE_ZONE_0 =
            "/sys/class/thermal/thermal_zone0/temp"
        private const val CPU_TEMPERATURE_ZONE_1 =
            "/sys/class/thermal/thermal_zone1/temp"
    }
}
