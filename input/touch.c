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

#include <cutils/properties.h>
#include <limits.h>
#include <linux/input.h>
#include <stdio.h>
#include <stdlib.h>

#include "../minui/minui.h"
#include "../common.h"
#include "touch.h"

#ifndef ABS_MT_ANGLE
#define ABS_MT_ANGLE 0x38
#endif

/* Handle axis swap */
#ifdef BOARD_RECOVERY_SWIPE_SWAPXY
#define EV_CODE_POS_X  ABS_MT_POSITION_Y
#define EV_CODE_POS_Y  ABS_MT_POSITION_X
#else
#define EV_CODE_POS_X  ABS_MT_POSITION_X
#define EV_CODE_POS_Y  ABS_MT_POSITION_Y
#endif

#define KEY_SWIPE_UP   KEY_VOLUMEUP
#define KEY_SWIPE_DOWN KEY_VOLUMEDOWN

#define MAX_VIRTUAL_KEYS 20

typedef struct {
    int x;
    int y;
} point;

typedef struct {
    int keycode;
    int center_x;
    int center_y;
    int width;
    int height;
} virtual_key;

static point touch_min;
static point touch_max;
static point touch_start;
static point touch_pos;
static point touch_track;

static int in_touch = 0;
static int in_swipe = 0;
static int saw_mt_report = 0;
static int slot_count_active = 0;
static int slot_first = 0;
static int slot_current = 0;
static int tracking_id = -1;
static int saw_pos_x = 0;
static int saw_pos_y = 0;

// Defaults, will be changed based on dpi in calibrate_swipe()
static int min_swipe_dx = 100;
static int min_swipe_dy = 80;

static virtual_key virtual_keys[MAX_VIRTUAL_KEYS];
static int virtual_key_count = 0;

static void show_event(int fd, struct input_event *ev) {
#if DEBUG_TOUCH_EVENTS
    char typebuf[40];
    char codebuf[40];
    const char *evtypestr = NULL;
    const char *evcodestr = NULL;

    sprintf(typebuf, "0x%04x", ev->type);
    evtypestr = typebuf;

    sprintf(codebuf, "0x%04x", ev->code);
    evcodestr = codebuf;

    switch (ev->type) {
    case EV_SYN:
        evtypestr = "EV_SYN";
        switch (ev->code) {
        case SYN_REPORT:
            evcodestr = "SYN_REPORT";
            break;
        case SYN_MT_REPORT:
            evcodestr = "SYN_MT_REPORT";
            break;
        }
        break;
    case EV_ABS:
        evtypestr = "EV_ABS";
        switch (ev->code) {
        case ABS_MT_TOUCH_MAJOR:
            evcodestr = "ABS_MT_TOUCH_MAJOR";
            break;
        case ABS_MT_TOUCH_MINOR:
            evcodestr = "ABS_MT_TOUCH_MINOR";
            break;
        case ABS_MT_WIDTH_MAJOR:
            evcodestr = "ABS_MT_WIDTH_MAJOR";
            break;
        case ABS_MT_WIDTH_MINOR:
            evcodestr = "ABS_MT_WIDTH_MINOR";
            break;
        case ABS_MT_ORIENTATION:
            evcodestr = "ABS_MT_ORIGENTATION";
            break;
        case ABS_MT_POSITION_X:
            evcodestr = "ABS_MT_POSITION_X";
            break;
        case ABS_MT_POSITION_Y:
            evcodestr = "ABS_MT_POSITION_Y";
            break;
        case ABS_MT_TRACKING_ID:
            evcodestr = "ABS_MT_TRACKING_ID";
            break;
        case ABS_MT_PRESSURE:
            evcodestr = "ABS_MT_PRESSURE";
            break;
        case ABS_MT_ANGLE:
            evcodestr = "ABS_MT_ANGLE";
            break;
        }
        break;
    }
    LOGI("show_event: fd=%d, type=%s, code=%s, val=%d\n", fd, evtypestr, evcodestr, ev->value);
#endif
}

static int virtual_keys_setup = 0;
static int setup_virtual_keys(int fd) {
    if (virtual_keys_setup)
        return 0;
    virtual_keys_setup = 1;

    char name[256] = "unknown";
    char vkpath[PATH_MAX] = "/sys/board_properties/virtualkeys.";

    if (ioctl(fd, EVIOCGNAME(sizeof(name)), name) < 0) {
        LOGI("Could not find virtual keys device name\n");
        return -1;
    }
    snprintf(vkpath, PATH_MAX, "%s%s", vkpath, name);
    LOGI("Loading virtual keys file: %s\n", vkpath);

    FILE *f;
    char buf[1024], tmp[1024];

    f = fopen(vkpath, "r");
    if (f != NULL) {
        if (fgets(buf, 1024, f) == NULL) {
            LOGI("Non-standard virtual keys file, skipping\n");
            return -1;
        }
        fclose(f);
    } else {
        LOGI("Could not open virtual keys file\n");
        return -1;
    }

    int i;
    char *token;
    const char *sep = ":";

    // Count the number of valid virtual keys
    strcpy(tmp, buf);
    token = strtok(tmp, sep);
    int count = 0;
    while (token && (count/6) < MAX_VIRTUAL_KEYS) {
        if (count % 6 == 0 && strcmp("0x01", token) != 0) {
            for (i = 0; i < 6; i++)
                token = strtok(NULL, sep);
            continue;
        }
        token = strtok(NULL, sep);
        count++;
    }
    if (count % 6 != 0 || count == 0) {
        LOGI("Non-standard virtual keys file, skipping\n");
        return -1;
    }
    virtual_key_count = count / 6;

    // Assign the virtual key parameters
    char *endp;
    strcpy(tmp, buf);
    token = strtok(tmp, sep);
    count = 0;
    while (token) {
        if (count % 6 == 0 && strcmp("0x01", token) != 0) {
            for (i = 0; i < 6; i++)
                token = strtok(NULL, sep);
            continue;
        }
        if (count % 6 == 1)
            virtual_keys[count / 6].keycode = (int)strtol(token, &endp, 10);
        if (count % 6 == 2)
            virtual_keys[count / 6].center_x = (int)strtol(token, &endp, 10);
        if (count % 6 == 3)
            virtual_keys[count / 6].center_y = (int)strtol(token, &endp, 10);
        if (count % 6 == 4)
            virtual_keys[count / 6].width = (int)strtol(token, &endp, 10);
        if (count % 6 == 5)
            virtual_keys[count / 6].height = (int)strtol(token, &endp, 10);

        token = strtok(NULL, sep);
        count++;
    }
#if DEBUG_TOUCH_EVENTS
    for (i = 0; i < count/6; i++) {
        LOGI("Virtual key: code=%d, x=%d, y=%d, width=%d, height=%d\n",
            virtual_keys[i].keycode, virtual_keys[i].center_x,
            virtual_keys[i].center_y, virtual_keys[i].width,
            virtual_keys[i].height);
    }
#endif

    return 0;
}

static int swipe_calibrated = 0;
static void calibrate_swipe() {
    if (swipe_calibrated)
        return;
    swipe_calibrated = 1;

    char value[PROPERTY_VALUE_MAX];
    property_get("ro.sf.lcd_density", value, "0");
    int screen_density = atoi(value);
    if(screen_density > 160) {
        min_swipe_dx = (int)(0.5 * screen_density); // Roughly 0.5in
        min_swipe_dy = (int)(0.3 * screen_density); // Roughly 0.3in
    } else {
        min_swipe_dx = gr_fb_width()/4;
        min_swipe_dy = 3*BOARD_RECOVERY_CHAR_HEIGHT;
    }
}

static int touch_calibrated = 0;
static void calibrate_touch(int fd) {
    if (touch_calibrated)
        return;

    struct input_absinfo info;
    int xcal = 0;
    int ycal = 0;

    memset(&info, 0, sizeof(info));
    if (ioctl(fd, EVIOCGABS(EV_CODE_POS_X), &info) == 0) {
        touch_min.x = info.minimum;
        touch_max.x = info.maximum;
        touch_pos.x = info.value;
        xcal = 1;
    }
    memset(&info, 0, sizeof(info));
    if (ioctl(fd, EVIOCGABS(EV_CODE_POS_Y), &info) == 0) {
        touch_min.y = info.minimum;
        touch_max.y = info.maximum;
        touch_pos.y = info.value;
        ycal = 1;
    }

    if (xcal && ycal)
        touch_calibrated = 1;
}

static void handle_press(struct input_event *ev) {
    touch_start = touch_pos;
    touch_track = touch_pos;
    in_touch = 0;
    in_swipe = 0;
}

static void handle_release(struct input_event *ev) {
    int i;
    int dx, dy;

    if (in_swipe) {
        dx = touch_pos.x - touch_start.x;
        dy = touch_pos.y - touch_start.y;
        if (abs(dx) > abs(dy) && abs(dx) > min_swipe_dx) {
            ev->type = EV_KEY;
            ev->code = (dx > 0 ? KEY_ENTER : KEY_BACK);
            ev->value = 2;
        } else {
            /* Vertical swipe, handled real-time */
        }
    } else {
        // Check if virtual key pressed
        for (i = 0; i < virtual_key_count; i++) {
            dx = virtual_keys[i].center_x - touch_pos.x;
            dy = virtual_keys[i].center_y - touch_pos.y;
            if (abs(dx) < virtual_keys[i].width/2
                    && abs(dy) < virtual_keys[i].height/2) {
#if DEBUG_TOUCH_EVENTS
                LOGI("Virtual key event: code=%d, touch-x=%d, touch-y=%d",
                        virtual_keys[i].keycode, touch_pos.x, touch_pos.y);
#endif
                ev->type = EV_KEY;
                ev->code = virtual_keys[i].keycode;
                ev->value = 2;
            }
        }
    }

    ui_clear_key_queue();
}

static void handle_gestures(struct input_event *ev) {
    int dx = touch_pos.x - touch_start.x;
    int dy = touch_pos.y - touch_start.y;
    if (abs(dx) > abs(dy)) {
        if (abs(dx) > min_swipe_dx) {
            /* Horizontal swipe, handle it on release */
            in_swipe = 1;
        }
    } else {
        dy = touch_pos.y - touch_track.y;
        if (abs(dy) > min_swipe_dy) {
            in_swipe = 1;
            touch_track = touch_pos;
            ev->type = EV_KEY;
            ev->code = (dy < 0) ? KEY_SWIPE_UP : KEY_SWIPE_DOWN;
            ev->value = 2;
        }
    }
}

static void process_syn(struct input_event *ev) {
    /*
     * Type A device release:
     *   1. Lack of position update
     *   2. BTN_TOUCH | ABS_PRESSURE | SYN_MT_REPORT
     *   3. SYN_REPORT
     *
     * Type B device release:
     *   1. ABS_MT_TRACKING_ID == -1 for "first" slot
     *   2. SYN_REPORT
     */

    if (ev->code == SYN_MT_REPORT) {
        saw_mt_report = 1;
        return;
    }
    if (ev->code == SYN_REPORT) {
        if (in_touch) {
            /* Detect release */
            if (saw_mt_report == 0 && !saw_pos_x && !saw_pos_y) {
                /* type A release */
                handle_release(ev);
                in_touch = 0;
                in_swipe = 0;
                slot_first = 0;
            } else if (slot_current == slot_first && tracking_id == -1) {
                /* type B release */
                handle_release(ev);
                in_touch = 0;
                in_swipe = 0;
                slot_first = 0;
            }
        } else {
            handle_press(ev);
            in_touch = 1;
        }

        handle_gestures(ev);
        saw_pos_x = 0;
        saw_pos_y = 0;
        saw_mt_report = 0;
    }
}

static void process_abs(struct input_event *ev) {
    if (ev->code == ABS_MT_SLOT) {
        slot_current = ev->value;
        if (slot_first == -1)
            slot_first = ev->value;

        return;
    }
    if (ev->code == ABS_MT_TRACKING_ID) {
        if (ev->value != tracking_id) {
            tracking_id = ev->value;
            if (tracking_id < 0)
                slot_count_active--;
            else
                slot_count_active++;
        }
        return;
    }
    /*
     * For type A devices, we "lock" onto the first coordinates by ignoring
     * position updates from the time we see a SYN_MT_REPORT until the next
     * SYN_REPORT
     *
     * For type B devices, we "lock" onto the first slot seen until all slots
     * are released
     */
    if (slot_count_active == 0) {
        /* type A */
        if (saw_pos_x && saw_pos_y) {
            return;
        }
    } else {
        /* type B */
        if (slot_current != slot_first)
            return;
    }
    if (ev->code == EV_CODE_POS_X) {
        saw_pos_x = 1;
        touch_pos.x = ev->value * gr_fb_width() / (touch_max.x - touch_min.x);
    } else if (ev->code == EV_CODE_POS_Y) {
        saw_pos_y = 1;
        touch_pos.y = ev->value * gr_fb_height() / (touch_max.y - touch_min.y);
    }
}

static void init_touch(int fd) {
    calibrate_swipe();
    calibrate_touch(fd);
    if (touch_calibrated)
        setup_virtual_keys(fd);
}

void touch_handle_input(int fd, struct input_event *ev) {
    if (ev->type == EV_SYN || ev->type == EV_ABS) {
        show_event(fd, ev);
        init_touch(fd);
    }

    if (ev->type == EV_SYN)
        process_syn(ev);
    else if (ev->type == EV_ABS)
        process_abs(ev);
}