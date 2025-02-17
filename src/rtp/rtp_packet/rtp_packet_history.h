/*
 * @Author: DI JUNKUN
 * @Date: 2025-02-14
 * Copyright (c) 2025 by DI JUNKUN, All Rights Reserved.
 */

#ifndef _RTP_PACKET_HISTORY_H_
#define _RTP_PACKET_HISTORY_H_

#include <deque>
#include <memory>

#include "api/clock/clock.h"
#include "rtp_packet_to_send.h"

class RtpPacketHistory {
 public:
  static constexpr size_t kMaxCapacity = 600;
  // Maximum number of entries in prioritized queue of padding packets.
  static constexpr size_t kMaxPaddingHistory = 63;
  // Don't remove packets within max(1 second, 3x RTT).
  static constexpr webrtc::TimeDelta kMinPacketDuration =
      webrtc::TimeDelta::Seconds(1);
  static constexpr int kMinPacketDurationRtt = 3;
  // With kStoreAndCull, always remove packets after 3x max(1000ms, 3x rtt).
  static constexpr int kPacketCullingDelayFactor = 3;

 public:
  RtpPacketHistory(std::shared_ptr<webrtc::Clock> clock);
  ~RtpPacketHistory();

 public:
  void SetRtt(webrtc::TimeDelta rtt);
  void AddPacket(std::shared_ptr<webrtc::RtpPacketToSend> rtp_packet,
                 webrtc::Timestamp send_time);
  void RemoveDeadPackets();

 private:
  std::shared_ptr<webrtc::RtpPacketToSend> RemovePacket(int packet_index);
  int GetPacketIndex(uint16_t sequence_number) const;

 private:
  struct RtpPacketToSendInfo {
    RtpPacketToSendInfo() = default;
    RtpPacketToSendInfo(std::shared_ptr<webrtc::RtpPacketToSend> rtp_packet,
                        webrtc::Timestamp send_time, uint64_t index)
        : rtp_packet(rtp_packet), send_time(send_time), index(index) {}
    RtpPacketToSendInfo(RtpPacketToSendInfo&&) = default;
    RtpPacketToSendInfo& operator=(RtpPacketToSendInfo&&) = default;
    ~RtpPacketToSendInfo() = default;

    std::shared_ptr<webrtc::RtpPacketToSend> rtp_packet;
    webrtc::Timestamp send_time = webrtc::Timestamp::Zero();
    uint64_t index;
  };

 private:
  std::shared_ptr<webrtc::Clock> clock_;
  std::deque<RtpPacketToSendInfo> rtp_packet_history_;
  uint64_t packets_inserted_;
  webrtc::TimeDelta rtt_;
  size_t number_to_store_;
};

#endif