/*
 * @Author: DI JUNKUN
 * @Date: 2025-01-03
 * Copyright (c) 2025 by DI JUNKUN, All Rights Reserved.
 */

#ifndef _VIDEO_CHANNEL_SEND_H_
#define _VIDEO_CHANNEL_SEND_H_

#include "rtp_codec.h"
#include "rtp_video_sender.h"

class VideoChannelSend {
 public:
  VideoChannelSend();
  ~VideoChannelSend();

 public:
  void Initialize(RtpPacket::PAYLOAD_TYPE negotiated_video_pt);
  void Destroy();

  int SendVideo(char *encoded_frame, size_t size);

 private:
  std::unique_ptr<RtpCodec> video_rtp_codec_ = nullptr;
  std::unique_ptr<RtpVideoSender> rtp_video_sender_ = nullptr;
};

#endif