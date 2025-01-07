/*
 * @Author: DI JUNKUN
 * @Date: 2024-12-18
 * Copyright (c) 2024 by DI JUNKUN, All Rights Reserved.
 */

#ifndef _RTP_PACKET_RECEIVED_H_
#define _RTP_PACKET_RECEIVED_H_

#include <limits>

#include "rtp_packet.h"

class RtpPacketReceived : public RtpPacket {
 public:
  RtpPacketReceived();
  explicit RtpPacketReceived(
      int64_t arrival_time = (std::numeric_limits<int64_t>::min)());
  RtpPacketReceived(const RtpPacketReceived& packet);
  RtpPacketReceived(RtpPacketReceived&& packet);

  RtpPacketReceived& operator=(const RtpPacketReceived& packet);
  RtpPacketReceived& operator=(RtpPacketReceived&& packet);

  ~RtpPacketReceived();

 private:
  int64_t arrival_time_ = (std::numeric_limits<int64_t>::min)();
};

#endif