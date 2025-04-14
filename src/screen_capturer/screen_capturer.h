/*
 * @Author: DI JUNKUN
 * @Date: 2023-12-15
 * Copyright (c) 2023 by DI JUNKUN, All Rights Reserved.
 */

#ifndef _SCREEN_CAPTURER_H_
#define _SCREEN_CAPTURER_H_

#include <functional>

class ScreenCapturer {
 public:
  typedef std::function<void(unsigned char *, int, int, int)> cb_desktop_data;

 public:
  virtual ~ScreenCapturer() {}

 public:
  virtual int Init(const int fps, cb_desktop_data cb) = 0;
  virtual int Destroy() = 0;
  virtual int Start() = 0;
  virtual int Stop() = 0;
};

#endif