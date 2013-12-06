/*
 * Copyright (C) 2007 The Android Open Source Project
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

#ifndef RECOVERY_COMMON_H
#define RECOVERY_COMMON_H

#include <stdio.h>
#include <fs_mgr.h>

// Initialize the graphics system.
void ui_init();

// Use KEY_* codes from <linux/input.h> or KEY_DREAM_* from "minui/minui.h".
void ui_cancel_wait_key();
int ui_wait_key();            // waits for a key/button press, returns the code
int ui_wait_key_with_repeat();
int ui_key_pressed(int key);  // returns >0 if the code is currently pressed
int ui_text_visible();        // returns >0 if text log is currently visible
int ui_text_ever_visible();   // returns >0 if text log was ever visible
void ui_show_text(int visible);
void ui_clear_key_queue();

// Write a message to the on-screen log shown with Alt-L (also to stderr).
// The screen is small, and users may need to report these messages to support,
// so keep the output short and not too cryptic.
void ui_print(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
void ui_printlogtail(int nb_lines);

void ui_delete_line();
void ui_set_show_text(int value);
void ui_set_nice(int enabled);
#define ui_nice_print(...) { ui_set_nice(1); ui_print(__VA_ARGS__); ui_set_nice(0); }
int ui_was_niced();
int ui_get_text_cols();
void ui_increment_frame();

#ifdef ENABLE_LOKI
// Toggle for loki support
extern int loki_support_enabled;
int loki_check();
#endif

// Display some header text followed by a menu of items, which appears
// at the top of the screen (in place of any scrolling ui_print()
// output, if necessary).
int ui_start_menu(const char** headers, char** items, int initial_selection);

// Set the menu highlight to the given index, and return it (capped to
// the range [0..numitems).
int ui_menu_select(int sel);

// End menu mode, resetting the text overlay so that ui_print()
// statements will be displayed.
void ui_end_menu();

int ui_get_showing_back_button();
void ui_set_showing_back_button(int showBackButton);

void ui_set_log_stdout(int enabled);
int ui_should_log_stdout();

int ui_get_rainbow_mode();
void ui_rainbow_mode();
void ui_set_rainbow_mode(int rainbowMode);

// Set the icon (normally the only thing visible besides the progress bar).
enum {
  BACKGROUND_ICON_NONE,
  BACKGROUND_ICON_INSTALLING,
  BACKGROUND_ICON_ERROR,
  BACKGROUND_ICON_CLOCKWORK,
  BACKGROUND_ICON_CID,
  BACKGROUND_ICON_FIRMWARE_INSTALLING,
  BACKGROUND_ICON_FIRMWARE_ERROR,
  NUM_BACKGROUND_ICONS
};
void ui_set_background(int icon);

// Get a malloc'd copy of the screen image showing (only) the specified icon.
// Also returns the width, height, and bits per pixel of the returned image.
// TODO: Use some sort of "struct Bitmap" here instead of all these variables?
char *ui_copy_image(int icon, int *width, int *height, int *bpp);

// Show a progress bar and define the scope of the next operation:
//   portion - fraction of the progress bar the next operation will use
//   seconds - expected time interval (progress bar moves at this minimum rate)
void ui_show_progress(float portion, int seconds);
void ui_set_progress(float fraction);  // 0.0 - 1.0 within the defined scope

// Default allocation of progress bar segments to operations
static const int VERIFICATION_PROGRESS_TIME = 60;
static const float VERIFICATION_PROGRESS_FRACTION = 0.25;
static const float DEFAULT_FILES_PROGRESS_FRACTION = 0.4;
static const float DEFAULT_IMAGE_PROGRESS_FRACTION = 0.1;

// Show a rotating "barberpole" for ongoing operations.  Updates automatically.
void ui_show_indeterminate_progress();

// Hide and reset the progress bar.
void ui_reset_progress();

#define LOGE(...) ui_print("E:" __VA_ARGS__)
#define LOGW(...) fprintf(stdout, "W:" __VA_ARGS__)
#define LOGI(...) fprintf(stdout, "I:" __VA_ARGS__)

#if 0
#define LOGV(...) fprintf(stdout, "V:" __VA_ARGS__)
#define LOGD(...) fprintf(stdout, "D:" __VA_ARGS__)
#else
#define LOGV(...) do {} while (0)
#define LOGD(...) do {} while (0)
#endif

#define STRINGIFY(x) #x
#define EXPAND(x) STRINGIFY(x)

typedef struct fstab_rec Volume;

typedef struct {
    // number of frames in indeterminate progress bar animation
    int indeterminate_frames;

    // number of frames per second to try to maintain when animating
    int update_fps;

    // number of frames in installing animation.  may be zero for a
    // static installation icon.
    int installing_frames;

    // the install icon is animated by drawing images containing the
    // changing part over the base icon.  These specify the
    // coordinates of the upper-left corner.
    int install_overlay_offset_x;
    int install_overlay_offset_y;

} UIParameters;

// fopen a file, mounting volumes and making parent dirs as necessary.
FILE* fopen_path(const char *path, const char *mode);

int ui_get_selected_item();
int ui_is_showing_back_button();

void set_perf_mode(int on);

#endif  // RECOVERY_COMMON_H
