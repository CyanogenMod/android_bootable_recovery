#define MOUNTPOINT_DATAROOT "/data_root"
#define TDB_FILE MOUNTPOINT_DATAROOT "/.truedualboot"
#define PATH_USERDATA_NODE "/dev/block/mmcblk0p26"
#define PATH_USERDATA_NODE_BACKUP "/dev/part_backup_userdata"

enum dualboot_system {
	INVALID_SYSTEM = -1,
	SYSTEM1,
	SYSTEM2
};

int dualboot_select_system(const char* title);
void dualboot_init(void);
void dualboot_show_selection_ui(void);
enum dualboot_system get_selected_system(void);
void dualboot_setup_env(void);
void dualboot_set_system(enum dualboot_system sys);

void dualboot_set_tdb_enabled(int enable);
int dualboot_is_tdb_enabled(void);
int is_force_raw_format_enabled(void);
void set_force_raw_format_enabled(int enabled);
