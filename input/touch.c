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

typedef struct {
    int x;
    int y;
} point;

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

static void calibrate_swipe() {
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
        // Handle tap
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