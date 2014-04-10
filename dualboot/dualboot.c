#include <sys/stat.h>
#include <sys/limits.h>
#include <unistd.h>

#include "dualboot.h"
#include "common.h"
#include "recovery_ui.h"
#include "mounts.h"
#include "roots.h"

extern int ui_menu_level;
extern int ui_root_menu;
static enum dualboot_system selected_system = INVALID_SYSTEM;
static int force_raw_format = 0;

struct partition {
	char path[PATH_MAX];
	struct stat statbuf;
};
static struct partition part_table[6];
#define PART_SYSTEM	0
#define PART_SYSTEM1	1
#define PART_BOOT	2
#define PART_BOOT1	3
#define PART_MODEM	4
#define PART_MODEM1	5

enum dualboot_system get_selected_system(void) {
	return selected_system;
}

int dualboot_select_system(const char* title) {
	const char* headers[] = { title, "", NULL };
	char* items[] = { "System1", "System2", NULL };
	enum dualboot_system chosen_item = -1;

	do {
		chosen_item = get_menu_selection(headers, items, 0, 0);
		if(chosen_item==GO_BACK)
			return chosen_item;
	}
	while(chosen_item!=SYSTEM1 && chosen_item!=SYSTEM2);
		
	return chosen_item;
}

static void dualboot_init_part(int num, const char* part) {
	char path_source[PATH_MAX];
	char path_target[PATH_MAX];
	ssize_t len;

	// build paths
	sprintf(path_source, "/dev/block/platform/msm_sdcc.1/by-name/%s", part);
	sprintf(path_target, "/dev/part_backup_%s", part);

	// resolve symlink
	if((len = readlink(path_source, part_table[num].path, sizeof(part_table[num].path)-1)) != -1)
		part_table[num].path[len] = '\0';


	// check if moved node already exist
	if(stat(path_target, &part_table[num].statbuf)==0) {
		return;
	}

	// check for original otherwise
	else if(stat(part_table[num].path, &part_table[num].statbuf)==0) {
		// move
		if(rename(part_table[num].path, path_target)!=0) {
			LOGE("could not move %s to %s!\n", part_table[num].path, path_target);
			return;
		}
	}
}

static void dualboot_prepare_env(void) {
	dualboot_init_part(PART_SYSTEM, "system");
	dualboot_init_part(PART_SYSTEM1, "system1");
	dualboot_init_part(PART_BOOT, "boot");
	dualboot_init_part(PART_BOOT1, "boot1");
	dualboot_init_part(PART_MODEM, "modem");
	dualboot_init_part(PART_MODEM1, "modem1");
}

static int replace_device_node(int num, struct stat* statbuf) {
	scan_mounted_volumes();
	MountedVolume * mv = find_mounted_volume_by_real_node(part_table[num].path);

	if(mv!=NULL && unmount_mounted_volume(mv)!=0) {
		LOGE("could not unmount device!\n");
		return -1;
	} 

	unlink(part_table[num].path);
	if(mknod(part_table[num].path, statbuf->st_mode, statbuf->st_rdev)!=0) {
		LOGE("could not create node!\n");
		return -1;
	}

	return 0;
}

void dualboot_setup_env(void) {
	if(ensure_path_unmounted("/data")!=0) {
		LOGE("Error unmounting /data!\n");
		return;
	}

	if(selected_system==SYSTEM1) {
		replace_device_node(PART_SYSTEM, &part_table[PART_SYSTEM].statbuf);
		replace_device_node(PART_SYSTEM1, &part_table[PART_SYSTEM].statbuf);
		replace_device_node(PART_BOOT, &part_table[PART_BOOT].statbuf);
		replace_device_node(PART_BOOT1, &part_table[PART_BOOT].statbuf);
		replace_device_node(PART_MODEM, &part_table[PART_MODEM].statbuf);
		replace_device_node(PART_MODEM1, &part_table[PART_MODEM].statbuf);
	}
	else if(selected_system==SYSTEM2) {
		replace_device_node(PART_SYSTEM, &part_table[PART_SYSTEM1].statbuf);
		replace_device_node(PART_SYSTEM1, &part_table[PART_SYSTEM1].statbuf);
		replace_device_node(PART_BOOT, &part_table[PART_BOOT1].statbuf);
		replace_device_node(PART_BOOT1, &part_table[PART_BOOT1].statbuf);
		replace_device_node(PART_MODEM, &part_table[PART_MODEM1].statbuf);
		replace_device_node(PART_MODEM1, &part_table[PART_MODEM1].statbuf);
	}
	else {
		LOGE("Invalid system '%d'\n", selected_system);
	}
}

void dualboot_show_selection_ui(void) {
	int sys = dualboot_select_system("Select System to manage:");
	if(sys!=GO_BACK) selected_system = sys;
}

void dualboot_init(void) {
	// backup old values
	int backup_root = ui_root_menu;
	int backup_level = ui_menu_level;

	// show ui
	ui_set_show_text(1);
	ui_set_background(BACKGROUND_ICON_CLOCKWORK);
	ui_root_menu = 1;
        ui_menu_level = 0;

	// get selection
	dualboot_show_selection_ui();
	dualboot_prepare_env();
	dualboot_setup_env();

	// hide ui
        ui_menu_level = backup_level;
        ui_root_menu = backup_root;
	ui_set_background(BACKGROUND_ICON_NONE);
	ui_set_show_text(1);
}

void dualboot_set_system(enum dualboot_system sys) {
	selected_system = sys;
	dualboot_setup_env();
}

static int dualboot_mount_dataroot(void) {
	int rc = ensure_path_mounted_at_mount_point("/data", MOUNTPOINT_DATAROOT);
	if(rc)
		LOGE("failed mounting dataroot!\n");
	return rc;
}

int dualboot_is_tdb_enabled(void) {
	struct stat st;

	if(dualboot_mount_dataroot())
		return 0;

	return lstat(TDB_FILE, &st)==0;
}

void dualboot_set_tdb_enabled(int enable) {
	if(dualboot_mount_dataroot())
		return;

		if(enable) {
			FILE * pFile = fopen(TDB_FILE, "w");
			if(pFile==NULL) {
				LOGE("TrueDualBoot: failed creating file");
			return;
		}
		fclose(pFile);
	}
	else remove(TDB_FILE);
}

int is_force_raw_format_enabled(void) {
	return !!force_raw_format;
}

void set_force_raw_format_enabled(int enabled) {
	force_raw_format = enabled;
}
