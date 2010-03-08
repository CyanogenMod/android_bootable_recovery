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

int
do_nandroid_backup(char* backup_name);

int
do_nandroid_restore();

void
show_nandroid_restore_menu();

void
do_mount_usb_storage();

void
show_choose_zip_menu();

int
install_zip(const char* packagefilepath);

