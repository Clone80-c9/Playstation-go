#include <aaudio/AAudio.h>
#include <android/log.h>
#include <SLES/OpenSLES.h>
#include <SLES/OpenSLES_Android.h>

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <limits>
#include <mutex>
#include <vector>

#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, "PSG_CORE", __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, "PSG_CORE", __VA_ARGS__)
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN, "PSG_CORE", __VA_ARGS__)

class PSGAudioEngine {
public:
    PSGAudioEngine() = default;
    PSGAudioEngine(const PSGAudioEngine&) = delete;
    PSGAudioEngine& operator=(const PSGAudioEngine&) = delete;
    ~PSGAudioEngine() {
        destroy();
    }

    bool init(int32_t sample_rate, int32_t channel_count,
              int32_t buffer_size_frames) {
        std::lock_guard<std::mutex> lock(mutex_);
        destroyLocked();
        if (sample_rate <= 0 || sample_rate > 384000 ||
            channel_count < 1 || channel_count > 2 ||
            buffer_size_frames <= 0) {
            LOGE("Invalid audio configuration");
            return false;
        }

        sample_rate_ = sample_rate;
        channel_count_ = channel_count;
        buffer_size_frames_ = buffer_size_frames;
        if (!allocateBuffers()) {
            clearConfiguration();
            return false;
        }

        if (openAAudio()) {
            LOGI("Audio backend selected: AAudio (low-latency exclusive)");
            return true;
        }
        LOGW("AAudio initialization failed; falling back to OpenSL ES");
        if (openOpenSLES()) {
            LOGI("Audio backend selected: OpenSL ES");
            return true;
        }

        LOGE("Unable to initialize AAudio or OpenSL ES");
        destroyLocked();
        return false;
    }

    bool write(const int16_t* buffer, int32_t frames) {
        if (buffer == nullptr || frames <= 0) {
            LOGE("Audio write received an invalid buffer or frame count");
            return false;
        }

        std::lock_guard<std::mutex> lock(mutex_);
        if (backend_ == Backend::kAAudio &&
            aaudio_disconnected_.load(std::memory_order_acquire)) {
            if (!reconnectAttempted_.exchange(true, std::memory_order_acq_rel)) {
                LOGW("AAudio stream disconnected; attempting one reconnect");
                closeAAudio();
                if (openAAudio()) {
                    LOGI("AAudio stream reconnected");
                } else {
                    LOGW("AAudio reconnect failed; attempting OpenSL ES fallback");
                    if (!openOpenSLES()) {
                        LOGE("Audio reconnect and fallback both failed");
                        return false;
                    }
                }
            } else {
                LOGE("Audio stream remains disconnected after reconnect attempt");
                return false;
            }
        }

        if (backend_ == Backend::kAAudio) {
            return writeAAudio(buffer, frames);
        }
        if (backend_ == Backend::kOpenSLES) {
            return writeOpenSLES(buffer, frames);
        }

        LOGE("Audio engine is not initialized");
        return false;
    }

    bool setVolume(float volume) {
        if (!(volume >= 0.0f && volume <= 1.0f)) {
            LOGE("Volume must be between 0.0 and 1.0");
            return false;
        }
        volume_.store(volume, std::memory_order_release);
        return true;
    }

    void destroy() {
        std::lock_guard<std::mutex> lock(mutex_);
        destroyLocked();
    }

private:
    enum class Backend {
        kNone,
        kAAudio,
        kOpenSLES,
    };

    static constexpr uint32_t kOpenSLBufferCount = 4;

    static aaudio_data_callback_result_t audioDataCallback(
        AAudioStream*, void* user_data, void* audio_data, int32_t num_frames) {
        if (user_data == nullptr || audio_data == nullptr || num_frames <= 0) {
            return AAUDIO_CALLBACK_RESULT_STOP;
        }
        return static_cast<PSGAudioEngine*>(user_data)->renderAAudio(
            static_cast<int16_t*>(audio_data), num_frames);
    }

    static void audioErrorCallback(AAudioStream*, void* user_data,
                                   aaudio_result_t error) {
        if (user_data == nullptr) {
            return;
        }
        auto* engine = static_cast<PSGAudioEngine*>(user_data);
        if (error == AAUDIO_ERROR_DISCONNECTED) {
            engine->aaudio_disconnected_.store(true, std::memory_order_release);
        } else {
            LOGE("AAudio stream error: %s", AAudio_convertResultToText(error));
        }
    }

    static void openSLBufferQueueCallback(SLAndroidSimpleBufferQueueItf,
                                          void* user_data) {
        if (user_data == nullptr) {
            return;
        }
        auto* engine = static_cast<PSGAudioEngine*>(user_data);
        uint32_t queued = engine->open_sl_queued_buffers_.load(
            std::memory_order_relaxed);
        while (queued > 0 &&
               !engine->open_sl_queued_buffers_.compare_exchange_weak(
                   queued, queued - 1, std::memory_order_release,
                   std::memory_order_relaxed)) {
        }
    }

    bool allocateBuffers() {
        const size_t frame_count = static_cast<size_t>(buffer_size_frames_);
        const size_t channel_count = static_cast<size_t>(channel_count_);
        if (frame_count > std::numeric_limits<size_t>::max() / channel_count ||
            frame_count * channel_count >
                std::numeric_limits<size_t>::max() / kOpenSLBufferCount) {
            LOGE("Audio buffer size overflows addressable memory");
            return false;
        }

        const size_t samples_per_buffer = frame_count * channel_count;
        ring_capacity_samples_ = samples_per_buffer * 4;
        if (ring_capacity_samples_ == 0) {
            LOGE("Audio ring buffer size is invalid");
            return false;
        }
        try {
            ring_buffer_.assign(ring_capacity_samples_, 0);
            open_sl_buffers_.assign(kOpenSLBufferCount,
                                    std::vector<int16_t>(samples_per_buffer, 0));
        } catch (...) {
            LOGE("Unable to allocate audio buffers");
            ring_buffer_.clear();
            open_sl_buffers_.clear();
            ring_capacity_samples_ = 0;
            return false;
        }
        return true;
    }

    bool openAAudio() {
        AAudioStreamBuilder* builder = nullptr;
        aaudio_result_t result = AAudio_createStreamBuilder(&builder);
        if (result != AAUDIO_OK || builder == nullptr) {
            LOGE("AAudio builder creation failed: %s",
                 AAudio_convertResultToText(result));
            return false;
        }

        AAudioStreamBuilder_setDirection(builder, AAUDIO_DIRECTION_OUTPUT);
        AAudioStreamBuilder_setFormat(builder, AAUDIO_FORMAT_PCM_I16);
        AAudioStreamBuilder_setSampleRate(builder, sample_rate_);
        AAudioStreamBuilder_setChannelCount(builder, channel_count_);
        AAudioStreamBuilder_setPerformanceMode(
            builder, AAUDIO_PERFORMANCE_MODE_LOW_LATENCY);
        AAudioStreamBuilder_setSharingMode(builder, AAUDIO_SHARING_MODE_EXCLUSIVE);
        AAudioStreamBuilder_setDataCallback(builder, audioDataCallback, this);
        AAudioStreamBuilder_setErrorCallback(builder, audioErrorCallback, this);

        result = AAudioStreamBuilder_openStream(builder, &aaudio_stream_);
        const aaudio_result_t delete_result =
            AAudioStreamBuilder_delete(builder);
        if (delete_result != AAUDIO_OK) {
            LOGE("AAudio builder cleanup failed: %s",
                 AAudio_convertResultToText(delete_result));
        }
        if (result != AAUDIO_OK || aaudio_stream_ == nullptr) {
            LOGE("AAudio stream open failed: %s",
                 AAudio_convertResultToText(result));
            aaudio_stream_ = nullptr;
            return false;
        }

        result = AAudioStream_requestStart(aaudio_stream_);
        if (result != AAUDIO_OK) {
            LOGE("AAudio stream start failed: %s",
                 AAudio_convertResultToText(result));
            closeAAudio();
            return false;
        }

        backend_ = Backend::kAAudio;
        aaudio_disconnected_.store(false, std::memory_order_release);
        reconnectAttempted_.store(false, std::memory_order_release);
        return true;
    }

    bool openOpenSLES() {
        closeOpenSLES();

        SLresult result = slCreateEngine(&open_sl_engine_object_, 0, nullptr, 0,
                                         nullptr, nullptr);
        if (result != SL_RESULT_SUCCESS || open_sl_engine_object_ == nullptr) {
            LOGE("OpenSL ES engine creation failed: %d", result);
            open_sl_engine_object_ = nullptr;
            return false;
        }
        result = (*open_sl_engine_object_)->Realize(open_sl_engine_object_,
                                                    SL_BOOLEAN_FALSE);
        if (result != SL_RESULT_SUCCESS) {
            LOGE("OpenSL ES engine realization failed: %d", result);
            closeOpenSLES();
            return false;
        }
        result = (*open_sl_engine_object_)
                     ->GetInterface(open_sl_engine_object_, SL_IID_ENGINE,
                                    &open_sl_engine_);
        if (result != SL_RESULT_SUCCESS || open_sl_engine_ == nullptr) {
            LOGE("OpenSL ES engine interface unavailable: %d", result);
            closeOpenSLES();
            return false;
        }

        result = (*open_sl_engine_)
                     ->CreateOutputMix(open_sl_engine_, &open_sl_output_mix_,
                                       0, nullptr, nullptr);
        if (result != SL_RESULT_SUCCESS || open_sl_output_mix_ == nullptr) {
            LOGE("OpenSL ES output mix creation failed: %d", result);
            closeOpenSLES();
            return false;
        }
        result = (*open_sl_output_mix_)
                     ->Realize(open_sl_output_mix_, SL_BOOLEAN_FALSE);
        if (result != SL_RESULT_SUCCESS) {
            LOGE("OpenSL ES output mix realization failed: %d", result);
            closeOpenSLES();
            return false;
        }

        SLDataLocator_AndroidSimpleBufferQueue queue_locator = {
            SL_DATALOCATOR_ANDROIDSIMPLEBUFFERQUEUE,
            static_cast<SLuint32>(kOpenSLBufferCount)};
        SLDataFormat_PCM audio_format = {
            SL_DATAFORMAT_PCM,
            static_cast<SLuint32>(channel_count_),
            static_cast<SLuint32>(sample_rate_ * 1000),
            SL_PCMSAMPLEFORMAT_FIXED_16,
            SL_PCMSAMPLEFORMAT_FIXED_16,
            channel_count_ == 1 ? SL_SPEAKER_FRONT_CENTER
                                : SL_SPEAKER_FRONT_LEFT | SL_SPEAKER_FRONT_RIGHT,
            SL_BYTEORDER_LITTLEENDIAN};
        SLDataSource source = {&queue_locator, &audio_format};
        SLDataLocator_OutputMix output_locator = {
            SL_DATALOCATOR_OUTPUTMIX, open_sl_output_mix_};
        SLDataSink sink = {&output_locator, nullptr};
        const SLInterfaceID interface_ids[] = {SL_IID_ANDROIDSIMPLEBUFFERQUEUE};
        const SLboolean interface_required[] = {SL_BOOLEAN_TRUE};

        result = (*open_sl_engine_)
                     ->CreateAudioPlayer(open_sl_engine_, &open_sl_player_object_,
                                         &source, &sink, 1, interface_ids,
                                         interface_required);
        if (result != SL_RESULT_SUCCESS || open_sl_player_object_ == nullptr) {
            LOGE("OpenSL ES audio player creation failed: %d", result);
            closeOpenSLES();
            return false;
        }
        result = (*open_sl_player_object_)
                     ->Realize(open_sl_player_object_, SL_BOOLEAN_FALSE);
        if (result != SL_RESULT_SUCCESS) {
            LOGE("OpenSL ES audio player realization failed: %d", result);
            closeOpenSLES();
            return false;
        }
        result = (*open_sl_player_object_)
                     ->GetInterface(open_sl_player_object_, SL_IID_PLAY,
                                    &open_sl_play_);
        if (result != SL_RESULT_SUCCESS || open_sl_play_ == nullptr) {
            LOGE("OpenSL ES play interface unavailable: %d", result);
            closeOpenSLES();
            return false;
        }
        result = (*open_sl_player_object_)
                     ->GetInterface(open_sl_player_object_,
                                    SL_IID_ANDROIDSIMPLEBUFFERQUEUE,
                                    &open_sl_buffer_queue_);
        if (result != SL_RESULT_SUCCESS || open_sl_buffer_queue_ == nullptr) {
            LOGE("OpenSL ES buffer queue interface unavailable: %d", result);
            closeOpenSLES();
            return false;
        }
        result = (*open_sl_buffer_queue_)
                     ->RegisterCallback(open_sl_buffer_queue_,
                                        openSLBufferQueueCallback, this);
        if (result != SL_RESULT_SUCCESS) {
            LOGE("OpenSL ES queue callback registration failed: %d", result);
            closeOpenSLES();
            return false;
        }
        result = (*open_sl_play_)
                     ->SetPlayState(open_sl_play_, SL_PLAYSTATE_PLAYING);
        if (result != SL_RESULT_SUCCESS) {
            LOGE("OpenSL ES playback start failed: %d", result);
            closeOpenSLES();
            return false;
        }

        open_sl_next_buffer_.store(0, std::memory_order_release);
        open_sl_queued_buffers_.store(0, std::memory_order_release);
        backend_ = Backend::kOpenSLES;
        return true;
    }

    bool writeAAudio(const int16_t* buffer, int32_t frames) {
        if (ring_buffer_.empty() || channel_count_ <= 0) {
            LOGE("AAudio ring buffer is not initialized");
            return false;
        }
        const size_t samples = static_cast<size_t>(frames) *
                               static_cast<size_t>(channel_count_);
        const uint64_t write_index =
            ring_write_index_.load(std::memory_order_relaxed);
        const uint64_t read_index =
            ring_read_index_.load(std::memory_order_acquire);
        if (samples > ring_capacity_samples_ -
                          std::min<uint64_t>(write_index - read_index,
                                             ring_capacity_samples_)) {
            LOGE("AAudio ring buffer is full");
            return false;
        }

        const float volume = volume_.load(std::memory_order_acquire);
        for (size_t index = 0; index < samples; ++index) {
            ring_buffer_[(write_index + index) % ring_capacity_samples_] =
                scaleSample(buffer[index], volume);
        }
        ring_write_index_.store(write_index + samples, std::memory_order_release);
        reconnectAttempted_.store(false, std::memory_order_release);
        return true;
    }

    aaudio_data_callback_result_t renderAAudio(int16_t* output,
                                               int32_t frames) {
        if (output == nullptr || frames <= 0 || channel_count_ <= 0) {
            return AAUDIO_CALLBACK_RESULT_STOP;
        }
        const size_t requested_samples = static_cast<size_t>(frames) *
                                         static_cast<size_t>(channel_count_);
        std::fill(output, output + requested_samples, 0);

        const uint64_t read_index =
            ring_read_index_.load(std::memory_order_relaxed);
        const uint64_t write_index =
            ring_write_index_.load(std::memory_order_acquire);
        const size_t available = static_cast<size_t>(
            std::min<uint64_t>(write_index - read_index, requested_samples));
        for (size_t index = 0; index < available; ++index) {
            output[index] = ring_buffer_[(read_index + index) %
                                         ring_capacity_samples_];
        }
        ring_read_index_.store(read_index + available, std::memory_order_release);
        return AAUDIO_CALLBACK_RESULT_CONTINUE;
    }

    bool writeOpenSLES(const int16_t* buffer, int32_t frames) {
        if (open_sl_buffer_queue_ == nullptr || open_sl_buffers_.empty()) {
            LOGE("OpenSL ES queue is not initialized");
            return false;
        }
        if (frames > buffer_size_frames_) {
            LOGE("OpenSL ES write exceeds configured buffer size");
            return false;
        }
        if (open_sl_queued_buffers_.load(std::memory_order_acquire) >=
            kOpenSLBufferCount) {
            LOGE("OpenSL ES output queue is full");
            return false;
        }

        const uint32_t index =
            open_sl_next_buffer_.load(std::memory_order_relaxed) %
            kOpenSLBufferCount;
        const size_t samples =
            static_cast<size_t>(frames) * static_cast<size_t>(channel_count_);
        std::vector<int16_t>& target = open_sl_buffers_[index];
        const float volume = volume_.load(std::memory_order_acquire);
        for (size_t sample = 0; sample < samples; ++sample) {
            target[sample] = scaleSample(buffer[sample], volume);
        }

        open_sl_queued_buffers_.fetch_add(1, std::memory_order_acq_rel);
        const SLresult result = (*open_sl_buffer_queue_)
                                    ->Enqueue(open_sl_buffer_queue_,
                                              target.data(),
                                              static_cast<SLuint32>(
                                                  samples * sizeof(int16_t)));
        if (result != SL_RESULT_SUCCESS) {
            open_sl_queued_buffers_.fetch_sub(1, std::memory_order_acq_rel);
            LOGE("OpenSL ES buffer enqueue failed: %d", result);
            if (!reconnectAttempted_.exchange(true, std::memory_order_acq_rel)) {
                LOGW("Attempting one OpenSL ES reconnect");
                closeOpenSLES();
                if (!openOpenSLES()) {
                    LOGE("OpenSL ES reconnect failed");
                }
            } else {
                LOGE("OpenSL ES reconnect was already attempted");
            }
            return false;
        }

        open_sl_next_buffer_.store(index + 1, std::memory_order_release);
        reconnectAttempted_.store(false, std::memory_order_release);
        return true;
    }

    static int16_t scaleSample(int16_t sample, float volume) {
        const int32_t scaled = static_cast<int32_t>(sample * volume);
        return static_cast<int16_t>(
            std::clamp(scaled, static_cast<int32_t>(
                                   std::numeric_limits<int16_t>::min()),
                       static_cast<int32_t>(
                           std::numeric_limits<int16_t>::max())));
    }

    void closeAAudio() {
        if (aaudio_stream_ != nullptr) {
            const aaudio_result_t stop_result =
                AAudioStream_requestStop(aaudio_stream_);
            if (stop_result != AAUDIO_OK &&
                stop_result != AAUDIO_ERROR_INVALID_STATE) {
                LOGE("AAudio stop failed: %s",
                     AAudio_convertResultToText(stop_result));
            }
            const aaudio_result_t close_result =
                AAudioStream_close(aaudio_stream_);
            if (close_result != AAUDIO_OK) {
                LOGE("AAudio close failed: %s",
                     AAudio_convertResultToText(close_result));
            }
            aaudio_stream_ = nullptr;
        }
        if (backend_ == Backend::kAAudio) {
            backend_ = Backend::kNone;
        }
    }

    void closeOpenSLES() {
        if (open_sl_play_ != nullptr) {
            const SLresult result =
                (*open_sl_play_)->SetPlayState(open_sl_play_,
                                               SL_PLAYSTATE_STOPPED);
            if (result != SL_RESULT_SUCCESS) {
                LOGE("OpenSL ES playback stop failed: %d", result);
            }
            open_sl_play_ = nullptr;
        }
        if (open_sl_player_object_ != nullptr) {
            (*open_sl_player_object_)->Destroy(open_sl_player_object_);
            open_sl_player_object_ = nullptr;
        }
        open_sl_buffer_queue_ = nullptr;

        if (open_sl_output_mix_ != nullptr) {
            (*open_sl_output_mix_)->Destroy(open_sl_output_mix_);
            open_sl_output_mix_ = nullptr;
        }
        if (open_sl_engine_object_ != nullptr) {
            (*open_sl_engine_object_)->Destroy(open_sl_engine_object_);
            open_sl_engine_object_ = nullptr;
        }
        open_sl_engine_ = nullptr;
        open_sl_queued_buffers_.store(0, std::memory_order_release);
        if (backend_ == Backend::kOpenSLES) {
            backend_ = Backend::kNone;
        }
    }

    void clearConfiguration() {
        sample_rate_ = 0;
        channel_count_ = 0;
        buffer_size_frames_ = 0;
        ring_capacity_samples_ = 0;
        ring_buffer_.clear();
        open_sl_buffers_.clear();
        ring_read_index_.store(0, std::memory_order_release);
        ring_write_index_.store(0, std::memory_order_release);
        open_sl_next_buffer_.store(0, std::memory_order_release);
        aaudio_disconnected_.store(false, std::memory_order_release);
        reconnectAttempted_.store(false, std::memory_order_release);
    }

    void destroyLocked() {
        closeAAudio();
        closeOpenSLES();
        clearConfiguration();
        backend_ = Backend::kNone;
    }

    std::mutex mutex_;
    std::atomic<float> volume_{1.0f};
    std::atomic<bool> aaudio_disconnected_{false};
    std::atomic<bool> reconnectAttempted_{false};
    Backend backend_ = Backend::kNone;
    int32_t sample_rate_ = 0;
    int32_t channel_count_ = 0;
    int32_t buffer_size_frames_ = 0;

    AAudioStream* aaudio_stream_ = nullptr;
    std::vector<int16_t> ring_buffer_;
    size_t ring_capacity_samples_ = 0;
    std::atomic<uint64_t> ring_read_index_{0};
    std::atomic<uint64_t> ring_write_index_{0};

    SLObjectItf open_sl_engine_object_ = nullptr;
    SLEngineItf open_sl_engine_ = nullptr;
    SLObjectItf open_sl_output_mix_ = nullptr;
    SLObjectItf open_sl_player_object_ = nullptr;
    SLPlayItf open_sl_play_ = nullptr;
    SLAndroidSimpleBufferQueueItf open_sl_buffer_queue_ = nullptr;
    std::vector<std::vector<int16_t>> open_sl_buffers_;
    std::atomic<uint32_t> open_sl_next_buffer_{0};
    std::atomic<uint32_t> open_sl_queued_buffers_{0};
};

namespace {

PSGAudioEngine g_audio_engine;

}  // namespace

extern "C" bool psg_audio_init(int32_t sample_rate, int32_t channel_count,
                               int32_t buffer_size_frames) {
    return g_audio_engine.init(sample_rate, channel_count, buffer_size_frames);
}

extern "C" bool psg_audio_write(const int16_t* buffer, int32_t frames) {
    return g_audio_engine.write(buffer, frames);
}

extern "C" bool psg_audio_set_volume(float volume) {
    return g_audio_engine.setVolume(volume);
}

extern "C" void psg_audio_destroy() {
    g_audio_engine.destroy();
}
