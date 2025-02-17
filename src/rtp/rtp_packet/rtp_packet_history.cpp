#include "rtp_packet_history.h"

#include "log.h"
#include "sequence_number_compare.h"

RtpPacketHistory::RtpPacketHistory(std::shared_ptr<webrtc::Clock> clock)
    : clock_(clock),
      rtt_(webrtc::TimeDelta::MinusInfinity()),
      number_to_store_(0),
      packets_inserted_(0) {}

RtpPacketHistory::~RtpPacketHistory() {}

void RtpPacketHistory::SetRtt(webrtc::TimeDelta rtt) {
  rtt_ = rtt;
  RemoveDeadPackets();
}

void RtpPacketHistory::AddPacket(
    std::shared_ptr<webrtc::RtpPacketToSend> rtp_packet,
    webrtc::Timestamp send_time) {
  RemoveDeadPackets();
  const uint16_t rtp_seq_no = rtp_packet->SequenceNumber();
  int packet_index = GetPacketIndex(rtp_packet->SequenceNumber());
  if (packet_index >= 0 &&
      static_cast<size_t>(packet_index) < rtp_packet_history_.size() &&
      rtp_packet_history_[packet_index].rtp_packet != nullptr) {
    LOG_WARN("Duplicate packet inserted: {}", rtp_seq_no);
    // Remove previous packet to avoid inconsistent state.
    RemovePacket(packet_index);
    packet_index = GetPacketIndex(rtp_seq_no);
  }

  // Packet to be inserted ahead of first packet, expand front.
  for (; packet_index < 0; ++packet_index) {
    rtp_packet_history_.emplace_front();
  }
  // Packet to be inserted behind last packet, expand back.
  while (static_cast<int>(rtp_packet_history_.size()) <= packet_index) {
    rtp_packet_history_.emplace_back();
  }

  rtp_packet_history_[packet_index] = {rtp_packet, send_time,
                                       packets_inserted_++};
}

void RtpPacketHistory::RemoveDeadPackets() {
  webrtc::Timestamp now = clock_->CurrentTime();
  webrtc::TimeDelta packet_duration =
      rtt_.IsFinite()
          ? (std::max)(kMinPacketDurationRtt * rtt_, kMinPacketDuration)
          : kMinPacketDuration;
  while (!rtp_packet_history_.empty()) {
    if (rtp_packet_history_.size() >= kMaxCapacity) {
      // We have reached the absolute max capacity, remove one packet
      // unconditionally.
      RemovePacket(0);
      continue;
    }

    const RtpPacketToSendInfo& stored_packet = rtp_packet_history_.front();

    if (stored_packet.send_time + packet_duration > now) {
      // Don't cull packets too early to avoid failed retransmission requests.
      return;
    }

    if (rtp_packet_history_.size() >= number_to_store_ ||
        stored_packet.send_time +
                (packet_duration * kPacketCullingDelayFactor) <=
            now) {
      // Too many packets in history, or this packet has timed out. Remove it
      // and continue.
      RemovePacket(0);
    } else {
      // No more packets can be removed right now.
      return;
    }
  }
}

std::shared_ptr<webrtc::RtpPacketToSend> RtpPacketHistory::RemovePacket(
    int packet_index) {
  // Move the packet out from the StoredPacket container.
  std::shared_ptr<webrtc::RtpPacketToSend> rtp_packet =
      std::move(rtp_packet_history_[packet_index].rtp_packet);
  if (packet_index == 0) {
    while (!rtp_packet_history_.empty() &&
           rtp_packet_history_.front().rtp_packet == nullptr) {
      rtp_packet_history_.pop_front();
    }
  }

  return rtp_packet;
}

int RtpPacketHistory::GetPacketIndex(uint16_t sequence_number) const {
  if (rtp_packet_history_.empty()) {
    return 0;
  }

  int first_seq = rtp_packet_history_.front().rtp_packet->SequenceNumber();
  if (first_seq == sequence_number) {
    return 0;
  }

  int packet_index = sequence_number - first_seq;
  constexpr int kSeqNumSpan = std::numeric_limits<uint16_t>::max() + 1;

  if (IsNewerSequenceNumber(sequence_number, first_seq)) {
    if (sequence_number < first_seq) {
      // Forward wrap.
      packet_index += kSeqNumSpan;
    }
  } else if (sequence_number > first_seq) {
    // Backwards wrap.
    packet_index -= kSeqNumSpan;
  }

  return packet_index;
}