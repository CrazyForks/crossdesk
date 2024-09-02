/*
 * @Author: DI JUNKUN
 * @Date: 2024-08-02
 * Copyright (c) 2024 by DI JUNKUN, All Rights Reserved.
 */

#ifndef _SPEAKER_CAPTURER_LINUX_H_
#define _SPEAKER_CAPTURER_LINUX_H_

#include <thread>
#include <vector>

#include "speaker_capturer.h"

class SpeakerCapturerLinux : public SpeakerCapturer {
 public:
  SpeakerCapturerLinux();
  ~SpeakerCapturerLinux();

 public:
  virtual int Init(speaker_data_cb cb);
  virtual int Destroy();
  virtual int Start();
  virtual int Stop();

  int Pause();
  int Resume();

 private:
  speaker_data_cb cb_ = nullptr;

 private:
  bool inited_ = false;
  // thread
  std::thread capture_thread_;
};

#endif