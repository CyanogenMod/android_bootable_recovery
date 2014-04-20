/*
 * Copyright (C) 2009 The Android Open Source Project
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

#ifndef _RECOVERY_UI_H
#define _RECOVERY_UI_H

#include "common.h"
#include "minzip/Zip.h"

// Called before UI library is initialized.  Can change things like
// how many frames are included in various animations, etc.
extern void device_ui_init(UIParameters* ui_parameters);

// Called when recovery starts up.  Returns 0.
extern int device_recovery_start();

// Called in the input thread when a new key (key_code) is pressed.
// *key_pressed is an array of KEY_MAX+1 bytes indicating which other
// keys are already pressed.  Return true if the text display should
// be toggled.
extern int device_toggle_display(volatile char* key_pressed, int key_code);
int get_allow_toggle_display();

// Called in the input thread when a new key (key_code) is pressed.
// *key_pressed is an array of KEY_MAX+1 bytes indicating which other
// keys are already pressed.  Return true if the device should reboot
// immediately.
extern int device_reboot_now(volatile char* key_pressed, int key_code);

// Called from the main thread when recovery is waiting for input and
// a key is pressed.  key is the code of the key pressed; visible is
// true if the recovery menu is being shown.  Implementations can call
// ui_key_pressed() to discover if other keys are being held down.
// Return one of the defined constants below in order to:
//
//   - move the menu highlight (HIGHLIGHT_*)
//   - invoke the highlighted item (SELECT_ITEM)
//   - do nothing (NO_ACTION)
//   - invoke a specific action (a menu position: any non-negative number)
extern int device_handle_key(int key, int visible);

// Perform a recovery action selected from the menu.  'which' will be
// the item number of the selected menu item, or a non-negative number
// returned from device_handle_key().  The menu will be hidden when
// this is called; implementations can call ui_print() to print
// information to the screen.
extern int device_perform_action(int which);

// Called when we do a wipe data/factory reset operation (either via a
// reboot from the main system with the --wipe_data flag, or when the
// user boots into recovery manually and selects the option from the
// menu.)  Can perform whatever device-specific wiping actions are
// needed.  Return 0 on success.  The userdata and cache partitions
// are erased after this returns (whether it returns success or not).
int device_wipe_data();

#ifdef BOARD_NATIVE_DUALBOOT
// Called instead of 'verify_root_and_recovery' and should be used
// to invoke 'verify_root_and_recovery' for every system.
extern int device_verify_root_and_recovery(void);

// Called from 'confirm_selection' to prepare the displayed title
// 'buf' will be used as title after this function has been called
// which means that you at least should write the title into this buffer.
// This should be used to add a prefix to the title which tells the user
// on which system the current action will be performed to.
extern int device_build_selection_title(char* buf, const char* title);

#ifdef BOARD_NATIVE_DUALBOOT_SINGLEDATA
extern void device_toggle_truedualboot(void);
extern void device_choose_bootmode(void);
extern int device_get_truedualboot_entry(char* tdb_name);
extern int device_get_bootmode(char* bootmode_name);
extern int device_truedualboot_mount(const char* path, const char* mount_point);
extern int device_truedualboot_unmount(const char* path);
extern int device_truedualboot_format_volume(const char* volume);
extern int device_truedualboot_format_device(const char *device, const char *path, const char *fs_type);
extern int device_truedualboot_before_update(const char *path, ZipArchive *zip);
extern void device_truedualboot_after_load_volume_table();
#endif
#endif

// ui_wait_key() special return codes
/*
#define REBOOT              -1 // ui_wait_key() timeout to reboot
#define CANCEL              -2 // ui_cancel_wait_key()
*/
#define REFRESH             -3

// return actions by ui_handle_key() for get_menu_selection()
#define NO_ACTION           -1

#define HIGHLIGHT_UP        -2
#define HIGHLIGHT_DOWN      -3
#define SELECT_ITEM         -4
#define GO_BACK             -5

// main menu items for prompt_and_wait()
#define ITEM_REBOOT          0
#define ITEM_APPLY_EXT       1
#define ITEM_APPLY_SDCARD    1  // historical synonym for ITEM_APPLY_EXT
#define ITEM_APPLY_ZIP       1  // used for installing an update from a zip
#define ITEM_WIPE_DATA       2
#define ITEM_WIPE_CACHE      3
// unused in cwr
#define ITEM_APPLY_CACHE     4
#define ITEM_NANDROID        4
#define ITEM_PARTITION       5
#define ITEM_ADVANCED        6

// Header text to display above the main menu.
extern char* MENU_HEADERS[];

// Text of menu items.
extern char* MENU_ITEMS[];

// Loosely track the depth of the current menu
extern int ui_root_menu;

int
get_menu_selection(const char** headers, char** items, int menu_only, int initial_selection);

void
set_sdcard_update_bootloader_message();

extern int ui_handle_key(int key, int visible);

// call a clean reboot
void reboot_main_system(int cmd, int flags, char *arg);

#endif
