#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <limits.h>
#include <linux/input.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/reboot.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include <sys/wait.h>
#include <sys/limits.h>
#include <dirent.h>
#include <sys/stat.h>

#include <signal.h>
#include <sys/wait.h>

#include "bootloader.h"
#include "common.h"
#include "cutils/properties.h"
#include "firmware.h"
#include "install.h"
#include "minui/minui.h"
#include "minzip/DirUtil.h"
#include "roots.h"
#include "recovery_ui.h"

#include "commands.h"
#include "amend/amend.h"

#include "mtdutils/dump_image.h"
#include "../../external/yaffs2/yaffs2/utils/mkyaffs2image.h"
#include "../../external/yaffs2/yaffs2/utils/unyaffs.h"

#include <sys/vfs.h>

#include "extendedcommands.h"
#include "nandroid.h"

int print_and_error(char* message) {
    ui_print(message);
    return 1;
}

int yaffs_files_total = 0;
int yaffs_files_count = 0;
void yaffs_callback(char* filename)
{
    char* justfile = basename(filename);
    if (strlen(justfile) < 30)
        ui_print(justfile);
    yaffs_files_count++;
    if (yaffs_files_total != 0)
        ui_set_progress((float)yaffs_files_count / (float)yaffs_files_total);
    ui_reset_text_col();
}

void compute_directory_stats(char* directory)
{
    char tmp[PATH_MAX];
    sprintf(tmp, "find %s | wc -l > /tmp/dircount", directory);
    __system(tmp);
    char count_text[100];
    FILE* f = fopen("/tmp/dircount", "r");
    fread(count_text, 1, sizeof(count_text), f);
    fclose(f);
    yaffs_files_count = 0;
    yaffs_files_total = atoi(count_text);
    ui_reset_progress();
    ui_show_progress(1, 0);
}

int nandroid_backup_partition(char* backup_path, char* root, char* name) {
    int ret = 0;
    char mount_point[PATH_MAX];
    sprintf(mount_point, "/%s", name);
    
    ui_print("Backing up %s...\n", name);
    if (0 != (ret = ensure_root_path_mounted(root) != 0)) {
        ui_print("Can't mount %s!\n", mount_point);
        return ret;
    }
    compute_directory_stats(mount_point);
    char tmp[PATH_MAX];
    sprintf(tmp, "%s/%s.img", backup_path, name);
    ret = mkyaffs2image(mount_point, tmp, 0, yaffs_callback);
    ensure_root_path_unmounted(root);
    if (0 != ret) {
        ui_print("Error while making a yaffs2 image of %s!\n", mount_point);
        return ret;
    }
    return 0;
}

int nandroid_backup(char* backup_path)
{
    ui_set_background(BACKGROUND_ICON_INSTALLING);
    
    if (ensure_root_path_mounted("SDCARD:") != 0)
        return print_and_error("Can't mount /sdcard\n");
    
    int ret;
	struct statfs s;
    if (0 != (ret = statfs("/sdcard", &s)))
        return print_and_error("Unable to stat /sdcard\n");
    long sdcard_free = s.f_bfree * s.f_bsize;
    if (sdcard_free < 150000000L)
        return print_and_error("There is not enough free space on the SD Card! At least 150MB is required to create a backup.\n");
    
    char tmp[PATH_MAX];
    sprintf(tmp, "mkdir -p %s", backup_path);
    __system(tmp);

    ui_print("Backing up boot...\n");
    sprintf(tmp, "%s/%s", backup_path, "boot.img");
    ret = dump_image("boot", tmp, NULL);
    if (0 != ret)
        return print_and_error("Error while dumping boot image!\n");
    
    if (0 != (ret = nandroid_backup_partition(backup_path, "SYSTEM:", "system")))
        return ret;

    if (0 != (ret = nandroid_backup_partition(backup_path, "DATA:", "data")))
        return ret;

    if (0 != (ret = nandroid_backup_partition(backup_path, "CACHE:", "cache")))
        return ret;
    
    ui_print("Generating md5 sum...\n");
    sprintf(tmp, "cd %s && (md5sum *img > nandroid.md5)", backup_path);
    if (0 != (ret = __system(tmp))) {
        ui_print("Error while generating md5 sum!\n");
        return ret;
    }
    
    sync();
    ui_set_background(BACKGROUND_ICON_NONE);
    ui_reset_progress();
    ui_print("\nBackup complete!\n");
    return 0;
}

int nandroid_restore_partition(char* backup_path, char* root, char* name) {
    int ret = 0;
    char mount_point[PATH_MAX];
    sprintf(mount_point, "/%s", name);
    
    ui_print("Restoring %s...\n", name);
    if (0 != (ret = ensure_root_path_unmounted(root))) {
        ui_print("Can't unmount %s!\n", mount_point);
        return ret;
    }
    if (0 != (ret = format_root_device(root))) {
        ui_print("Error while formatting %s!\n", root);
        return ret;
    }
    
    if (0 != (ret = ensure_root_path_mounted(root))) {
        ui_print("Can't mount %s!\n", mount_point);
        return ret;
    }
    
    char tmp[PATH_MAX];
    sprintf(tmp, "%s/%s.img", backup_path, name);
    if (0 != (ret = unyaffs(tmp, mount_point, yaffs_callback))) {
        ui_print("Error while restoring %s!\n", mount_point);
        return ret;
    }
    return 0;
}

int nandroid_restore(char* backup_path, int restore_boot, int restore_system, int restore_data, int restore_cache)
{
    ui_set_background(BACKGROUND_ICON_INSTALLING);
    ui_show_indeterminate_progress();
    yaffs_files_total = 0;

    if (ensure_root_path_mounted("SDCARD:") != 0)
        return print_and_error("Can't mount /sdcard\n");
    
    char tmp[PATH_MAX];

    ui_print("Checking MD5 sums...\n");
    sprintf(tmp, "cd %s && md5sum -c nandroid.md5", backup_path);
    if (0 != __system(tmp))
        return print_and_error("MD5 mismatch!\n");
    
    int ret;
    if (restore_boot)
    {
        sprintf(tmp, "flash_image boot %s/boot.img", backup_path);
        ui_print("Restoring boot image...\n");
        if (0 != (ret = __system(tmp))) {
            ui_print("Error while flashing boot image!");
            return ret;
        }
    }
    
    if (restore_system && 0 != (ret = nandroid_restore_partition(backup_path, "SYSTEM:", "system")))
        return ret;

    if (restore_data && 0 != (ret = nandroid_restore_partition(backup_path, "DATA:", "data")))
        return ret;

    if (restore_cache && 0 != (ret = nandroid_restore_partition(backup_path, "CACHE:", "cache")))
        return ret;

    sync();
    ui_set_background(BACKGROUND_ICON_NONE);
    ui_reset_progress();
    ui_print("\nRestore complete!\n");
    return 0;
}
