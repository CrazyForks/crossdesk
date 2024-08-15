#include "IconsFontAwesome6.h"
#include "layout_style.h"
#include "localization.h"
#include "rd_log.h"
#include "render.h"

int Render::ControlBar() {
  ImVec2 bar_pos = ImGui::GetWindowPos();

  ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 3.0f);

  if (control_bar_expand_) {
    ImGui::SetCursorPosX(is_control_bar_in_left_ ? (control_window_width_ + 5)
                                                 : 53);
    // Mouse control
    std::string mouse =
        mouse_control_button_pressed_ ? ICON_FA_HAND_BACK_FIST : ICON_FA_HAND;
    if (ImGui::Button(mouse.c_str(), ImVec2(25, 25))) {
      if (connection_established_) {
        control_mouse_ = true;
      } else {
        control_mouse_ = false;
      }
      mouse_control_button_pressed_ = !mouse_control_button_pressed_;
      mouse_control_button_label_ =
          mouse_control_button_pressed_
              ? localization::release_mouse[localization_language_index_]
              : localization::control_mouse[localization_language_index_];
    }

    ImGui::SameLine();
    // Audio capture
    std::string audio = audio_capture_button_pressed_ ? ICON_FA_VOLUME_XMARK
                                                      : ICON_FA_VOLUME_HIGH;
    if (ImGui::Button(audio.c_str(), ImVec2(25, 25))) {
      if (connection_established_) {
        audio_capture_ = true;
      } else {
        audio_capture_ = false;
      }
      audio_capture_button_pressed_ = !audio_capture_button_pressed_;
      audio_capture_button_label_ =
          audio_capture_button_pressed_
              ? localization::audio_capture[localization_language_index_]
              : localization::mute[localization_language_index_];

      RemoteAction remote_action;
      remote_action.type = ControlType::audio_capture;
      remote_action.a = audio_capture_button_pressed_;
      SendData(peer_, DATA_TYPE::DATA, (const char *)&remote_action,
               sizeof(remote_action));
    }

    ImGui::SameLine();
    // Fullscreen
    std::string fullscreen =
        fullscreen_button_pressed_ ? ICON_FA_COMPRESS : ICON_FA_EXPAND;
    if (ImGui::Button(fullscreen.c_str(), ImVec2(25, 25))) {
      fullscreen_button_pressed_ = !fullscreen_button_pressed_;
      fullscreen_button_label_ =
          fullscreen_button_pressed_
              ? localization::exit_fullscreen[localization_language_index_]
              : localization::fullscreen[localization_language_index_];
      if (fullscreen_button_pressed_) {
        SDL_SetWindowFullscreen(main_window_, SDL_WINDOW_FULLSCREEN_DESKTOP);
      } else {
        SDL_SetWindowFullscreen(main_window_, SDL_FALSE);
      }
    }

    ImGui::SameLine();
  }

  ImGui::SetCursorPosX(
      is_control_bar_in_left_ ? (control_window_width_ * 2 - 18) : 3);

  std::string control_bar =
      control_bar_expand_
          ? (is_control_bar_in_left_ ? ICON_FA_ANGLE_LEFT : ICON_FA_ANGLE_RIGHT)
          : (is_control_bar_in_left_ ? ICON_FA_ANGLE_RIGHT
                                     : ICON_FA_ANGLE_LEFT);
  if (ImGui::Button(control_bar.c_str(), ImVec2(15, 25))) {
    control_bar_expand_ = !control_bar_expand_;
    control_bar_button_pressed_time_ = ImGui::GetTime();
    control_window_width_is_changing_ = true;
  }

  ImGui::PopStyleVar();

  return 0;
}