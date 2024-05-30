/*
 * @Author: DI JUNKUN
 * @Date: 2024-05-29
 * Copyright (c) 2024 by DI JUNKUN, All Rights Reserved.
 */

#ifndef _CONNECTION_H_
#define _CONNECTION_H_

#include <chrono>
#include <string>

#include "../../thirdparty/projectx/src/interface/x.h"
#include "device_controller_factory.h"
#include "screen_capturer_factory.h"

class Connection {
 public:
  Connection();
  ~Connection();

 public:
  int DeskConnectionInit();
  int DeskConnectionCreate(const char *input_password);

 private:
  PeerPtr *peer_ = nullptr;

  std::string mac_addr_str_ = "";

  Params params_;

  std::string user_id_ = "";

  std::string input_password_ = "";

  ScreenCapturerFactory *screen_capturer_factory_ = nullptr;
  ScreenCapturer *screen_capturer_ = nullptr;

  DeviceControllerFactory *device_controller_factory_ = nullptr;
  MouseController *mouse_controller_ = nullptr;

  bool is_created_connection_ = false;

#ifdef __linux__
  std::chrono::_V2::system_clock::time_point last_frame_time_;
#else
  std::chrono::steady_clock::time_point last_frame_time_;
#endif

  std::atomic<ConnectionStatus> connection_status_{ConnectionStatus::Closed};
  std::atomic<SignalStatus> signal_status_{SignalStatus::SignalClosed};
};

#endif