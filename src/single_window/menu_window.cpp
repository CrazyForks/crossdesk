#include "IconsFontAwesome6.h"
#include "layout_style.h"
#include "localization.h"
#include "rd_log.h"
#include "render.h"

int Render::MenuWindow() {
  ImGui::PushStyleColor(ImGuiCol_MenuBarBg, ImVec4(255, 255, 255, 1));
  static bool a, b, c, d, e;
  ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_Always);
  ImGui::SetWindowFontScale(0.8f);
  ImGui::BeginChild(
      "MenuWindow", ImVec2(main_window_width_, menu_window_height_),
      ImGuiChildFlags_Border,
      ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoBringToFrontOnFocus);
  ImGui::PopStyleColor();

  ImGui::SetWindowFontScale(1.0f);
  if (ImGui::BeginMenuBar()) {
    if (ImGui::BeginMenu(ICON_FA_BARS, true)) {
      ImGui::SetWindowFontScale(0.5f);
      if (ImGui::MenuItem(
              localization::about[localization_language_index_].c_str())) {
        show_about_window_ = true;
      }
      ImGui::SetWindowFontScale(1.0f);
      ImGui::EndMenu();
    }

    ImGui::Dummy(ImVec2(main_window_width_ - 97, 0));

    SettingButton();

    if (show_about_window_) {
      AboutWindow();
    }
  }

  ImGui::EndChild();

  return 0;
}

int Render::AboutWindow() {
  const ImGuiViewport *viewport = ImGui::GetMainViewport();

  ImGui::SetNextWindowPos(ImVec2(
      (viewport->WorkSize.x - viewport->WorkPos.x - about_window_width_) / 2,
      (viewport->WorkSize.y - viewport->WorkPos.y - about_window_height_) / 2));

  ImGui::SetNextWindowSize(ImVec2(about_window_width_, about_window_height_));

  // ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(1.0, 1.0, 1.0, 1.0));
  ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.0f);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 5.0f);
  ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 3.0f);
  ImGui::SetWindowFontScale(0.5f);
  ImGui::Begin(localization::about[localization_language_index_].c_str(),
               nullptr,
               ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse |
                   ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings);
  ImGui::SetWindowFontScale(1.0f);
  ImGui::SetWindowFontScale(0.5f);
  std::string text = "Version 0.0.1";
  ImGui::Text("%s", text.c_str());

  ImGui::SetCursorPosX(about_window_width_ * 0.42f);
  ImGui::SetCursorPosY(about_window_height_ * 0.75f);
  // OK
  if (ImGui::Button(localization::ok[localization_language_index_].c_str())) {
    show_about_window_ = false;
  }

  ImGui::SetWindowFontScale(1.0f);
  ImGui::SetWindowFontScale(0.5f);
  ImGui::End();
  ImGui::SetWindowFontScale(1.0f);
  ImGui::PopStyleVar(3);

  return 0;
}