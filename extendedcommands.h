/*
 * Copyright (C) 2014 The CyanogenMod Project
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

#ifndef __EXTENDEDCOMMANDS_H
#define __EXTENDEDCOMMANDS_H

extern int signature_check_enabled;

int __system(const char *command);

int show_nandroid_menu();
int show_advanced_menu();
int show_partition_menu();
int show_install_update_menu();
int confirm_selection(const char* title, const char* confirm);
int install_zip(const char* packagefilepath);

int empty_nandroid_bitmask(unsigned char flags);
int has_datadata();
void process_volumes();
int volume_main(int argc, char **argv);
int format_device(const char *device, const char *path, const char *fs_type);
int format_unknown_device(const char *device, const char* path, const char *fs_type);

void handle_failure(int ret);
void write_recovery_version();
int verify_root_and_recovery();

#ifdef USE_F2FS
extern int make_f2fs_main(int argc, char **argv);
extern int fsck_f2fs_main(int argc, char **argv);
extern int fibmap_main(int argc, char **argv);
#endif

#ifdef RECOVERY_EXTEND_NANDROID_MENU
void extend_nandroid_menu(char** items, int item_count, int max_items);
void handle_nandroid_menu(int item_count, int selected);
#endif

#endif  // __EXTENDEDCOMMANDS_H