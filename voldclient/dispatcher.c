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
#include <pthread.h>

#include "ResponseCode.h"

#include "common.h"

static struct vold_callbacks* callbacks = NULL;
static int should_automount = 0;

struct volume_node {
    const char *label;
    const char *path;
    int state;
    struct volume_node *next;
};

static struct volume_node *volume_head = NULL;
static struct volume_node *volume_tail = NULL;

static pthread_rwlock_t rwlock = PTHREAD_RWLOCK_INITIALIZER;

void vold_set_callbacks(struct vold_callbacks* ev_callbacks) {
    callbacks = ev_callbacks;
}

void vold_set_automount(int automount) {
    should_automount = automount;
}

void vold_mount_all() {

    struct volume_node *node;

    LOGI("Mouting all available volumes..");

    pthread_rwlock_rdlock(&rwlock);
    for (node = volume_head; node; node = node->next) {
        if (node->state == State_Idle) {
            LOGI("mounting volume %s state=%d\n", node->path, node->state);
            vold_mount_volume(node->path, 0);
        }
    }
    pthread_rwlock_unlock(&rwlock);
}

int vold_get_volume_state(const char *path) {

    int ret = 0;
    struct volume_node *node;

    pthread_rwlock_rdlock(&rwlock);
    for (node = volume_head; node; node = node->next) {
        if (strcmp(path, node->path) == 0) {
            ret = node->state;
            break;
        }
    }
    pthread_rwlock_unlock(&rwlock);
    return ret;
}

int vold_is_volume_available(const char *path) {
    return vold_get_volume_state(path) > 0;
}

static void free_volume_list_locked() {

    struct volume_node *node;

    node = volume_head;
    while (node) {
        struct volume_node *next = node->next;
        free((void *)node->path);
        free((void *)node->label);
        free(node);
        node = next;
    }
    volume_head = volume_tail = NULL;
}

static int is_listing_volumes = 0;

static void vold_handle_volume_list(const char* label, const char* path, int state) {

    struct volume_node *node;

    pthread_rwlock_wrlock(&rwlock);
    if (is_listing_volumes == 0) {
        free_volume_list_locked();
        is_listing_volumes = 1;
    }

    node = (struct volume_node *)malloc(sizeof(struct volume_node));
    node->label = strdup(label);
    node->path = strdup(path);
    node->state = state;
    node->next = NULL;

    if (volume_head == NULL)
        volume_head = volume_tail = node;
    else {
        volume_tail->next = node;
        volume_tail = node;
    }
    pthread_rwlock_unlock(&rwlock);

    LOGI("Volume \"%s\" (%s): %s\n", label, path, stateToStr(state));
}

static void vold_handle_volume_list_done() {

    pthread_rwlock_wrlock(&rwlock);
    is_listing_volumes = 0;
    pthread_rwlock_unlock(&rwlock);
}

static void set_volume_state(char* path, int state) {

    struct volume_node *node;

    pthread_rwlock_wrlock(&rwlock);
    for (node = volume_head; node; node = node->next) {
        if (strcmp(node->path, path) == 0) {
            node->state = state;
            break;
        }
    }
    pthread_rwlock_unlock(&rwlock);
}

static void vold_handle_volume_state_change(char* label, char* path, int state) {

    set_volume_state(path, state);

    if (callbacks && callbacks->state_changed)
        callbacks->state_changed(label, path, state);

    LOGI("Volume \"%s\" changed: %s\n", label, stateToStr(state));
}

static void vold_handle_volume_inserted(char* label, char* path) {

    set_volume_state(path, State_Idle);

    if (callbacks && callbacks->state_changed)
        callbacks->disk_added(label, path);

    LOGI("Volume \"%s\" inserted\n", label);

    if (should_automount)
        vold_mount_volume(path, 0);
}

static void vold_handle_volume_removed(char* label, char* path) {

    set_volume_state(path, State_NoMedia);

    if (callbacks && callbacks->disk_removed)
        callbacks->disk_removed(label, path);

    LOGI("Volume \"%s\" removed\n", label);
}

int vold_dispatch(int code, char** tokens, int len) {

    int i = 0;
    int ret = 0;

    if (code == VolumeListResult) {
        // <code> <seq> <label> <path> <state>
        vold_handle_volume_list(tokens[2], tokens[3], atoi(tokens[4]));

    } else if (code == VolumeStateChange) {
        // <code> "Volume <label> <path> state changed from <old_#> (<old_str>) to <new_#> (<new_str>)"
        vold_handle_volume_state_change(tokens[2], tokens[3], atoi(tokens[10]));

    } else if (code == VolumeDiskInserted) {
        // <code> Volume <label> <path> disk inserted (<blk_id>)"
        vold_handle_volume_inserted(tokens[2], tokens[3]);

    } else if (code == VolumeDiskRemoved || code == VolumeBadRemoval) {
        // <code> Volume <label> <path> disk removed (<blk_id>)"
        vold_handle_volume_removed(tokens[2], tokens[3]);

    } else if (code == CommandOkay && is_listing_volumes) {
        vold_handle_volume_list_done();

    } else {
        ret = 0;
    }

    return ret;
}

