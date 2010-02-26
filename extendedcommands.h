extern int signature_check_enabled;
extern int script_assert_enabled;

void
toggle_signature_check();

void
toggle_script_asserts();

void
show_choose_zip_menu();

int
get_allow_toggle_display();

void
ui_set_show_text(int value);

void
do_nandroid_backup();

void
show_nandroid_restore_menu();

void
do_mount_usb_storage();