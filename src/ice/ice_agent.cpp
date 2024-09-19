#include "ice_agent.h"

#include <glib.h>

#include <cstring>
#include <iostream>

#include "log.h"

IceAgent::IceAgent(bool offer_peer, bool use_trickle_ice, bool use_reliable_ice,
                   bool enable_turn, bool force_turn, std::string &stun_ip,
                   uint16_t stun_port, std::string &turn_ip, uint16_t turn_port,
                   std::string &turn_username, std::string &turn_password)
    : stun_ip_(stun_ip),
      use_trickle_ice_(use_trickle_ice),
      use_reliable_ice_(use_reliable_ice),
      enable_turn_(enable_turn),
      force_turn_(force_turn),
      stun_port_(stun_port),
      turn_ip_(turn_ip),
      turn_port_(turn_port),
      turn_username_(turn_username),
      turn_password_(turn_password),
      controlling_(offer_peer) {}

IceAgent::~IceAgent() {
  if (!destroyed_) {
    DestroyIceAgent();
  }
  g_object_unref(agent_);
  g_free(ice_ufrag_);
  g_free(ice_password_);
}

int IceAgent::CreateIceAgent(nice_cb_state_changed_t on_state_changed,
                             nice_cb_new_candidate_t on_new_candidate,
                             nice_cb_gathering_done_t on_gathering_done,
                             nice_cb_new_selected_pair_t on_new_selected_pair,
                             nice_cb_recv_t on_recv, void *user_ptr) {
  destroyed_ = false;
  on_state_changed_ = on_state_changed;
  on_new_selected_pair_ = on_new_selected_pair;
  on_new_candidate_ = on_new_candidate;

  on_gathering_done_ = on_gathering_done;
  on_recv_ = on_recv;
  user_ptr_ = user_ptr;

  g_networking_init();

  exit_nice_thread_ = false;

  nice_thread_ = std::thread([this]() {
    gloop_ = g_main_loop_new(nullptr, false);

    agent_ = nice_agent_new_full(
        g_main_loop_get_context(gloop_), NICE_COMPATIBILITY_RFC5245,
        (NiceAgentOption)(use_trickle_ice_
                              ? (NICE_AGENT_OPTION_ICE_TRICKLE |
                                 (use_reliable_ice_ ? NICE_AGENT_OPTION_RELIABLE
                                                    : NICE_AGENT_OPTION_NONE))
                              : (use_reliable_ice_ ? NICE_AGENT_OPTION_RELIABLE
                                                   : NICE_AGENT_OPTION_NONE)));

    LOG_INFO(
        "Nice agent init with [trickle ice|{}], [reliable mode|{}], [turn "
        "support|{}], [force turn|{}]]",
        use_trickle_ice_, use_reliable_ice_, enable_turn_, force_turn_);

    if (agent_ == nullptr) {
      LOG_ERROR("Failed to create agent_");
    }

    g_object_set(agent_, "stun-server", stun_ip_.c_str(), nullptr);
    g_object_set(agent_, "stun-server-port", stun_port_, nullptr);
    g_object_set(agent_, "controlling-mode", controlling_, nullptr);

    g_signal_connect(agent_, "candidate-gathering-done",
                     G_CALLBACK(on_gathering_done_), user_ptr_);
    g_signal_connect(agent_, "new-selected-pair",
                     G_CALLBACK(on_new_selected_pair_), user_ptr_);
    g_signal_connect(agent_, "new-candidate", G_CALLBACK(on_new_candidate_),
                     user_ptr_);
    g_signal_connect(agent_, "component-state-changed",
                     G_CALLBACK(on_state_changed_), user_ptr_);

    stream_id_ = nice_agent_add_stream(agent_, n_components_);
    if (stream_id_ == 0) {
      LOG_ERROR("Failed to add stream");
    }

    if (has_video_stream_) {
      nice_agent_set_stream_name(agent_, stream_id_, "video");
    }

    if (enable_turn_) {
      nice_agent_set_relay_info(agent_, stream_id_, n_components_,
                                turn_ip_.c_str(), turn_port_,
                                turn_username_.c_str(), turn_password_.c_str(),
                                NICE_RELAY_TYPE_TURN_TCP);
    }

    if (force_turn_) {
      g_object_set(agent_, "force-relay", true, NULL);
    }

    nice_agent_attach_recv(agent_, stream_id_, NICE_COMPONENT_TYPE_RTP,
                           g_main_loop_get_context(gloop_), on_recv_,
                           user_ptr_);

    nice_inited_ = true;
    g_main_loop_run(gloop_);
    exit_nice_thread_ = true;
  });

  do {
    g_usleep(1000);
  } while (!nice_inited_);

  LOG_INFO("Nice agent init finish");

  return 0;
}

void cb_closed(GObject *src, GAsyncResult *res, gpointer data) {
  NiceAgent *agent = NICE_AGENT(src);
  g_debug("test-turn:%s: %p", G_STRFUNC, agent);

  *((gboolean *)data) = TRUE;
}

int IceAgent::DestroyIceAgent() {
  if (!nice_inited_) {
    LOG_ERROR("Nice agent has not been initialized");
    return -1;
  }

  nice_agent_remove_stream(agent_, stream_id_);
  nice_agent_close_async(agent_, cb_closed, &agent_closed_);

  destroyed_ = true;
  g_main_loop_quit(gloop_);

  if (nice_thread_.joinable()) {
    nice_thread_.join();
  }

  LOG_INFO("Destroy nice agent success");
  return 0;
}

const char *IceAgent::GenerateLocalSdp() {
  if (!nice_inited_) {
    LOG_ERROR("Nice agent has not been initialized");
    return nullptr;
  }

  if (nullptr == agent_) {
    LOG_ERROR("Nice agent is nullptr");
    return nullptr;
  }

  if (destroyed_) {
    LOG_ERROR("Nice agent is destroyed");
    return nullptr;
  }

  video_stream_sdp_ = nice_agent_generate_local_sdp(agent_);
  audio_stream_sdp_ = video_stream_sdp_;
  data_stream_sdp_ = video_stream_sdp_;
  local_sdp_ = video_stream_sdp_;

  if (has_audio_stream_) {
    std::string to_replace = "video";
    std::string replacement = "audio";
    size_t pos = 0;
    while ((pos = audio_stream_sdp_.find(to_replace, pos)) !=
           std::string::npos) {
      audio_stream_sdp_.replace(pos, to_replace.length(), replacement);
      pos += replacement.length();
    }
    local_sdp_ += audio_stream_sdp_;
  }

  if (has_data_stream_) {
    std::string to_replace = "video";
    std::string replacement = "data";
    size_t pos = 0;
    while ((pos = data_stream_sdp_.find(to_replace, pos)) !=
           std::string::npos) {
      data_stream_sdp_.replace(pos, to_replace.length(), replacement);
      pos += replacement.length();
    }
    local_sdp_ += data_stream_sdp_;
  }

  // LOG_INFO("Generate local sdp:[\n{}]", local_sdp_.c_str());

  return local_sdp_.c_str();
}

const char *IceAgent::GetLocalStreamSdp(uint32_t stream_id) {
  if (!nice_inited_) {
    LOG_ERROR("Nice agent has not been initialized");
    return nullptr;
  }

  if (nullptr == agent_) {
    LOG_ERROR("Nice agent is nullptr");
    return nullptr;
  }

  if (destroyed_) {
    LOG_ERROR("Nice agent is destroyed");
    return nullptr;
  }

  local_sdp_ = nice_agent_generate_local_stream_sdp(agent_, stream_id, true);
  return local_sdp_.c_str();
}

int IceAgent::SetRemoteSdp(const char *remote_sdp) {
  if (!nice_inited_) {
    LOG_ERROR("Nice agent has not been initialized");
    return -1;
  }

  if (nullptr == agent_) {
    LOG_ERROR("Nice agent is nullptr");
    return -1;
  }

  if (destroyed_) {
    LOG_ERROR("Nice agent is destroyed");
    return -1;
  }

  int ret = nice_agent_parse_remote_sdp(agent_, remote_sdp);
  if (ret >= 0) {
    return 0;
  } else {
    LOG_ERROR("Failed to parse remote sdp: [{}]", remote_sdp);
    return -1;
  }
}

int IceAgent::GatherCandidates() {
  if (!nice_inited_) {
    LOG_ERROR("Nice agent has not been initialized");
    return -1;
  }

  if (nullptr == agent_) {
    LOG_ERROR("Nice agent is nullptr");
    return -1;
  }

  if (destroyed_) {
    LOG_ERROR("Nice agent is destroyed");
    return -1;
  }

  if (!nice_agent_gather_candidates(agent_, stream_id_)) {
    LOG_ERROR("Failed to start candidate gathering");
    return -1;
  }

  return 0;
}

NiceComponentState IceAgent::GetIceState() {
  if (!nice_inited_) {
    LOG_ERROR("Nice agent has not been initialized");
    return NiceComponentState::NICE_COMPONENT_STATE_LAST;
  }

  if (nullptr == agent_) {
    LOG_ERROR("Nice agent is nullptr");
    return NiceComponentState::NICE_COMPONENT_STATE_LAST;
  }

  if (destroyed_) {
    LOG_ERROR("Nice agent is destroyed");
    return NiceComponentState::NICE_COMPONENT_STATE_LAST;
  }

  state_ = nice_agent_get_component_state(agent_, stream_id_, 1);

  return state_;
}

int IceAgent::Send(const char *data, size_t size) {
  if (!nice_inited_) {
    LOG_ERROR("Nice agent has not been initialized");
    return -1;
  }

  if (nullptr == agent_) {
    LOG_ERROR("Nice agent is nullptr");
    return -1;
  }

  if (destroyed_) {
    LOG_ERROR("Nice agent is destroyed");
    return -1;
  }

  // if (NiceComponentState::NICE_COMPONENT_STATE_READY !=
  //     nice_agent_get_component_state(agent_, stream_id_, 1)) {
  //   LOG_ERROR("Nice agent not ready");
  //   return -1;
  // }

  int ret = nice_agent_send(agent_, stream_id_, 1, size, data);
  return 0;
}