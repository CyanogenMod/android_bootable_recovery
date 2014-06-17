#include <linux/input.h>
#include <stdio.h>
#include <stdlib.h>

#include "../minui/minui.h"
#include "../common.h"
#include "touch.h"

#define CHAR_WIDTH BOARD_RECOVERY_CHAR_WIDTH
#define CHAR_HEIGHT BOARD_RECOVERY_CHAR_HEIGHT

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

static void touch_reset() {
    in_touch = 0;
    in_swipe = 0;
    touch_start.x = touch_last.x = touch_end.x = -1;
    touch_start.y = touch_last.y = touch_end.y = -1;
}

static int touch_scale_x(int val)
{
    int touch_width = (touch_max.x - touch_min.x);
    if (touch_width) {
        val = val * gr_fb_width() / touch_width;
    }
    return val;
}

static int touch_scale_y(int val)
{
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
    int rc = 1;
    int dx = touch_end.x - touch_start.x;
    int dy = touch_end.y - touch_start.y;

    if (in_swipe) {
        if (abs(dx) > abs(dy)) {
            if (abs(dx) > gr_fb_width()/4) {
                ev->type = EV_KEY;
                ev->code = (dx > 0 ? KEY_ENTER : KEY_BACK);
                ev->value = 2;
                rc = 0;
            }
        } else {
            /* Vertical swipe, handled real-time */
        }
    }
/*
 *  else {} here can cover the case of non-swipe touch
 */

    touch_reset();
    return rc;
}

int touch_handle_gestures(struct input_event *ev) {
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
        if (abs(diff.y) > 3*CHAR_HEIGHT) {
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
                    touch_reset();
                    touch_current_slot = touch_first_slot = 0;
                } else if (touch_current_slot == touch_first_slot && touch_tracking_id == -1) {
                    /* type B release */
                    rc = touch_handle_release(ev);
                    touch_reset();
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