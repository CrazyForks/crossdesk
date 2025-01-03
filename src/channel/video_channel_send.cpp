#include "video_channel_send.h"

VideoChannelSend::VideoChannelSend() {}

VideoChannelSend::~VideoChannelSend() {}

void VideoChannelSend::Initialize(RtpPacket::PAYLOAD_TYPE negotiated_video_pt) {
  video_rtp_codec_ = std::make_unique<RtpCodec>(negotiated_video_pt);

  rtp_video_sender_ = std::make_unique<RtpVideoSender>(ice_io_statistics_);
  rtp_video_sender_->SetSendDataFunc(
      [this](const char *data, size_t size) -> int {
        if (!ice_agent_) {
          LOG_ERROR("ice_agent_ is nullptr");
          return -1;
        }

        if (state_ != NICE_COMPONENT_STATE_CONNECTED &&
            state_ != NICE_COMPONENT_STATE_READY) {
          LOG_ERROR("Ice is not connected, state = [{}]",
                    nice_component_state_to_string(state_));
          return -2;
        }

        ice_io_statistics_->UpdateVideoOutboundBytes((uint32_t)size);
        return ice_agent_->Send(data, size);
      });

  rtp_video_sender_->Start();
}

void VideoChannelSend::Destroy() {
  if (rtp_video_sender_) {
    rtp_video_sender_->Stop();
  }
}

int VideoChannelSend::SendVideo(char *encoded_frame, size_t size) {
  std::vector<RtpPacket> packets;
  if (rtp_video_sender_) {
    if (video_rtp_codec_) {
      video_rtp_codec_->Encode(
          static_cast<RtpCodec::VideoFrameType>(frame_type),
          (uint8_t *)encoded_frame, (uint32_t)size, packets);
    }
    rtp_video_sender_->Enqueue(packets);
  }

  return 0;
}