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

#ifndef _RECOVERY_CMDS_H
#define _RECOVERY_CMDS_H

#include <stdio.h>
#include <string.h>

int vold_main(int argc, char **argv);
int minizip_main(int argc, char **argv);
int miniunz_main(int argc, char **argv);
int make_ext4fs_main(int argc, char **argv);
int reboot_main(int argc, char **argv);
int poweroff_main(int argc, char **argv);
int fsck_msdos_main(int argc, char **argv);
int newfs_msdos_main(int argc, char **argv);
int pigz_main(int argc, char **argv);
int start_main(int argc, char **argv);
int stop_main(int argc, char **argv);
int mksh_main(int argc, char **argv);
int vdc_main(int argc, char **argv);

int toybox_driver(int argc, char **argv);

struct recovery_cmd {
    const char *name;
    int (*main_func)(int argc, char **argv);
};

static const struct recovery_cmd recovery_cmds[] = {
    { "minivold",       vold_main },
    { "minizip",        minizip_main },
    { "make_ext4fs",    make_ext4fs_main },
    { "reboot",         reboot_main },
    { "poweroff",       reboot_main },
    { "fsck_msdos",     fsck_msdos_main },
    { "newfs_msdos",    newfs_msdos_main },
    { "pigz",           pigz_main },
    { "gzip",           pigz_main },
    { "gunzip",         pigz_main },
    { "zip",            minizip_main },
    { "unzip",          miniunz_main },
    { "start",          start_main },
    { "stop",           stop_main },
    { "sh",             mksh_main },
    { "vdc",            vdc_main },
    { NULL, NULL },
};

struct recovery_cmd get_command(char* command) {
    int i;

    for (i = 0; recovery_cmds[i].name; i++) {
        if (strcmp(command, recovery_cmds[i].name) == 0)
            break;
    }

    return recovery_cmds[i];
}
#endif
