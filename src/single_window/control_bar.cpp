#include "IconsFontAwesome6.h"
#include "layout_style.h"
#include "localization.h"
#include "log.h"
#include "render.h"

int Render::ControlBar() {
  ImGui::SetWindowFontScale(0.5f);
  ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 3.0f);
  // Mouse control
  std::string mouse = ICON_FA_COMPUTER_MOUSE;
  if (ImGui::Button(mouse.c_str(), ImVec2(25, 25))) {
    if (mouse_control_button_label_ ==
            localization::control_mouse[localization_language_index_] &&
        connection_established_) {
      mouse_control_button_pressed_ = true;
      control_mouse_ = true;
      mouse_control_button_label_ =
          localization::release_mouse[localization_language_index_];
    } else {
      control_mouse_ = false;
      mouse_control_button_label_ =
          localization::control_mouse[localization_language_index_];
    }
    mouse_control_button_pressed_ = !mouse_control_button_pressed_;
  }

  ImGui::SameLine();
  // Fullscreen
  std::string fullscreen =
      fullscreen_button_pressed_ ? ICON_FA_COMPRESS : ICON_FA_EXPAND;
  if (ImGui::Button(fullscreen.c_str(), ImVec2(25, 25))) {
    fullscreen_button_pressed_ = !fullscreen_button_pressed_;
    if (fullscreen_button_pressed_) {
      main_window_width_before_fullscreen_ = main_window_width_;
      main_window_height_before_fullscreen_ = main_window_height_;
      SDL_SetWindowFullscreen(main_window_, SDL_WINDOW_FULLSCREEN_DESKTOP);
    } else {
      SDL_SetWindowFullscreen(main_window_, SDL_FALSE);
      SDL_SetWindowSize(main_window_, main_window_width_before_fullscreen_,
                        main_window_height_before_fullscreen_);
      main_window_width_ = main_window_width_before_fullscreen_;
      main_window_height_ = main_window_height_before_fullscreen_;
    }
  }

  return 0;
}