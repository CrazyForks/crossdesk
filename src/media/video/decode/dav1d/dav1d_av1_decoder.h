/*
 * @Author: DI JUNKUN
 * @Date: 2024-03-04
 * Copyright (c) 2024 by DI JUNKUN, All Rights Reserved.
 */

#ifndef _DAV1D_AV1_DECODER_H_
#define _DAV1D_AV1_DECODER_H_

#include "dav1d/dav1d.h"

#ifdef _WIN32
extern "C" {
#include "libavcodec/avcodec.h"
};
#else
#ifdef __cplusplus
extern "C" {
#endif
#include <libavcodec/avcodec.h>
#ifdef __cplusplus
};
#endif
#endif

#include <functional>

#include "video_decoder.h"

class Dav1dAv1Decoder : public VideoDecoder {
 public:
  Dav1dAv1Decoder();
  virtual ~Dav1dAv1Decoder();

 public:
  int Init();
  int Decode(const uint8_t *data, int size,
             std::function<void(VideoFrame)> on_receive_decoded_frame);

 private:
  AVCodecID codec_id_;
  const AVCodec *codec_;
  AVCodecContext *codec_ctx_ = nullptr;
  AVPacket *packet_ = nullptr;
  AVFrame *frame_ = nullptr;
  AVFrame *frame_nv12_ = nullptr;
  struct SwsContext *img_convert_ctx = nullptr;

  VideoFrame *decoded_frame_yuv_ = nullptr;
  VideoFrame *decoded_frame_nv12_ = nullptr;

  FILE *file_av1_ = nullptr;
  FILE *file_nv12_ = nullptr;

  bool first_ = false;

  // dav1d
  Dav1dContext *context_ = nullptr;
};

#endif