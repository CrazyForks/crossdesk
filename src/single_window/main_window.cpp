#include "localization.h"
#include "rd_log.h"
#include "render.h"

int Render::MainWindow() {
  ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_Always);
  // ImGui::SetNextWindowSize(
  //     ImVec2(main_window_width_default_, main_window_height_default_),
  //     ImGuiCond_Always);

  ImGui::SetNextWindowPos(ImVec2(0, title_bar_height_), ImGuiCond_Always);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
  ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(1.0f, 1.0f, 1.0f, 1.0f));
  ImGui::BeginChild("MainWindow",
                    ImVec2(main_window_width_default_, local_window_height_),
                    ImGuiChildFlags_Border,
                    ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse |
                        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoTitleBar |
                        ImGuiWindowFlags_NoBringToFrontOnFocus);
  ImGui::PopStyleVar();
  ImGui::PopStyleColor();

  LocalWindow();

  ImDrawList* draw_list = ImGui::GetWindowDrawList();
  draw_list->AddLine(
      ImVec2(main_window_width_default_ / 2, title_bar_height_ + 15.0f),
      ImVec2(main_window_width_default_ / 2, title_bar_height_ + 225.0f),
      IM_COL32(0, 0, 0, 122), 1.0f);

  RemoteWindow();
  ImGui::EndChild();

  ImGui::SetNextWindowPos(
      ImVec2(0, title_bar_height_ + local_window_height_ - 1.0f),
      ImGuiCond_Always);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
  ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(1.0f, 1.0f, 1.0f, 1.0f));
  ImGui::BeginChild(
      "RecentConnectionsWindow",
      ImVec2(main_window_width_default_,
             main_window_height_default_ - title_bar_height_ -
                 local_window_height_ - status_bar_height_ + 1.0f),
      ImGuiChildFlags_Border,
      ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse |
          ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoTitleBar |
          ImGuiWindowFlags_NoBringToFrontOnFocus);
  ImGui::PopStyleVar();
  ImGui::PopStyleColor();

  ImGui::SetCursorPosY(ImGui::GetCursorPosY() + main_window_text_y_padding_);
  ImGui::Indent(main_child_window_x_padding_);
  ImGui::SetWindowFontScale(0.8f);

  ImGui::TextColored(
      ImVec4(0.0f, 0.0f, 0.0f, 0.5f), "%s",
      localization::recent_connections[localization_language_index_].c_str());
  ImGui::SetWindowFontScale(1.0f);

  ShowRecentConnections();

  ImGui::EndChild();

  StatusBar();
  ConnectionStatusWindow();

  return 0;
}

int Render::ShowRecentConnections() {
  ImGui::SetCursorPosX(25.0f);
  ImVec2 sub_window_pos = ImGui::GetCursorPos();
  std::map<std::string, ImVec2> sub_containers_pos;
  int recent_connection_sub_container_width =
      recent_connection_image_width_ + 16.0f;
  int recent_connection_sub_container_height =
      recent_connection_image_height_ + 36.0f;
  ImGui::PushStyleColor(ImGuiCol_ChildBg,
                        ImVec4(239.0 / 255, 240.0 / 255, 242.0 / 255, 1.0f));
  ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 10.0f);
  ImGui::BeginChild("RecentConnectionsContainer",
                    ImVec2(main_window_width_default_ - 50.0f, 145.0f),
                    ImGuiChildFlags_Border,
                    ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse |
                        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoTitleBar |
                        ImGuiWindowFlags_NoBringToFrontOnFocus |
                        ImGuiWindowFlags_AlwaysHorizontalScrollbar |
                        ImGuiWindowFlags_NoScrollbar |
                        ImGuiWindowFlags_NoScrollWithMouse);
  ImGui::PopStyleVar();
  ImGui::PopStyleColor();
  int recent_connections_count = recent_connection_textures_.size();
  int count = 0;
  int button_width = 22;
  int button_height = 22;
  for (auto it = recent_connection_textures_.begin();
       it != recent_connection_textures_.end(); ++it) {
    sub_containers_pos[it->first] = ImGui::GetCursorPos();
    std::string recent_connection_sub_window_name =
        "RecentConnectionsSubContainer" + it->first;
    // recent connections sub container

    ImGui::BeginChild(recent_connection_sub_window_name.c_str(),
                      ImVec2(recent_connection_sub_container_width,
                             recent_connection_sub_container_height),
                      ImGuiChildFlags_None,
                      ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse |
                          ImGuiWindowFlags_NoMove |
                          ImGuiWindowFlags_NoTitleBar |
                          ImGuiWindowFlags_NoBringToFrontOnFocus |
                          ImGuiWindowFlags_NoScrollbar);
    std::string connection_info = it->first;
    std::string remote_id;
    std::string password;
    std::string host_name;

    // remote id length is 9
    // password length is 6
    // connection_info -> remote_id + 'Y' + password + host_name
    //                 -> remote_id + 'N' + host_name
    if ('Y' == connection_info[9] && connection_info.size() >= 16) {
      remote_id = connection_info.substr(0, 9);
      password = connection_info.substr(10, 6);
      host_name = connection_info.substr(16);
    } else if ('N' == connection_info[9] && connection_info.size() >= 10) {
      remote_id = connection_info.substr(0, 9);
      host_name = connection_info.substr(10);
    } else {
      host_name = "unknown";
    }

    ImVec2 image_screen_pos = ImVec2(ImGui::GetCursorScreenPos().x + 5.0f,
                                     ImGui::GetCursorScreenPos().y + 5.0f);
    ImVec2 image_pos =
        ImVec2(ImGui::GetCursorPosX() + 5.0f, ImGui::GetCursorPosY() + 5.0f);
    ImGui::SetCursorPos(image_pos);
    ImGui::Image((ImTextureID)(intptr_t)it->second,
                 ImVec2((float)recent_connection_image_width_,
                        (float)recent_connection_image_height_));

    // remote id display button
    {
      ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0.2f));
      ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0, 0, 0, 0.2f));
      ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0, 0, 0, 0.2f));

      ImVec2 dummy_button_pos =
          ImVec2(image_pos.x, image_pos.y + recent_connection_image_height_);
      std::string dummy_button_name = "##DummyButton" + remote_id;
      ImGui::SetCursorPos(dummy_button_pos);
      ImGui::SetWindowFontScale(0.6f);
      ImGui::Button(dummy_button_name.c_str(),
                    ImVec2(recent_connection_image_width_ - 2 * button_width,
                           button_height));
      ImGui::SetCursorPos(
          ImVec2(dummy_button_pos.x + 2.0f, dummy_button_pos.y + 1.0f));
      ImGui::Text(remote_id.c_str());
      ImGui::PopStyleColor(3);

      if (ImGui::IsItemHovered()) {
        ImGui::BeginTooltip();
        ImGui::SetWindowFontScale(0.5f);
        ImGui::Text(host_name.c_str());
        ImGui::SetWindowFontScale(1.0f);
        ImGui::EndTooltip();
      }
    }

    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0.2f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
                          ImVec4(0.1f, 0.4f, 0.8f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive,
                          ImVec4(1.0f, 1.0f, 1.0f, 0.7f));

    // trash button
    {
      ImVec2 trash_can_button_pos = ImVec2(
          image_pos.x + recent_connection_image_width_ - 2 * button_width,
          image_pos.y + recent_connection_image_height_);
      ImGui::SetCursorPos(trash_can_button_pos);
      ImGui::SetWindowFontScale(0.5f);
      std::string trash_can = ICON_FA_TRASH_CAN;
      std::string recent_connection_delete_button_name =
          trash_can + "##RecentConnectionDelete" +
          std::to_string(trash_can_button_pos.x);
      if (ImGui::Button(recent_connection_delete_button_name.c_str(),
                        ImVec2(button_width, button_height))) {
        show_confirm_delete_connection_ = true;
      }

      if (delete_connection_) {
        if (!thumbnail_->DeleteThumbnail(it->first)) {
          reload_recent_connections_ = true;
          delete_connection_ = false;
        }
      }
    }

    // connect button
    {
      ImVec2 connect_button_pos =
          ImVec2(image_pos.x + recent_connection_image_width_ - button_width,
                 image_pos.y + recent_connection_image_height_);
      ImGui::SetCursorPos(connect_button_pos);
      std::string connect = ICON_FA_ARROW_RIGHT_LONG;
      std::string connect_to_this_connection_button_name =
          connect + "##ConnectionTo" + it->first;
      if (ImGui::Button(connect_to_this_connection_button_name.c_str(),
                        ImVec2(button_width, button_height))) {
        LOG_ERROR("Connect to [{}], password [{}]", remote_id.c_str(),
                  password.c_str());
        remote_id_ = remote_id;
        strncpy(remote_password_, password.c_str(), 6);
        ConnectTo();
      }
    }

    ImGui::SetWindowFontScale(1.0f);
    ImGui::PopStyleColor(3);

    ImGui::EndChild();

    // 绘制纵向分割线
    if (count != recent_connections_count - 1) {
      ImVec2 line_start =
          ImVec2(image_screen_pos.x + recent_connection_image_width_ + 20.0f,
                 image_screen_pos.y);  // 起点
      ImVec2 line_end = ImVec2(
          image_screen_pos.x + recent_connection_image_width_ + 20.0f,
          image_screen_pos.y + recent_connection_image_height_ + button_height);
      ImGui::GetForegroundDrawList()->AddLine(line_start, line_end,
                                              IM_COL32(0, 0, 0, 122), 1.0f);
    }

    count++;
    ImGui::SameLine(0, count != recent_connections_count ? 26.0f : 0.0f);
  }

  ImGui::EndChild();

  if (show_confirm_delete_connection_) {
    ConfirmDeleteConnection();
  }

  return 0;
}

int Render::ConfirmDeleteConnection() {
  const ImGuiViewport* viewport = ImGui::GetMainViewport();
  ImGui::SetNextWindowPos(ImVec2((viewport->WorkSize.x - viewport->WorkPos.x -
                                  connection_status_window_width_) /
                                     2,
                                 (viewport->WorkSize.y - viewport->WorkPos.y -
                                  connection_status_window_height_) /
                                     2));

  ImGui::SetNextWindowSize(ImVec2(connection_status_window_width_,
                                  connection_status_window_height_));
  ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 3.0f);
  ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(1.0, 1.0, 1.0, 1.0));
  ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.0f);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 5.0f);

  ImGui::Begin("ConfirmDeleteConnectionWindow", nullptr,
               ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse |
                   ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoTitleBar |
                   ImGuiWindowFlags_NoSavedSettings);
  ImGui::PopStyleVar(2);
  ImGui::PopStyleColor();

  ImGui::SetWindowFontScale(0.5f);
  std::string text =
      localization::confirm_delete_connection[localization_language_index_];
  ImGui::SetCursorPosX(connection_status_window_width_ * 6 / 19);
  ImGui::SetCursorPosY(connection_status_window_height_ * 2 / 3);

  // ok
  if (ImGui::Button(localization::ok[localization_language_index_].c_str()) ||
      ImGui::IsKeyPressed(ImGuiKey_Enter)) {
    delete_connection_ = true;
    show_confirm_delete_connection_ = false;
  }

  ImGui::SameLine();
  // cancel
  if (ImGui::Button(
          localization::cancel[localization_language_index_].c_str()) ||
      ImGui::IsKeyPressed(ImGuiKey_Escape)) {
    delete_connection_ = false;
    show_confirm_delete_connection_ = false;
  }

  auto window_width = ImGui::GetWindowSize().x;
  auto window_height = ImGui::GetWindowSize().y;
  auto text_width = ImGui::CalcTextSize(text.c_str()).x;
  ImGui::SetCursorPosX((window_width - text_width) * 0.5f);
  ImGui::SetCursorPosY(window_height * 0.2f);
  ImGui::Text(text.c_str());
  ImGui::SetWindowFontScale(1.0f);

  ImGui::End();
  ImGui::PopStyleVar();
  return 0;
}