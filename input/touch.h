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

#ifndef TOUCH_H_
#define TOUCH_H_

#define DEBUG_TOUCH_EVENTS 0
#define MAX_INPUT_DEVS   8
#define MAX_VIRTUAL_KEYS 8

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

typedef struct {
    int          fd;            // Initialize to -1
    int          touch_calibrated;
    int          slot_first;
    int          slot_current;
    int          slot_count_active;
    int          tracking_id;
    int          rel_sum;        // Accumulated relative movement

    int          saw_pos_x;      // Did this sequence have an ABS_MT_POSITION_X?
    int          saw_pos_y;      // Did this sequence have an ABS_MT_POSITION_Y?
    int          saw_mt_report;  // Did this sequence have an SYN_MT_REPORT?
    int          saw_tracking_id;// Did this sequence have a SYN_TRACKING_ID?
    int          in_touch;       // Are we in a touch event?
    int          in_swipe;       // Are we in a swipe event?

    point        touch_min;
    point        touch_max;
    point        touch_pos;      // Current touch coordinates
    point        touch_start;    // Coordinates of touch start
    point        touch_track;    // Last tracked coordinates

    virtual_key  virtual_keys[MAX_VIRTUAL_KEYS];
    int          virtual_key_count;
} input_device;

void touch_handle_input(int fd, struct input_event *ev);

#endif // TOUCH_H_