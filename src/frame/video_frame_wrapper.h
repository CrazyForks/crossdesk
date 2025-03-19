/*
 * @Author: DI JUNKUN
 * @Date: 2025-02-21
 * Copyright (c) 2025 by DI JUNKUN, All Rights Reserved.
 */

#ifndef _VIDEO_FRAME_WRAPPER_H_
#define _VIDEO_FRAME_WRAPPER_H_

#include "video_frame.h"

enum VideoFrameType {
  kEmptyFrame = 0,
  kVideoFrameKey = 3,
  kVideoFrameDelta = 4,
};

class VideoFrameWrapper : public VideoFrame {
 public:
  VideoFrameWrapper(const uint8_t *buffer, size_t size, uint32_t width,
                    uint32_t height)
      : VideoFrame(buffer, size, width, height) {}
  VideoFrameWrapper() = delete;
  ~VideoFrameWrapper() = default;

  int64_t CaptureTimestamp() { return capture_timestamp_us_; }

  void SetCaptureTimestamp(int64_t capture_timestamp_us) {
    capture_timestamp_us_ = capture_timestamp_us;
  }

  VideoFrameType FrameType() { return frame_type_; }

  void SetFrameType(VideoFrameType frame_type) { frame_type_ = frame_type; }

  uint32_t EncodedWidth() { return encoded_width_; }

  void SetEncodedWidth(uint32_t encoded_width) {
    encoded_width_ = encoded_width;
  }

  uint32_t EncodedHeight() { return encoded_height_; }

  void SetEncodedHeight(uint32_t encoded_height) {
    encoded_height_ = encoded_height;
  }

 private:
  int64_t capture_timestamp_us_ = 0;
  VideoFrameType frame_type_ = VideoFrameType::kVideoFrameDelta;
  uint32_t encoded_width_ = 0;
  uint32_t encoded_height_ = 0;
};

#endif