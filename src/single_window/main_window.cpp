#include "localization.h"
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
      ImVec2(main_window_width_default_ / 2, title_bar_height_ + 25.0f),
      ImVec2(main_window_width_default_ / 2, title_bar_height_ + 240.0f),
      IM_COL32(0, 0, 0, 122), 1.0f);

  RemoteWindow();
  ImGui::EndChild();

  ImGui::SetNextWindowPos(
      ImVec2(0, title_bar_height_ + local_window_height_ - 1),
      ImGuiCond_Always);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
  ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(1.0f, 1.0f, 1.0f, 0.0f));
  ImGui::BeginChild("RecentConnectionsWindow",
                    ImVec2(main_window_width_default_,
                           main_window_height_default_ - title_bar_height_ -
                               local_window_height_ - status_bar_height_),
                    ImGuiChildFlags_Border,
                    ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse |
                        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoTitleBar |
                        ImGuiWindowFlags_NoBringToFrontOnFocus);
  ImGui::PopStyleVar();
  ImGui::PopStyleColor();

  ImGui::SetCursorPosY(ImGui::GetCursorPosY() +
                       main_window_text_y_padding_ / 2);
  ImGui::Indent(main_child_window_x_padding_);
  ImGui::SetWindowFontScale(0.8f);

  ImGui::TextColored(
      ImVec4(0.0f, 0.0f, 0.0f, 0.5f), "%s",
      localization::recent_connections[localization_language_index_].c_str());

  draw_list->AddLine(
      ImVec2(25.0f, title_bar_height_ + local_window_height_ + 35.0f),
      ImVec2(main_window_width_default_ - 25.0f,
             title_bar_height_ + local_window_height_ + 35.0f),
      IM_COL32(0, 0, 0, 122), 1.0f);

  ImGui::SetCursorPosY(ImGui::GetCursorPosY() +
                       main_window_text_y_padding_ / 2);
  ShowRecentConnections();

  ImGui::EndChild();

  StatusBar();
  ConnectionStatusWindow();

  return 0;
}

int Render::ShowRecentConnections() {
  ImGui::Image((ImTextureID)(intptr_t)recent_connection_texture_,
               ImVec2((float)recent_connection_image_width_,
                      (float)recent_connection_image_height_));
  return 0;
}