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

#ifndef RECOVERY_SCREEN_UI_H
#define RECOVERY_SCREEN_UI_H

#include <pthread.h>
#include <stdio.h>

#include "ui.h"
#include "minui/minui.h"

// Implementation of RecoveryUI appropriate for devices with a screen
// (shows an icon + a progress bar, text logging, menu, etc.)
class ScreenRecoveryUI : public RecoveryUI {
  public:
    ScreenRecoveryUI();

    void Init();
    void SetLocale(const char* locale);

    // overall recovery state ("background image")
    void SetBackground(Icon icon);

    // progress indicator
    void SetProgressType(ProgressType type);
    void ShowProgress(float portion, float seconds);
    void SetProgress(float fraction);

    void SetStage(int current, int max);

    // text log
    void ShowText(bool visible);
    bool IsTextVisible();
    bool WasTextEverVisible();

    // printing messages
    void Print(const char* fmt, ...) __printflike(2, 3);
    void PrintOnScreenOnly(const char* fmt, ...) __printflike(2, 3);
    void ShowFile(const char* filename);

    void DialogShowInfo(const char* text);
    void DialogShowError(const char* text);
    void DialogShowErrorLog(const char* text);
    int  DialogShowing() const { return (dialog_text != NULL); }
    bool DialogDismissable() const { return (dialog_icon == D_ERROR); }
    void DialogDismiss();
    void SetHeadlessMode();

    // menu display
    virtual int MenuItemStart() const { return menu_item_start_; }
    virtual int MenuItemHeight() const { return 3 * char_height_; }
    void StartMenu(const char* const * headers, const char* const * items,
                   int initial_selection);
    int SelectMenu(int sel, bool abs = false);
    void EndMenu();

    void KeyLongPress(int);

    void Redraw();

    enum UIElement {
        HEADER, MENU, MENU_SEL_BG, MENU_SEL_BG_ACTIVE, MENU_SEL_FG, LOG, TEXT_FILL, INFO, ERROR_TEXT
    };
    void SetColor(UIElement e);

  private:
    Icon currentIcon;
    int installingFrame;
    const char* locale;
    bool rtl_locale;

    pthread_mutex_t updateMutex;
    pthread_cond_t  progressCondition;

    GRSurface* headerIcon;
    GRSurface* backgroundIcon[NR_ICONS];
    GRSurface* backgroundText[NR_ICONS];
    GRSurface** installation;
    GRSurface* progressBarEmpty;
    GRSurface* progressBarFill;
    GRSurface* stageMarkerEmpty;
    GRSurface* stageMarkerFill;

    ProgressType progressBarType;

    float progressScopeStart, progressScopeSize, progress;
    double progressScopeTime, progressScopeDuration;

    // true when both graphics pages are the same (except for the progress bar).
    bool pagesIdentical;

    size_t log_text_cols_, log_text_rows_;
    size_t text_cols_, text_rows_;

    // Log text overlay, displayed when a magic key is pressed.
    char** text_;
    size_t text_col_, text_row_, text_top_;

    bool show_text;
    bool show_text_ever;   // has show_text ever been true?

    Icon dialog_icon;
    char *dialog_text;
    bool dialog_show_log;

    char** menu_;
    const char* const* menu_headers_;
    int header_items;
    bool show_menu;
    int menu_items, menu_sel;

    int menu_show_start_;
    int max_menu_rows_;

    // An alternate text screen, swapped with 'text_' when we're viewing a log file.
    char** file_viewer_text_;

    int menu_item_start_;

    pthread_t progress_thread_;

    int animation_fps;
    int installing_frames;

    int iconX, iconY;

    int stage, max_stage;

    bool rainbow;
    int wrap_count;

    int log_char_height_, log_char_width_;
    int char_height_, char_width_;

    int header_height_, header_width_;
    int text_first_row_;

    bool update_waiting;

    void draw_background_locked(Icon icon);
    void draw_progress_locked();
    int  draw_header_icon();
    void draw_menu_item(int textrow, const char *text, int selected);
    void draw_dialog();
    void draw_screen_locked();
    void update_screen_locked();

    static void* ProgressThreadStartRoutine(void* data);
    void ProgressThreadLoop();

    void ShowFile(FILE*);
    void PrintV(const char*, bool, va_list);
    void PutChar(char);
    void ClearText();

    void DrawHorizontalRule(int* y);
    void DrawTextLine(int* y, const char* line, bool bold);
    void DrawTextLines(int* y, const char* const* lines);

    void LoadBitmap(const char* filename, GRSurface** surface);
    void LoadBitmapArray(const char* filename, int* frames, GRSurface*** surface);
    void LoadLocalizedBitmap(const char* filename, GRSurface** surface);

    void OMGRainbows();
};

#endif  // RECOVERY_UI_H
