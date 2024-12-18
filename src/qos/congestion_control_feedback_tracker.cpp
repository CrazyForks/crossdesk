#include "congestion_control_feedback_tracker.h"

#include <cstdint>
#include <tuple>
#include <vector>

#include "log.h"

void CongestionControlFeedbackTracker::ReceivedPacket(
    const RtpPacketReceived& packet) {
  int64_t unwrapped_sequence_number =
      unwrapper_.Unwrap(packet.SequenceNumber());
  if (last_sequence_number_in_feedback_ &&
      unwrapped_sequence_number < *last_sequence_number_in_feedback_ + 1) {
    LOG_WARN(
        "Received packet unorderered between feeedback. SSRC: {} Seq: {} last "
        "feedback: {}",
        packet.Ssrc(), packet.SequenceNumber(),
        static_cast<uint16_t>(*last_sequence_number_in_feedback_));
    // TODO: bugs.webrtc.org/374550342 - According to spec, the old packets
    // should be reported again. But at the moment, we dont store history of
    // packet we already reported and thus, they will be reported as lost. Note
    // that this is likely not a problem in webrtc since the packets will also
    // be removed from the send history when they are first reported as
    // received.
    last_sequence_number_in_feedback_ = unwrapped_sequence_number - 1;
  }
  packets_.push_back({.ssrc = packet.Ssrc(),
                      .unwrapped_sequence_number = unwrapped_sequence_number,
                      .arrival_time = packet.arrival_time(),
                      .ecn = packet.ecn()});
}

void CongestionControlFeedbackTracker::AddPacketsToFeedback(
    int64_t feedback_time,
    std::vector<CongestionControlFeedback::PacketInfo>& packet_feedback) {
  if (packets_.empty()) {
    return;
  }
  absl::c_sort(packets_, [](const PacketInfo& a, const PacketInfo& b) {
    return std::tie(a.unwrapped_sequence_number, a.arrival_time) <
           std::tie(b.unwrapped_sequence_number, b.arrival_time);
  });
  if (!last_sequence_number_in_feedback_) {
    last_sequence_number_in_feedback_ =
        packets_.front().unwrapped_sequence_number - 1;
  }

  auto packet_it = packets_.begin();
  uint32_t ssrc = packet_it->ssrc;
  for (int64_t sequence_number = *last_sequence_number_in_feedback_ + 1;
       sequence_number <= packets_.back().unwrapped_sequence_number;
       ++sequence_number) {
    if (packet_it == packets_.end()) {
      LOG_FATAL("Invalid packet_it");
      return;
    }
    if (ssrc != packet_it->ssrc) {
      LOG_FATAL("Invalid ssrc");
      return;
    }

    rtc::EcnMarking ecn = rtc::EcnMarking::kNotEct;
    TimeDelta arrival_time_offset = TimeDelta::MinusInfinity();

    if (sequence_number == packet_it->unwrapped_sequence_number) {
      arrival_time_offset = feedback_time - packet_it->arrival_time;
      ecn = packet_it->ecn;
      ++packet_it;
      while (packet_it != packets_.end() &&
             packet_it->unwrapped_sequence_number == sequence_number) {
        // According to RFC 8888:
        // If duplicate copies of a particular RTP packet are received, then the
        // arrival time of the first copy to arrive MUST be reported. If any of
        // the copies of the duplicated packet are ECN-CE marked, then an ECN-CE
        // mark MUST be reported for that packet; otherwise, the ECN mark of the
        // first copy to arrive is reported.
        if (packet_it->ecn == rtc::EcnMarking::kCe) {
          ecn = rtc::EcnMarking::kCe;
        }
        LOG_WARN("Received duplicate packet ssrc: {} seq: {} ecn: {}", ssrc,
                 static_cast<uint16_t>(sequence_number), static_cast<int>(ecn));
        ++packet_it;
      }
    }  // else - the packet has not been received yet.
    packet_feedback.push_back(
        {.ssrc = ssrc,
         .sequence_number = static_cast<uint16_t>(sequence_number),
         .arrival_time_offset = arrival_time_offset,
         .ecn = ecn});
  }
  last_sequence_number_in_feedback_ = packets_.back().unwrapped_sequence_number;
  packets_.clear();
}
