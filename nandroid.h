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

#ifndef NANDROID_H
#define NANDROID_H

int nandroid_main(int argc, char** argv);
int bu_main(int argc, char** argv);

int nandroid_backup(const char* backup_path);
int nandroid_restore(const char* backup_path, unsigned char flags);
void nandroid_dedupe_gc(const char* blob_dir);
void nandroid_force_backup_format(const char* fmt);
unsigned int nandroid_get_default_backup_format();

#define NANDROID_BACKUP_FORMAT_TAR 0
#define NANDROID_BACKUP_FORMAT_DUP 1
#define NANDROID_BACKUP_FORMAT_TGZ 2

#define NANDROID_ERROR_GENERAL 1

#define NANDROID_NONE   0
#define NANDROID_BOOT   1
#define NANDROID_SYSTEM 2
#define NANDROID_DATA   4
#define NANDROID_CACHE  8
#define NANDROID_SDEXT  16
#define NANDROID_WIMAX  32

#endif
