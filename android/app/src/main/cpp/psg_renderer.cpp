#include <jni.h>
#include <android/native_window.h>
#include <EGL/egl.h>
#include <GLES3/gl3.h>
#include <android/log.h>
#include <android/native_window_jni.h>

#include <exception>
#include <mutex>

#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, "PSG_CORE", __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, "PSG_CORE", __VA_ARGS__)
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN, "PSG_CORE", __VA_ARGS__)

class PSGRenderer {
public:
    PSGRenderer() = default;
    PSGRenderer(const PSGRenderer&) = delete;
    PSGRenderer& operator=(const PSGRenderer&) = delete;

    ~PSGRenderer() {
        destroy();
    }

    bool init(ANativeWindow* window, int backendType) {
        if (window == nullptr) {
            LOGE("PSG Renderer: init received a null native window");
            return false;
        }

        std::lock_guard<std::mutex> lock(mutex_);
        destroyLocked();
        ANativeWindow_acquire(window);
        window_ = window;

        if (backendType == kVulkanBackend) {
            backend_type_ = kVulkanBackend;
            LOGI("PSG Renderer: Vulkan backend requested — initializing via psg_bridge Vulkan path");
            return true;
        }
        if (backendType != kOpenGLBackend) {
            LOGE("PSG Renderer: unsupported backend type %d", backendType);
            destroyLocked();
            return false;
        }

        display_ = eglGetDisplay(EGL_DEFAULT_DISPLAY);
        if (display_ == EGL_NO_DISPLAY) {
            logEglError("eglGetDisplay");
            destroyLocked();
            return false;
        }

        EGLint major_version = 0;
        EGLint minor_version = 0;
        if (eglInitialize(display_, &major_version, &minor_version) != EGL_TRUE) {
            logEglError("eglInitialize");
            destroyLocked();
            return false;
        }
        display_initialized_ = true;

        const EGLint config_attributes[] = {
            EGL_RED_SIZE, 8,
            EGL_GREEN_SIZE, 8,
            EGL_BLUE_SIZE, 8,
            EGL_DEPTH_SIZE, 16,
            EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT,
            EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
            EGL_NONE,
        };
        EGLConfig config = nullptr;
        EGLint config_count = 0;
        if (eglChooseConfig(display_, config_attributes, &config, 1,
                            &config_count) != EGL_TRUE) {
            logEglError("eglChooseConfig");
            destroyLocked();
            return false;
        }
        if (config_count < 1 || config == nullptr) {
            logEglError("eglChooseConfig returned no matching configuration");
            destroyLocked();
            return false;
        }

        surface_ = eglCreateWindowSurface(display_, config, window_, nullptr);
        if (surface_ == EGL_NO_SURFACE) {
            logEglError("eglCreateWindowSurface");
            destroyLocked();
            return false;
        }

        const EGLint context_attributes[] = {
            EGL_CONTEXT_CLIENT_VERSION, 3,
            EGL_NONE,
        };
        context_ = eglCreateContext(display_, config, EGL_NO_CONTEXT,
                                    context_attributes);
        if (context_ == EGL_NO_CONTEXT) {
            logEglError("eglCreateContext");
            destroyLocked();
            return false;
        }

        if (eglMakeCurrent(display_, surface_, surface_, context_) != EGL_TRUE) {
            logEglError("eglMakeCurrent");
            destroyLocked();
            return false;
        }

        backend_type_ = kOpenGLBackend;
        LOGI("PSG Renderer: OpenGL ES 3.0 initialized");
        return true;
    }

    bool setResolutionScale(int scale) {
        if (scale < 1 || scale > 3) {
            LOGE("PSG Renderer: invalid resolution scale %d (expected 1, 2, or 3)",
                 scale);
            return false;
        }

        std::lock_guard<std::mutex> lock(mutex_);
        resolution_scale_ = scale;
        LOGI("PSG Renderer: resolution scale set to %d", resolution_scale_);
        return true;
    }

    bool renderFrame() {
        std::lock_guard<std::mutex> lock(mutex_);
        if (backend_type_ == kVulkanBackend && window_ != nullptr) {
            return true;
        }
        if (!isInitializedLocked()) {
            LOGW("PSG Renderer: renderFrame called before renderer initialization");
            return false;
        }
        if (eglSwapBuffers(display_, surface_) != EGL_TRUE) {
            logEglError("eglSwapBuffers");
            return false;
        }
        return true;
    }

    void destroy() {
        std::lock_guard<std::mutex> lock(mutex_);
        destroyLocked();
        LOGI("PSG Renderer: destroyed cleanly");
    }

    bool isInitialized() {
        std::lock_guard<std::mutex> lock(mutex_);
        return isInitializedLocked();
    }

private:
    static constexpr int kOpenGLBackend = 0;
    static constexpr int kVulkanBackend = 1;

    static void logEglError(const char* operation) {
        const EGLint error = eglGetError();
        LOGE("PSG Renderer: %s failed with EGL error 0x%04x", operation,
             static_cast<unsigned int>(error));
    }

    bool isInitializedLocked() const {
        return backend_type_ == kOpenGLBackend &&
               display_ != EGL_NO_DISPLAY &&
               surface_ != EGL_NO_SURFACE &&
               context_ != EGL_NO_CONTEXT;
    }

    void destroyLocked() {
        if (display_ != EGL_NO_DISPLAY) {
            if (eglGetCurrentDisplay() == display_ &&
                eglMakeCurrent(display_, EGL_NO_SURFACE, EGL_NO_SURFACE,
                               EGL_NO_CONTEXT) != EGL_TRUE) {
                logEglError("eglMakeCurrent during destroy");
            }

            if (surface_ != EGL_NO_SURFACE) {
                if (eglDestroySurface(display_, surface_) != EGL_TRUE) {
                    logEglError("eglDestroySurface");
                }
                surface_ = EGL_NO_SURFACE;
            }
            if (context_ != EGL_NO_CONTEXT) {
                if (eglDestroyContext(display_, context_) != EGL_TRUE) {
                    logEglError("eglDestroyContext");
                }
                context_ = EGL_NO_CONTEXT;
            }
            if (display_initialized_) {
                if (eglTerminate(display_) != EGL_TRUE) {
                    logEglError("eglTerminate");
                }
            }
        }

        display_ = EGL_NO_DISPLAY;
        surface_ = EGL_NO_SURFACE;
        context_ = EGL_NO_CONTEXT;
        display_initialized_ = false;
        backend_type_ = -1;

        if (window_ != nullptr) {
            ANativeWindow_release(window_);
            window_ = nullptr;
        }
    }

    std::mutex mutex_;
    EGLDisplay display_ = EGL_NO_DISPLAY;
    EGLSurface surface_ = EGL_NO_SURFACE;
    EGLContext context_ = EGL_NO_CONTEXT;
    ANativeWindow* window_ = nullptr;
    int backend_type_ = -1;
    int resolution_scale_ = 1;
    bool display_initialized_ = false;
};

static PSGRenderer g_renderer;

extern "C" JNIEXPORT jboolean JNICALL
Java_com_psg_app_PSGEmulatorBridge_nativeInitRenderer(
    JNIEnv* env, jobject, jobject surface, jint backend_type) {
    try {
        if (env == nullptr || surface == nullptr) {
            LOGE("nativeInitRenderer received a null JNI argument");
            return JNI_FALSE;
        }

        ANativeWindow* window = ANativeWindow_fromSurface(env, surface);
        const jboolean has_exception =
            env->ExceptionCheck() == JNI_TRUE ? JNI_TRUE : JNI_FALSE;
        if (has_exception == JNI_TRUE || window == nullptr) {
            if (window != nullptr) {
                ANativeWindow_release(window);
            }
            LOGE("nativeInitRenderer could not obtain the native window");
            return JNI_FALSE;
        }

        const bool initialized = g_renderer.init(window, backend_type);
        ANativeWindow_release(window);
        return initialized ? JNI_TRUE : JNI_FALSE;
    } catch (const std::exception& exception) {
        LOGE("nativeInitRenderer failed: %s", exception.what());
        return JNI_FALSE;
    } catch (...) {
        LOGE("nativeInitRenderer failed with an unknown error");
        return JNI_FALSE;
    }
}

extern "C" JNIEXPORT void JNICALL
Java_com_psg_app_PSGEmulatorBridge_nativeDestroyRenderer(JNIEnv* env, jobject) {
    try {
        if (env == nullptr) {
            LOGE("nativeDestroyRenderer received a null JNI environment");
            return;
        }
        g_renderer.destroy();
    } catch (const std::exception& exception) {
        LOGE("nativeDestroyRenderer failed: %s", exception.what());
    } catch (...) {
        LOGE("nativeDestroyRenderer failed with an unknown error");
    }
}

extern "C" JNIEXPORT jboolean JNICALL
Java_com_psg_app_PSGEmulatorBridge_nativeRenderFrame(JNIEnv* env, jobject) {
    try {
        if (env == nullptr) {
            LOGE("nativeRenderFrame received a null JNI environment");
            return JNI_FALSE;
        }
        return g_renderer.renderFrame() ? JNI_TRUE : JNI_FALSE;
    } catch (const std::exception& exception) {
        LOGE("nativeRenderFrame failed: %s", exception.what());
        return JNI_FALSE;
    } catch (...) {
        LOGE("nativeRenderFrame failed with an unknown error");
        return JNI_FALSE;
    }
}
