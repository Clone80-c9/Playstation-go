package com.psg.app

import android.content.Context
import android.content.SharedPreferences
import android.util.Log
import android.view.Surface
import org.json.JSONException
import org.json.JSONObject
import java.io.File
import java.util.Locale

data class PerformanceStats(
    val fps: Int,
    val cpuTemp: Float,
    val ramUsedMb: Int,
    val throttling: Boolean
)

data class EmulatorSettings(
    val resolutionScale: Int = 1,
    val fpsLimit: Int = 60,
    val graphicsBackend: String = "opengl",
    val textureFiltering: String = "bilinear",
    val skipBios: Boolean = true,
    val audioLatencyMs: Int = 64
)

class PSGEmulatorBridge(context: Context) {
    private val appContext = context.applicationContext
    private val preferences: SharedPreferences =
        appContext.getSharedPreferences(PREFERENCES_NAME, Context.MODE_PRIVATE)
    private var currentRomPath: String? = null

    private external fun nativeInit(surface: Surface, filesDir: String): Boolean
    private external fun nativeReleaseSurface(): Boolean
    private external fun nativeInitRenderer(surface: Surface, backendType: Int): Boolean
    private external fun nativeDestroyRenderer()
    private external fun nativeRenderFrame(): Boolean
    private external fun nativeLoadCore(corePath: String): String
    private external fun nativeLaunchGame(
        romPath: String,
        consoleType: String,
        biosPath: String,
        settingsJson: String
    ): String
    private external fun nativeStopGame(): Boolean
    private external fun nativeIsGameRunning(): Boolean
    private external fun nativeSaveState(slot: Int): Boolean
    private external fun nativeLoadState(slot: Int): Boolean
    private external fun nativeGetPerformanceStats(): String
    private external fun nativeApplySettings(settingsJson: String): Boolean
    private external fun nativeGetDeviceTier(): String?
    private external fun nativeProcessKeyEvent(
        keyCode: Int,
        action: Int,
        source: Int
    ): Boolean
    private external fun nativeProcessMotionEvent(
        axisX: Float,
        axisY: Float,
        axisZ: Float,
        axisRZ: Float,
        axisLT: Float,
        axisRT: Float,
        hatX: Float,
        hatY: Float
    ): Boolean
    private external fun nativeProcessVirtualInput(
        buttonId: Int,
        pressed: Boolean,
        axisX: Float,
        axisY: Float
    ): Boolean

    internal fun processNativeKeyEvent(
        keyCode: Int,
        action: Int,
        source: Int
    ): Boolean {
        if (!nativeLibraryLoaded) {
            Log.e(TAG, "Cannot process controller key: native library is unavailable")
            return false
        }
        return try {
            nativeProcessKeyEvent(keyCode, action, source)
        } catch (error: UnsatisfiedLinkError) {
            Log.e(TAG, "Native controller key handler is unavailable", error)
            false
        } catch (exception: RuntimeException) {
            Log.e(TAG, "Unable to process controller key", exception)
            false
        }
    }

    internal fun processNativeMotionEvent(
        axisX: Float,
        axisY: Float,
        axisZ: Float,
        axisRZ: Float,
        axisLT: Float,
        axisRT: Float,
        hatX: Float,
        hatY: Float
    ): Boolean {
        if (!nativeLibraryLoaded) {
            Log.e(TAG, "Cannot process controller motion: native library is unavailable")
            return false
        }
        return try {
            nativeProcessMotionEvent(
                axisX, axisY, axisZ, axisRZ, axisLT, axisRT, hatX, hatY
            )
        } catch (error: UnsatisfiedLinkError) {
            Log.e(TAG, "Native controller motion handler is unavailable", error)
            false
        } catch (exception: RuntimeException) {
            Log.e(TAG, "Unable to process controller motion", exception)
            false
        }
    }

    internal fun processNativeVirtualInput(
        buttonId: Int,
        pressed: Boolean,
        axisX: Float,
        axisY: Float
    ): Boolean {
        if (!nativeLibraryLoaded) {
            Log.e(TAG, "Cannot process virtual input: native library is unavailable")
            return false
        }
        return try {
            nativeProcessVirtualInput(buttonId, pressed, axisX, axisY)
        } catch (error: UnsatisfiedLinkError) {
            Log.e(TAG, "Native virtual input handler is unavailable", error)
            false
        } catch (exception: RuntimeException) {
            Log.e(TAG, "Unable to process virtual input", exception)
            false
        }
    }

    fun initSurface(surface: Surface): Boolean {
        if (!nativeLibraryLoaded) {
            Log.e(TAG, "Cannot initialize surface: native library is unavailable")
            return false
        }

        if (!surface.isValid) {
            Log.e(TAG, "Cannot initialize an invalid surface")
            return false
        }
        return try {
            nativeInit(surface, appContext.filesDir.absolutePath)
        } catch (exception: UnsatisfiedLinkError) {
            Log.e(TAG, "Native surface initialization is unavailable", exception)
            false
        } catch (exception: RuntimeException) {
            Log.e(TAG, "Native surface initialization failed", exception)
            false
        }
    }

    fun releaseSurface(): Boolean {
        if (!nativeLibraryLoaded) {
            Log.e(TAG, "Cannot release surface: native library is unavailable")
            return false
        }
        return try {
            nativeReleaseSurface()
        } catch (error: UnsatisfiedLinkError) {
            Log.e(TAG, "Native surface release is unavailable", error)
            false
        } catch (exception: RuntimeException) {
            Log.e(TAG, "Native surface release failed", exception)
            false
        }
    }

    fun initRenderer(surface: Surface, backendType: Int): Boolean {
        if (!nativeLibraryLoaded) {
            Log.e(TAG, "Cannot initialize renderer: native library is unavailable")
            return false
        }
        if (!surface.isValid) {
            Log.e(TAG, "Cannot initialize renderer with an invalid surface")
            return false
        }
        if (backendType !in RENDERER_BACKEND_OPENGL..RENDERER_BACKEND_VULKAN) {
            Log.e(TAG, "Unsupported renderer backend type: $backendType")
            return false
        }
        return try {
            nativeInitRenderer(surface, backendType)
        } catch (error: UnsatisfiedLinkError) {
            Log.e(TAG, "Native renderer initialization is unavailable", error)
            false
        } catch (exception: RuntimeException) {
            Log.e(TAG, "Native renderer initialization failed", exception)
            false
        }
    }

    fun renderFrame(): Boolean {
        if (!nativeLibraryLoaded) {
            Log.e(TAG, "Cannot render frame: native library is unavailable")
            return false
        }
        return try {
            nativeRenderFrame()
        } catch (error: UnsatisfiedLinkError) {
            Log.e(TAG, "Native renderer frame entry point is unavailable", error)
            false
        } catch (exception: RuntimeException) {
            Log.e(TAG, "Native renderer frame failed", exception)
            false
        }
    }

    fun destroyRenderer() {
        if (!nativeLibraryLoaded) {
            Log.e(TAG, "Cannot destroy renderer: native library is unavailable")
            return
        }
        try {
            nativeDestroyRenderer()
        } catch (error: UnsatisfiedLinkError) {
            Log.e(TAG, "Native renderer destroy entry point is unavailable", error)
        } catch (exception: RuntimeException) {
            Log.e(TAG, "Native renderer destroy failed", exception)
        }
    }

    fun loadCore(consoleType: String): Result<Unit> {
        if (!nativeLibraryLoaded) {
            return failure("Native library is unavailable")
        }
        val console = normalizeConsole(consoleType)
            ?: return failure("Unsupported console type: $consoleType")
        val coreName = CORE_NAMES[console]
            ?: return failure("Unsupported console type: $consoleType")
        val coreFile = File(appContext.filesDir, "cores/lib$coreName.so")
        if (!coreFile.exists() || !coreFile.isFile) {
            return failure(
                "Core not found. Please install the $consoleType core in PSG settings."
            )
        }

        return try {
            val response = nativeLoadCore(coreFile.absolutePath)
            if (response == "OK") {
                Result.success(Unit)
            } else {
                failure(response.ifBlank { "Native core loading failed without an error message" })
            }
        } catch (exception: UnsatisfiedLinkError) {
            Log.e(TAG, "Native core loading is unavailable", exception)
            Result.failure(exception)
        } catch (exception: RuntimeException) {
            Log.e(TAG, "Native core loading failed", exception)
            Result.failure(exception)
        }
    }

    fun launchGame(romPath: String, consoleType: String): Result<Unit> {
        if (!nativeLibraryLoaded) {
            return failure("Native library is unavailable")
        }
        if (romPath.isBlank()) {
            return failure("ROM path must not be empty")
        }
        val romFile = File(romPath)
        if (!romFile.exists() || !romFile.isFile) {
            return failure("ROM file does not exist: $romPath")
        }
        if (romFile.length() <= 0L) {
            return failure("ROM file is empty: $romPath")
        }
        val console = normalizeConsole(consoleType)
            ?: return failure("Unsupported console type: $consoleType")
        val settings = settingsFromPreferences()
        settings.validationError()?.let { return failure(it) }
        val biosFileName = when (console) {
            "ps1" -> "scph1001.bin"
            "ps2" -> "ps2-0230e-20080220.bin"
            else -> "$console.bin"
        }
        val biosPath = File(appContext.filesDir, "bios/$biosFileName")
        if (!settings.skipBios && (!biosPath.exists() || !biosPath.isFile)) {
            return failure("BIOS file not found: ${biosPath.absolutePath}")
        }

        return try {
            val response = JSONObject(
                nativeLaunchGame(
                    romFile.absolutePath,
                    console,
                    biosPath.absolutePath,
                    settings.toJson().toString()
                )
            )
            if (response.optString("status") == "launched") {
                currentRomPath = romFile.absolutePath
                Result.success(Unit)
            } else {
                val reason = response.optString("reason", "Native game launch failed")
                failure(reason.ifBlank { "Native game launch failed" })
            }
        } catch (exception: JSONException) {
            Log.e(TAG, "Native game launch returned invalid JSON", exception)
            Result.failure(exception)
        } catch (exception: UnsatisfiedLinkError) {
            Log.e(TAG, "Native game launch is unavailable", exception)
            Result.failure(exception)
        } catch (exception: RuntimeException) {
            Log.e(TAG, "Native game launch failed", exception)
            Result.failure(exception)
        }
    }

    fun stopGame(): Boolean {
        if (!nativeLibraryLoaded) {
            Log.e(TAG, "Cannot stop game: native library is unavailable")
            return false
        }
        return try {
            val stopped = nativeStopGame()
            if (stopped) {
                currentRomPath = null
            }
            stopped
        } catch (exception: UnsatisfiedLinkError) {
            Log.e(TAG, "Native game stop is unavailable", exception)
            false
        } catch (exception: RuntimeException) {
            Log.e(TAG, "Native game stop failed", exception)
            false
        }
    }

    fun isGameRunning(): Boolean {
        if (!nativeLibraryLoaded) {
            return false
        }
        return try {
            nativeIsGameRunning()
        } catch (error: UnsatisfiedLinkError) {
            Log.e(TAG, "Native game status is unavailable", error)
            false
        } catch (exception: RuntimeException) {
            Log.e(TAG, "Unable to query native game status", exception)
            false
        }
    }

    fun saveState(slot: Int): Boolean {
        if (!validateSlot(slot)) {
            Log.e(TAG, "Invalid save-state slot: $slot")
            return false
        }
        if (!nativeLibraryLoaded) {
            Log.e(TAG, "Cannot save state: native library is unavailable")
            return false
        }
        return try {
            nativeSaveState(slot)
        } catch (exception: UnsatisfiedLinkError) {
            Log.e(TAG, "Native save-state operation is unavailable", exception)
            false
        } catch (exception: RuntimeException) {
            Log.e(TAG, "Native save-state operation failed", exception)
            false
        }
    }

    fun loadState(slot: Int): Boolean {
        if (!validateSlot(slot)) {
            Log.e(TAG, "Invalid save-state slot: $slot")
            return false
        }
        if (!nativeLibraryLoaded) {
            Log.e(TAG, "Cannot load state: native library is unavailable")
            return false
        }
        val romPath = currentRomPath
        if (romPath.isNullOrBlank()) {
            Log.e(TAG, "Cannot load state: no game is currently selected")
            return false
        }
        val gameId = File(romPath).nameWithoutExtension
        if (gameId.isBlank()) {
            Log.e(TAG, "Cannot determine game ID for save-state lookup")
            return false
        }
        val stateFile = File(
            appContext.filesDir,
            "psg/saves/$gameId/slot_$slot.state"
        )
        if (!stateFile.exists() || !stateFile.isFile) {
            Log.e(TAG, "Save-state file not found: ${stateFile.absolutePath}")
            return false
        }

        return try {
            nativeLoadState(slot)
        } catch (exception: UnsatisfiedLinkError) {
            Log.e(TAG, "Native load-state operation is unavailable", exception)
            false
        } catch (exception: RuntimeException) {
            Log.e(TAG, "Native load-state operation failed", exception)
            false
        }
    }

    fun getPerformanceStats(): PerformanceStats? {
        if (!nativeLibraryLoaded) {
            Log.e(TAG, "Cannot get performance stats: native library is unavailable")
            return null
        }
        return try {
            val json = JSONObject(nativeGetPerformanceStats())
            PerformanceStats(
                fps = json.getInt("fps"),
                cpuTemp = json.getDouble("cpu_temp").toFloat(),
                ramUsedMb = json.getInt("ram_used_mb"),
                throttling = json.getBoolean("throttling")
            )
        } catch (exception: JSONException) {
            Log.e(TAG, "Unable to parse native performance statistics", exception)
            null
        } catch (exception: UnsatisfiedLinkError) {
            Log.e(TAG, "Native performance statistics are unavailable", exception)
            null
        } catch (exception: RuntimeException) {
            Log.e(TAG, "Unable to retrieve native performance statistics", exception)
            null
        }
    }

    fun applySettings(settings: EmulatorSettings): Boolean {
        if (!nativeLibraryLoaded) {
            Log.e(TAG, "Cannot apply settings: native library is unavailable")
            return false
        }
        val validationError = settings.validationError()
        if (validationError != null) {
            Log.e(TAG, "Invalid emulator settings: $validationError")
            return false
        }
        return try {
            nativeApplySettings(settings.toJson().toString())
        } catch (exception: JSONException) {
            Log.e(TAG, "Unable to encode emulator settings", exception)
            false
        } catch (exception: UnsatisfiedLinkError) {
            Log.e(TAG, "Native settings application is unavailable", exception)
            false
        } catch (exception: RuntimeException) {
            Log.e(TAG, "Native settings application failed", exception)
            false
        }
    }

    fun isSkipBiosEnabled(): Boolean =
        preferences.getBoolean(KEY_SKIP_BIOS, true)

    fun setSkipBiosEnabled(skipBios: Boolean) {
        preferences.edit().putBoolean(KEY_SKIP_BIOS, skipBios).apply()
    }

    fun getDeviceTier(): String {
        if (!nativeLibraryLoaded) {
            Log.e(TAG, "Cannot detect device tier: native library is unavailable")
            return DEFAULT_DEVICE_TIER
        }
        return try {
            nativeGetDeviceTier()?.takeIf { it in DEVICE_TIERS } ?: run {
                Log.e(TAG, "Native device-tier detection returned an invalid result")
                DEFAULT_DEVICE_TIER
            }
        } catch (exception: UnsatisfiedLinkError) {
            Log.e(TAG, "Native device-tier detection is unavailable", exception)
            DEFAULT_DEVICE_TIER
        } catch (exception: RuntimeException) {
            Log.e(TAG, "Native device-tier detection failed", exception)
            DEFAULT_DEVICE_TIER
        }
    }

    private fun settingsFromPreferences(): EmulatorSettings = EmulatorSettings(
        resolutionScale = preferences.getInt(KEY_RESOLUTION_SCALE, 1),
        fpsLimit = preferences.getInt(KEY_FPS_LIMIT, 60),
        graphicsBackend = preferences.getString(KEY_GRAPHICS_BACKEND, "opengl")
            ?: "opengl",
        textureFiltering = preferences.getString(KEY_TEXTURE_FILTERING, "bilinear")
            ?: "bilinear",
        skipBios = isSkipBiosEnabled(),
        audioLatencyMs = preferences.getInt(KEY_AUDIO_LATENCY_MS, 64)
    )

    private fun EmulatorSettings.toJson(): JSONObject = JSONObject()
        .put("resolution_scale", resolutionScale)
        .put("fps_limit", fpsLimit)
        .put("graphics_backend", graphicsBackend)
        .put("texture_filtering", textureFiltering)
        .put("skip_bios", skipBios)
        .put("audio_latency_ms", audioLatencyMs)

    private fun EmulatorSettings.validationError(): String? {
        if (resolutionScale !in 1..8) {
            return "resolutionScale must be between 1 and 8"
        }
        if (fpsLimit !in 0..240) {
            return "fpsLimit must be between 0 and 240"
        }
        if (graphicsBackend !in GRAPHICS_BACKENDS) {
            return "unsupported graphicsBackend"
        }
        if (textureFiltering !in TEXTURE_FILTERS) {
            return "unsupported textureFiltering"
        }
        if (audioLatencyMs !in 16..512) {
            return "audioLatencyMs must be between 16 and 512"
        }
        return null
    }

    private fun normalizeConsole(consoleType: String): String? =
        consoleType.trim().lowercase(Locale.ROOT).takeIf { it in CORE_NAMES }

    private fun validateSlot(slot: Int): Boolean = slot in 0..9

    private fun failure(message: String): Result<Unit> {
        Log.e(TAG, message)
        return Result.failure(IllegalStateException(message))
    }

    companion object {
        private const val TAG = "PSGEmulatorBridge"
        private const val PREFERENCES_NAME = "psg_settings"
        private const val KEY_RESOLUTION_SCALE = "resolutionScale"
        private const val KEY_FPS_LIMIT = "fpsLimit"
        private const val KEY_GRAPHICS_BACKEND = "graphicsBackend"
        private const val KEY_TEXTURE_FILTERING = "textureFiltering"
        private const val KEY_SKIP_BIOS = "skipBios"
        private const val KEY_AUDIO_LATENCY_MS = "audioLatencyMs"
        private const val DEFAULT_DEVICE_TIER = "TIER_MID"
        private const val RENDERER_BACKEND_OPENGL = 0
        private const val RENDERER_BACKEND_VULKAN = 1
        private val CORE_NAMES = mapOf(
            "ps1" to "pcsx_rearmed",
            "ps2" to "play",
            "ps3" to "rpcs3_lite",
            "ps4" to "spine_lite"
        )
        private val GRAPHICS_BACKENDS = setOf("opengl", "vulkan")
        private val TEXTURE_FILTERS = setOf("nearest", "bilinear", "trilinear")
        private val DEVICE_TIERS = setOf(
            "TIER_LOW",
            "TIER_MID",
            "TIER_HIGH",
            "TIER_FLAGSHIP"
        )
        private val nativeLibraryLoaded: Boolean

        init {
            nativeLibraryLoaded = try {
                System.loadLibrary("psg_bridge")
                Log.i(TAG, "Loaded native library psg_bridge")
                true
            } catch (error: UnsatisfiedLinkError) {
                Log.e(TAG, "Unable to load native library psg_bridge", error)
                false
            } catch (exception: SecurityException) {
                Log.e(TAG, "Permission denied while loading psg_bridge", exception)
                false
            }
        }
    }
}
