#include "ice_transport.h"

#include <chrono>
#include <map>
#include <nlohmann/json.hpp>
#include <thread>

#include "common.h"
#include "log.h"
#if __APPLE__
#else
#include "nvcodec_api.h"
#endif

using nlohmann::json;

IceTransport::IceTransport(
    bool offer_peer, std::string &transmission_id, std::string &user_id,
    std::string &remote_user_id, std::shared_ptr<WsClient> ice_ws_transmission,
    std::function<void(std::string, const std::string &)> on_ice_status_change,
    void *user_data)
    : offer_peer_(offer_peer),
      transmission_id_(transmission_id),
      user_id_(user_id),
      remote_user_id_(remote_user_id),
      ice_ws_transport_(ice_ws_transmission),
      on_ice_status_change_(on_ice_status_change),
      user_data_(user_data) {}

IceTransport::~IceTransport() {
  user_data_ = nullptr;
  video_codec_inited_ = false;
  audio_codec_inited_ = false;
  load_nvcodec_dll_success_ = false;

#ifdef __APPLE__
#else
  if (hardware_acceleration_ && load_nvcodec_dll_success_) {
    ReleaseNvCodecDll();
  }
#endif
}

int IceTransport::SetLocalCapabilities(bool hardware_acceleration,
                                       bool use_trickle_ice,
                                       bool use_reliable_ice, bool enable_turn,
                                       bool force_turn,
                                       std::vector<int> &video_payload_types,
                                       std::vector<int> &audio_payload_types) {
  hardware_acceleration_ = hardware_acceleration;
  use_trickle_ice_ = use_trickle_ice;
  use_reliable_ice_ = use_reliable_ice;
  enable_turn_ = enable_turn;
  force_turn_ = force_turn;
  support_video_payload_types_ = video_payload_types;
  support_audio_payload_types_ = audio_payload_types;
  support_data_payload_types_ = {RtpPacket::PAYLOAD_TYPE::DATA};
  return 0;
}

int IceTransport::InitIceTransmission(
    std::string &stun_ip, int stun_port, std::string &turn_ip, int turn_port,
    std::string &turn_username, std::string &turn_password,
    RtpPacket::PAYLOAD_TYPE video_codec_payload_type) {
  ice_io_statistics_ = std::make_unique<IOStatistics>(
      [this](const IOStatistics::NetTrafficStats &net_traffic_stats) {
        if (on_receive_net_status_report_) {
          XNetTrafficStats xnet_traffic_stats;

          memcpy(&xnet_traffic_stats, &net_traffic_stats,
                 sizeof(XNetTrafficStats));
          on_receive_net_status_report_(
              user_id_.data(), user_id_.size(), TraversalMode(traversal_type_),
              &xnet_traffic_stats, remote_user_id_.data(),
              remote_user_id_.size(), user_data_);
        }
      });

  video_codec_payload_type_ = video_codec_payload_type;

  rtp_video_receiver_ = std::make_unique<RtpVideoReceiver>(ice_io_statistics_);
  // rr sender
  rtp_video_receiver_->SetSendDataFunc(
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

        return ice_agent_->Send(data, size);
      });
  rtp_video_receiver_->SetOnReceiveCompleteFrame(
      [this](VideoFrame &video_frame) -> void {
        ice_io_statistics_->UpdateVideoInboundBytes(
            (uint32_t)video_frame.Size());
        [[maybe_unused]] int num_frame_returned = video_decoder_->Decode(
            (uint8_t *)video_frame.Buffer(), video_frame.Size(),
            [this](VideoFrame video_frame) {
              if (on_receive_video_) {
                XVideoFrame x_video_frame;
                x_video_frame.data = (const char *)video_frame.Buffer();
                x_video_frame.width = video_frame.Width();
                x_video_frame.height = video_frame.Height();
                x_video_frame.size = video_frame.Size();
                on_receive_video_(&x_video_frame, remote_user_id_.data(),
                                  remote_user_id_.size(), user_data_);
              }
            });
      });

  rtp_video_receiver_->Start();

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

  rtp_audio_receiver_ = std::make_unique<RtpAudioReceiver>(ice_io_statistics_);
  // rr sender
  rtp_audio_receiver_->SetSendDataFunc(
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

        return ice_agent_->Send(data, size);
      });
  rtp_audio_receiver_->SetOnReceiveData([this](const char *data,
                                               size_t size) -> void {
    ice_io_statistics_->UpdateAudioInboundBytes((uint32_t)size);

    [[maybe_unused]] int num_frame_returned = audio_decoder_->Decode(
        (uint8_t *)data, size, [this](uint8_t *data, int size) {
          if (on_receive_audio_) {
            on_receive_audio_((const char *)data, size, remote_user_id_.data(),
                              remote_user_id_.size(), user_data_);
          }
        });
  });

  rtp_audio_sender_ = std::make_unique<RtpAudioSender>(ice_io_statistics_);
  rtp_audio_sender_->SetSendDataFunc(
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

        ice_io_statistics_->UpdateAudioOutboundBytes((uint32_t)size);
        return ice_agent_->Send(data, size);
      });

  rtp_audio_sender_->Start();

  rtp_data_receiver_ = std::make_unique<RtpDataReceiver>(ice_io_statistics_);
  // rr sender
  rtp_data_receiver_->SetSendDataFunc(
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

        return ice_agent_->Send(data, size);
      });
  rtp_data_receiver_->SetOnReceiveData(
      [this](const char *data, size_t size) -> void {
        ice_io_statistics_->UpdateDataInboundBytes((uint32_t)size);

        if (on_receive_data_) {
          on_receive_data_(data, size, remote_user_id_.data(),
                           remote_user_id_.size(), user_data_);
        }
      });

  rtp_data_sender_ = std::make_unique<RtpDataSender>(ice_io_statistics_);
  rtp_data_sender_->SetSendDataFunc(
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

        ice_io_statistics_->UpdateDataOutboundBytes((uint32_t)size);
        return ice_agent_->Send(data, size);
      });

  rtp_data_sender_->Start();

  ice_agent_ = std::make_unique<IceAgent>(
      offer_peer_, use_trickle_ice_, use_reliable_ice_, enable_turn_,
      force_turn_, stun_ip, stun_port, turn_ip, turn_port, turn_username,
      turn_password);

  ice_agent_->CreateIceAgent(
      []([[maybe_unused]] NiceAgent *agent, [[maybe_unused]] guint stream_id,
         [[maybe_unused]] guint component_id, NiceComponentState state,
         gpointer user_ptr) {
        if (auto ice_transport = static_cast<IceTransport *>(user_ptr)) {
          if (!ice_transport->is_closed_) {
            LOG_INFO("[{}->{}] state_change: {}", ice_transport->user_id_,
                     ice_transport->remote_user_id_,
                     nice_component_state_to_string(state));
            ice_transport->state_ = state;

            if (state == NICE_COMPONENT_STATE_READY ||
                state == NICE_COMPONENT_STATE_CONNECTED) {
              ice_transport->ice_io_statistics_->Start();
            }

            ice_transport->on_ice_status_change_(
                nice_component_state_to_string(state),
                ice_transport->remote_user_id_);
          }
        }
      },
      [](NiceAgent *agent, guint stream_id, guint component_id,
         gchar *foundation, gpointer user_ptr) {
        if (auto ice_transport = static_cast<IceTransport *>(user_ptr)) {
          if (ice_transport->use_trickle_ice_) {
            GSList *cands =
                nice_agent_get_local_candidates(agent, stream_id, component_id);
            NiceCandidate *cand;
            for (GSList *i = cands; i; i = i->next) {
              cand = (NiceCandidate *)i->data;
              if (g_strcmp0(cand->foundation, foundation) == 0) {
                ice_transport->new_local_candidate_ =
                    nice_agent_generate_local_candidate_sdp(agent, cand);

                json message = {
                    {"type", "new_candidate"},
                    {"transmission_id", ice_transport->transmission_id_},
                    {"user_id", ice_transport->user_id_},
                    {"remote_user_id", ice_transport->remote_user_id_},
                    {"sdp", ice_transport->new_local_candidate_}};
                // LOG_INFO("[{}] Send new candidate to [{}]]:[{}]",
                //          ice_transport->user_id_,
                //          ice_transport->remote_user_id_,
                //          ice_transport->new_local_candidate_);

                if (ice_transport->ice_ws_transport_) {
                  ice_transport->ice_ws_transport_->Send(message.dump());
                }
              }
            }

            g_slist_free_full(cands, (GDestroyNotify)nice_candidate_free);
          }
        }
      },
      []([[maybe_unused]] NiceAgent *agent, [[maybe_unused]] guint stream_id,
         gpointer user_ptr) {
        // non-trickle
        if (auto ice_transport = static_cast<IceTransport *>(user_ptr)) {
          LOG_INFO("[{}->{}] gather_done", ice_transport->user_id_,
                   ice_transport->remote_user_id_);

          if (!ice_transport->use_trickle_ice_) {
            if (ice_transport->offer_peer_) {
              ice_transport->SendOffer();
            } else {
              ice_transport->SendAnswer();
            }
          }
        }
      },
      [](NiceAgent *agent, guint stream_id, guint component_id,
         const char *lfoundation, const char *rfoundation, gpointer user_ptr) {
        LOG_INFO("new selected pair: [{}] [{}]", lfoundation, rfoundation);
        NiceCandidate *local = nullptr;
        NiceCandidate *remote = nullptr;
        nice_agent_get_selected_pair(agent, stream_id, component_id, &local,
                                     &remote);
        if (auto ice_transport = static_cast<IceTransport *>(user_ptr)) {
          if (local->type == NICE_CANDIDATE_TYPE_RELAYED &&
              remote->type == NICE_CANDIDATE_TYPE_RELAYED) {
            LOG_INFO("Traversal using relay server");
            ice_transport->traversal_type_ = TraversalType::TRelay;
          } else {
            LOG_INFO("Traversal using p2p");
            ice_transport->traversal_type_ = TraversalType::TP2P;
          }
          XNetTrafficStats net_traffic_stats;
          memset(&net_traffic_stats, 0, sizeof(net_traffic_stats));

          ice_transport->on_receive_net_status_report_(
              ice_transport->user_id_.data(), ice_transport->user_id_.size(),
              TraversalMode(ice_transport->traversal_type_), &net_traffic_stats,
              ice_transport->remote_user_id_.data(),
              ice_transport->remote_user_id_.size(), ice_transport->user_data_);
        }
      },
      []([[maybe_unused]] NiceAgent *agent, [[maybe_unused]] guint stream_id,
         [[maybe_unused]] guint component_id, guint size, gchar *buffer,
         gpointer user_ptr) {
        if (auto ice_transport = static_cast<IceTransport *>(user_ptr)) {
          if (ice_transport && !ice_transport->is_closed_) {
            if (ice_transport->CheckIsRtpPacket(buffer, size)) {
              if (ice_transport->CheckIsVideoPacket(buffer, size)) {
                RtpPacket packet((uint8_t *)buffer, size);
                ice_transport->rtp_video_receiver_->InsertRtpPacket(packet);
              } else if (ice_transport->CheckIsAudioPacket(buffer, size)) {
                RtpPacket packet((uint8_t *)buffer, size);
                ice_transport->rtp_audio_receiver_->InsertRtpPacket(packet);
              } else if (ice_transport->CheckIsDataPacket(buffer, size)) {
                RtpPacket packet((uint8_t *)buffer, size);
                ice_transport->rtp_data_receiver_->InsertRtpPacket(packet);
              }
            } else if (ice_transport->CheckIsRtcpPacket(buffer, size)) {
              LOG_ERROR("Rtcp packet [{}]", (uint8_t)(buffer[1]));
            } else {
              LOG_ERROR("Unknown packet");
            }
          }
        }
      },
      this);
  return 0;
}

int IceTransport::DestroyIceTransmission() {
  LOG_INFO("[{}->{}] Destroy ice transmission", user_id_, remote_user_id_);
  is_closed_ = true;

  if (on_ice_status_change_) {
    on_ice_status_change_("closed", remote_user_id_);
  }

  if (ice_io_statistics_) {
    ice_io_statistics_->Stop();
  }

  if (rtp_video_receiver_) {
    rtp_video_receiver_->Stop();
  }

  if (rtp_video_sender_) {
    rtp_video_sender_->Stop();
  }

  if (rtp_audio_sender_) {
    rtp_audio_sender_->Stop();
  }

  if (rtp_data_sender_) {
    rtp_data_sender_->Stop();
  }

  return ice_agent_->DestroyIceAgent();
}

int IceTransport::CreateMediaCodec() {
  video_rtp_codec_ = std::make_unique<RtpCodec>(negotiated_video_pt_);
  audio_rtp_codec_ = std::make_unique<RtpCodec>(negotiated_audio_pt_);
  data_rtp_codec_ = std::make_unique<RtpCodec>(negotiated_data_pt_);
  return 0;
}

int IceTransport::CreateVideoCodec(RtpPacket::PAYLOAD_TYPE video_pt,
                                   bool hardware_acceleration) {
  if (video_codec_inited_) {
    return 0;
  }

  hardware_acceleration_ = hardware_acceleration;

  if (RtpPacket::PAYLOAD_TYPE::AV1 == video_pt) {
    if (hardware_acceleration_) {
      hardware_acceleration_ = false;
      LOG_WARN("Only support software codec for AV1");
    }
    video_encoder_ = VideoEncoderFactory::CreateVideoEncoder(false, true);
    video_decoder_ = VideoDecoderFactory::CreateVideoDecoder(false, true);
  } else if (RtpPacket::PAYLOAD_TYPE::H264 == video_pt) {
#ifdef __APPLE__
    if (hardware_acceleration_) {
      hardware_acceleration_ = false;
      LOG_WARN(
          "MacOS not support hardware acceleration, use default software "
          "codec");
      video_encoder_ = VideoEncoderFactory::CreateVideoEncoder(false, false);
      video_decoder_ = VideoDecoderFactory::CreateVideoDecoder(false, false);
    } else {
      video_encoder_ = VideoEncoderFactory::CreateVideoEncoder(false, false);
      video_decoder_ = VideoDecoderFactory::CreateVideoDecoder(false, false);
    }
#else
    if (hardware_acceleration_) {
      if (0 == LoadNvCodecDll()) {
        load_nvcodec_dll_success_ = true;
        video_encoder_ = VideoEncoderFactory::CreateVideoEncoder(true, false);
        video_decoder_ = VideoDecoderFactory::CreateVideoDecoder(true, false);
      } else {
        LOG_WARN(
            "Hardware accelerated codec not available, use default software "
            "codec");
        video_encoder_ = VideoEncoderFactory::CreateVideoEncoder(false, false);
        video_decoder_ = VideoDecoderFactory::CreateVideoDecoder(false, false);
      }
    } else {
      video_encoder_ = VideoEncoderFactory::CreateVideoEncoder(false, false);
      video_decoder_ = VideoDecoderFactory::CreateVideoDecoder(false, false);
    }
#endif
  }

  if (!video_encoder_) {
    video_encoder_ = VideoEncoderFactory::CreateVideoEncoder(false, false);
    LOG_ERROR("Create encoder failed, try to use software H.264 encoder");
  }
  if (!video_encoder_ || 0 != video_encoder_->Init()) {
    LOG_ERROR("Encoder init failed");
    return -1;
  }

  if (!video_decoder_) {
    video_decoder_ = VideoDecoderFactory::CreateVideoDecoder(false, false);
    LOG_ERROR("Create decoder failed, try to use software H.264 decoder");
  }
  if (!video_decoder_ || video_decoder_->Init()) {
    LOG_ERROR("Decoder init failed");
    return -1;
  }

  video_codec_inited_ = true;
  LOG_INFO("Create video codec [{}|{}] finish",
           video_encoder_->GetEncoderName(), video_decoder_->GetDecoderName());

  return 0;
}

int IceTransport::CreateAudioCodec() {
  if (audio_codec_inited_) {
    return 0;
  }

  audio_encoder_ = std::make_unique<AudioEncoder>(AudioEncoder(48000, 1, 480));
  if (!audio_encoder_ || 0 != audio_encoder_->Init()) {
    LOG_ERROR("Audio encoder init failed");
    return -1;
  }

  audio_decoder_ = std::make_unique<AudioDecoder>(AudioDecoder(48000, 1, 480));
  if (!audio_decoder_ || 0 != audio_decoder_->Init()) {
    LOG_ERROR("Audio decoder init failed");
    return -1;
  }

  audio_codec_inited_ = true;
  LOG_INFO("Create audio codec [{}|{}] finish",
           audio_encoder_->GetEncoderName(), audio_decoder_->GetDecoderName());

  return 0;
}

int IceTransport::SetTransmissionId(const std::string &transmission_id) {
  transmission_id_ = transmission_id;

  return 0;
}

int IceTransport::JoinTransmission() {
  LOG_INFO("[{}] Join transmission", user_id_);

  if (use_trickle_ice_) {
    SendOffer();
  } else {
    GatherCandidates();
  }
  return 0;
}

int IceTransport::GatherCandidates() {
  int ret = ice_agent_->GatherCandidates();
  if (ret < 0) {
    LOG_ERROR("Gather candidates failed");
  }
  return 0;
}

int IceTransport::SetRemoteSdp(const std::string &remote_sdp) {
  std::string media_stream_sdp = GetRemoteCapabilities(remote_sdp);
  if (media_stream_sdp.empty()) {
    LOG_ERROR("Set remote sdp failed due to negotiation failed");
    return -1;
  }

  ice_agent_->SetRemoteSdp(media_stream_sdp.c_str());
  // LOG_INFO("[{}] set remote sdp", user_id_);

  remote_ice_username_ = GetIceUsername(media_stream_sdp);
  return 0;
}

int IceTransport::SendOffer() {
  local_sdp_ = ice_agent_->GenerateLocalSdp();
  AppendLocalCapabilitiesToOffer(local_sdp_);
  json message = {{"type", "offer"},
                  {"transmission_id", transmission_id_},
                  {"user_id", user_id_},
                  {"remote_user_id", remote_user_id_},
                  {"sdp", local_sdp_.c_str()}};
  // LOG_INFO("Send offer with sdp:\n[\n{}]", local_sdp_.c_str());
  if (ice_ws_transport_) {
    ice_ws_transport_->Send(message.dump());
    LOG_INFO("[{}->{}] send offer", user_id_, remote_user_id_);
  }
  return 0;
}

int IceTransport::SendAnswer() {
  local_sdp_ = ice_agent_->GenerateLocalSdp();
  AppendLocalCapabilitiesToAnswer(local_sdp_);
  json message = {{"type", "answer"},
                  {"transmission_id", transmission_id_},
                  {"user_id", user_id_},
                  {"remote_user_id", remote_user_id_},
                  {"sdp", local_sdp_.c_str()}};
  // LOG_INFO("Send answer with sdp:\n[\n{}]", local_sdp_.c_str());
  if (ice_ws_transport_) {
    ice_ws_transport_->Send(message.dump());
    LOG_INFO("[{}->{}] send answer", user_id_, remote_user_id_);
  }

  return 0;
}

int IceTransport::AppendLocalCapabilitiesToOffer(
    const std::string &remote_sdp) {
  std::string preferred_video_pt;
  std::string to_replace = "ICE/SDP";
  std::string video_capabilities = "UDP/TLS/RTP/SAVPF ";
  std::string audio_capabilities = "UDP/TLS/RTP/SAVPF 111";
  std::string data_capabilities = "UDP/TLS/RTP/SAVPF 127";

  switch (video_codec_payload_type_) {
    case RtpPacket::PAYLOAD_TYPE::H264: {
      preferred_video_pt = std::to_string(RtpPacket::PAYLOAD_TYPE::H264);
      video_capabilities += preferred_video_pt + " 97 98 99";
      break;
    }
    case RtpPacket::PAYLOAD_TYPE::AV1: {
      preferred_video_pt = std::to_string(RtpPacket::PAYLOAD_TYPE::AV1);
      video_capabilities += preferred_video_pt + " 96 97 98";
      break;
    }
    default: {
      preferred_video_pt = std::to_string(RtpPacket::PAYLOAD_TYPE::H264);
      video_capabilities += preferred_video_pt + " 97 98 99";
      break;
    }
  }

  std::size_t video_start = remote_sdp.find("m=video");
  std::size_t video_end = remote_sdp.find("\n", video_start);
  std::size_t audio_start = remote_sdp.find("m=audio");
  std::size_t audio_end = remote_sdp.find("\n", audio_start);
  std::size_t data_start = remote_sdp.find("m=data");
  std::size_t data_end = remote_sdp.find("\n", data_start);

  size_t pos = 0;
  if (video_start != std::string::npos && video_end != std::string::npos) {
    if ((pos = local_sdp_.find(to_replace, video_start)) != std::string::npos) {
      local_sdp_.replace(pos, to_replace.length(), video_capabilities);
      pos += video_capabilities.length();
    }
  }

  if (audio_start != std::string::npos && audio_end != std::string::npos) {
    if ((pos = local_sdp_.find(to_replace, audio_start)) != std::string::npos) {
      local_sdp_.replace(pos, to_replace.length(), audio_capabilities);
      pos += audio_capabilities.length();
    }
  }

  if (data_start != std::string::npos && data_end != std::string::npos) {
    if ((pos = local_sdp_.find(to_replace, data_start)) != std::string::npos) {
      local_sdp_.replace(pos, to_replace.length(), data_capabilities);
      pos += data_capabilities.length();
    }
  }

  return 0;
}

int IceTransport::AppendLocalCapabilitiesToAnswer(
    const std::string &remote_sdp) {
  std::string negotiated_video_pt;
  std::string negotiated_audio_pt;
  std::string negotiated_data_pt;
  std::string to_replace = "ICE/SDP";
  std::string protocol = "UDP/TLS/RTP/SAVPF ";
  negotiated_video_pt = protocol + std::to_string(negotiated_video_pt_);
  negotiated_audio_pt = protocol + std::to_string(negotiated_audio_pt_);
  negotiated_data_pt = protocol + std::to_string(negotiated_data_pt_);

  std::size_t video_start = remote_sdp.find("m=video");
  std::size_t video_end = remote_sdp.find("\n", video_start);

  size_t pos = 0;
  if (video_start != std::string::npos && video_end != std::string::npos) {
    if ((pos = local_sdp_.find(to_replace, video_start)) != std::string::npos) {
      local_sdp_.replace(pos, to_replace.length(), negotiated_video_pt);
      pos += negotiated_video_pt.length();
    }
  }

  std::size_t audio_start = remote_sdp.find("m=audio");
  std::size_t audio_end = remote_sdp.find("\n", audio_start);

  if (audio_start != std::string::npos && audio_end != std::string::npos) {
    if ((pos = local_sdp_.find(to_replace, audio_start)) != std::string::npos) {
      local_sdp_.replace(pos, to_replace.length(), negotiated_audio_pt);
      pos += negotiated_audio_pt.length();
    }
  }

  std::size_t data_start = remote_sdp.find("m=data");
  std::size_t data_end = remote_sdp.find("\n", data_start);

  if (data_start != std::string::npos && data_end != std::string::npos) {
    if ((pos = local_sdp_.find(to_replace, data_start)) != std::string::npos) {
      local_sdp_.replace(pos, to_replace.length(), negotiated_data_pt);
      pos += negotiated_data_pt.length();
    }
  }

  return 0;
}

std::string IceTransport::GetRemoteCapabilities(const std::string &remote_sdp) {
  std::string media_stream_sdp;
  std::size_t video_start = remote_sdp.find("m=video");
  std::size_t video_end = remote_sdp.find("m=audio");
  std::size_t audio_start = video_end;
  std::size_t audio_end = remote_sdp.find("m=data");
  std::size_t data_start = audio_end;
  std::size_t data_end = remote_sdp.find("a=candidate");
  std::size_t candidate_start = data_end;

  if (!remote_capabilities_got_) {
    if (!NegotiateVideoPayloadType(remote_sdp)) {
      return std::string();
    }
    if (!NegotiateAudioPayloadType(remote_sdp)) {
      return std::string();
    }
    if (!NegotiateDataPayloadType(remote_sdp)) {
      return std::string();
    }

    CreateMediaCodec();
    CreateVideoCodec(negotiated_video_pt_, hardware_acceleration_);
    CreateAudioCodec();

    remote_capabilities_got_ = true;
  }

  if ((video_start != std::string::npos && video_end != std::string::npos) ||
      (audio_start != std::string::npos && audio_end != std::string::npos) ||
      (data_start != std::string::npos && data_end != std::string::npos)) {
    if (video_start != std::string::npos && video_end != std::string::npos) {
      media_stream_sdp =
          remote_sdp.substr(video_start, video_end - video_start);
    } else if (audio_start != std::string::npos &&
               audio_end != std::string::npos) {
      media_stream_sdp =
          remote_sdp.substr(audio_start, audio_end - audio_start);
    } else {
      media_stream_sdp = remote_sdp.substr(data_start, data_end - data_start);
    }

    if (candidate_start != std::string::npos) {
      media_stream_sdp += remote_sdp.substr(candidate_start);
    }
  } else {
    return remote_sdp;
  }

  return media_stream_sdp;
}

bool IceTransport::NegotiateVideoPayloadType(const std::string &remote_sdp) {
  std::string remote_video_capabilities;
  std::string local_video_capabilities;
  std::string remote_prefered_video_pt;

  std::size_t start =
      remote_sdp.find("m=video") + std::string("m=video").length();
  if (start != std::string::npos) {
    std::size_t end = remote_sdp.find("\n", start);
    std::string::size_type pos1 = remote_sdp.find(' ', start);
    std::string::size_type pos2 = remote_sdp.find(' ', pos1 + 1);
    std::string::size_type pos3 = remote_sdp.find(' ', pos2 + 1);
    if (end != std::string::npos && pos1 != std::string::npos &&
        pos2 != std::string::npos && pos3 != std::string::npos) {
      remote_video_capabilities = remote_sdp.substr(pos3 + 1, end - pos3 - 1);
    }
  }
  LOG_INFO("remote video capabilities [{}]", remote_video_capabilities.c_str());

  for (size_t index = 0; index < support_video_payload_types_.size(); ++index) {
    if (index == support_video_payload_types_.size() - 1) {
      local_video_capabilities +=
          std::to_string(support_video_payload_types_[index]);
    } else {
      local_video_capabilities +=
          std::to_string(support_video_payload_types_[index]) + " ";
    }
  }
  LOG_INFO("local video capabilities [{}]", local_video_capabilities.c_str());

  std::size_t prefered_pt_start = 0;

  while (prefered_pt_start <= remote_video_capabilities.length()) {
    std::size_t prefered_pt_end =
        remote_video_capabilities.find(' ', prefered_pt_start);
    if (prefered_pt_end != std::string::npos) {
      remote_prefered_video_pt =
          remote_video_capabilities.substr(0, prefered_pt_end);
    } else {
      remote_prefered_video_pt = remote_video_capabilities;
    }

    remote_prefered_video_pt_ =
        (RtpPacket::PAYLOAD_TYPE)(atoi(remote_prefered_video_pt.c_str()));

    bool is_support_this_video_pt =
        std::find(support_video_payload_types_.begin(),
                  support_video_payload_types_.end(),
                  remote_prefered_video_pt_) !=
        support_video_payload_types_.end();

    if (is_support_this_video_pt) {
      negotiated_video_pt_ = (RtpPacket::PAYLOAD_TYPE)remote_prefered_video_pt_;
      break;
    } else {
      if (prefered_pt_end != std::string::npos) {
        prefered_pt_start = prefered_pt_end + 1;
      } else {
        break;
      }
    }
  }

  if (negotiated_video_pt_ == RtpPacket::PAYLOAD_TYPE::UNDEFINED) {
    LOG_ERROR("Negotiate video pt failed");
    return false;
  } else {
    LOG_INFO("negotiated video pt [{}]", (int)negotiated_video_pt_);
    return true;
  }
}

bool IceTransport::NegotiateAudioPayloadType(const std::string &remote_sdp) {
  std::string remote_audio_capabilities;
  std::string remote_prefered_audio_pt;

  std::size_t start =
      remote_sdp.find("m=audio") + std::string("m=audio").length();
  if (start != std::string::npos) {
    std::size_t end = remote_sdp.find("\n", start);
    std::string::size_type pos1 = remote_sdp.find(' ', start);
    std::string::size_type pos2 = remote_sdp.find(' ', pos1 + 1);
    std::string::size_type pos3 = remote_sdp.find(' ', pos2 + 1);
    if (end != std::string::npos && pos1 != std::string::npos &&
        pos2 != std::string::npos && pos3 != std::string::npos) {
      remote_audio_capabilities = remote_sdp.substr(pos3 + 1, end - pos3 - 1);
    }
  }
  LOG_INFO("remote audio capabilities [{}]", remote_audio_capabilities.c_str());

  std::size_t prefered_pt_start = 0;

  while (prefered_pt_start <= remote_audio_capabilities.length()) {
    std::size_t prefered_pt_end =
        remote_audio_capabilities.find(' ', prefered_pt_start);
    if (prefered_pt_end != std::string::npos) {
      remote_prefered_audio_pt =
          remote_audio_capabilities.substr(0, prefered_pt_end);
    } else {
      remote_prefered_audio_pt = remote_audio_capabilities;
    }

    remote_prefered_audio_pt_ =
        (RtpPacket::PAYLOAD_TYPE)(atoi(remote_prefered_audio_pt.c_str()));

    bool is_support_this_audio_pt =
        std::find(support_audio_payload_types_.begin(),
                  support_audio_payload_types_.end(),
                  remote_prefered_audio_pt_) !=
        support_audio_payload_types_.end();

    if (is_support_this_audio_pt) {
      negotiated_audio_pt_ = (RtpPacket::PAYLOAD_TYPE)remote_prefered_audio_pt_;
      break;
    } else {
      if (prefered_pt_end != std::string::npos) {
        prefered_pt_start = prefered_pt_end + 1;
      } else {
        break;
      }
    }
  }

  if (negotiated_audio_pt_ == RtpPacket::PAYLOAD_TYPE::UNDEFINED) {
    LOG_ERROR("Negotiate audio pt failed");
    return false;
  } else {
    LOG_INFO("negotiated audio pt [{}]", (int)negotiated_audio_pt_);
    return true;
  }
}

bool IceTransport::NegotiateDataPayloadType(const std::string &remote_sdp) {
  std::string remote_data_capabilities;
  std::string remote_prefered_data_pt;

  std::size_t start =
      remote_sdp.find("m=data") + std::string("m=data").length();
  if (start != std::string::npos) {
    std::size_t end = remote_sdp.find("\n", start);
    std::string::size_type pos1 = remote_sdp.find(' ', start);
    std::string::size_type pos2 = remote_sdp.find(' ', pos1 + 1);
    std::string::size_type pos3 = remote_sdp.find(' ', pos2 + 1);
    if (end != std::string::npos && pos1 != std::string::npos &&
        pos2 != std::string::npos && pos3 != std::string::npos) {
      remote_data_capabilities = remote_sdp.substr(pos3 + 1, end - pos3 - 1);
    }
  }
  LOG_INFO("remote data capabilities [{}]", remote_data_capabilities.c_str());

  std::size_t prefered_pt_start = 0;

  while (prefered_pt_start <= remote_data_capabilities.length()) {
    std::size_t prefered_pt_end =
        remote_data_capabilities.find(' ', prefered_pt_start);
    if (prefered_pt_end != std::string::npos) {
      remote_prefered_data_pt =
          remote_data_capabilities.substr(0, prefered_pt_end);
    } else {
      remote_prefered_data_pt = remote_data_capabilities;
    }

    remote_prefered_data_pt_ =
        (RtpPacket::PAYLOAD_TYPE)(atoi(remote_prefered_data_pt.c_str()));

    bool is_support_this_data_pt =
        std::find(support_data_payload_types_.begin(),
                  support_data_payload_types_.end(),
                  remote_prefered_data_pt_) !=
        support_data_payload_types_.end();

    if (is_support_this_data_pt) {
      negotiated_data_pt_ = (RtpPacket::PAYLOAD_TYPE)remote_prefered_data_pt_;
      break;
    } else {
      if (prefered_pt_end != std::string::npos) {
        prefered_pt_start = prefered_pt_end + 1;
      } else {
        break;
      }
    }
  }

  if (negotiated_data_pt_ == RtpPacket::PAYLOAD_TYPE::UNDEFINED) {
    LOG_ERROR("Negotiate data pt failed");
    return false;
  } else {
    LOG_INFO("negotiated data pt [{}]", (int)negotiated_data_pt_);
    return true;
  }
}

std::vector<RtpPacket::PAYLOAD_TYPE> IceTransport::GetNegotiatedCapabilities() {
  return {negotiated_video_pt_, negotiated_audio_pt_, negotiated_data_pt_};
}

int IceTransport::SendVideoFrame(const XVideoFrame *video_frame) {
  if (state_ != NICE_COMPONENT_STATE_CONNECTED &&
      state_ != NICE_COMPONENT_STATE_READY) {
    LOG_ERROR("Ice is not connected, state = [{}]",
              nice_component_state_to_string(state_));
    return -2;
  }

  if (b_force_i_frame_) {
    video_encoder_->ForceIdr();
    LOG_INFO("Force I frame");
    b_force_i_frame_ = false;
  }

  int ret = video_encoder_->Encode(
      video_frame,
      [this](char *encoded_frame, size_t size,
             VideoEncoder::VideoFrameType frame_type) -> int {
        if (video_channel_send_) {
          video_channel_send_->SendVideo(encoded_frame, size);
        }

        return 0;
      });

  if (0 != ret) {
    LOG_ERROR("Encode failed");
    return -1;
  }

  return 0;
}

int IceTransport::SendAudioFrame(const char *data, size_t size) {
  if (state_ != NICE_COMPONENT_STATE_CONNECTED &&
      state_ != NICE_COMPONENT_STATE_READY) {
    LOG_ERROR("Ice is not connected, state = [{}]",
              nice_component_state_to_string(state_));
    return -2;
  }

  int ret = audio_encoder_->Encode(
      (uint8_t *)data, size,
      [this](char *encoded_audio_buffer, size_t size) -> int {
        if (rtp_audio_sender_) {
          if (audio_rtp_codec_) {
            std::vector<RtpPacket> packets;
            audio_rtp_codec_->Encode((uint8_t *)encoded_audio_buffer,
                                     (uint32_t)size, packets);
            rtp_audio_sender_->Enqueue(packets);
          }
        }
        return 0;
      });

  return ret;
}

int IceTransport::SendDataFrame(const char *data, size_t size) {
  if (state_ != NICE_COMPONENT_STATE_CONNECTED &&
      state_ != NICE_COMPONENT_STATE_READY) {
    LOG_ERROR("Ice is not connected, state = [{}]",
              nice_component_state_to_string(state_));
    return -2;
  }

  std::vector<RtpPacket> packets;

  if (rtp_data_sender_) {
    if (data_rtp_codec_) {
      data_rtp_codec_->Encode((uint8_t *)data, (uint32_t)size, packets);
      rtp_data_sender_->Enqueue(packets);
    }
  }

  return 0;
}

uint8_t IceTransport::CheckIsRtpPacket(const char *buffer, size_t size) {
  if (size < 2) {
    return 0;
  }

  uint8_t payload_type = buffer[1] & 0x7F;
  if (payload_type >= 96 && payload_type <= 127) {
    return payload_type;
  } else {
    return 0;
  }
}

uint8_t IceTransport::CheckIsRtcpPacket(const char *buffer, size_t size) {
  if (size < 2) {
    return 0;
  }

  uint8_t payload_type = buffer[1];
  if (payload_type >= 192 && payload_type <= 223) {
    return payload_type;
    ;
  }

  return 0;
}

uint8_t IceTransport::CheckIsVideoPacket(const char *buffer, size_t size) {
  if (size < 4) {
    return 0;
  }

  uint8_t v = (buffer[0] >> 6) & 0x03;
  if (2 != v) {
    return 0;
  }

  uint8_t pt = buffer[1] & 0x7F;
  if (RtpPacket::PAYLOAD_TYPE::H264 == pt ||
      RtpPacket::PAYLOAD_TYPE::H264_FEC_SOURCE == pt ||
      RtpPacket::PAYLOAD_TYPE::H264_FEC_REPAIR == pt ||
      RtpPacket::PAYLOAD_TYPE::AV1 == pt) {
    return pt;
  } else {
    return 0;
  }
}

uint8_t IceTransport::CheckIsAudioPacket(const char *buffer, size_t size) {
  if (size < 4) {
    return 0;
  }

  uint8_t v = (buffer[0] >> 6) & 0x03;
  if (2 != v) {
    return 0;
  }

  uint8_t pt = buffer[1] & 0x7F;
  if (RtpPacket::PAYLOAD_TYPE::OPUS == pt) {
    return pt;
  } else {
    return 0;
  }
}

uint8_t IceTransport::CheckIsDataPacket(const char *buffer, size_t size) {
  if (size < 4) {
    return 0;
  }

  uint8_t v = (buffer[0] >> 6) & 0x03;
  if (2 != v) {
    return 0;
  }

  uint8_t pt = buffer[1] & 0x7F;
  if (RtpPacket::PAYLOAD_TYPE::DATA == pt) {
    return pt;
  } else {
    return 0;
  }
}
