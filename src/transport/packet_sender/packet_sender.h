/*
 * @Author: DI JUNKUN
 * @Date: 2025-03-17
 * Copyright (c) 2025 by DI JUNKUN, All Rights Reserved.
 */

#ifndef _PACKET_SENDER_H_
#define _PACKET_SENDER_H_

#include <memory>
#include <vector>

#include "rtp_packet.h"
#include "rtp_packet_to_send.h"

class PacketSender {
 public:
  PacketSender() {}
  virtual ~PacketSender() {}

  virtual int Send() = 0;
  virtual int EnqueueRtpPacket(
      std::vector<std::unique_ptr<RtpPacket>>& rtp_packets,
      int64_t captured_timestamp_us) = 0;

  virtual int EnqueueRtpPacket(
      std::vector<std::unique_ptr<webrtc::RtpPacketToSend>>& rtp_packets) = 0;
  ;
};

#endif