/*
 * @Author: DI JUNKUN
 * @Date: 2024-12-18
 * Copyright (c) 2024 by DI JUNKUN, All Rights Reserved.
 */

#ifndef _CONGESTION_CONTROL_FEEDBACK_TRACKER_H_
#define _CONGESTION_CONTROL_FEEDBACK_TRACKER_H_

#include <optional>
#include <vector>

#include "congestion_control_feedback.h"
#include "rtp_packet_received.h"

class CongestionControlFeedbackTracker {
 public:
  CongestionControlFeedbackTracker() = default;

  void ReceivedPacket(const RtpPacketReceived& packet);

  // Adds received packets to `packet_feedback`
  // RTP sequence numbers are continous from the last created feedback unless
  // reordering has occured between feedback packets. If so, the sequence
  // number range may overlap with previousely sent feedback.
  void AddPacketsToFeedback(
      int64_t feedback_time,
      std::vector<CongestionControlFeedback::PacketInfo>& packet_feedback);

 private:
  struct PacketInfo {
    uint32_t ssrc;
    int64_t unwrapped_sequence_number = 0;
    int64_t arrival_time;
    EcnMarking ecn = EcnMarking::kNotEct;
  };

  std::optional<int64_t> last_sequence_number_in_feedback_;
  SeqNumUnwrapper<uint16_t> unwrapper_;

  std::vector<PacketInfo> packets_;
};

#endif