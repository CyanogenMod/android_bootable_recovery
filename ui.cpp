/*
 * Copyright (C) 2011 The Android Open Source Project
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

#include <errno.h>
#include <fcntl.h>
#include <linux/input.h>
#include <pthread.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>
#include <sys/epoll.h>

#include <cutils/properties.h>
#include <cutils/android_reboot.h>
#include <cutils/properties.h>

#include "common.h"
#include "roots.h"
#include "device.h"
#include "minui/minui.h"
#include "screen_ui.h"
#include "ui.h"

#include "voldclient.h"

#include "messagesocket.h"

#define UI_WAIT_KEY_TIMEOUT_SEC    120

/* Some extra input defines */
#ifndef ABS_MT_ANGLE
#define ABS_MT_ANGLE 0x38
#endif

static int string_split(char* s, char** fields, int maxfields)
{
    int n = 0;
    while (n+1 < maxfields) {
        char* p = strchr(s, ' ');
        if (!p)
            break;
        *p = '\0';
        printf("string_split: field[%d]=%s\n", n, s);
        fields[n++] = s;
        s = p+1;
    }
    fields[n] = s;
    printf("string_split: last field[%d]=%s\n", n, s);
    return n+1;
}

struct ms_info {
    RecoveryUI*         ui;
    MessageSocket*      sock;
};

static int message_socket_client_event(int fd, uint32_t epevents, void *data)
{
    ms_info* info = (ms_info*)data;

    printf("message_socket client event\n");
    if (!(epevents & EPOLLIN)) {
        return 0;
    }

    char buf[256];
    ssize_t nread;
    nread = info->sock->Read(buf, sizeof(buf));
    if (nread <= 0) {
        ev_del_fd(fd);
        info->ui->DialogDismiss();
        info->sock->Close();
        delete info->sock;
        delete info;
        return 0;
    }

    printf("message_socket client message <%s>\n", buf);

    // Parse the message.  Right now we support:
    //   dialog show <string>
    //   dialog dismiss
    char* fields[3];
    int nfields;
    nfields = string_split(buf, fields, 3);
    printf("fields=%d\n", nfields);
    if (nfields < 2)
        return 0;
    printf("field[0]=%s, field[1]=%s\n", fields[0], fields[1]);
    if (strcmp(fields[0], "dialog") == 0) {
        if (strcmp(fields[1], "show") == 0 && nfields > 2) {
            info->ui->DialogShowInfo(fields[2]);
        }
        if (strcmp(fields[1], "dismiss") == 0) {
            info->ui->DialogDismiss();
        }
    }

    return 0;
}

static int message_socket_listen_event(int fd, uint32_t epevents, void *data)
{
    ms_info* info = (ms_info*)data;
    MessageSocket* sock = info->sock->Accept();
    printf("message_socket_listen_event: event on %d\n", fd);
    if (sock) {
        printf("message_socket client connected\n");
        ms_info* clientinfo = new ms_info;
        clientinfo->ui = info->ui;
        clientinfo->sock = sock;
        ev_add_fd(sock->fd(), message_socket_client_event, clientinfo);
    }
    return 0;
}

RecoveryUI::RecoveryUI()
        : key_queue_len(0),
          key_last_down(-1),
          key_long_press(false),
          key_down_count(0),
          enable_reboot(true),
          v_changed(0),
          consecutive_power_keys(0),
          last_key(-1),
          has_power_key(false),
          has_up_key(false),
          has_down_key(false),
          has_back_key(false),
          has_home_key(false) {
    pthread_mutex_init(&key_queue_mutex, nullptr);
    pthread_cond_init(&key_queue_cond, nullptr);
    memset(key_pressed, 0, sizeof(key_pressed));
}

void RecoveryUI::OnKeyDetected(int key_code) {
    if (key_code == KEY_POWER) {
        has_power_key = true;
    } else if (key_code == KEY_DOWN || key_code == KEY_VOLUMEDOWN) {
        has_down_key = true;
    } else if (key_code == KEY_UP || key_code == KEY_VOLUMEUP) {
        has_up_key = true;
    } else if (key_code == KEY_BACK) {
        has_back_key = true;
        LOGI("Detected back key, disabling virtual back button\n");
    } else if (key_code == KEY_HOMEPAGE || key_code == KEY_HOME) {
        has_home_key = true;
        LOGI("Detected home key, disabling virtual home button\n");
    }
}

int RecoveryUI::InputCallback(int fd, uint32_t epevents, void* data) {
    return reinterpret_cast<RecoveryUI*>(data)->OnInputEvent(fd, epevents);
}

// Reads input events, handles special hot keys, and adds to the key queue.
static void* InputThreadLoop(void*) {
    while (true) {
        if (!ev_wait(-1)) {
            ev_dispatch();
        }
    }
    return nullptr;
}

void RecoveryUI::Init() {
    calibrate_swipe();
    ev_init(InputCallback, this);

    message_socket.ServerInit();
    ms_info* info = new ms_info;
    info->ui = this;
    info->sock = &message_socket;
    ev_add_fd(message_socket.fd(), message_socket_listen_event, info);

    ev_iterate_available_keys(std::bind(&RecoveryUI::OnKeyDetected, this, std::placeholders::_1));

    pthread_create(&input_thread_, nullptr, InputThreadLoop, nullptr);
}

int RecoveryUI::OnInputEvent(int fd, uint32_t epevents) {
    struct input_event ev;
    if (ev_get_input(fd, epevents, &ev) == -1) {
        return -1;
    }

    input_device* dev = NULL;
    int n;
    for (n = 0; n < MAX_NR_INPUT_DEVICES; ++n) {
        if (input_devices[n].fd == fd) {
            dev = &input_devices[n];
            break;
        }
        if (input_devices[n].fd == -1) {
            dev = &input_devices[n];
            memset(dev, 0, sizeof(input_device));
            dev->fd = fd;
            dev->tracking_id = -1;
            calibrate_touch(dev);
            setup_vkeys(dev);
            break;
        }
    }
    if (!dev) {
        LOGE("input_callback: no more available input devices\n");
        return -1;
    }

    if (ev.type != EV_REL) {
        dev->rel_sum = 0;
    }

    switch (ev.type) {
    case EV_SYN:
        ProcessSyn(dev, ev.code, ev.value);
        break;
    case EV_ABS:
        ProcessAbs(dev, ev.code, ev.value);
        break;
    case EV_REL:
        ProcessRel(dev, ev.code, ev.value);
        break;
    case EV_KEY:
        ProcessKey(dev, ev.code, ev.value);
        break;
    }

    return 0;
}

// Process a key-up or -down event.  A key is "registered" when it is
// pressed and then released, with no other keypresses or releases in
// between.  Registered keys are passed to CheckKey() to see if it
// should trigger a visibility toggle, an immediate reboot, or be
// queued to be processed next time the foreground thread wants a key
// (eg, for the menu).
//
// We also keep track of which keys are currently down so that
// CheckKey can call IsKeyPressed to see what other keys are held when
// a key is registered.
//
// updown == 1 for key down events; 0 for key up events
void RecoveryUI::ProcessKey(input_device* dev, int key_code, int updown) {
    bool register_key = false;
    bool long_press = false;
    bool reboot_enabled;

    if (key_code > KEY_MAX)
        return;

    pthread_mutex_lock(&key_queue_mutex);
    key_pressed[key_code] = updown;
    if (updown) {
        ++key_down_count;
        key_last_down = key_code;
        key_long_press = false;
        key_timer_t* info = new key_timer_t;
        info->ui = this;
        info->key_code = key_code;
        info->count = key_down_count;
        pthread_t thread;
        pthread_create(&thread, nullptr, &RecoveryUI::time_key_helper, info);
        pthread_detach(thread);
    } else {
        if (key_last_down == key_code) {
            long_press = key_long_press;
            register_key = true;
        }
        key_last_down = -1;
    }
    reboot_enabled = enable_reboot;
    pthread_mutex_unlock(&key_queue_mutex);

    if (register_key) {
        switch (CheckKey(key_code, long_press)) {
          case RecoveryUI::IGNORE:
            break;

          case RecoveryUI::TOGGLE:
            ShowText(!IsTextVisible());
            break;

          case RecoveryUI::REBOOT:
#ifndef VERIFIER_TEST
            vdc->unmountAll();
#endif
            if (reboot_enabled) {
                property_set(ANDROID_RB_PROPERTY, "reboot,");
                while(1) { pause(); }
            }
            break;

          case RecoveryUI::ENQUEUE:
            EnqueueKey(key_code);
            break;
        }
    }
}

void RecoveryUI::ProcessSyn(input_device* dev, int code, int value) {
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

    if (code == SYN_MT_REPORT) {
        if (!dev->in_touch && (dev->saw_pos_x && dev->saw_pos_y)) {
#ifdef DEBUG_TOUCH
            LOGI("process_syn: type a press\n");
#endif
            handle_press(dev);
        }
        dev->saw_mt_report = true;
        return;
    }
    if (code == SYN_REPORT) {
        if (dev->in_touch) {
            handle_gestures(dev);
        }
        else {
            if (dev->saw_tracking_id) {
#ifdef DEBUG_TOUCH
            LOGI("process_syn: type b press\n");
#endif
                handle_press(dev);
            }
        }

        /* Detect release */
        if (dev->saw_mt_report) {
            if (dev->in_touch && !dev->saw_pos_x && !dev->saw_pos_y) {
                /* type A release */
#ifdef DEBUG_TOUCH
            LOGI("process_syn: type a release\n");
#endif
                handle_release(dev);
                dev->slot_first = 0;
            }
        }
        else {
            if (dev->in_touch && dev->saw_tracking_id && dev->tracking_id == -1 &&
                    dev->slot_current == dev->slot_first) {
                /* type B release */
#ifdef DEBUG_TOUCH
            LOGI("process_syn: type b release\n");
#endif
                handle_release(dev);
                dev->slot_first = 0;
            }
        }

        dev->saw_pos_x = dev->saw_pos_y = false;
        dev->saw_mt_report = dev->saw_tracking_id = false;
    }
}

void RecoveryUI::ProcessAbs(input_device* dev, int code, int value) {
    if (code == ABS_MT_SLOT) {
        dev->slot_current = value;
        if (dev->slot_first == -1) {
            dev->slot_first = value;
        }
        return;
    }
    if (code == ABS_MT_TRACKING_ID) {
        /*
         * Some devices send an initial ABS_MT_SLOT event before switching
         * to type B events, so discard any type A state related to slot.
         */
        dev->saw_tracking_id = true;
        dev->slot_first = dev->slot_current = 0;

        if (value != dev->tracking_id) {
            dev->tracking_id = value;
            if (dev->tracking_id < 0) {
                dev->slot_nr_active--;
            }
            else {
                dev->slot_nr_active++;
            }
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
    if (dev->slot_nr_active == 0) {
        /* type A */
        if (dev->saw_pos_x && dev->saw_pos_y) {
            return;
        }
    }
    else {
        if (dev->slot_current != dev->slot_first) {
            return;
        }
    }
    if (code == ABS_MT_POSITION_X) {
        dev->saw_pos_x = true;
        dev->touch_pos.x = value * fb_dimensions.x / (dev->touch_max.x - dev->touch_min.x);
    }
    else if (code == ABS_MT_POSITION_Y) {
        dev->saw_pos_y = true;
        dev->touch_pos.y = value * fb_dimensions.y / (dev->touch_max.y - dev->touch_min.y);
    }
}

void RecoveryUI::ProcessRel(input_device* dev, int code, int value) {
#ifdef BOARD_RECOVERY_NEEDS_REL_INPUT
    if (code == REL_Y) {
        // accumulate the up or down motion reported by
        // the trackball.  When it exceeds a threshold
        // (positive or negative), fake an up/down
        // key event.
        dev->rel_sum += value;
        if (dev->rel_sum > 3) {
            process_key(dev, KEY_DOWN, 1);   // press down key
            process_key(dev, KEY_DOWN, 0);   // and release it
            dev->rel_sum = 0;
        } else if (dev->rel_sum < -3) {
            process_key(dev, KEY_UP, 1);     // press up key
            process_key(dev, KEY_UP, 0);     // and release it
            dev->rel_sum = 0;
        }
    }
#endif
}

void* RecoveryUI::time_key_helper(void* cookie) {
    key_timer_t* info = (key_timer_t*) cookie;
    info->ui->time_key(info->key_code, info->count);
    delete info;
    return nullptr;
}

void RecoveryUI::time_key(int key_code, int count) {
    usleep(750000);  // 750 ms == "long"
    bool long_press = false;
    pthread_mutex_lock(&key_queue_mutex);
    if (key_last_down == key_code && key_down_count == count) {
        long_press = key_long_press = true;
    }
    pthread_mutex_unlock(&key_queue_mutex);
    if (long_press) KeyLongPress(key_code);
}

void RecoveryUI::calibrate_touch(input_device* dev) {
    fb_dimensions.x = gr_fb_width();
    fb_dimensions.y = gr_fb_height();

    struct input_absinfo info;
    memset(&info, 0, sizeof(info));
    if (ioctl(dev->fd, EVIOCGABS(ABS_MT_POSITION_X), &info) == 0) {
        dev->touch_min.x = info.minimum;
        dev->touch_max.x = info.maximum;
        dev->touch_pos.x = info.value;
    }
    memset(&info, 0, sizeof(info));
    if (ioctl(dev->fd, EVIOCGABS(ABS_MT_POSITION_Y), &info) == 0) {
        dev->touch_min.y = info.minimum;
        dev->touch_max.y = info.maximum;
        dev->touch_pos.y = info.value;
    }
#ifdef DEBUG_TOUCH
    LOGI("calibrate_touch: fd=%d, (%d,%d)-(%d,%d) pos (%d,%d)\n", dev->fd,
            dev->touch_min.x, dev->touch_min.y,
            dev->touch_max.x, dev->touch_max.y,
            dev->touch_pos.x, dev->touch_pos.y);
#endif
}

void RecoveryUI::setup_vkeys(input_device* dev) {
    int n;
    char name[256];
    char path[PATH_MAX];
    char buf[64*MAX_NR_VKEYS];

    for (n = 0; n < MAX_NR_VKEYS; ++n) {
        dev->virtual_keys[n].keycode = -1;
    }

    memset(name, 0, sizeof(name));
    if (ioctl(dev->fd, EVIOCGNAME(sizeof(name)), name) < 0) {
        LOGI("setup_vkeys: no vkeys\n");
        return;
    }
    sprintf(path, "/sys/board_properties/virtualkeys.%s", name);
    int vkfd = open(path, O_RDONLY);
    if (vkfd < 0) {
        LOGI("setup_vkeys: could not open %s\n", path);
        return;
    }
    ssize_t len = read(vkfd, buf, sizeof(buf));
    close(vkfd);
    if (len <= 0) {
        LOGE("setup_vkeys: could not read %s\n", path);
        return;
    }
    buf[len] = '\0';

    char* p = buf;
    char* endp;
    for (n = 0; n < MAX_NR_VKEYS && p < buf+len && *p == '0'; ++n) {
        int val[6];
        int f;
        for (f = 0; *p && f < 6; ++f) {
            val[f] = strtol(p, &endp, 0);
            if (p == endp)
                break;
            p = endp+1;
        }
        if (f != 6 || val[0] != 0x01)
            break;
        dev->virtual_keys[n].keycode = val[1];
        dev->virtual_keys[n].min.x = val[2] - val[4]/2;
        dev->virtual_keys[n].min.y = val[3] - val[5]/2;
        dev->virtual_keys[n].max.x = val[2] + val[4]/2;
        dev->virtual_keys[n].max.y = val[3] + val[5]/2;

#ifdef DEBUG_TOUCH
        LOGI("vkey: fd=%d, [%d]=(%d,%d)-(%d,%d)\n", dev->fd,
                dev->virtual_keys[n].keycode,
                dev->virtual_keys[n].min.x, dev->virtual_keys[n].min.y,
                dev->virtual_keys[n].max.x, dev->virtual_keys[n].max.y);
#endif
    }
}

void RecoveryUI::calibrate_swipe() {
    char strvalue[PROPERTY_VALUE_MAX];
    int  intvalue;
    property_get("ro.sf.lcd_density", strvalue, "160");
    intvalue = atoi(strvalue);
    int screen_density = (intvalue >= 160 ? intvalue : 160);
    min_swipe_px.x = screen_density * 50 / 100; // Roughly 0.5in
    min_swipe_px.y = screen_density * 30 / 100; // Roughly 0.3in
#ifdef DEBUG_TOUCH
    LOGI("calibrate_swipe: density=%d, min_swipe=(%d,%d)\n",
            screen_density, min_swipe_px.x, min_swipe_px.y);
#endif
}

void RecoveryUI::handle_press(input_device* dev) {
    dev->touch_start = dev->touch_track = dev->touch_pos;
    dev->in_touch = true;
    dev->in_swipe = false;
    if (dev->touch_pos.y >= gr_fb_height() - GetSysbarHeight()) {
        SetSysbarState(1 << (3 * dev->touch_pos.x / gr_fb_width()));
    }
    else {
        SetSysbarState(0);
    }
}

void RecoveryUI::handle_release(input_device* dev) {
    struct point diff = dev->touch_pos - dev->touch_start;
    bool in_touch = dev->in_touch;
    bool in_swipe = dev->in_swipe;

    dev->in_touch = dev->in_swipe = false;

    if (!in_swipe) {
        int n;
        for (n = 0; dev->virtual_keys[n].keycode != -1 && n < MAX_NR_VKEYS; ++n) {
            vkey* vk = &dev->virtual_keys[n];
            if (dev->touch_start.x >= vk->min.x && dev->touch_start.x < vk->max.x &&
                    dev->touch_start.y >= vk->min.y && dev->touch_start.y < vk->max.y) {
#ifdef DEBUG_TOUCH
                LOGI("handle_release: vkey %d\n", vk->keycode);
#endif
                EnqueueKey(vk->keycode);
                return;
            }
        }

        int sysbar_state = GetSysbarState();
        SetSysbarState(0);
        if (sysbar_state == SYSBAR_BACK) {
            ProcessKey(dev, KEY_BACK, 1);
            ProcessKey(dev, KEY_BACK, 0);
            return;
        }
        if (sysbar_state == SYSBAR_HOME) {
            ProcessKey(dev, KEY_HOME, 1);
            ProcessKey(dev, KEY_HOME, 0);
            return;
        }
    }

    if (DialogShowing()) {
        if (DialogDismissable() && !dev->in_swipe) {
            DialogDismiss();
        }
        return;
    }

    if (in_swipe) {
        if (abs(diff.x) > abs(diff.y)) {
            if (abs(diff.x) > min_swipe_px.x) {
                int key = (diff.x > 0 ? KEY_ENTER : KEY_BACK);
                ProcessKey(dev, key, 1);
                ProcessKey(dev, key, 0);
            }
        }
        else {
            /* Vertical swipe, handled realtime */
        }
    }
    else {
        int sel, start_menu_pos;
        // Make sure touch pos is not less than menu start pos.
        // No need to check if beyond end of menu items, since
        // that is checked by get_menu_selection().
        start_menu_pos = MenuItemStart();
        if (dev->touch_pos.y >= start_menu_pos) {
            sel = (dev->touch_pos.y - start_menu_pos)/MenuItemHeight();
            EnqueueKey(KEY_FLAG_ABS | sel);
        }
    }
}

void RecoveryUI::handle_gestures(input_device* dev) {
    struct point diff;
    diff = dev->touch_pos - dev->touch_start;

    if (abs(diff.x) > abs(diff.y)) {
        if (abs(diff.x) > min_swipe_px.x) {
            /* Horizontal swipe, handle it on release */
            dev->in_swipe = true;
        }
    }
    else {
        diff.y = dev->touch_pos.y - dev->touch_track.y;
        if (abs(diff.y) > MenuItemHeight()) {
            dev->in_swipe = true;
            if (!DialogShowing()) {
                dev->touch_track = dev->touch_pos;
                int key = (diff.y < 0) ? KEY_VOLUMEUP : KEY_VOLUMEDOWN;
                ProcessKey(dev, key, 1);
                ProcessKey(dev, key, 0);
            }
        }
    }
}

void RecoveryUI::EnqueueKey(int key_code) {
    if (DialogShowing()) {
        if (DialogDismissable()) {
            DialogDismiss();
        }
        return;
    }
    pthread_mutex_lock(&key_queue_mutex);
    const int queue_max = sizeof(key_queue) / sizeof(key_queue[0]);
    if (key_queue_len < queue_max) {
        key_queue[key_queue_len++] = key_code;
        pthread_cond_signal(&key_queue_cond);
    }
    pthread_mutex_unlock(&key_queue_mutex);
}

int RecoveryUI::WaitKey() {
    pthread_mutex_lock(&key_queue_mutex);
    int timeouts = UI_WAIT_KEY_TIMEOUT_SEC;

    // Time out after UI_WAIT_KEY_TIMEOUT_SEC, unless a USB cable is
    // plugged in.
    do {
        struct timeval now;
        struct timespec timeout;
        gettimeofday(&now, nullptr);
        timeout.tv_sec = now.tv_sec;
        timeout.tv_nsec = now.tv_usec * 1000;
        timeout.tv_sec += 1;

        int rc = 0;
        while (key_queue_len == 0 && rc != ETIMEDOUT) {
            rc = pthread_cond_timedwait(&key_queue_cond, &key_queue_mutex, &timeout);
            if (VolumesChanged()) {
                pthread_mutex_unlock(&key_queue_mutex);
                return Device::kRefresh;
            }
            timeouts--;
        }
    } while ((timeouts || IsUsbConnected()) && key_queue_len == 0);

    int key = -1;
    if (key_queue_len > 0) {
        key = key_queue[0];
        memcpy(&key_queue[0], &key_queue[1], sizeof(int) * --key_queue_len);
    }
    pthread_mutex_unlock(&key_queue_mutex);
    return key;
}

void RecoveryUI::CancelWaitKey()
{
    pthread_mutex_lock(&key_queue_mutex);
    key_queue[key_queue_len] = -2;
    key_queue_len++;
    pthread_cond_signal(&key_queue_cond);
    pthread_mutex_unlock(&key_queue_mutex);
}

bool RecoveryUI::IsUsbConnected() {
    int fd = open("/sys/class/android_usb/android0/state", O_RDONLY);
    if (fd < 0) {
        printf("failed to open /sys/class/android_usb/android0/state: %s\n",
               strerror(errno));
        return 0;
    }

    char buf;
    // USB is connected if android_usb state is CONNECTED or CONFIGURED.
    int connected = (TEMP_FAILURE_RETRY(read(fd, &buf, 1)) == 1) && (buf == 'C');
    if (close(fd) < 0) {
        printf("failed to close /sys/class/android_usb/android0/state: %s\n",
               strerror(errno));
    }
    return connected;
}

bool RecoveryUI::IsKeyPressed(int key) {
    pthread_mutex_lock(&key_queue_mutex);
    int pressed = key_pressed[key];
    pthread_mutex_unlock(&key_queue_mutex);
    return pressed;
}

bool RecoveryUI::IsLongPress() {
    pthread_mutex_lock(&key_queue_mutex);
    bool result = key_long_press;
    pthread_mutex_unlock(&key_queue_mutex);
    return result;
}

bool RecoveryUI::HasThreeButtons() {
    return has_power_key && has_up_key && has_down_key;
}

void RecoveryUI::FlushKeys() {
    pthread_mutex_lock(&key_queue_mutex);
    key_queue_len = 0;
    pthread_mutex_unlock(&key_queue_mutex);
}

RecoveryUI::KeyAction RecoveryUI::CheckKey(int key, bool is_long_press) {
    pthread_mutex_lock(&key_queue_mutex);
    key_long_press = false;
    pthread_mutex_unlock(&key_queue_mutex);

    // If we have power and volume up keys, that chord is the signal to toggle the text display.
    if (HasThreeButtons()) {
        if (key == KEY_VOLUMEUP && IsKeyPressed(KEY_POWER)) {
            return TOGGLE;
        }
    } else {
        // Otherwise long press of any button toggles to the text display,
        // and there's no way to toggle back (but that's pretty useless anyway).
        if (is_long_press && !IsTextVisible()) {
            return TOGGLE;
        }

        // Also, for button-limited devices, a long press is translated to KEY_ENTER.
        if (is_long_press && IsTextVisible()) {
            EnqueueKey(KEY_ENTER);
            return IGNORE;
        }
    }

    // Press power seven times in a row to reboot.
    if (key == KEY_POWER) {
        pthread_mutex_lock(&key_queue_mutex);
        bool reboot_enabled = enable_reboot;
        pthread_mutex_unlock(&key_queue_mutex);

        if (reboot_enabled) {
            ++consecutive_power_keys;
            if (consecutive_power_keys >= 7) {
                return REBOOT;
            }
        }
    } else {
        consecutive_power_keys = 0;
    }

    last_key = key;
    return IsTextVisible() ? ENQUEUE : IGNORE;
}

void RecoveryUI::KeyLongPress(int) {
}

void RecoveryUI::SetEnableReboot(bool enabled) {
    pthread_mutex_lock(&key_queue_mutex);
    enable_reboot = enabled;
    pthread_mutex_unlock(&key_queue_mutex);
}

bool RecoveryUI::VolumesChanged() {
    int ret = v_changed;
    if (v_changed > 0)
        v_changed = 0;
    return ret == 1;
}
