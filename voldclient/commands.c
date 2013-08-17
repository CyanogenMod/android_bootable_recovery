/*
 * Copyright (c) 2013 The CyanogenMod Project
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

#include "voldclient.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>


int vold_scan_volumes(int wait) {

    const char *cmd[2] = {"volume", "list"};
    return vold_command(2, cmd, wait);
}

int vold_mount_volume(const char* name, int wait) {

    const char *cmd[3] = { "volume", "mount" };
    cmd[2] = name;
    return vold_command(3, cmd, wait);
}

int vold_unmount_volume(const char* name, int wait) {

    const char *cmd[3] = { "volume", "unmount" };
    cmd[2] = name;
    return vold_command(3, cmd, wait);
}

int vold_share_volume(const char* name, int wait) {

    const char *cmd[3] = { "volume", "share" };
    cmd[2] = name;
    return vold_command(3, cmd, wait);
}

int vold_unshare_volume(const char* name, int wait) {

    const char *cmd[3] = { "volume", "unshare" };
    cmd[2] = name;
    return vold_command(3, cmd, wait);
}
