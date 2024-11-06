#include "IconsFontAwesome6.h"
#include "layout_style.h"
#include "localization.h"
#include "rd_log.h"
#include "render.h"

static int InputTextCallback(ImGuiInputTextCallbackData *data);

int Render::RemoteWindow() {
  ImGui::SetNextWindowPos(ImVec2(local_window_width_ + 1.0f, title_bar_height_),
                          ImGuiCond_Always);
  ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 3.0f);

  ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(1.0f, 1.0f, 1.0f, 1.0f));
  ImGui::BeginChild("RemoteDesktopWindow",
                    ImVec2(remote_window_width_, remote_window_height_),
                    ImGuiChildFlags_None,
                    ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse |
                        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoTitleBar |
                        ImGuiWindowFlags_NoBringToFrontOnFocus);
  ImGui::PopStyleColor();

  ImGui::SetCursorPosY(ImGui::GetCursorPosY() + main_window_text_y_padding_);
  ImGui::Indent(main_child_window_x_padding_ - 1.0f);
  ImGui::SetWindowFontScale(1.0f);

  ImGui::TextColored(
      ImVec4(0.0f, 0.0f, 0.0f, 0.5f), "%s",
      localization::remote_desktop[localization_language_index_].c_str());

  ImGui::Spacing();
  {
    ImGui::SetNextWindowPos(
        ImVec2(local_window_width_ + main_child_window_x_padding_ - 1.0f,
               title_bar_height_ + main_child_window_y_padding_),
        ImGuiCond_Always);
    ImGui::PushStyleColor(ImGuiCol_ChildBg,
                          ImVec4(239.0 / 255, 240.0 / 255, 242.0 / 255, 1.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 10.0f);

    ImGui::BeginChild(
        "RemoteDesktopWindow_1",
        ImVec2(remote_child_window_width_, remote_child_window_height_),
        ImGuiChildFlags_Border,
        ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse |
            ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoTitleBar |
            ImGuiWindowFlags_NoBringToFrontOnFocus);
    ImGui::PopStyleVar();
    ImGui::PopStyleColor();
    {
      ImGui::SetWindowFontScale(0.5f);
      ImGui::Text(
          "%s", localization::remote_id[localization_language_index_].c_str());

      ImGui::Spacing();
      ImGui::SetNextItemWidth(IPUT_WINDOW_WIDTH);
      ImGui::SetWindowFontScale(1.0f);
      ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);
      bool enter_pressed = ImGui::InputText(
          "##remote_id_", remote_id_display_, IM_ARRAYSIZE(remote_id_display_),
          ImGuiInputTextFlags_CharsUppercase |
              ImGuiInputTextFlags_EnterReturnsTrue |
              ImGuiInputTextFlags_CallbackEdit |
              ImGuiInputTextFlags_CharsNoBlank,
          InputTextCallback);

      ImGui::PopStyleVar();
      ImGui::SameLine();
      if (ImGui::Button(ICON_FA_ARROW_RIGHT_LONG, ImVec2(55, 38)) ||
          enter_pressed || rejoin_) {
        connect_button_pressed_ = true;
        connection_status_ = ConnectionStatus::Connecting;
        int ret = -1;
        if (signal_connected_) {
          if (!connection_established_) {
            remote_id_ = remote_id_display_;
            remote_id_.erase(remove_if(remote_id_.begin(), remote_id_.end(),
                                       static_cast<int (*)(int)>(&isspace)),
                             remote_id_.end());
            if (0 == strcmp(remote_id_.c_str(), client_id_) &&
                !peer_reserved_) {
              peer_reserved_ = CreatePeer(&params_);
              if (peer_reserved_) {
                LOG_INFO("Create peer[reserved] instance successful");
                std::string client_id = "C-";
                client_id += client_id_;
                Init(peer_reserved_, client_id.c_str());
                LOG_INFO("Peer[reserved] init finish");
              } else {
                LOG_INFO("Create peer[reserved] instance failed");
              }
            }
            ret = JoinConnection(peer_reserved_ ? peer_reserved_ : peer_,
                                 remote_id_.c_str(), remote_password_);
            if (0 == ret) {
              is_client_mode_ = true;
              rejoin_ = false;
            } else {
              rejoin_ = true;
            }
          }
        }
      }
    }
    ImGui::EndChild();
  }
  ImGui::EndChild();
  ImGui::PopStyleVar();

  return 0;
}

static int InputTextCallback(ImGuiInputTextCallbackData *data) {
  if (data->BufTextLen > 3 && data->Buf[3] != ' ') {
    data->InsertChars(3, " ");
  }

  if (data->BufTextLen > 7 && data->Buf[7] != ' ') {
    data->InsertChars(7, " ");
  }

  return 0;
}