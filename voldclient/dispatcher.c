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

#include "Volume.h"
#include "ResponseCode.h"

#include "common.h"

static int vold_handle_volume_list(char* label, char* mountpoint, int state) {
    ui_print("Volume \"%s\" (%s): %s\n", label, mountpoint, stateToStr(state));
    return 0;
}

static int vold_handle_volume_state_change(char* label, char* mountpoint, int old_state, int state) {
    ui_print("Volume \"%s\" changed: %s\n", label, stateToStr(state));
    return 0;
}

static int vold_handle_volume_inserted(char* label, char* mountpoint) {
    ui_print("Volume \"%s\" inserted\n", label);
    return 0;
}

static int vold_handle_volume_removed(char* label, char* mountpoint) {
    ui_print("Volume \"%s\" removed\n", label);
    return 0;
}

int vold_dispatch(int code, char** tokens, int len) {

    int i = 0;
    int ret = -1;

    if (code == VolumeListResult) {
        // <code> <seq> <label> <mountpoint> <state>
        ret = vold_handle_volume_list(tokens[2], tokens[3], atoi(tokens[4]));

    } else if (code == VolumeStateChange) {
        // <code> "Volume <label> <path> state changed from <old_#> (<old_str>) to <new_#> (<new_str>)"
        ret = vold_handle_volume_state_change(tokens[2], tokens[3], atoi(tokens[7]), atoi(tokens[10]));

    } else if (code == VolumeDiskInserted) {
        // <code> Volume <label> <path> disk inserted (<blk_id>)"
        ret = vold_handle_volume_inserted(tokens[2], tokens[3]);

    } else if (code == VolumeDiskRemoved) {
        // <code> Volume <label> <path> disk removed (<blk_id>)"
        ret = vold_handle_volume_removed(tokens[2], tokens[3]);

    } else if (code == VolumeBadRemoval) {
        // <code> Volume <label> <path> bad removal (<blk_id>)"
        ret = vold_handle_volume_removed(tokens[2], tokens[3]);
    }

    return ret;
}

