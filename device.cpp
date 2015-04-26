/*
 * Copyright (C) 2015 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "device.h"

static const char* MENU_ITEMS[] = {
    "Reboot system now",
#ifdef DOWNLOAD_MODE
    "Reboot to download mode",
#else
    "Reboot to bootloader",
#endif
    "Apply update",
    "Wipe data/factory reset",
    "Wipe cache partition",
    "Wipe media",
    "Mount /system",
    "View recovery logs",
    "Run graphics test",
    "Power off",
    NULL
};

extern int ui_root_menu;

const char* const* Device::GetMenuItems() {
  return MENU_ITEMS;
}

Device::BuiltinAction Device::InvokeMenuItem(int menu_position) {
  switch (menu_position) {
    case 0: return REBOOT;
    case 1: return REBOOT_BOOTLOADER;
    case 2: return APPLY_UPDATE;
    case 3: return WIPE_DATA;
    case 4: return WIPE_CACHE;
    case 5: return WIPE_MEDIA;
    case 6: return MOUNT_SYSTEM;
    case 7: return VIEW_RECOVERY_LOGS;
    case 8: return RUN_GRAPHICS_TEST;
    case 9: return SHUTDOWN;
    default: return NO_ACTION;
  }
}

int Device::HandleMenuKey(int key, int visible) {
  if (!visible) {
    return kNoAction;
  }

  if (key & KEY_FLAG_ABS) {
    return key;
  }

  switch (key) {
    case KEY_RIGHTSHIFT:
    case KEY_DOWN:
    case KEY_VOLUMEDOWN:
    case KEY_MENU:
      return kHighlightDown;

    case KEY_LEFTSHIFT:
    case KEY_UP:
    case KEY_VOLUMEUP:
    case KEY_SEARCH:
      return kHighlightUp;

    case KEY_ENTER:
    case KEY_POWER:
    case BTN_MOUSE:
    case KEY_HOME:
    case KEY_HOMEPAGE:
    case KEY_SEND:
      return kInvokeItem;

    case KEY_BACKSPACE:
    case KEY_BACK:
      if (!ui_root_menu)
        return kGoBack;

    default:
      // If you have all of the above buttons, any other buttons
      // are ignored. Otherwise, any button cycles the highlight.
      return ui_->HasThreeButtons() ? kNoAction : kHighlightDown;
  }
}
