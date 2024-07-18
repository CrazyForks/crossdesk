#include "layout_style.h"
#include "localization.h"
#include "rd_log.h"
#include "render.h"

int Render::ConnectionStatusWindow() {
  if (connect_button_pressed_) {
    const ImGuiViewport *viewport = ImGui::GetMainViewport();

    ImGui::SetNextWindowPos(ImVec2((viewport->WorkSize.x - viewport->WorkPos.x -
                                    connection_status_window_width_) /
                                       2,
                                   (viewport->WorkSize.y - viewport->WorkPos.y -
                                    connection_status_window_height_) /
                                       2));

    ImGui::SetNextWindowSize(ImVec2(connection_status_window_width_,
                                    connection_status_window_height_));

    ImGui::SetWindowFontScale(0.5f);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 3.0f);

    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(1.0, 1.0, 1.0, 1.0));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 5.0f);

    ImGui::Begin("ConnectionStatusWindow", nullptr,
                 ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse |
                     ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoTitleBar |
                     ImGuiWindowFlags_NoSavedSettings);
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor();
    ImGui::SetWindowFontScale(1.0f);

    ImGui::SetWindowFontScale(0.6f);
    std::string text;

    if (ConnectionStatus::Connecting == connection_status_) {
      text = "Connecting...";
      ImGui::SetCursorPosX(connection_status_window_width_ * 3 / 7);
      ImGui::SetCursorPosY(connection_status_window_height_ * 2 / 3);
      // Cancel
      if (ImGui::Button(
              localization::cancel[localization_language_index_].c_str())) {
        connect_button_pressed_ = false;
      }
    } else if (ConnectionStatus::Connected == connection_status_) {
      text = "Connected";
    } else if (ConnectionStatus::Disconnected == connection_status_) {
      text = "Disonnected";
      ImGui::SetCursorPosX(connection_status_window_width_ * 3 / 7);
      ImGui::SetCursorPosY(connection_status_window_height_ * 2 / 3);
      // Cancel
      if (ImGui::Button(
              localization::cancel[localization_language_index_].c_str())) {
        connect_button_pressed_ = false;
      }
    } else if (ConnectionStatus::Failed == connection_status_) {
      text = "Failed";
      ImGui::SetCursorPosX(connection_status_window_width_ * 3 / 7);
      ImGui::SetCursorPosY(connection_status_window_height_ * 2 / 3);
      // Cancel
      if (ImGui::Button(
              localization::cancel[localization_language_index_].c_str())) {
        connect_button_pressed_ = false;
      }
    } else if (ConnectionStatus::Closed == connection_status_) {
      text = "Closed";
      ImGui::SetCursorPosX(connection_status_window_width_ * 3 / 7);
      ImGui::SetCursorPosY(connection_status_window_height_ * 2 / 3);
      // Cancel
      if (ImGui::Button(
              localization::cancel[localization_language_index_].c_str())) {
        connect_button_pressed_ = false;
      }
    } else if (ConnectionStatus::IncorrectPassword == connection_status_) {
      text = "Please input password";
      auto window_width = ImGui::GetWindowSize().x;
      auto window_height = ImGui::GetWindowSize().y;
      ImGui::SetCursorPosX((window_width - IPUT_WINDOW_WIDTH / 2) * 0.5f);
      ImGui::SetCursorPosY(window_height * 0.4f);
      ImGui::SetNextItemWidth(IPUT_WINDOW_WIDTH / 2);

      ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);
      ImGui::InputText("##password", (char *)remote_password_.c_str(), 7,
                       ImGuiInputTextFlags_CharsNoBlank);
      ImGui::PopStyleVar();

      ImGui::SetCursorPosX(window_width * 0.28f);
      ImGui::SetCursorPosY(window_height * 0.75f);
      // OK
      if (ImGui::Button(
              localization::ok[localization_language_index_].c_str())) {
        connect_button_pressed_ = true;
        JoinConnection(peer_reserved_ ? peer_reserved_ : peer_, remote_id_,
                       remote_password_.c_str());
      }

      ImGui::SameLine();

      if (ImGui::Button(
              localization::cancel[localization_language_index_].c_str())) {
        remote_password_ = "";
        connect_button_pressed_ = false;
      }

    } else if (ConnectionStatus::NoSuchTransmissionId == connection_status_) {
      text = "No such transmissionId";
      ImGui::SetCursorPosX(connection_status_window_width_ * 3 / 7);
      ImGui::SetCursorPosY(connection_status_window_height_ * 2 / 3);
      // Cancel
      if (ImGui::Button(
              localization::cancel[localization_language_index_].c_str())) {
        connect_button_pressed_ = false;
      }
    }

    auto window_width = ImGui::GetWindowSize().x;
    auto window_height = ImGui::GetWindowSize().y;
    auto text_width = ImGui::CalcTextSize(text.c_str()).x;
    ImGui::SetCursorPosX((window_width - text_width) * 0.5f);
    ImGui::SetCursorPosY(window_height * 0.2f);
    ImGui::Text("%s", text.c_str());
    ImGui::SetWindowFontScale(1.0f);

    ImGui::SetWindowFontScale(0.5f);
    ImGui::End();
    ImGui::PopStyleVar();
    ImGui::SetWindowFontScale(1.0f);
  }
  return 0;
}