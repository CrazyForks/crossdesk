#ifndef _ICE_AGENT_H_
#define _ICE_AGENT_H_

#include <atomic>
#include <iostream>
#include <thread>

#include "gio/gnetworking.h"
#include "glib.h"
#include "nice/agent.h"

typedef void (*nice_cb_state_changed_t)(NiceAgent* agent, guint stream_id,
                                        guint component_id,
                                        NiceComponentState state,
                                        gpointer data);
typedef void (*nice_cb_new_candidate_t)(NiceAgent* agent, guint stream_id,
                                        guint component_id, gchar* foundation,
                                        gpointer data);
typedef void (*nice_cb_new_selected_pair_t)(NiceAgent* agent, guint stream_id,
                                            guint component_id,
                                            const char* lfoundation,
                                            const char* rfoundation,
                                            gpointer data);
typedef void (*nice_cb_gathering_done_t)(NiceAgent* agent, guint stream_id,
                                         gpointer data);
typedef void (*nice_cb_recv_t)(NiceAgent* agent, guint stream_id,
                               guint component_id, guint size, gchar* buffer,
                               gpointer data);

class IceAgent {
 public:
  IceAgent(bool enable_turn, bool trickle_ice, bool offer_peer,
           std::string& stun_ip, uint16_t stun_port, std::string& turn_ip,
           uint16_t turn_port, std::string& turn_username,
           std::string& turn_password);
  ~IceAgent();

  int CreateIceAgent(nice_cb_state_changed_t on_state_changed,
                     nice_cb_new_candidate_t on_new_candidate,
                     nice_cb_gathering_done_t on_gathering_done,
                     nice_cb_new_selected_pair_t on_new_selected_pair,
                     nice_cb_recv_t on_recv, void* user_ptr);

  int DestroyIceAgent();

  char* GetLocalStreamSdp();

  char* GenerateLocalSdp();

  int SetRemoteSdp(const char* remote_sdp);

  int GatherCandidates();

  NiceComponentState GetIceState();

  int SetRemoteGatheringDone();

  int Send(const char* data, size_t size);

 public:
  bool enable_turn_ = false;
  std::string stun_ip_ = "";
  uint16_t stun_port_ = 0;
  std::string turn_ip_ = "";
  uint16_t turn_port_ = 0;
  std::string turn_username_ = "";
  std::string turn_password_ = "";

  std::unique_ptr<std::thread> nice_thread_;
  std::atomic<NiceAgent*> agent_{nullptr};
  std::atomic<GMainLoop*> gloop_{nullptr};
  std::atomic<bool> nice_inited_{false};

  gboolean exit_nice_thread_ = false;
  bool trickle_ice_ = true;
  bool controlling_ = false;
  gchar* ice_ufrag_ = nullptr;
  gchar* ice_password_ = nullptr;
  gchar* stream_sdp_ = nullptr;
  uint32_t stream_id_ = 0;
  uint32_t n_components_ = 1;
  char* local_sdp_ = nullptr;
  NiceComponentState state_ = NiceComponentState::NICE_COMPONENT_STATE_LAST;
  bool destroyed_ = false;
  gboolean agent_closed_ = false;

  nice_cb_state_changed_t on_state_changed_;
  nice_cb_new_selected_pair_t on_new_selected_pair_;
  nice_cb_new_candidate_t on_new_candidate_;
  nice_cb_gathering_done_t on_gathering_done_;
  nice_cb_recv_t on_recv_;
  void* user_ptr_;
};

#endif