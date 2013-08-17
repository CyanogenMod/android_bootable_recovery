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

#include "common.h"

int vold_update_volumes() {

    const char *cmd[2] = {"volume", "list"};
    return vold_command(2, cmd, 1);
}

int vold_mount_volume(const char* path, int wait) {

    const char *cmd[3] = { "volume", "mount" };
    int state = vold_get_volume_state(path);

    if (state == State_Mounted) {
        LOGI("Volume %s already mounted\n", path);
        return 0;
    }

    if (state != State_Idle) {
        LOGI("Volume %s is not idle, current state is %d\n", path, state);
        return -1;
    }

    cmd[2] = path;
    return vold_command(3, cmd, wait);
}

int vold_unmount_volume(const char* path, int wait) {

    const char *cmd[3] = { "volume", "unmount" };
    int state = vold_get_volume_state(path);

    if (state <= State_Idle) {
        LOGI("Volume %s is not mounted", path);
        return 0;
    }

    if (state != State_Mounted) {
        LOGI("Volume %s cannot be unmounted in state %d\n", path, state);
        return -1;
    }

    cmd[2] = path;
    return vold_command(3, cmd, wait);
}

int vold_share_volume(const char* path, int wait) {

    const char *cmd[3] = { "volume", "share" };
    cmd[2] = path;
    return vold_command(3, cmd, wait);
}

int vold_unshare_volume(const char* path, int wait) {

    const char *cmd[3] = { "volume", "unshare" };
    cmd[2] = path;
    return vold_command(3, cmd, wait);
}

int vold_format_volume(const char* path, int wait) {

    const char* cmd[3] = { "volume", "format" };
    cmd[2] = path;
    return vold_command(3, cmd, wait);
}
