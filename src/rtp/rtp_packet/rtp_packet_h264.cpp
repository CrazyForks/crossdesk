#include "rtp_packet_h264.h"

RtpPacketH264::RtpPacketH264() {}

RtpPacketH264::~RtpPacketH264() {}

bool RtpPacketH264::GetFrameHeaderInfo() {
  if (fu_info_got_) {
    return true;
  }

  const uint8_t* frame_buffer = Payload();

  fu_indicator_.forbidden_bit = (frame_buffer[0] >> 7) & 0x01;
  fu_indicator_.nal_reference_idc = (frame_buffer[0] >> 5) & 0x03;
  fu_indicator_.nal_unit_type = frame_buffer[0] & 0x1F;

  if (rtp::NAL_UNIT_TYPE::NALU == fu_indicator_.nal_unit_type) {
    add_offset_to_payload(1);
  } else if (rtp::NAL_UNIT_TYPE::FU_A == fu_indicator_.nal_unit_type) {
    fu_header_.start = (frame_buffer[1] >> 7) & 0x01;
    fu_header_.end = (frame_buffer[1] >> 6) & 0x01;
    fu_header_.remain_bit = (frame_buffer[1] >> 5) & 0x01;
    fu_header_.nal_unit_type = frame_buffer[1] & 0x1F;
    add_offset_to_payload(2);
  }

  fu_info_got_ = true;

  return true;
}