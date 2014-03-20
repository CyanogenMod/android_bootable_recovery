/* Touch functions */

/* #define DEBUG_TOUCH_EVENTS */

/* Some extra input defines */
#ifndef ABS_MT_ANGLE
#define ABS_MT_ANGLE 0x38
#endif

/* Set colors if they aren't already defined */
#ifndef MENU_TEXT_COLOR
#define MENU_TEXT_COLOR 0, 191, 255, 255
#endif
#ifndef NORMAL_TEXT_COLOR
#define NORMAL_TEXT_COLOR 200, 200, 200, 255
#endif
#ifndef HEADER_TEXT_COLOR
#define HEADER_TEXT_COLOR NORMAL_TEXT_COLOR
#endif
#ifndef HILITE_TEXT_COLOR
#define HILITE_TEXT_COLOR 255, 255, 255, 255
#endif
#ifndef BACKGROUND_COLOR
#define BACKGROUND_COLOR 0, 0, 0, 255
#endif
#ifndef SEPARATOR_COLOR
#define SEPARATOR_COLOR 160, 160, 160, 255
#endif

struct point {
    int x;
    int y;
};

static int calibrated;
static struct point min_swipe;
static struct point touch_min;
static struct point touch_max;

static int in_touch;
static int touch_release_button;
static int in_swipe;
static struct point touch_start;
static struct point touch_end;
static int touch_last_y;

int touch_handle_key(int key, int visible) {
    if (visible) {
        switch (key) {
            case KEY_DOWN:
            case KEY_VOLUMEDOWN:
            case KEY_MENU:
                return HIGHLIGHT_DOWN;

            case KEY_UP:
            case KEY_VOLUMEUP:
            case KEY_HOME:
                return HIGHLIGHT_UP;

            case KEY_POWER:
                if (ui_get_showing_back_button()) {
                    return SELECT_ITEM;
                }
                if (!get_allow_toggle_display() && !ui_root_menu) {
                    return GO_BACK;
                }
                break;

            case KEY_BACK:
                if (!ui_root_menu) {
                    return GO_BACK;
                }
                break;

            case KEY_ENTER:
                return SELECT_ITEM;
        }
    }

    return NO_ACTION;
}

static void touch_draw_text_line(int row, const char *t)
{
    if (t[0] != '\0') {
        gr_text(0, (row+1)*CHAR_HEIGHT-1, t, 0);
    }
}

static void touch_draw_menu_item(int textrow, const char *text, int selected)
{
    if (selected) {
        gr_color(MENU_TEXT_COLOR);
        gr_fill(0, (textrow-1)*CHAR_HEIGHT,
                gr_fb_width(), (textrow+2)*CHAR_HEIGHT-1);
        gr_color(HILITE_TEXT_COLOR);
        gr_text(0, (textrow+1)*CHAR_HEIGHT-1, text, 0);
    }
    else {
        gr_color(BACKGROUND_COLOR);
        gr_fill(0, (textrow-1)*CHAR_HEIGHT,
                gr_fb_width(), (textrow+2)*CHAR_HEIGHT-1);
        gr_color(MENU_TEXT_COLOR);
        gr_text(0, (textrow+1)*CHAR_HEIGHT-1, text, 0);
    }

    gr_color(SEPARATOR_COLOR);
    gr_fill(0, (textrow+2)*CHAR_HEIGHT-1,
            gr_fb_width(), (textrow+2)*CHAR_HEIGHT+1);
}

int draw_touch_menu(char menu[MENU_MAX_ROWS][MENU_MAX_COLS], int menu_items, int menu_top, int menu_sel, int menu_show_start)
{
    int i = 0;
    int j = 0;
    int row = 0;

    gr_color(HEADER_TEXT_COLOR);
    for (i = 0; i < menu_top; ++i) {
        touch_draw_text_line(i*2, menu[i]);
        row++;
    }

    if (menu_items - menu_show_start + menu_top >= max_menu_rows)
        j = max_menu_rows - menu_top;
    else
        j = menu_items - menu_show_start;

    for (i = menu_show_start; i < (menu_show_start + j); ++i) {
            int textrow = menu_top + 3*(i - menu_show_start) + 1;
            touch_draw_menu_item(textrow, menu[menu_top + i], (i == menu_sel));
        row++;
        if (row >= max_menu_rows)
            break;
    }

    return row;
}

void touch_init()
{
    in_touch = 0;
    in_swipe = 0;
    touch_start.x = touch_end.x = -1;
    touch_start.y = touch_end.y = -1;
    touch_last_y = -1;
}

int get_max_menu_rows(int max_menu_rows)
{
    int textrows = gr_fb_height() / CHAR_HEIGHT;
    return (textrows - menu_top) / 3;
}

static void touch_calibrate(int fd)
{
    if (!calibrated) {
        char value [PROPERTY_VALUE_MAX];
        struct input_absinfo xinfo, yinfo;
        int xrc, yrc;

        calibrated = 1;

        memset(&xinfo, 0, sizeof(xinfo));
        memset(&yinfo, 0, sizeof(yinfo));

        property_get("ro.sf.lcd_density", value, "0");
        int screen_density = atoi(value);
        if (screen_density == 0) {
            screen_density = 160;
        }
        min_swipe.x = (screen_density * 50) / 100;
        min_swipe.y = (screen_density * 30) / 100;
        LOGI("calibrate: min_swipe=(%d,%d)\n", min_swipe.x, min_swipe.y);
        LOGI("calibrate: CHAR=(%d,%d)\n", (int)CHAR_WIDTH, (int)CHAR_HEIGHT);

        xrc = ioctl(fd, EVIOCGABS(ABS_MT_POSITION_X), &xinfo);
        yrc = ioctl(fd, EVIOCGABS(ABS_MT_POSITION_Y), &yinfo);
        if (xrc == 0 && yrc == 0) {
            touch_min.x = xinfo.minimum;
            touch_max.x = xinfo.maximum;
            touch_min.y = yinfo.minimum;
            touch_max.y = yinfo.maximum;
        }
        else {
            LOGI("touch_calibrate: EVIOCGABS failed: scaling disabled\n");
            touch_min.x = 0;
            touch_max.x = gr_fb_width();
            touch_min.y = 0;
            touch_max.y = gr_fb_height();
        }
        LOGI("calibrate: x=(%d-%d), y=(%d-%d)\n",
                touch_min.x, touch_max.x, touch_min.y, touch_max.y);
    }
}

static int touch_x_scale(int val)
{
    return val * gr_fb_width() / (touch_max.x - touch_min.x);
}

static int touch_y_scale(int val)
{
    return val * gr_fb_height() / (touch_max.y - touch_min.y);
}

static void show_event(struct input_event *ev)
{
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
    case EV_REL:
        evtypestr = "EV_REL";
        switch (ev->code) {
        case REL_X:
            evcodestr = "REL_X";
            break;
        case REL_Y:
            evcodestr = "REL_Y";
            break;
        case REL_Z:
            evcodestr = "REL_Z";
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

static int touch_handle_release(struct input_event *ev)
{
    int rc = 1;
    int dx = touch_end.x - touch_start.x;
    int dy = touch_end.y - touch_start.y;

    if (in_swipe) {
        if (abs(dx) > abs(dy)) {
            if (abs(dx) >= min_swipe.x) {
                ev->type = EV_KEY;
                ev->code = (dx > 0 ? KEY_ENTER : KEY_BACK);
                ev->value = 2;
                rc = 0;
            }
        }
        else {
            /* Vertical swipe, handled realtime */
        }
    }
    else {
        int sel;
        sel = (touch_end.y - (menu_top*CHAR_HEIGHT))/(CHAR_HEIGHT*3) + menu_show_start;
        if (sel >= 0 && sel < menu_items) {
            int textrow = menu_top + 3*sel + 1;
            touch_draw_menu_item(textrow, menu[menu_top + sel], 1);
            gr_flip();
            usleep(1000*50);
            menu_sel = sel;
            ev->type = EV_KEY;
            ev->code = KEY_ENTER;
            ev->value = 2;
            rc = 0;
        }
    }
    in_touch = 0;
    touch_release_button = 0;
    in_swipe = 0;
    touch_start.x = touch_end.x = -1;
    touch_start.y = touch_end.y = -1;
    touch_last_y = -1;
    return rc;
}

int touch_handle_gestures(struct input_event *ev)
{
    int rc = 1;
    int dx = touch_end.x - touch_start.x;
    int dy = touch_end.y - touch_start.y;

    if (touch_end.x == -1 || touch_end.y == -1) {
        return 1;
    }
    if (abs(dx) > abs(dy)) {
        if (abs(dx) >= min_swipe.x) {
            /* Horizontal swipe, handle it on release */
            in_swipe = 1;
        }
    }
    else {
        if (touch_last_y == -1) {
            touch_last_y = touch_end.y;
        }
        dy = touch_end.y - touch_last_y;
        if (abs(dy) > 3*CHAR_HEIGHT) {
            in_swipe = 1;
            touch_last_y = touch_end.y;
            ev->type = EV_KEY;
            ev->code = (dy < 0) ? KEY_VOLUMEUP : KEY_VOLUMEDOWN;
            ev->value = 2;
            rc = 0;
        }
    }
    return rc;
}

static int touch_event_is_release(struct input_event *ev)
{
    if (!in_touch) {
        return 0;
    }

    if (touch_release_button) {
        if (ev->type == EV_KEY && ev->code == BTN_TOUCH && ev->value == 0) {
            return 1;
        }
        return 0;
    }

    if (ev->type == EV_SYN && ev->code == SYN_MT_REPORT) {
        return 1;
    }
    if (ev->type == EV_ABS && ev->code == ABS_MT_TRACKING_ID && ev->value == -1) {
        return 1;
    }

    return 0;
}

int touch_handle_input(int fd, struct input_event *ev)
{
    int rc = 1;
    int dx, dy;

    show_event(ev);

    touch_calibrate(fd);

    if (touch_event_is_release(ev)) {
        rc = touch_handle_release(ev);
    }
    else if (ev->type == EV_KEY && ev->code == BTN_TOUCH && ev->value != 0) {
        touch_release_button = 1;
    }
    else if (ev->type == EV_ABS) {
        switch (ev->code) {
        case ABS_MT_POSITION_X:
            in_touch = 1;
            touch_end.x = touch_x_scale(ev->value);
            if (touch_start.x == -1) {
                touch_start.x = touch_end.x;
            }
            rc = touch_handle_gestures(ev);
            break;
        case ABS_MT_POSITION_Y:
            in_touch = 1;
            touch_end.y = touch_y_scale(ev->value);
            if (touch_start.y == -1) {
                touch_start.y = touch_end.y;
            }
            rc = touch_handle_gestures(ev);
            break;
        }
    }

    return rc;
}
