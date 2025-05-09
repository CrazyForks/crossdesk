#ifndef _SCREEN_CAPTURER_WGC_H_
#define _SCREEN_CAPTURER_WGC_H_

#include <atomic>
#include <functional>
#include <string>
#include <thread>
#include <vector>

#include "screen_capturer.h"
#include "wgc_session.h"
#include "wgc_session_impl.h"

class ScreenCapturerWgc : public ScreenCapturer,
                          public WgcSession::wgc_session_observer {
 public:
  ScreenCapturerWgc();
  ~ScreenCapturerWgc();

 public:
  bool IsWgcSupported();

  virtual int Init(const int fps, cb_desktop_data cb) override;
  virtual int Destroy() override;
  virtual int Start() override;
  virtual int Stop() override;

  virtual int Pause(int monitor_index) override;
  virtual int Resume(int monitor_index) override;

  std::vector<DisplayInfo> GetDisplayList() { return display_list_; }

  int SwitchTo(int monitor_index);

  void OnFrame(const WgcSession::wgc_session_frame &frame);

 protected:
  void CleanUp();

 private:
  HMONITOR monitor_;
  MONITORINFOEX monitor_info_;
  std::vector<DisplayInfo> display_list_;
  int monitor_index_ = 0;

 private:
  class WgcSessionInfo {
   public:
    std::unique_ptr<WgcSession> session_;
    bool inited_ = false;
    bool running_ = false;
    bool paused_ = false;
  };

  std::vector<WgcSessionInfo> sessions_;

  std::atomic_bool running_;
  std::atomic_bool inited_;

  int fps_;

  cb_desktop_data on_data_ = nullptr;

  unsigned char *nv12_frame_ = nullptr;
  unsigned char *nv12_frame_scaled_ = nullptr;
};

#endif