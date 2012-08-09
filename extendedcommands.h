extern int signature_check_enabled;
extern int script_assert_enabled;

void
toggle_signature_check();

void
show_choose_zip_menu();

int
do_nandroid_backup(const char* backup_name);

int
do_nandroid_restore();

void
show_nandroid_restore_menu(const char* path);

void
show_nandroid_advanced_restore_menu(const char* path);

void
show_nandroid_menu();

void
show_partition_menu();

void
show_choose_zip_menu();

int
install_zip(const char* packagefilepath);

int
__system(const char *command);

void
show_advanced_menu();

int format_unknown_device(const char *device, const char* path, const char *fs_type);

void
wipe_battery_stats();

void create_fstab();

int has_datadata();

void handle_failure(int ret);

void process_volumes();

int extendedcommand_file_exists();

void show_install_update_menu();

int confirm_selection(const char* title, const char* confirm);

int run_and_remove_extendedcommand();

int verify_root_and_recovery();

#ifdef RECOVERY_EXTEND_NANDROID_MENU
void extend_nandroid_menu(char** items, int item_count, int max_items);
void handle_nandroid_menu(int item_count, int selected);
#endif