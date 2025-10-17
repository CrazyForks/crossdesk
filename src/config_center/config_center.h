/*
 * @Author: DI JUNKUN
 * @Date: 2024-05-29
 * Copyright (c) 2024 by DI JUNKUN, All Rights Reserved.
 */

#ifndef _CONFIG_CENTER_H_
#define _CONFIG_CENTER_H_

class ConfigCenter {
 public:
  enum class LANGUAGE { CHINESE = 0, ENGLISH = 1 };
  enum class VIDEO_QUALITY { LOW = 0, MEDIUM = 1, HIGH = 2 };
  enum class VIDEO_FRAME_RATE { FPS_30 = 0, FPS_60 = 1 };
  enum class VIDEO_ENCODE_FORMAT { AV1 = 0, H264 = 1 };

 public:
  ConfigCenter();
  ~ConfigCenter();

 public:
  int SetLanguage(LANGUAGE language);
  int SetVideoQuality(VIDEO_QUALITY video_quality);
  int SetVideoFrameRate(VIDEO_FRAME_RATE video_frame_rate);
  int SetVideoEncodeFormat(VIDEO_ENCODE_FORMAT video_encode_format);
  int SetHardwareVideoCodec(bool hardware_video_codec);
  int SetTurn(bool enable_turn);
  int SetSrtp(bool enable_srtp);

 public:
  LANGUAGE GetLanguage();
  VIDEO_QUALITY GetVideoQuality();
  int GetVideoFrameRate();
  VIDEO_ENCODE_FORMAT GetVideoEncodeFormat();
  bool IsHardwareVideoCodec();
  bool IsEnableTurn();
  bool IsEnableSrtp();

 private:
  // Default value should be same with parameters in localization.h
  LANGUAGE language_ = LANGUAGE::CHINESE;
  VIDEO_QUALITY video_quality_ = VIDEO_QUALITY::MEDIUM;
  VIDEO_FRAME_RATE video_frame_rate_ = VIDEO_FRAME_RATE::FPS_30;
  VIDEO_ENCODE_FORMAT video_encode_format_ = VIDEO_ENCODE_FORMAT::AV1;
  bool hardware_video_codec_ = false;
  bool enable_turn_ = false;
  bool enable_srtp_ = false;
};

#endif