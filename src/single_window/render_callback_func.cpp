#include "device_controller.h"
#include "localization.h"
#include "platform.h"
#include "rd_log.h"
#include "render.h"

// Refresh Event
#define REFRESH_EVENT (SDL_USEREVENT + 1)
#define NV12_BUFFER_SIZE 1280 * 720 * 3 / 2

#ifdef REMOTE_DESK_DEBUG
#else
#define MOUSE_CONTROL 1
#endif

int Render::SendKeyEvent(int key_code, bool is_down) {
  RemoteAction remote_action;
  remote_action.type = ControlType::keyboard;
  if (is_down) {
    remote_action.k.flag = KeyFlag::key_down;
  } else {
    remote_action.k.flag = KeyFlag::key_up;
  }
  remote_action.k.key_value = key_code;

  SendDataFrame(peer_, (const char *)&remote_action, sizeof(remote_action));

  return 0;
}

int Render::ProcessKeyEvent(int key_code, bool is_down) {
  if (keyboard_capturer_) {
    keyboard_capturer_->SendKeyboardCommand(key_code, is_down);
  }

  return 0;
}

void Render::SdlCaptureAudioIn(void *userdata, Uint8 *stream, int len) {
  Render *render = (Render *)userdata;
  if (!render) {
    return;
  }

  if (1) {
    if ("Connected" == render->connection_status_str_) {
      SendAudioFrame(render->peer_, (const char *)stream, len);
    }
  } else {
    memcpy(render->audio_buffer_, stream, len);
    render->audio_len_ = len;
    SDL_Delay(10);
    render->audio_buffer_fresh_ = true;
  }
}

void Render::SdlCaptureAudioOut([[maybe_unused]] void *userdata,
                                [[maybe_unused]] Uint8 *stream,
                                [[maybe_unused]] int len) {
  // Render *render = (Render *)userdata;
  // if ("Connected" == render->connection_status_str_) {
  //   SendAudioFrame(render->peer_,  (const char *)stream, len);
  // }

  // if (!render->audio_buffer_fresh_) {
  //   return;
  // }

  // SDL_memset(stream, 0, len);

  // if (render->audio_len_ == 0) {
  //   return;
  // } else {
  // }

  // len = (len > render->audio_len_ ? render->audio_len_ : len);
  // SDL_MixAudioFormat(stream, render->audio_buffer_, AUDIO_S16LSB, len,
  //                    SDL_MIX_MAXVOLUME);
  // render->audio_buffer_fresh_ = false;
}

void Render::OnReceiveVideoBufferCb(const XVideoFrame *video_frame,
                                    [[maybe_unused]] const char *user_id,
                                    [[maybe_unused]] size_t user_id_size,
                                    void *user_data) {
  Render *render = (Render *)user_data;
  if (!render) {
    return;
  }

  if (render->connection_established_) {
    if (!render->dst_buffer_) {
      render->dst_buffer_capacity_ = video_frame->size;
      render->dst_buffer_ = new unsigned char[video_frame->size];
    }

    if (render->dst_buffer_capacity_ < video_frame->size) {
      delete render->dst_buffer_;
      render->dst_buffer_capacity_ = video_frame->size;
      render->dst_buffer_ = new unsigned char[video_frame->size];
    }

    memcpy(render->dst_buffer_, video_frame->data, video_frame->size);
    render->video_width_ = video_frame->width;
    render->video_height_ = video_frame->height;
    render->video_size_ = video_frame->size;

    SDL_Event event;
    event.type = REFRESH_EVENT;
    SDL_PushEvent(&event);
    render->streaming_ = true;
  }
}

void Render::OnReceiveAudioBufferCb(const char *data, size_t size,
                                    [[maybe_unused]] const char *user_id,
                                    [[maybe_unused]] size_t user_id_size,
                                    void *user_data) {
  Render *render = (Render *)user_data;
  if (!render) {
    return;
  }

  render->audio_buffer_fresh_ = true;
  SDL_QueueAudio(render->output_dev_, data, (uint32_t)size);
}

void Render::OnReceiveDataBufferCb(const char *data, size_t size,
                                   const char *user_id, size_t user_id_size,
                                   void *user_data) {
  Render *render = (Render *)user_data;
  if (!render) {
    LOG_ERROR("??????????????????");
    return;
  }

  std::string user(user_id, user_id_size);
  LOG_INFO("Receive data from: {}", user);
  RemoteAction remote_action;
  memcpy(&remote_action, data, size);

  if (ControlType::mouse == remote_action.type && render->mouse_controller_) {
    render->mouse_controller_->SendCommand(remote_action);
  } else if (ControlType::audio_capture == remote_action.type) {
    if (remote_action.a) {
      render->StartSpeakerCapturer();
    } else {
      render->StopSpeakerCapturer();
    }
  } else if (ControlType::keyboard == remote_action.type) {
    render->ProcessKeyEvent((int)remote_action.k.key_value,
                            remote_action.k.flag == KeyFlag::key_down);
  } else if (ControlType::host_infomation == remote_action.type) {
    render->remote_host_name_ =
        std::string(remote_action.i.host_name, remote_action.i.host_name_size);
    LOG_INFO("Remote hostname: [{}]", render->remote_host_name_);
  } else {
    LOG_ERROR("Unknown control type: {}", (int)remote_action.type);
  }
}

void Render::OnSignalStatusCb(SignalStatus status, void *user_data) {
  Render *render = (Render *)user_data;
  if (!render) {
    return;
  }

  render->signal_status_ = status;
  if (SignalStatus::SignalConnecting == status) {
    render->signal_status_str_ = "SignalConnecting";
    render->signal_connected_ = false;
  } else if (SignalStatus::SignalConnected == status) {
    render->signal_status_str_ = "SignalConnected";
    render->signal_connected_ = true;
  } else if (SignalStatus::SignalFailed == status) {
    render->signal_status_str_ = "SignalFailed";
    render->signal_connected_ = false;
  } else if (SignalStatus::SignalClosed == status) {
    render->signal_status_str_ = "SignalClosed";
    render->signal_connected_ = false;
  } else if (SignalStatus::SignalReconnecting == status) {
    render->signal_status_str_ = "SignalReconnecting";
    render->signal_connected_ = false;
  } else if (SignalStatus::SignalServerClosed == status) {
    render->signal_status_str_ = "SignalServerClosed";
    render->signal_connected_ = false;
    render->is_create_connection_ = false;
  }
}

void Render::OnConnectionStatusCb(ConnectionStatus status,
                                  [[maybe_unused]] const char *user_id,
                                  [[maybe_unused]] const size_t user_id_size,
                                  void *user_data) {
  Render *render = (Render *)user_data;
  if (!render) {
    return;
  }

  render->connection_status_ = status;
  render->show_connection_status_window_ = true;
  if (ConnectionStatus::Connecting == status) {
    render->connection_status_str_ = "Connecting";
  } else if (ConnectionStatus::Gathering == status) {
    render->connection_status_str_ = "Gathering";
  } else if (ConnectionStatus::Connected == status) {
    std::string remote_id(user_id, user_id_size);
    render->connection_properties_[remote_id] = SubStreamWindowProperties();
    render->connection_status_str_ = "Connected";
    render->connection_established_ = true;
    if (render->peer_reserved_ || !render->is_client_mode_) {
      render->start_screen_capturer_ = true;
      render->start_mouse_controller_ = true;
    }
    if (!render->hostname_sent_) {
      // TODO: self and remote hostname
      std::string host_name = GetHostName();
      RemoteAction remote_action;
      remote_action.type = ControlType::host_infomation;
      memcpy(&remote_action.i.host_name, host_name.data(), host_name.size());
      remote_action.i.host_name_size = host_name.size();
      int ret = SendDataFrame(render->peer_, (const char *)&remote_action,
                              sizeof(remote_action));
      if (0 == ret) {
        render->hostname_sent_ = true;
      }
      LOG_ERROR("1111111111111111 [{}|{}]", ret, host_name);
    } else {
      LOG_ERROR("2222222222222222");
    }
  } else if (ConnectionStatus::Disconnected == status) {
    render->connection_status_str_ = "Disconnected";
    render->password_validating_time_ = 0;
  } else if (ConnectionStatus::Failed == status) {
    render->connection_status_str_ = "Failed";
    render->password_validating_time_ = 0;
  } else if (ConnectionStatus::Closed == status) {
    render->connection_status_str_ = "Closed";
    render->password_validating_time_ = 0;
    render->start_screen_capturer_ = false;
    render->start_mouse_controller_ = false;
    render->connection_established_ = false;
    render->control_mouse_ = false;
    render->mouse_control_button_pressed_ = false;
    render->start_keyboard_capturer_ = false;
    render->hostname_sent_ = false;
    if (render->audio_capture_) {
      render->StopSpeakerCapturer();
      render->audio_capture_ = false;
      render->audio_capture_button_pressed_ = false;
    }
    if (!render->rejoin_) {
      memset(render->remote_password_, 0, sizeof(render->remote_password_));
    }
    if (render->dst_buffer_) {
      memset(render->dst_buffer_, 0, render->dst_buffer_capacity_);
      SDL_UpdateTexture(render->stream_texture_, NULL, render->dst_buffer_,
                        render->texture_width_);
    }
  } else if (ConnectionStatus::IncorrectPassword == status) {
    render->connection_status_str_ = "Incorrect password";
    render->password_validating_ = false;
    render->password_validating_time_++;
    if (render->connect_button_pressed_) {
      render->connection_established_ = false;
      render->connect_button_label_ =
          render->connect_button_pressed_
              ? localization::disconnect[render->localization_language_index_]
              : localization::connect[render->localization_language_index_];
    }
  } else if (ConnectionStatus::NoSuchTransmissionId == status) {
    render->connection_status_str_ = "No such transmission id";
    if (render->connect_button_pressed_) {
      render->connection_established_ = false;
      render->connect_button_label_ =
          render->connect_button_pressed_
              ? localization::disconnect[render->localization_language_index_]
              : localization::connect[render->localization_language_index_];
    }
  }
}

void Render::NetStatusReport(const char *client_id, size_t client_id_size,
                             TraversalMode mode,
                             const XNetTrafficStats *net_traffic_stats,
                             void *user_data) {
  Render *render = (Render *)user_data;
  if (!render) {
    return;
  }

  if (0 == strcmp(render->client_id_, "")) {
    memset(&render->client_id_, 0, sizeof(render->client_id_));
    memcpy(render->client_id_, client_id, client_id_size);
    LOG_INFO("Use client id [{}] and save id into cache file", client_id);
    render->SaveSettingsIntoCacheFile();
  }
  if (render->traversal_mode_ != mode) {
    render->traversal_mode_ = mode;
    LOG_INFO("Net mode: [{}]", int(render->traversal_mode_));
  }

  if (!net_traffic_stats) {
    return;
  }

  // only display client side net status if connected to itself
  if (!(render->peer_reserved_ && !strstr(client_id, "C-"))) {
    render->net_traffic_stats_ = *net_traffic_stats;
  }
}