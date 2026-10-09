package com.psg.app

import android.content.Context
import android.util.Log
import android.view.SurfaceHolder
import android.view.SurfaceView
import io.flutter.plugin.common.BinaryMessenger
import io.flutter.plugin.common.MethodChannel
import io.flutter.plugin.platform.PlatformView
import io.flutter.plugin.platform.PlatformViewFactory
import io.flutter.plugin.common.StandardMessageCodec

class PSGGameSurfaceView(
    context: Context,
    private val emulatorBridge: PSGEmulatorBridge,
    messenger: BinaryMessenger
) : PlatformView, SurfaceHolder.Callback {
    private val surfaceView = SurfaceView(context)
    private val surfaceChannel = MethodChannel(messenger, SURFACE_CHANNEL)

    init {
        surfaceView.holder.addCallback(this)
    }

    override fun getView() = surfaceView

    override fun dispose() {
        surfaceView.holder.removeCallback(this)
        emulatorBridge.stopGame()
        emulatorBridge.releaseSurface()
    }

    override fun surfaceCreated(holder: SurfaceHolder) {
        surfaceView.isFocusable = true
        surfaceView.isFocusableInTouchMode = true
        surfaceView.requestFocus()
        val ready = emulatorBridge.initSurface(holder.surface)
        if (!ready) {
            Log.e(TAG, "Unable to initialize native game surface")
        }
        surfaceChannel.invokeMethod(
            "surfaceChanged",
            mapOf(
                "ready" to ready,
                "message" to if (ready) null else "Native game surface initialization failed"
            )
        )
    }

    override fun surfaceChanged(
        holder: SurfaceHolder,
        format: Int,
        width: Int,
        height: Int
    ) = Unit

    override fun surfaceDestroyed(holder: SurfaceHolder) {
        emulatorBridge.stopGame()
        val released = emulatorBridge.releaseSurface()
        surfaceChannel.invokeMethod(
            "surfaceChanged",
            mapOf(
                "ready" to false,
                "message" to if (released) null else "Native game surface release failed"
            )
        )
    }

    companion object {
        const val VIEW_TYPE = "com.psg.app/game_surface"
        const val SURFACE_CHANNEL = "com.psg.emulator/surface"
        private const val TAG = "PSGGameSurfaceView"
    }
}

class PSGGameSurfaceViewFactory(
    private val emulatorBridge: PSGEmulatorBridge,
    private val messenger: BinaryMessenger
) : PlatformViewFactory(StandardMessageCodec.INSTANCE) {
    override fun create(context: Context, viewId: Int, args: Any?): PlatformView =
        PSGGameSurfaceView(context, emulatorBridge, messenger)
}
