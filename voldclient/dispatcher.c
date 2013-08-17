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
#include <errno.h>

#include "ResponseCode.h"

#include "common.h"

static struct vold_callbacks* callbacks;
static int should_automount = 0;

void vold_set_config(struct vold_callbacks* ev_callbacks, int automount) {
    callbacks = ev_callbacks;
    should_automount = automount;
}

static int vold_handle_volume_list(char* label, char* mountpoint, int state) {
    LOGI("Volume \"%s\" (%s): %s\n", label, mountpoint, stateToStr(state));
    return 0;
}

static int vold_handle_volume_state_change(char* label, char* mountpoint, int old_state, int state) {
    LOGI("Volume \"%s\" changed: %s\n", label, stateToStr(state));
    return 0;
}

static int vold_handle_volume_inserted(char* label, char* mountpoint) {
    LOGI("Volume \"%s\" inserted\n", label);
    return 0;
}

static int vold_handle_volume_removed(char* label, char* mountpoint) {
    LOGI("Volume \"%s\" removed\n", label);
    return 0;
}

int vold_dispatch(int code, char** tokens, int len) {

    int i = 0;
    int ret = -1;
    if (code == VolumeListResult) {
        // <code> <seq> <label> <mountpoint> <state>
        int state = atoi(tokens[4]);
        ret = vold_handle_volume_list(tokens[2], tokens[3], state);
        if (should_automount && state == State_Idle)
            vold_mount_volume(tokens[2], 0);

    } else if (code == VolumeStateChange) {
        // <code> "Volume <label> <path> state changed from <old_#> (<old_str>) to <new_#> (<new_str>)"
        if (callbacks && callbacks->state_changed) {
            ret = callbacks->state_changed(tokens[2], tokens[3], atoi(tokens[7]), atoi(tokens[10]));
        } else {
            ret = vold_handle_volume_state_change(tokens[2], tokens[3], atoi(tokens[7]), atoi(tokens[10]));
        }

    } else if (code == VolumeDiskInserted) {
        // <code> Volume <label> <path> disk inserted (<blk_id>)"
        if (callbacks && callbacks->disk_inserted) {
            ret = callbacks->disk_inserted(tokens[2], tokens[3]);
        } else {
            ret = vold_handle_volume_inserted(tokens[2], tokens[3]);
        }
        if (should_automount)
            vold_mount_volume(tokens[2], 0);

    } else if (code == VolumeDiskRemoved || code == VolumeBadRemoval) {
        // <code> Volume <label> <path> disk removed (<blk_id>)"
        if (callbacks && callbacks->disk_removed) {
            ret = callbacks->disk_removed(tokens[2], tokens[3]);
        } else {
            ret = vold_handle_volume_removed(tokens[2], tokens[3]);
        }
    }

    return ret;
}

