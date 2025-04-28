#include "localization.h"
#include "rd_log.h"
#include "render.h"

int Render::StreamWindow() {
  ImGui::SetNextWindowPos(
      ImVec2(0, fullscreen_button_pressed_ ? 0 : title_bar_height_),
      ImGuiCond_Always);
  ImGui::SetNextWindowSize(ImVec2(stream_window_width_, stream_window_height_),
                           ImGuiCond_Always);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
  ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0, 0, 0, 0));
  ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0, 0, 0, 0));
  ImGui::PushStyleColor(ImGuiCol_DockingEmptyBg, ImVec4(0, 0, 0, 0));
  ImGui::Begin("VideoBg", nullptr,
               ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoDecoration |
                   ImGuiWindowFlags_NoBringToFrontOnFocus);

  ImGuiID dockspace_id = ImGui::GetID("MyDockSpace");
  ImGui::DockSpace(
      dockspace_id, ImVec2(stream_window_width_, stream_window_height_),
      ImGuiDockNodeFlags_AutoHideTabBar | ImGuiDockNodeFlags_KeepAliveOnly);

  ImGui::PopStyleColor(3);
  ImGui::PopStyleVar();

  for (auto& it : client_properties_) {
    auto& props = it.second;

    // ImGui::SetNextWindowPos(
    //     ImVec2(0, fullscreen_button_pressed_ ? 0 : title_bar_height_),
    //     ImGuiCond_Once);
    ImGui::SetNextWindowSize(
        ImVec2(stream_window_width_, stream_window_height_), ImGuiCond_Once);
    ImGui::SetWindowFontScale(0.5f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0, 0, 0, 0));
    ImGui::Begin(props->remote_id_.c_str(), nullptr,
                 ImGuiWindowFlags_NoTitleBar);
    ImGui::PopStyleColor(2);
    ImGui::PopStyleVar();
    ImGui::SetWindowFontScale(1.0f);

    ImVec2 stream_window_pos = ImGui::GetWindowPos();
    ImGuiViewport* viewport = ImGui::GetWindowViewport();
    ImVec2 stream_window_size = ImGui::GetWindowSize();
    props->render_window_x_ = stream_window_pos.x;
    props->render_window_y_ = stream_window_pos.y;
    props->render_window_width_ = stream_window_size.x;
    props->render_window_height_ = stream_window_size.y;

    ControlWindow(props);

    ImGui::End();
  }

  UpdateRenderRect();

  ImGui::End();

  return 0;
}
