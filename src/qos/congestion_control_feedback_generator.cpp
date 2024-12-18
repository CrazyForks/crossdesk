#include "congestion_control_feedback_generator.h"

#include <algorithm>
#include <cstdint>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

uint32_t ConvertToCompactNtp(int64_t now_ms) {
  int64_t seconds = now_ms / 1000;
  int64_t milliseconds = now_ms % 1000;
  uint16_t ntp_seconds = static_cast<uint16_t>(seconds & 0xFFFF);
  uint16_t ntp_fraction = static_cast<uint16_t>((milliseconds * 65536) / 1000);
  uint32_t compact_ntp = (ntp_seconds << 16) | ntp_fraction;
  return compact_ntp;
}

CongestionControlFeedbackGenerator::CongestionControlFeedbackGenerator(
    RtcpSender rtcp_sender)
    : rtcp_sender_(std::move(rtcp_sender)) {}

void CongestionControlFeedbackGenerator::OnReceivedPacket(
    const RtpPacketReceived& packet) {
  marker_bit_seen_ |= packet.Marker();
  if (!first_arrival_time_since_feedback_) {
    first_arrival_time_since_feedback_ = packet.arrival_time();
  }
  feedback_trackers_[packet.Ssrc()].ReceivedPacket(packet);
  if (NextFeedbackTime() < packet.arrival_time()) {
    auto now = std::chrono::system_clock::now();
    auto now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                      now.time_since_epoch())
                      .count();
    SendFeedback(now_ms);
  }
}

int64_t CongestionControlFeedbackGenerator::NextFeedbackTime() const {
  auto now = std::chrono::system_clock::now();
  auto now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                    now.time_since_epoch())
                    .count();

  if (!first_arrival_time_since_feedback_) {
    return std::max(now_ms + min_time_between_feedback_,
                    next_possible_feedback_send_time_);
  }

  if (!marker_bit_seen_) {
    return std::max(next_possible_feedback_send_time_,
                    *first_arrival_time_since_feedback_ +
                        max_time_to_wait_for_packet_with_marker_);
  }
  return next_possible_feedback_send_time_;
}

int64_t CongestionControlFeedbackGenerator::Process(int64_t now_ms) {
  if (NextFeedbackTime() <= now_ms) {
    SendFeedback(now_ms);
  }
  return NextFeedbackTime() - now_ms;
}

void CongestionControlFeedbackGenerator::OnSendBandwidthEstimateChanged(
    DataRate estimate) {
  // Feedback reports should max occupy 5% of total bandwidth.
  max_feedback_rate_ = estimate * 0.05;
}

void CongestionControlFeedbackGenerator::SetTransportOverhead(
    DataSize overhead_per_packet) {
  packet_overhead_ = overhead_per_packet;
}

void CongestionControlFeedbackGenerator::SendFeedback(int64_t now_ms) {
  uint32_t compact_ntp = ConvertToCompactNtp(now_ms);
  std::vector<rtcp::CongestionControlFeedback::PacketInfo> rtcp_packet_info;
  for (auto& [unused, tracker] : feedback_trackers_) {
    tracker.AddPacketsToFeedback(now_ms, rtcp_packet_info);
  }

  marker_bit_seen_ = false;
  first_arrival_time_since_feedback_ = std::nullopt;

  auto feedback = std::make_unique<rtcp::CongestionControlFeedback>(
      std::move(rtcp_packet_info), compact_ntp);
  CalculateNextPossibleSendTime(feedback->BlockLength(), now_ms);

  std::vector<std::unique_ptr<rtcp::RtcpPacket>> rtcp_packets;
  rtcp_packets.push_back(std::move(feedback));
  rtcp_sender_(std::move(rtcp_packets));
}

void CongestionControlFeedbackGenerator::CalculateNextPossibleSendTime(
    int64_t feedback_size, int64_t now_ms) {
  int64_t time_since_last_sent = now - last_feedback_sent_time_;
  size_t debt_payed = time_since_last_sent * max_feedback_rate_;
  send_rate_debt_ =
      debt_payed > send_rate_debt_ ? 0 : send_rate_debt_ - debt_payed;
  send_rate_debt_ += feedback_size + packet_overhead_;
  last_feedback_sent_time_ = now_ms;
  next_possible_feedback_send_time_ =
      now_ms +
      std::clamp(max_feedback_rate_ == 0 ? std::numeric_limits<int64_t>::max()
                                         : send_rate_debt_ / max_feedback_rate_,
                 min_time_between_feedback_, max_time_between_feedback_);
}
