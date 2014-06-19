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

struct point {
    int x;
    int y;
};

static struct point touch_min;
static struct point touch_max;

static int in_touch;
static int in_swipe;
static struct point touch_start;
static struct point touch_last;
static struct point touch_end;

static int touch_active_slot_count = 0;
static int touch_first_slot = 0;
static int touch_current_slot = 0;
static int touch_tracking_id = -1;
static bool touch_saw_x = false;
static bool touch_saw_y = false;

// Defaults, will be reset based on dpi in set_min_swipe_lengths()
static int min_x_swipe_px = 100;
static int min_y_swipe_px = 80;

typedef struct {
    int keycode;
    int center_x;
    int center_y;
    int width;
    int height;
} virtual_key;

static virtual_key *virtual_keys;
static int virtual_key_count = 0;

static void reset_gestures() {
    in_touch = 0;
    in_swipe = 0;
    touch_start.x = touch_last.x = touch_end.x = -1;
    touch_start.y = touch_last.y = touch_end.y = -1;
    ui_clear_key_queue();
}

static int touch_scale_x(int val) {
    int touch_width = (touch_max.x - touch_min.x);
    if (touch_width) {
        val = val * gr_fb_width() / touch_width;
    }
    return val;
}

static int touch_scale_y(int val) {
    int touch_height = (touch_max.y - touch_min.y);
    if (touch_height) {
        val = val * gr_fb_height() / touch_height;
    }
    return val;
}

static void show_event(struct input_event *ev) {
#ifdef DEBUG_TOUCH_EVENTS
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
    case EV_KEY:
        evtypestr = "EV_KEY";
        switch (ev->code) {
        case BTN_TOUCH:
            evcodestr = "BTN_TOUCH";
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
        case ABS_MT_POSITION_X:
            evcodestr = "ABS_MT_POSITION_X";
            break;
        case ABS_MT_POSITION_Y:
            evcodestr = "ABS_MT_POSITION_Y";
            break;
        case ABS_MT_TRACKING_ID:
            evcodestr = "ABS_MT_TRACKING_ID";
            break;
        case ABS_MT_ANGLE:
            evcodestr = "ABS_MT_ANGLE";
            break;
        }
        break;
    }
    LOGI("show_event: type=%s, code=%s, val=%d\n", evtypestr, evcodestr, ev->value);
#endif
}

static int virtual_keys_did_setup = 0;
static int setup_virtual_keys(int fd) {
    if (virtual_keys_did_setup)
        return 0;
    virtual_keys_did_setup = 1;

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
    while (token) {
        if (count % 6 == 0 && strcmp("0x01", token) != 0) {
            for (i = 0; i < 6; i++)
                token = strtok(NULL, sep);
            continue;
        }
        token = strtok(NULL, sep);
        count++;
    }
    if(count % 6 != 0) {
        LOGI("Non-standard virtual keys file, skipping\n");
        return -1;
    } else {
        virtual_key_count = count / 6;
    }

    // Assign the virtual key parameters
    virtual_keys = malloc(sizeof(virtual_key) * virtual_key_count);

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
#ifdef DEBUG_TOUCH_EVENTS
    for (i = 0; i < count/6; i++) {
        LOGI("Virtual key: code=%d, x=%d, y=%d, width=%d, height=%d\n",
            virtual_keys[i].keycode, virtual_keys[i].center_x,
            virtual_keys[i].center_y, virtual_keys[i].width,
            virtual_keys[i].height);
    }
#endif

    return 0;
}

static void set_min_swipe_lengths() {
    char value[PROPERTY_VALUE_MAX];
    property_get("ro.sf.lcd_density", value, "0");
    int screen_density = atoi(value);
    if(screen_density > 0) {
        min_x_swipe_px = (int)(0.5 * screen_density); // Roughly 0.5in
        min_y_swipe_px = (int)(0.3 * screen_density); // Roughly 0.3in
    } else {
        min_x_swipe_px = gr_fb_width()/4;
        min_y_swipe_px = 3*BOARD_RECOVERY_CHAR_HEIGHT;
    }
}

static int touch_did_calibrate = 0;
static void touch_calibrate(int fd) {
    struct input_absinfo info;

    if (touch_did_calibrate)
        return;
    touch_did_calibrate = 1;

    memset(&info, 0, sizeof(info));
    if (ioctl(fd, EVIOCGABS(EV_CODE_POS_X), &info)) {
        LOGE("Touch calibration failed on x-pos\n");
        return;
    }
    touch_min.x = info.minimum;
    touch_max.x = info.maximum;

    memset(&info, 0, sizeof(info));
    if (ioctl(fd, EVIOCGABS(EV_CODE_POS_Y), &info)) {
        LOGE("Touch calibration failed on y-pos\n");
        return;
    }
    touch_min.y = info.minimum;
    touch_max.y = info.maximum;
}

static void touch_handle_press(struct input_event *ev) {
    /* Empty for now */
}

static int touch_handle_release(struct input_event *ev) {
    int i, dx, dy;
    int rc = 1;

    if (in_swipe) {
        dx = touch_end.x - touch_start.x;
        dy = touch_end.y - touch_start.y;
        if (abs(dx) > abs(dy)) {
            if (abs(dx) > min_x_swipe_px) {
                ev->type = EV_KEY;
                ev->code = (dx > 0 ? KEY_ENTER : KEY_BACK);
                ev->value = 2;
                rc = 0;
            }
        } else {
            /* Vertical swipe, handled real-time */
        }
    } else {
        // Check if virtual key pressed
        for (i = 0; i < virtual_key_count; i++) {
            dx = virtual_keys[i].center_x - touch_end.x;
            dy = virtual_keys[i].center_y - touch_end.y;
            if (abs(dx) < virtual_keys[i].width/2
                    && abs(dy) < virtual_keys[i].height/2) {
                ev->type = EV_KEY;
                ev->code = virtual_keys[i].keycode;
                ev->value = 2;
                rc = 0;
            }
        }
    }

    reset_gestures();
    return rc;
}

static int touch_handle_gestures(struct input_event *ev) {
    int rc = 1;
    struct point diff;
    diff.x = touch_end.x - touch_start.x;
    diff.y = touch_end.y - touch_start.y;

    if (touch_end.x == -1 || touch_end.y == -1) {
        return 1;
    }
    if (abs(diff.x) > abs(diff.y)) {
        if (abs(diff.x) > gr_fb_width()/4) {
            /* Horizontal swipe, handle it on release */
            in_swipe = 1;
        }
    } else {
        if (touch_last.y == -1) {
            touch_last.y = touch_end.y;
        }
        diff.y = touch_end.y - touch_last.y;
        if (abs(diff.y) > min_y_swipe_px) {
            in_swipe = 1;
            touch_last.y = touch_end.y;
            ev->type = EV_KEY;
            ev->code = (diff.y < 0) ? KEY_SWIPE_UP : KEY_SWIPE_DOWN;
            ev->value = 2;
            rc = 0;
        }
    }
    return rc;
}

int touch_handle_input(int fd, struct input_event *ev) {
    int rc = 0;
    int dx, dy;

    show_event(ev);
    touch_calibrate(fd);
    set_min_swipe_lengths();
    setup_virtual_keys(fd);

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

    if (ev->type == EV_SYN) {
        rc = 1;
        if (ev->code == SYN_REPORT) {
            if (in_touch) {
                /* Detect release */
                if (touch_active_slot_count == 0 && !touch_saw_x && !touch_saw_y) {
                    /* type A release */
                    rc = touch_handle_release(ev);
                    reset_gestures();
                    touch_current_slot = touch_first_slot = 0;
                } else if (touch_current_slot == touch_first_slot && touch_tracking_id == -1) {
                    /* type B release */
                    rc = touch_handle_release(ev);
                    reset_gestures();
                    touch_current_slot = touch_first_slot = 0;
                } else {
                    rc = touch_handle_gestures(ev);
                }
            } else {
                if (touch_saw_x && touch_saw_y) {
                    touch_handle_press(ev);
                    in_touch = 1;
                    rc = touch_handle_gestures(ev);
                }
            }
        }
    } else if (ev->type == EV_ABS) {
        rc = 1;
        if (ev->code == ABS_MT_SLOT) {
            touch_current_slot = ev->value;
            if (touch_first_slot == -1) {
                touch_first_slot = touch_current_slot;
            }
            return 1;
        }
        if (ev->code == ABS_MT_TRACKING_ID) {
            touch_tracking_id = ev->value;
            if (touch_tracking_id == -1) {
                touch_active_slot_count--;
            } else {
                touch_active_slot_count++;
            }
            return 1;
        }
        /*
         * For type A devices, we "lock" onto the first coordinates by ignoring
         * position updates from the time we see a SYN_MT_REPORT until the next
         * SYN_REPORT
         *
         * For type B devices, we "lock" onto the first slot seen until all slots
         * are released
         */
        if (touch_active_slot_count == 0) {
            /* type A */
            if (touch_saw_x && touch_saw_y) {
                return 1;
            }
        } else {
            /* type B */
            if (touch_current_slot != touch_first_slot) {
                return 1;
            }
        }
        if (ev->code == EV_CODE_POS_X) {
            touch_saw_x = true;
            touch_end.x = touch_scale_x(ev->value);
            if (touch_start.x == -1) {
                touch_start.x = touch_last.x = touch_end.x;
            }
        } else if (ev->code == EV_CODE_POS_Y) {
            touch_saw_y = true;
            touch_end.y = touch_scale_y(ev->value);
            if (touch_start.y == -1) {
                touch_start.y = touch_last.y = touch_end.y;
            }
        }
    }

    return rc;
}