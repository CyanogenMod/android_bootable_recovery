static int in_touch = 0; // 1 = in a touch
static int slide_right = 0;
static int slide_left = 0;
static int touch_x = 0;
static int touch_y = 0;
static int old_x = 0;
static int old_y = 0;
static int diff_x = 0;
static int diff_y = 0;

static void reset_gestures() {
    diff_x = 0;
    diff_y = 0;
    old_x = 0;
    old_y = 0;
    touch_x = 0;
    touch_y = 0;
}

void swipe_handle_input(int fd, struct input_event *ev) {
    int abs_store[6] = {0};
    int k;

    ioctl(fd, EVIOCGABS(ABS_MT_POSITION_X), abs_store);
    int max_x_touch = abs_store[2];

    ioctl(fd, EVIOCGABS(ABS_MT_POSITION_Y), abs_store);
    int max_y_touch = abs_store[2];

    if(ev->type == EV_ABS && ev->code == ABS_MT_TRACKING_ID) {
        if(in_touch == 0) {
            in_touch = 1;
            reset_gestures();
        } else { // finger lifted
            ev->type = EV_KEY;
            int keywidth = gr_fb_width() / 4;
            if(slide_right == 1) {
                ev->code = KEY_POWER;
                slide_right = 0;
            } else if(slide_left == 1) {
                ev->code = KEY_BACK;
                slide_left = 0;
            }

            ev->value = 1;
            in_touch = 0;
            reset_gestures();
        }
    } else if(ev->type == EV_ABS && ev->code == ABS_MT_POSITION_X) {
        old_x = touch_x;
        float touch_x_rel = (float)ev->value / (float)max_x_touch;
        touch_x = touch_x_rel * gr_fb_width();

        if(old_x != 0) diff_x += touch_x - old_x;

        if(diff_x > 100) {
            slide_right = 1;
            reset_gestures();
        } else if(diff_x < -100) {
            slide_left = 1;
            reset_gestures();
        }
    } else if(ev->type == EV_ABS && ev->code == ABS_MT_POSITION_Y) {
        old_y = touch_y;
        float touch_y_rel = (float)ev->value / (float)max_y_touch;
        touch_y = touch_y_rel * gr_fb_height();

        if(old_y != 0) diff_y += touch_y - old_y;

        if(diff_y > 80) {
            ev->code = KEY_VOLUMEDOWN;
            ev->type = EV_KEY;
            reset_gestures();
        } else if(diff_y < -80) {
            ev->code = KEY_VOLUMEUP;
            ev->type = EV_KEY;
            reset_gestures();
        }
    }

    return;
}
