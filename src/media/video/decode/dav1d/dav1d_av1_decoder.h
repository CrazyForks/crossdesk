/*
 * @Author: DI JUNKUN
 * @Date: 2024-03-04
 * Copyright (c) 2024 by DI JUNKUN, All Rights Reserved.
 */

#ifndef _DAV1D_AV1_DECODER_H_
#define _DAV1D_AV1_DECODER_H_

#include <functional>

#include "dav1d/dav1d.h"
#include "video_decoder.h"

class Dav1dAv1Decoder : public VideoDecoder {
 public:
  Dav1dAv1Decoder();
  virtual ~Dav1dAv1Decoder();

 public:
  int Init();

  int Decode(const uint8_t *data, int size,
             std::function<void(VideoFrame)> on_receive_decoded_frame);

  std::string GetDecoderName() { return "Dav1dAv1"; }

 private:
  VideoFrame *nv12_frame_ = 0;
  int nv12_frame_capacity_ = 0;
  int nv12_frame_size_ = 0;

  int frame_width_ = 0;
  int frame_height_ = 0;

  FILE *file_av1_ = nullptr;
  FILE *file_nv12_ = nullptr;

  bool first_ = false;

  // dav1d
  Dav1dContext *context_ = nullptr;
};

#endif