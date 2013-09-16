extern int signature_check_enabled;
extern int script_assert_enabled;

void
write_recovery_version();

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

int
show_nandroid_menu();

int
show_partition_menu();

int
install_zip(const char* packagefilepath);

int
__system(const char *command);

int
show_advanced_menu();

int format_device(const char *device, const char *path, const char *fs_type);

int format_unknown_device(const char *device, const char* path, const char *fs_type);

void create_fstab();

int has_datadata();

void handle_failure(int ret);

void process_volumes();

int extendedcommand_file_exists();

int show_install_update_menu();

int confirm_selection(const char* title, const char* confirm);

int run_and_remove_extendedcommand();

int verify_root_and_recovery();

void free_string_array(char** array);

int can_partition(const char* volume);

static int is_path_mounted(const char* path);

int volume_main(int argc, char **argv);

#ifdef RECOVERY_EXTEND_NANDROID_MENU
void extend_nandroid_menu(char** items, int item_count, int max_items);
void handle_nandroid_menu(int item_count, int selected);
#endif
