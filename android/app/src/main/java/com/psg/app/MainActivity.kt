package com.psg.app

import android.util.Log
import android.view.KeyEvent
import android.view.MotionEvent
import io.flutter.embedding.android.FlutterActivity
import io.flutter.embedding.engine.FlutterEngine
import io.flutter.plugin.common.MethodCall
import io.flutter.plugin.common.MethodChannel
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.SupervisorJob
import kotlinx.coroutines.cancel

class MainActivity : FlutterActivity() {
    private val emulatorBridge: PSGEmulatorBridge by lazy(LazyThreadSafetyMode.NONE) {
        PSGEmulatorBridge(applicationContext)
    }
    private val thermalManager: PSGThermalManager by lazy(LazyThreadSafetyMode.NONE) {
        PSGThermalManager(applicationContext)
    }
    private val controllerManager: PSGControllerManager by lazy(LazyThreadSafetyMode.NONE) {
        PSGControllerManager(applicationContext, emulatorBridge)
    }
    private val thermalScope = CoroutineScope(
        SupervisorJob() + Dispatchers.Main.immediate
    )

    private var thermalMethodChannel: MethodChannel? = null

    override fun configureFlutterEngine(flutterEngine: FlutterEngine) {
        super.configureFlutterEngine(flutterEngine)

        val messenger = flutterEngine.dartExecutor.binaryMessenger
        MethodChannel(messenger, EMULATOR_CHANNEL).setMethodCallHandler { call, result ->
            try {
                handleEmulatorCall(call, result)
            } catch (exception: Exception) {
                Log.e(TAG, "Emulator channel method ${call.method} failed", exception)
                result.error("EMULATOR_ERROR", exception.message ?: "Emulator operation failed", null)
            } catch (error: LinkageError) {
                Log.e(TAG, "Emulator channel method ${call.method} linkage failed", error)
                result.error("NATIVE_LINK_ERROR", error.message ?: "Native operation unavailable", null)
            }
        }

        MethodChannel(messenger, CONTROLLER_CHANNEL).setMethodCallHandler { call, result ->
            try {
                handleControllerCall(call, result)
            } catch (exception: Exception) {
                Log.e(TAG, "Controller channel method ${call.method} failed", exception)
                result.error("CONTROLLER_ERROR", exception.message ?: "Controller operation failed", null)
            } catch (error: LinkageError) {
                Log.e(TAG, "Controller channel method ${call.method} linkage failed", error)
                result.error("NATIVE_LINK_ERROR", error.message ?: "Native operation unavailable", null)
            }
        }

        thermalMethodChannel = MethodChannel(messenger, THERMAL_CHANNEL)
        thermalMethodChannel?.setMethodCallHandler { call, result ->
            try {
                handleThermalCall(call, result)
            } catch (exception: Exception) {
                Log.e(TAG, "Thermal channel method ${call.method} failed", exception)
                result.error("THERMAL_ERROR", exception.message ?: "Thermal operation failed", null)
            } catch (error: LinkageError) {
                Log.e(TAG, "Thermal channel method ${call.method} linkage failed", error)
                result.error("NATIVE_LINK_ERROR", error.message ?: "Native operation unavailable", null)
            }
        }
    }

    private fun handleEmulatorCall(call: MethodCall, result: MethodChannel.Result) {
        when (call.method) {
            "loadCore" -> {
                val consoleType = call.requiredStringArgument("consoleType")
                emulatorBridge.loadCore(consoleType).fold(
                    onSuccess = { result.success("OK") },
                    onFailure = { result.error("LOAD_CORE_FAILED", it.message, null) }
                )
            }
            "launchGame" -> {
                val romPath = call.requiredStringArgument("romPath")
                val consoleType = call.requiredStringArgument("consoleType")
                emulatorBridge.launchGame(romPath, consoleType).fold(
                    onSuccess = { result.success("OK") },
                    onFailure = { result.error("LAUNCH_GAME_FAILED", it.message, null) }
                )
            }
            "stopGame" -> result.success(emulatorBridge.stopGame())
            "saveState" -> result.success(
                emulatorBridge.saveState(
                    call.argumentMap().requiredInt("slot")
                )
            )
            "loadState" -> result.success(
                emulatorBridge.loadState(
                    call.argumentMap().requiredInt("slot")
                )
            )
            "getStats" -> {
                val stats = emulatorBridge.getPerformanceStats()
                result.success(
                    stats?.let {
                        mapOf(
                            "fps" to it.fps,
                            "cpuTemp" to it.cpuTemp,
                            "ramUsedMb" to it.ramUsedMb,
                            "throttling" to it.throttling
                        )
                    }
                )
            }
            "applySettings" -> {
                val arguments = call.argumentMap()
                val settings = EmulatorSettings(
                    resolutionScale = arguments.optionalInt(
                        "resolutionScale", EmulatorSettings().resolutionScale
                    ),
                    fpsLimit = arguments.optionalInt(
                        "fpsLimit", EmulatorSettings().fpsLimit
                    ),
                    graphicsBackend = arguments.optionalString(
                        "graphicsBackend", EmulatorSettings().graphicsBackend
                    ),
                    textureFiltering = arguments.optionalString(
                        "textureFiltering", EmulatorSettings().textureFiltering
                    ),
                    skipBios = arguments.optionalBoolean(
                        "skipBios", EmulatorSettings().skipBios
                    ),
                    audioLatencyMs = arguments.optionalInt(
                        "audioLatencyMs", EmulatorSettings().audioLatencyMs
                    )
                )
                result.success(emulatorBridge.applySettings(settings))
            }
            "getDeviceTier" -> result.success(emulatorBridge.getDeviceTier())
            else -> result.notImplemented()
        }
    }

    private fun handleControllerCall(call: MethodCall, result: MethodChannel.Result) {
        when (call.method) {
            "getControllers" -> {
                val controllers = controllerManager.getConnectedControllers().map {
                    mapOf(
                        "name" to it.name,
                        "deviceId" to it.deviceId,
                        "type" to it.type,
                        "isConnected" to it.isConnected
                    )
                }
                result.success(controllers)
            }
            "virtualButton" -> {
                val arguments = call.argumentMap()
                controllerManager.processVirtualButtonEvent(
                    arguments.requiredInt("buttonId"),
                    arguments.requiredBoolean("pressed")
                )
                result.success(true)
            }
            "virtualStick" -> {
                val arguments = call.argumentMap()
                controllerManager.processVirtualStickEvent(
                    arguments.requiredInt("stickId"),
                    arguments.requiredFloat("x"),
                    arguments.requiredFloat("y")
                )
                result.success(true)
            }
            else -> result.notImplemented()
        }
    }

    private fun handleThermalCall(call: MethodCall, result: MethodChannel.Result) {
        when (call.method) {
            "startMonitoring" -> {
                thermalManager.onThermalStateChanged = { tier, temperature ->
                    Log.i(TAG, "Thermal tier changed to ${tier.name} at ${temperature} C")
                    thermalMethodChannel?.invokeMethod(
                        "onThermalChange",
                        mapOf("tier" to tier.name, "temp" to temperature)
                    )
                }
                thermalManager.startMonitoring(thermalScope)
                result.success(true)
            }
            "stopMonitoring" -> {
                thermalManager.stopMonitoring()
                thermalManager.onThermalStateChanged = null
                result.success(true)
            }
            "getTemperature" -> result.success(thermalManager.readTemperature())
            else -> result.notImplemented()
        }
    }

    override fun onKeyDown(keyCode: Int, event: KeyEvent): Boolean {
        return try {
            if (controllerManager.processKeyEvent(event)) {
                true
            } else {
                super.onKeyDown(keyCode, event)
            }
        } catch (exception: Exception) {
            Log.e(TAG, "Unable to dispatch controller key-down event", exception)
            super.onKeyDown(keyCode, event)
        }
    }

    override fun onKeyUp(keyCode: Int, event: KeyEvent): Boolean {
        return try {
            if (controllerManager.processKeyEvent(event)) {
                true
            } else {
                super.onKeyUp(keyCode, event)
            }
        } catch (exception: Exception) {
            Log.e(TAG, "Unable to dispatch controller key-up event", exception)
            super.onKeyUp(keyCode, event)
        }
    }

    override fun onGenericMotionEvent(event: MotionEvent): Boolean {
        return try {
            if (controllerManager.processMotionEvent(event)) {
                true
            } else {
                super.onGenericMotionEvent(event)
            }
        } catch (exception: Exception) {
            Log.e(TAG, "Unable to dispatch controller motion event", exception)
            super.onGenericMotionEvent(event)
        }
    }

    override fun onDestroy() {
        try {
            thermalManager.stopMonitoring()
            thermalManager.onThermalStateChanged = null
            thermalScope.cancel()
        } catch (exception: Exception) {
            Log.e(TAG, "Unable to stop thermal monitoring", exception)
        }
        try {
            emulatorBridge.stopGame()
        } catch (exception: Exception) {
            Log.e(TAG, "Unable to stop emulator", exception)
        }
        try {
            emulatorBridge.destroyRenderer()
        } catch (exception: Exception) {
            Log.e(TAG, "Unable to destroy renderer", exception)
        } finally {
            super.onDestroy()
        }
    }

    private fun MethodCall.argumentMap(): Map<*, *> =
        arguments as? Map<*, *>
            ?: throw IllegalArgumentException("Expected a map of method arguments")

    private fun MethodCall.requiredStringArgument(name: String): String =
        argumentMap().requiredString(name)

    private fun Map<*, *>.requiredString(name: String): String =
        this[name] as? String
            ?: throw IllegalArgumentException("Missing or invalid '$name' argument")

    private fun Map<*, *>.requiredInt(name: String): Int =
        this[name].toExactInt(name)

    private fun Map<*, *>.requiredFloat(name: String): Float =
        (this[name] as? Number)?.toFloat()?.takeIf { it.isFinite() }
            ?: throw IllegalArgumentException("Missing or invalid '$name' argument")

    private fun Map<*, *>.requiredBoolean(name: String): Boolean =
        this[name] as? Boolean
            ?: throw IllegalArgumentException("Missing or invalid '$name' argument")

    private fun Map<*, *>.optionalInt(name: String, default: Int): Int =
        this[name]?.toExactInt(name) ?: default

    private fun Map<*, *>.optionalString(name: String, default: String): String =
        this[name]?.let {
            it as? String
                ?: throw IllegalArgumentException("Invalid '$name' argument")
        } ?: default

    private fun Map<*, *>.optionalBoolean(name: String, default: Boolean): Boolean =
        this[name]?.let {
            it as? Boolean
                ?: throw IllegalArgumentException("Invalid '$name' argument")
        } ?: default

    private fun Any?.toExactInt(name: String): Int {
        val number = this as? Number
            ?: throw IllegalArgumentException("Missing or invalid '$name' argument")
        val value = number.toDouble()
        if (!value.isFinite() || value % 1.0 != 0.0 ||
            value < Int.MIN_VALUE || value > Int.MAX_VALUE
        ) {
            throw IllegalArgumentException("Invalid '$name' argument")
        }
        return value.toInt()
    }

    companion object {
        private const val TAG = "PSGMainActivity"
        private const val EMULATOR_CHANNEL = "com.psg.emulator/core"
        private const val CONTROLLER_CHANNEL = "com.psg.emulator/controller"
        private const val THERMAL_CHANNEL = "com.psg.emulator/thermal"
    }
}
