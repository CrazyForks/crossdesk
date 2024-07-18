#include "render.h"

int Render::ControlWindow() {
  ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(1.0f, 1.0f, 1.0f, 1.0f));
  static bool a, b, c, d, e;
  ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_Always);
  ImGui::SetWindowFontScale(0.5f);

  auto time_duration = ImGui::GetTime() - control_bar_button_pressed_time_;
  auto control_window_width =
      !control_bar_button_pressed_
          ? time_duration < 0.25f
                ? main_window_width_ -
                      (main_window_width_ - control_window_min_width_) * 4 *
                          time_duration
                : control_window_min_width_
      : time_duration < 0.25f
          ? control_window_min_width_ +
                (main_window_width_ - control_window_min_width_) * 4 *
                    time_duration
          : main_window_width_;
  ImGui::BeginChild("ControlWindow",
                    ImVec2(control_window_width, control_window_height_),
                    ImGuiChildFlags_Border, ImGuiWindowFlags_NoDecoration);
  ImGui::SetWindowFontScale(1.0f);
  ImGui::PopStyleColor();

  ControlBar();

  ImGui::EndChild();

  return 0;
}