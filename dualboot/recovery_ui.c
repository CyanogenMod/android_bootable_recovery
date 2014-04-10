/*
 * Copyright (C) 2009 The Android Open Source Project
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

#include <linux/input.h>
#include <sys/limits.h>
#include <sys/stat.h>
#include <sys/mount.h>
#include <unistd.h>

#include "dualboot.h"
#include "recovery_ui.h"
#include "common.h"
#include "extendedcommands.h"
#include "mounts.h"
#include "roots.h"

char* MENU_HEADERS[] = { NULL };

char* MENU_ITEMS[] = { "Managed System",
                       "reboot system now",
                       "install zip",
                       "wipe data/factory reset",
                       "wipe cache partition",
                       "backup and restore",
                       "mounts and storage",
                       "advanced",
#ifdef PHILZ_TOUCH_RECOVERY
                       "PhilZ Settings",
                       "Power Options",
#endif
                       NULL };

static void update_menu_items(void) {
	int sys = get_selected_system();

	if(sys==SYSTEM1)
		MENU_ITEMS[0] = "Managed system [System1]";
	else if(sys==SYSTEM2)
		MENU_ITEMS[0] = "Managed system [System2]";
	else
		MENU_ITEMS[0] = "Managed system [Invalid]";
}

static void patch_mount_script(void) {
	ensure_path_mounted("/system");

	if(dualboot_is_tdb_enabled())
		__system("cp /res/dualboot/mount_ext4_tdb.sh /system/bin/mount_ext4.sh");
	else
		__system("cp /res/dualboot/mount_ext4_default.sh /system/bin/mount_ext4.sh");

	chmod("/system/bin/mount_ext4.sh", 0755);
}

void device_ui_init(UIParameters* ui_parameters) {
}

int device_recovery_start() {
    dualboot_init();
    update_menu_items();
    return 0;
}

int device_reboot_now(volatile char* key_pressed, int key_code) {
    return 0;
}

int device_perform_action(int which) {
	if(which==0) {
		dualboot_show_selection_ui();
		update_menu_items();
		dualboot_setup_env();
	}

    return which-1;
}

int device_wipe_data() {
    return 0;
}

int device_verify_root_and_recovery(void) {
	dualboot_set_system(SYSTEM1);
	verify_root_and_recovery();
	patch_mount_script();

	dualboot_set_system(SYSTEM2);
	verify_root_and_recovery();
	patch_mount_script();

	return 0;
}

int device_build_selection_title(char* buf, const char* title) {
	enum dualboot_system sys = get_selected_system();
	char* prefix = "?";

	if(sys==SYSTEM1)
		prefix = "System1";
	else if(sys==SYSTEM2)
		prefix = "System2";

	sprintf(buf, "[%s] %s", prefix, title);
	return 0;
}

void device_toggle_truedualboot(void) {
	char confirm[PATH_MAX];
	int enable = dualboot_is_tdb_enabled();

#ifndef PHILZ_TOUCH_RECOVERY
	ui_setMenuTextColor(MENU_TEXT_COLOR_RED);
#endif

	sprintf(confirm, "Yes - %s TrueDualBoot", enable?"DISABLE":"ENABLE");

	if (confirm_selection("This will WIPE DATA. Confirm?", confirm)) {
		// unmount /data
		if(ensure_path_unmounted("/data")!=0) {
			LOGE("Error unmounting /data!\n");
			return;
		}

		// format /data
		ui_set_background(BACKGROUND_ICON_INSTALLING);
		ui_show_indeterminate_progress();
		ui_print("Formatting /data...\n");
		set_force_raw_format_enabled(1);
		if(format_volume("/data")!=0) {
			ui_print("Error formatting /data!\n");
			ui_reset_progress();
			return;
		}
		ui_reset_progress();
		set_force_raw_format_enabled(0);
		ui_print("Done.\n");

		// toggle
		dualboot_set_tdb_enabled(!enable);
	}

#ifndef PHILZ_TOUCH_RECOVERY
	ui_setMenuTextColor(MENU_TEXT_COLOR);
#endif

	return;
}

int device_get_truedualboot_entry(char* tdb_name) {
	sprintf(tdb_name, "toggle TrueDualBoot [%s]", dualboot_is_tdb_enabled()?"enabled":"disabled");
	return 0;
}

int device_truedualboot_mount(const char* path, const char* mount_point) {
	if(strcmp(path, "/data") != 0)
		return 1;
	else if(mount_point!=NULL && strcmp(mount_point, MOUNTPOINT_DATAROOT)==0)
		return 1;

	if(!dualboot_is_tdb_enabled())
		return 1;

	ensure_path_mounted_at_mount_point("/data", MOUNTPOINT_DATAROOT);

	Volume* v = volume_for_path(path);
	if (v == NULL) {
		LOGE("unknown volume for path [%s]\n", path);
		return -1;
	}

	int result = scan_mounted_volumes();
	if (result < 0) {
		LOGE("failed to scan mounted volumes\n");
		return -1;
	}

	if (NULL == mount_point)
		mount_point = v->mount_point;

	const MountedVolume* mv =
		find_mounted_volume_by_mount_point(mount_point);
	if (mv) {
		// volume is already mounted
		return 0;
	}

	char* bind_path;
	if(get_selected_system()==SYSTEM1)
		bind_path = MOUNTPOINT_DATAROOT "/system0";
	else if(get_selected_system()==SYSTEM2)
		bind_path = MOUNTPOINT_DATAROOT "/system1";
	else return -1;

	mkdir(mount_point, 0755);  // in case it doesn't already exist
	mkdir(bind_path, 0755);

	char mount_cmd[PATH_MAX];
	sprintf(mount_cmd, "mount -o bind %s %s", bind_path, mount_point);
	int ret = __system(mount_cmd);
	if(ret!=0) return ret>0?-ret:ret;

	return 0;

}

int device_truedualboot_unmount(const char* path) {
	if(strcmp(path, "/data") != 0)
		return 1;

	umount(MOUNTPOINT_DATAROOT);
	return 1;
}

int device_truedualboot_format_volume(const char* volume) {
	if(strcmp(volume, "/data") != 0)
		return 1;

	if(is_force_raw_format_enabled() || !dualboot_is_tdb_enabled())
		return 1;

	int rc = format_unknown_device(NULL, volume, NULL);
	return rc>0?-rc:rc;
}

int device_truedualboot_format_device(const char *device, const char *path, const char *fs_type) {
	return device_truedualboot_format_volume(path);
}

int device_truedualboot_before_update(const char *path, ZipArchive *zip) {
	// HACK: delete node to userdata partition so updater_script
	// will not be able to mount it(the wrong way).
	// we'll mount it now so files won't land on ramdisk

	if(dualboot_is_tdb_enabled()) {
		unlink(PATH_USERDATA_NODE);
		ensure_path_mounted("/data");
	}
	else {
		struct stat statbuf;

		if(stat(PATH_USERDATA_NODE_BACKUP, &statbuf)) {
			LOGE("could not stat userdata!\n");
		}

		unlink(PATH_USERDATA_NODE);
		if(mknod(PATH_USERDATA_NODE, statbuf.st_mode, statbuf.st_rdev)!=0) {
			LOGE("could not create node!\n");
			return -1;
		}

		ensure_path_unmounted("/data");
	}

	return 0;
}

void device_truedualboot_after_load_volume_table() {
	ssize_t len;
	char path[PATH_MAX];

	Volume* v = volume_for_path("/data");
	if(v->blk_device!=NULL) {
		// resolve symlink
		if((len = readlink(v->blk_device, path, sizeof(path)-1)) != -1)
			path[len] = '\0';

		rename(path, PATH_USERDATA_NODE_BACKUP);
		v->blk_device = PATH_USERDATA_NODE_BACKUP;
	}
}

static int set_bootmode(char* bootmode) {
	// open misc-partition
	FILE* misc = fopen("/dev/block/platform/msm_sdcc.1/by-name/misc", "wb");
	if (misc == NULL) {
		printf("Error opening misc partition.\n");
		return -1;
	}

	// write bootmode
	fseek(misc, 0x1000, SEEK_SET);
	if(fputs(bootmode, misc)<0) {
		printf("Error writing bootmode to misc partition.\n");
		return -1;
	}

	// close
	fclose(misc);
	return 0;
}

static int get_bootmode(char* bootmode) {
	// open misc-partition
	FILE* misc = fopen("/dev/block/platform/msm_sdcc.1/by-name/misc", "rb");
	if (misc == NULL) {
		printf("Error opening misc partition.\n");
		return -1;
	}

	// read bootmode
	fseek(misc, 0x1000, SEEK_SET);
	if(fgets(bootmode, 13, misc)==NULL) {
		printf("Error reading bootmode from misc partition.\n");
		return -1;
	}

	// close
	fclose(misc);
	return 0;
}

void device_choose_bootmode(void) {
	int sys = dualboot_select_system("Set bootmode:");
	if(sys==GO_BACK) return;

	if(sys==SYSTEM1)
		set_bootmode("boot-system0");
	else if(sys==SYSTEM2)
		set_bootmode("boot-system1");
}

int device_get_bootmode(char* bootmode_name) {
	char* sysnum = 0;

	char bootmode[13];
	get_bootmode(&bootmode);
	if(strcmp(bootmode, "boot-system1")==0)
		sysnum = 1;

	sprintf(bootmode_name, "active system: %d", sysnum+1);
	return 0;
}
