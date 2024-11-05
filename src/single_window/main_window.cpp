#include "render.h"

int Render::MainWindow() {
  ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_Always);
  // ImGui::SetNextWindowSize(
  //     ImVec2(main_window_width_default_, main_window_height_default_),
  //     ImGuiCond_Always);

  LocalWindow();
  RemoteWindow();
  StatusBar();
  ConnectionStatusWindow();

  return 0;
}