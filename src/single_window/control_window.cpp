#include "render.h"

int Render::ControlWindow() {
  ImGui::PushStyleColor(ImGuiCol_MenuBarBg, ImVec4(0, 0, 0, 0));
  static bool a, b, c, d, e;
  ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_Always);
  ImGui::BeginChild("ControlWindow",
                    ImVec2(main_window_width_, menu_window_height_),
                    ImGuiChildFlags_Border, ImGuiWindowFlags_NoScrollbar);

  ControlBar();

  ImGui::SetWindowFontScale(1.0f);
  ImGui::PopStyleColor();
  ImGui::EndChild();

  return 0;
}