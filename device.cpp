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

enum menu_action_type {
    ACTION_NONE,
    ACTION_SUBMENU,
    ACTION_INVOKE
};

struct menu_entry;
struct menu {
    const char**        names;
    const menu_entry*   entries;
};

union menu_action {
    const menu*                 submenu;
    Device::BuiltinAction       action;
};

struct menu_entry {
    menu_action_type    action_type;
    const menu_action   action;
};

static const char* WIPE_MENU_NAMES[] = {
    "System reset (keep media)",
    "Full factory reset",
    "Wipe cache partition",
    nullptr
};
static const menu_entry WIPE_MENU_ENTRIES[] = {
    { ACTION_INVOKE, { .action = Device::WIPE_DATA } },
    { ACTION_INVOKE, { .action = Device::WIPE_FULL } },
    { ACTION_INVOKE, { .action = Device::WIPE_CACHE } },
    { ACTION_NONE, { .action = Device::NO_ACTION } }
};
static const menu WIPE_MENU = {
    WIPE_MENU_NAMES,
    WIPE_MENU_ENTRIES
};

static const char* ADVANCED_MENU_NAMES[] = {
    "Reboot recovery",
#ifdef DOWNLOAD_MODE
    "Reboot to download mode",
#else
    "Reboot to bootloader",
#endif
    "Mount /system",
    "View recovery logs",
    "Power off",
    nullptr
};
static const menu_entry ADVANCED_MENU_ENTRIES[] = {
    { ACTION_INVOKE, { .action = Device::REBOOT_RECOVERY } },
#ifdef DOWNLOAD_MODE
    { ACTION_INVOKE, { .action = Device::REBOOT_BOOTLOADER } },
#else
    { ACTION_INVOKE, { .action = Device::REBOOT_BOOTLOADER } },
#endif
    { ACTION_INVOKE, { .action = Device::MOUNT_SYSTEM } },
    { ACTION_INVOKE, { .action = Device::VIEW_RECOVERY_LOGS } },
    { ACTION_INVOKE, { .action = Device::SHUTDOWN } },
    { ACTION_NONE, { .action = Device::NO_ACTION } }
};
static const menu ADVANCED_MENU = {
    ADVANCED_MENU_NAMES,
    ADVANCED_MENU_ENTRIES
};

static const char* MAIN_MENU_NAMES[] = {
    "Reboot system now",
    "Apply update",
    "Factory reset",
    "Advanced",
    nullptr
};
static const menu_entry MAIN_MENU_ENTRIES[] = {
    { ACTION_INVOKE, { .action = Device::REBOOT } },
    { ACTION_INVOKE, { .action = Device::APPLY_UPDATE } },
    { ACTION_SUBMENU, { .submenu = &WIPE_MENU } },
    { ACTION_SUBMENU, { .submenu = &ADVANCED_MENU } },
    { ACTION_NONE, { .action = Device::NO_ACTION } }
};
static const menu MAIN_MENU = {
    MAIN_MENU_NAMES,
    MAIN_MENU_ENTRIES
};

Device::Device(RecoveryUI* ui) :
        ui_(ui) {
    menu_stack.push(&MAIN_MENU);
}

const char* const* Device::GetMenuItems() {
    const menu* m = menu_stack.top();
    return m->names;
}

Device::BuiltinAction Device::InvokeMenuItem(int menu_position) {
  if (menu_position < 0) {
    if (menu_position == Device::kGoBack) {
        if (menu_stack.size() > 1) {
            menu_stack.pop();
        }
    }
    return NO_ACTION;
  }
  const menu* m = menu_stack.top();
  const menu_entry* entry = m->entries + menu_position;
  if (entry->action_type == ACTION_SUBMENU) {
      menu_stack.push(entry->action.submenu);
      return NO_ACTION;
  }
  return entry->action.action;
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
      return kGoBack;

    default:
      // If you have all of the above buttons, any other buttons
      // are ignored. Otherwise, any button cycles the highlight.
      return ui_->HasThreeButtons() ? kNoAction : kHighlightDown;
  }
}
