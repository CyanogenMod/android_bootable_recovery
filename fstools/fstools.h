/*
 * Copyright (C) 2013 The CyanogenMod Project
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

#ifndef _FSTOOLS_CMDS_H
#define _FSTOOLS_CMDS_H

#include <stdio.h>
#include <string.h>

int e2fsck_main(int argc, char **argv);
int mke2fs_main(int argc, char **argv);
int tune2fs_main(int argc, char **argv);

#ifdef HAVE_EXFAT
int fsck_exfat_main(int argc, char **argv);
int mkfs_exfat_main(int argc, char **argv);
int mount_exfat_main(int argc, char **argv);
#endif

int fsck_ntfs3g_main(int argc, char **argv);
int mkfs_ntfs3g_main(int argc, char **argv);
int mount_ntfs3g_main(int argc, char **argv);

int mkfs_f2fs_main(int argc, char **argv);
int fsck_f2fs_main(int argc, char **argv);
int fibmap_main(int argc, char **argv);

int sgdisk_main(int argc, char **argv);

struct fstools_cmd {
    const char *name;
    int (*main_func)(int argc, char **argv);
};

static const struct fstools_cmd fstools_cmds[] = {
    { "e2fsck",         e2fsck_main },
    { "mke2fs",         mke2fs_main },
    { "tune2fs",        tune2fs_main },
    { "fsck.ext4",      e2fsck_main },
    { "mkfs.ext4",      mke2fs_main },
#ifdef HAVE_EXFAT
    { "fsck.exfat",     fsck_exfat_main },
    { "mkfs.exfat",     mkfs_exfat_main },
    { "mount.exfat",    mount_exfat_main },
#endif
    { "fsck.ntfs",      fsck_ntfs3g_main },
    { "mkfs.ntfs",      mkfs_ntfs3g_main },
    { "mount.ntfs",     mount_ntfs3g_main },
    { "mkfs.f2fs",      mkfs_f2fs_main },
    { "fsck.f2fs",      fsck_f2fs_main },
    { "sgdisk",         sgdisk_main },
    { NULL, NULL },
};

struct fstools_cmd get_command(char* command) {
    int i;

    for (i = 0; fstools_cmds[i].name; i++) {
        if (strcmp(command, fstools_cmds[i].name) == 0)
            break;
    }

    return fstools_cmds[i];
}
#endif
