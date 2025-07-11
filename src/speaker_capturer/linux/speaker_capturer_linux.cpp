#include "speaker_capturer_linux.h"

#include <pulse/error.h>
#include <pulse/pulseaudio.h>
#include <pulse/simple.h>

#include <atomic>
#include <condition_variable>
#include <cstdio>
#include <mutex>
#include <string>
#include <thread>
#include <tuple>
#include <vector>

#include "rd_log.h"

namespace {
constexpr int kSampleRate = 48000;
constexpr pa_sample_format_t kFormat = PA_SAMPLE_S16LE;
constexpr int kChannels = 1;
constexpr int kFrameSize = 480;

std::string GetDefaultMonitorSourceName() {
  std::string result;
  std::mutex mtx;
  std::condition_variable cv;
  bool done = false;

  pa_mainloop* m = pa_mainloop_new();
  pa_mainloop_api* api = pa_mainloop_get_api(m);
  pa_context* context = pa_context_new(api, "MonitorSourceQuery");

  auto user_data =
      new std::tuple<std::string*, bool*, std::mutex*,
                     std::condition_variable*>{&result, &done, &mtx, &cv};

  pa_context_set_state_callback(
      context,
      [](pa_context* c, void* userdata) {
        if (!c) return;
        auto state = pa_context_get_state(c);
        if (state == PA_CONTEXT_READY) {
          pa_operation* op = pa_context_get_server_info(
              c,
              [](pa_context* c, const pa_server_info* info, void* userdata) {
                if (!info) return;

                auto [result_ptr, done_ptr, mtx_ptr, cv_ptr] =
                    *static_cast<std::tuple<std::string*, bool*, std::mutex*,
                                            std::condition_variable*>*>(
                        userdata);

                std::string default_sink = info->default_sink_name;
                std::string expected_monitor = default_sink + ".monitor";

                pa_operation* o2 = pa_context_get_source_info_list(
                    c,
                    [](pa_context* c, const pa_source_info* i, int eol,
                       void* userdata) {
                      if (eol) {
                        auto [result_ptr, done_ptr, mtx_ptr, cv_ptr] =
                            *static_cast<
                                std::tuple<std::string*, bool*, std::mutex*,
                                           std::condition_variable*>*>(
                                userdata);
                        std::lock_guard<std::mutex> lock(*mtx_ptr);
                        *done_ptr = true;
                        cv_ptr->notify_one();
                        return;
                      }

                      if (i && i->monitor_of_sink != PA_INVALID_INDEX) {
                        auto [result_ptr, done_ptr, mtx_ptr, cv_ptr] =
                            *static_cast<
                                std::tuple<std::string*, bool*, std::mutex*,
                                           std::condition_variable*>*>(
                                userdata);
                        *result_ptr = i->name;
                      }
                    },
                    userdata);

                if (o2) pa_operation_unref(o2);
              },
              userdata);
          if (op) pa_operation_unref(op);
        } else if (state == PA_CONTEXT_FAILED ||
                   state == PA_CONTEXT_TERMINATED) {
          auto [result_ptr, done_ptr, mtx_ptr, cv_ptr] =
              *static_cast<std::tuple<std::string*, bool*, std::mutex*,
                                      std::condition_variable*>*>(userdata);
          std::lock_guard<std::mutex> lock(*mtx_ptr);
          *done_ptr = true;
          cv_ptr->notify_one();
        }
      },
      user_data);

  if (pa_context_connect(context, nullptr, PA_CONTEXT_NOFLAGS, nullptr) < 0) {
    LOG_ERROR("Failed to connect to PulseAudio context");
    pa_context_unref(context);
    pa_mainloop_free(m);
    delete user_data;
    return "";
  }

  std::thread loop_thread([&]() { pa_mainloop_run(m, nullptr); });

  {
    std::unique_lock<std::mutex> lock(mtx);
    cv.wait(lock, [&] { return done; });
  }

  pa_context_disconnect(context);
  pa_context_unref(context);
  pa_mainloop_quit(m, 0);
  loop_thread.join();
  pa_mainloop_free(m);
  delete user_data;

  return result;
}
}  // namespace

// ===================== SpeakerCapturerLinux 实现 ========================

SpeakerCapturerLinux::SpeakerCapturerLinux() = default;

SpeakerCapturerLinux::~SpeakerCapturerLinux() {
  Stop();
  Destroy();
}

int SpeakerCapturerLinux::Init(speaker_data_cb cb) {
  if (inited_) return 0;
  cb_ = cb;
  inited_ = true;
  return 0;
}

int SpeakerCapturerLinux::Destroy() {
  inited_ = false;
  return 0;
}

int SpeakerCapturerLinux::Start() {
  if (!inited_ || capture_thread_.joinable()) return -1;

  capture_thread_ = std::thread([this]() {
    std::string monitor_device = GetDefaultMonitorSourceName();
    if (monitor_device.empty()) {
      LOG_ERROR("Failed to find monitor source");
      return;
    }

    LOG_INFO("Using monitor device: {}", monitor_device.c_str());

    int error = 0;
    pa_sample_spec ss;
    ss.format = kFormat;
    ss.rate = kSampleRate;
    ss.channels = kChannels;

    pa_simple* stream = pa_simple_new(nullptr, "SpeakerCapture",
                                      PA_STREAM_RECORD, monitor_device.c_str(),
                                      "capture", &ss, nullptr, nullptr, &error);

    if (!stream) {
      LOG_ERROR("pa_simple_new() failed: {}", pa_strerror(error));
      return;
    }

    const size_t buffer_bytes = kFrameSize * sizeof(int16_t);
    std::vector<unsigned char> buffer(buffer_bytes);

    while (inited_) {
      if (!cb_) break;

      if (pa_simple_read(stream, buffer.data(), buffer.size(), &error) < 0) {
        LOG_ERROR("pa_simple_read() failed: {}", pa_strerror(error));
        break;
      }

      cb_(buffer.data(), buffer.size(), "audio");
    }

    pa_simple_free(stream);
  });

  return 0;
}

int SpeakerCapturerLinux::Stop() {
  if (!inited_) return -1;
  if (capture_thread_.joinable()) capture_thread_.join();
  return 0;
}

int SpeakerCapturerLinux::Pause() { return 0; }

int SpeakerCapturerLinux::Resume() { return 0; }
