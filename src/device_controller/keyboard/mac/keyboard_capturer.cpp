#include "keyboard_capturer.h"

#include "keyboard_converter.h"
#include "rd_log.h"

static OnKeyAction g_on_key_action = nullptr;
static void *g_user_ptr = nullptr;

CGEventRef eventCallback(CGEventTapProxy proxy, CGEventType type,
                         CGEventRef event, void *userInfo) {
  KeyboardCapturer *keyboard_capturer = (KeyboardCapturer *)userInfo;
  if (!keyboard_capturer) {
    LOG_ERROR("keyboard_capturer is nullptr");
    return event;
  }

  int vk_code = 0;

  if (type == kCGEventKeyDown || type == kCGEventKeyUp) {
    CGKeyCode key_code = static_cast<CGKeyCode>(
        CGEventGetIntegerValueField(event, kCGKeyboardEventKeycode));
    std::cout << "Key Down Event: key code = " << key_code << std::endl;
    if (CGKeyCodeToVkCode.find(key_code) != CGKeyCodeToVkCode.end()) {
      g_on_key_action(CGKeyCodeToVkCode[key_code], type == kCGEventKeyDown,
                      g_user_ptr);
    } else {
      LOG_ERROR("key_code not found");
    }
  } else if (type == kCGEventFlagsChanged) {
    CGEventFlags current_flags = CGEventGetFlags(event);
    CGKeyCode key_code = static_cast<CGKeyCode>(
        CGEventGetIntegerValueField(event, kCGKeyboardEventKeycode));
    // 检测 CapsLock 键
    bool caps_lock_state = (current_flags & kCGEventFlagMaskAlphaShift) != 0;
    if (caps_lock_state != keyboard_capturer->caps_lock_flag_) {
      keyboard_capturer->caps_lock_flag_ = caps_lock_state;
      if (keyboard_capturer->caps_lock_flag_) {
        std::cout << "CapsLock Pressed" << std::endl;
        g_on_key_action(CGKeyCodeToVkCode[key_code], true, g_user_ptr);
      } else {
        std::cout << "CapsLock Released" << std::endl;
        g_on_key_action(CGKeyCodeToVkCode[key_code], false, g_user_ptr);
      }
    }

    // 检测 Shift 键
    bool shift_state = (current_flags & kCGEventFlagMaskShift) != 0;
    if (shift_state != keyboard_capturer->shift_flag_) {
      keyboard_capturer->shift_flag_ = shift_state;
      if (keyboard_capturer->shift_flag_) {
        LOG_INFO("Shift Pressed: key_code = {:#04x} -> {:#04x}", key_code,
                 CGKeyCodeToVkCode[key_code]);
        g_on_key_action(CGKeyCodeToVkCode[key_code], true, g_user_ptr);
      } else {
        LOG_INFO("Shift Released: key_code = {:#04x} -> {:#04x}", key_code,
                 CGKeyCodeToVkCode[key_code]);
        g_on_key_action(CGKeyCodeToVkCode[key_code], false, g_user_ptr);
      }
    }

    // 检测 Control 键
    bool control_state = (current_flags & kCGEventFlagMaskControl) != 0;
    if (control_state != keyboard_capturer->control_flag_) {
      keyboard_capturer->control_flag_ = control_state;
      if (keyboard_capturer->control_flag_) {
        std::cout << "Control Pressed" << std::endl;
        g_on_key_action(CGKeyCodeToVkCode[key_code], true, g_user_ptr);
      } else {
        std::cout << "Control Released" << std::endl;
        g_on_key_action(CGKeyCodeToVkCode[key_code], false, g_user_ptr);
      }
    }

    // 检测 Option 键
    bool option_state = (current_flags & kCGEventFlagMaskAlternate) != 0;
    if (option_state != keyboard_capturer->option_flag_) {
      keyboard_capturer->option_flag_ = option_state;
      if (keyboard_capturer->option_flag_) {
        std::cout << "Option Pressed" << std::endl;
        g_on_key_action(CGKeyCodeToVkCode[key_code], true, g_user_ptr);
      } else {
        std::cout << "Option Released" << std::endl;
        g_on_key_action(CGKeyCodeToVkCode[key_code], false, g_user_ptr);
      }
    }

    // 检测 Command 键
    bool command_state = (current_flags & kCGEventFlagMaskCommand) != 0;
    if (command_state != keyboard_capturer->command_flag_) {
      keyboard_capturer->command_flag_ = command_state;
      if (keyboard_capturer->command_flag_) {
        std::cout << "Command Pressed" << std::endl;
        g_on_key_action(CGKeyCodeToVkCode[key_code], true, g_user_ptr);
      } else {
        std::cout << "Command Released" << std::endl;
        g_on_key_action(CGKeyCodeToVkCode[key_code], false, g_user_ptr);
      }
    }
  }

  return nullptr;  // 返回 null 表示阻止事件
}

KeyboardCapturer::KeyboardCapturer() {}

KeyboardCapturer::~KeyboardCapturer() {}

int KeyboardCapturer::Hook(OnKeyAction on_key_action, void *user_ptr) {
  g_on_key_action = on_key_action;
  g_user_ptr = user_ptr;

  CGEventMask eventMask = (1 << kCGEventKeyDown) | (1 << kCGEventKeyUp) |
                          (1 << kCGEventFlagsChanged);

  eventTap = CGEventTapCreate(kCGSessionEventTap,     // 事件 Tap 的作用范围
                              kCGHeadInsertEventTap,  // 插入到事件队列的最前面
                              kCGEventTapOptionDefault,  // 默认选项
                              eventMask,                 // 要拦截的事件类型
                              eventCallback,             // 事件回调函数
                              this                       // 用户数据指针（可选）
  );

  if (!eventTap) {
    std::cerr << "Failed to create event tap. Ensure Accessibility permissions "
                 "are granted."
              << std::endl;
    return -1;
  }

  runLoopSource =
      CFMachPortCreateRunLoopSource(kCFAllocatorDefault, eventTap, 0);

  CFRunLoopAddSource(CFRunLoopGetCurrent(), runLoopSource,
                     kCFRunLoopCommonModes);

  CGEventTapEnable(eventTap, true);
  return 0;
}

int KeyboardCapturer::Unhook() {
  CFRelease(runLoopSource);
  CFRelease(eventTap);
  return 0;
}

int KeyboardCapturer::SendKeyboardCommand(int key_code, bool is_down) {
  LOG_INFO("SendKeyboardCommand: key_code = {:#04x}", key_code);
  if (vkCodeToCGKeyCode.find(key_code) != vkCodeToCGKeyCode.end()) {
    CGKeyCode cg_key_code = vkCodeToCGKeyCode[key_code];
    CGEventRef event = CGEventCreateKeyboardEvent(NULL, cg_key_code, is_down);
    CGEventPost(kCGHIDEventTap, event);
    CFRelease(event);
  } else {
    LOG_ERROR("key_code not found");
  }

  return 0;
}