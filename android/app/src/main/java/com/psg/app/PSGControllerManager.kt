package com.psg.app

import android.content.Context
import android.hardware.input.InputManager
import android.util.Log
import android.view.InputDevice
import android.view.KeyEvent
import android.view.MotionEvent
import kotlin.math.abs

data class ControllerInfo(
    val name: String,
    val deviceId: Int,
    val type: String,
    val isConnected: Boolean
)

class PSGControllerManager(
    context: Context,
    private val emulatorBridge: PSGEmulatorBridge
) {
    private val appContext = context.applicationContext

    fun getConnectedControllers(): List<ControllerInfo> {
        return try {
            InputDevice.getDeviceIds().toList().mapNotNull { deviceId ->
                val device = InputDevice.getDevice(deviceId)
                if (device == null || !isController(device.sources)) {
                    return@mapNotNull null
                }
                ControllerInfo(
                    name = device.name,
                    deviceId = device.id,
                    type = identifyControllerType(device.vendorId, device.productId),
                    isConnected = isDeviceConnected(device.id)
                )
            }.filter { it.isConnected }
        } catch (exception: RuntimeException) {
            Log.e(TAG, "Unable to enumerate connected controllers", exception)
            emptyList()
        }
    }

    fun processKeyEvent(event: KeyEvent): Boolean {
        if ((event.source and InputDevice.SOURCE_GAMEPAD) !=
            InputDevice.SOURCE_GAMEPAD
        ) {
            return false
        }
        when (event.keyCode) {
            KeyEvent.KEYCODE_BUTTON_A,
            KeyEvent.KEYCODE_BUTTON_B,
            KeyEvent.KEYCODE_BUTTON_X,
            KeyEvent.KEYCODE_BUTTON_Y,
            KeyEvent.KEYCODE_BUTTON_L1,
            KeyEvent.KEYCODE_BUTTON_R1,
            KeyEvent.KEYCODE_BUTTON_L2,
            KeyEvent.KEYCODE_BUTTON_R2,
            KeyEvent.KEYCODE_BUTTON_THUMBL,
            KeyEvent.KEYCODE_BUTTON_THUMBR,
            KeyEvent.KEYCODE_BUTTON_START,
            KeyEvent.KEYCODE_BUTTON_SELECT -> Unit
            else -> return false
        }
        return emulatorBridge.processNativeKeyEvent(
            event.keyCode,
            event.action,
            event.source
        )
    }

    fun processMotionEvent(event: MotionEvent): Boolean {
        if ((event.source and InputDevice.SOURCE_JOYSTICK) !=
            InputDevice.SOURCE_JOYSTICK
        ) {
            return false
        }

        return emulatorBridge.processNativeMotionEvent(
            deadzone(event.getAxisValue(MotionEvent.AXIS_X)),
            deadzone(event.getAxisValue(MotionEvent.AXIS_Y)),
            deadzone(event.getAxisValue(MotionEvent.AXIS_Z)),
            deadzone(event.getAxisValue(MotionEvent.AXIS_RZ)),
            normalizedTrigger(event.getAxisValue(MotionEvent.AXIS_LTRIGGER)),
            normalizedTrigger(event.getAxisValue(MotionEvent.AXIS_RTRIGGER)),
            event.getAxisValue(MotionEvent.AXIS_HAT_X),
            event.getAxisValue(MotionEvent.AXIS_HAT_Y)
        )
    }

    fun processVirtualButtonEvent(buttonId: Int, pressed: Boolean) {
        if (buttonId !in 0..15) {
            Log.e(TAG, "Invalid virtual button ID: $buttonId")
            return
        }
        if (!emulatorBridge.processNativeVirtualInput(
                buttonId, pressed, 0.0f, 0.0f
            )
        ) {
            Log.e(TAG, "Unable to send virtual button event for ID: $buttonId")
        }
    }

    fun processVirtualStickEvent(stickId: Int, x: Float, y: Float) {
        if (stickId !in 16..17) {
            Log.e(TAG, "Invalid virtual stick ID: $stickId")
            return
        }
        if (!x.isFinite() || !y.isFinite()) {
            Log.e(TAG, "Virtual stick axes must be finite")
            return
        }
        if (!emulatorBridge.processNativeVirtualInput(
                stickId,
                true,
                deadzone(x, VIRTUAL_STICK_DEADZONE),
                deadzone(y, VIRTUAL_STICK_DEADZONE)
            )
        ) {
            Log.e(TAG, "Unable to send virtual stick event for ID: $stickId")
        }
    }

    private fun isDeviceConnected(deviceId: Int): Boolean {
        val inputManager = appContext.getSystemService(Context.INPUT_SERVICE)
            as? InputManager
        if (inputManager == null) {
            Log.w(TAG, "InputManager unavailable; cannot confirm controller connection")
            return false
        }
        return inputManager.inputDeviceIds.any { it == deviceId }
    }

    private fun identifyControllerType(vendorId: Int, productId: Int): String =
        when (vendorId) {
            SONY_VENDOR_ID -> when (productId) {
                DUALSHOCK_4_PRODUCT_ID,
                DUALSHOCK_4_V2_PRODUCT_ID -> "dualshock4"
                DUALSENSE_PRODUCT_ID -> "dualsense"
                else -> "sony"
            }
            XBOX_VENDOR_ID -> "xbox"
            else -> "generic"
        }

    private fun isController(sources: Int): Boolean =
        (sources and InputDevice.SOURCE_GAMEPAD) == InputDevice.SOURCE_GAMEPAD ||
            (sources and InputDevice.SOURCE_JOYSTICK) ==
            InputDevice.SOURCE_JOYSTICK

    private fun deadzone(value: Float, threshold: Float = STICK_DEADZONE): Float {
        if (!value.isFinite()) {
            return 0.0f
        }
        val normalized = value.coerceIn(-1.0f, 1.0f)
        return if (abs(normalized) < threshold) 0.0f else normalized
    }

    private fun normalizedTrigger(value: Float): Float =
        if (value.isFinite()) value.coerceIn(0.0f, 1.0f) else 0.0f

    companion object {
        private const val TAG = "PSGControllerManager"
        private const val STICK_DEADZONE = 0.15f
        private const val VIRTUAL_STICK_DEADZONE = 0.1f
        private const val SONY_VENDOR_ID = 0x054C
        private const val XBOX_VENDOR_ID = 0x045E
        private const val DUALSHOCK_4_PRODUCT_ID = 0x05C4
        private const val DUALSHOCK_4_V2_PRODUCT_ID = 0x09CC
        private const val DUALSENSE_PRODUCT_ID = 0x0CE6
    }
}
