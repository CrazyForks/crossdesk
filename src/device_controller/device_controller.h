/*
 * @Author: DI JUNKUN
 * @Date: 2023-12-14
 * Copyright (c) 2023 by DI JUNKUN, All Rights Reserved.
 */

#ifndef _DEVICE_CONTROLLER_H_
#define _DEVICE_CONTROLLER_H_

#include <stdio.h>

#include "display_info.h"

namespace crossdesk {

typedef enum {
  mouse = 0,
  keyboard,
  audio_capture,
  host_infomation,
  display_id,
} ControlType;
typedef enum {
  move = 0,
  left_down,
  left_up,
  right_down,
  right_up,
  middle_down,
  middle_up,
  wheel_vertical,
  wheel_horizontal
} MouseFlag;
typedef enum { key_down = 0, key_up } KeyFlag;
typedef struct {
  float x;
  float y;
  int s;
  MouseFlag flag;
} Mouse;

typedef struct {
  size_t key_value;
  KeyFlag flag;
} Key;

typedef struct {
  char host_name[64];
  size_t host_name_size;
  char** display_list;
  size_t display_num;
  int* left;
  int* top;
  int* right;
  int* bottom;
} HostInfo;

typedef struct {
  ControlType type;
  union {
    Mouse m;
    Key k;
    HostInfo i;
    bool a;
    int d;
  };
} RemoteAction;

// int key_code, bool is_down
typedef void (*OnKeyAction)(int, bool, void*);

class DeviceController {
 public:
  virtual ~DeviceController() {}

 public:
  // virtual int Init(int screen_width, int screen_height);
  // virtual int Destroy();
  // virtual int SendMouseCommand(RemoteAction remote_action);

  // virtual int Hook();
  // virtual int Unhook();
};
}  // namespace crossdesk
#endif