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

#include "extendedcommands.h"
#include "nandroid.h"

int yaffs_files_total = 0;
int yaffs_files_count = 0;
void yaffs_callback(char* filename)
{
    char* justfile = basename(filename);
    if (strlen(justfile) < 30)
        ui_print(basename(filename));
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

int nandroid_backup(char* backup_path)
{
    ui_set_background(BACKGROUND_ICON_INSTALLING);
    
    if (ensure_root_path_mounted("SDCARD:") != 0)
        return print_and_error("Can't mount /sdcard\n");
    
    char tmp[PATH_MAX];
    sprintf(tmp, "mkdir -p %s", backup_path);
    __system(tmp);

    int ret;
    ui_print("Backing up boot...\n");
    sprintf(tmp, "%s/%s", backup_path, "boot.img");
    ret = dump_image("boot", tmp, NULL);
    if (0 != ret)
        return print_and_error("Error while dumping boot image!\n");
    
    // TODO: Wrap this up in a loop?
    ui_print("Backing up system...\n");
    sprintf(tmp, "%s/%s", backup_path, "system.img");
    if (ensure_root_path_mounted("SYSTEM:") != 0)
        return print_and_error("Can't mount /system!\n");
    compute_directory_stats("/system");
    ret = mkyaffs2image("/system", tmp, 0, yaffs_callback);
    ensure_root_path_unmounted("SYSTEM:");
    if (0 != ret)
        return print_and_error("Error while making a yaffs2 image of system!\n");
    
    ui_print("Backing up data...\n");
    sprintf(tmp, "%s/%s", backup_path, "data.img");
    if (ensure_root_path_mounted("DATA:") != 0)
        return print_and_error("Can't mount /data!\n");
    compute_directory_stats("/data");
    ret = mkyaffs2image("/data", tmp, 0, yaffs_callback);
    ensure_root_path_unmounted("DATA:");
    if (0 != ret)
        return print_and_error("Error while making a yaffs2 image of data!\n");
    
    ui_print("Backing up cache...\n");
    sprintf(tmp, "%s/%s", backup_path, "cache.img");
    if (ensure_root_path_mounted("CACHE:") != 0)
        return print_and_error("Can't mount /cache!\n");
    compute_directory_stats("/cache");
    ret = mkyaffs2image("/cache", tmp, 0, yaffs_callback);
    ensure_root_path_unmounted("CACHE:");
    if (0 != ret)
        return print_and_error("Error while making a yaffs2 image of cache!\n");
    
    sprintf(tmp, "md5sum %s/*img > %s/nandroid.md5", backup_path, backup_path);
    __system(tmp);
    
    sync();
    ui_set_background(BACKGROUND_ICON_NONE);
    ui_reset_progress();
    ui_print("Backup complete!\n");
    return 0;
}

int nandroid_restore(char* backup_path)
{
    ui_set_background(BACKGROUND_ICON_INSTALLING);
    ui_show_indeterminate_progress();
    yaffs_files_total = 0;

    if (ensure_root_path_mounted("SDCARD:") != 0)
        return print_and_error("Can't mount /sdcard\n");
    
    char tmp[PATH_MAX];

    ui_print("Checking MD5 sums...\n");
    sprintf(tmp, "md5sum -c %s/nandroid.md5", backup_path);
    if (0 != __system(tmp))
        return print_and_error("MD5 mismatch!\n");
    
    // TODO: put this in a loop?
    ui_print("Restoring system...\n");
    if (0 != ensure_root_path_unmounted("SYSTEM:")) 
        return print_and_error("Can't unmount /system!\n");
    if (0 != format_root_device("SYSTEM:"))
        return print_and_error("Error while formatting /system!\n");
    if (ensure_root_path_mounted("SYSTEM:") != 0)
        return print_and_error("Can't mount /system!\n");
    sprintf(tmp, "%s/system.img", backup_path);
    if (0 != unyaffs(tmp, "/system", yaffs_callback))
        return print_and_error("Error while restoring /system!\n");

    ui_print("Restoring data...\n");
    if (0 != ensure_root_path_unmounted("DATA:")) 
        return print_and_error("Can't unmount /data!\n");
    if (0 != format_root_device("DATA:"))
        return print_and_error("Error while formatting /data!\n");
    if (ensure_root_path_mounted("DATA:") != 0)
        return print_and_error("Can't mount /data!\n");
    sprintf(tmp, "%s/data.img", backup_path);
    if (0 != unyaffs(tmp, "/data", yaffs_callback))
        return print_and_error("Error while restoring /data!\n");

    ui_print("Restoring cache...\n");
    if (0 != ensure_root_path_unmounted("CACHE:")) 
        return print_and_error("Can't unmount /cache!\n");
    if (0 != format_root_device("CACHE:"))
        return print_and_error("Error while formatting /cache!\n");
    if (ensure_root_path_mounted("CACHE:") != 0)
        return print_and_error("Can't mount /cache!\n");
    sprintf(tmp, "%s/cache.img", backup_path);
    if (0 != unyaffs(tmp, "/cache", yaffs_callback))
        return print_and_error("Error while restoring /cache!\n");
        
    sync();
    ui_set_background(BACKGROUND_ICON_NONE);
    ui_reset_progress();
    ui_print("Restore complete!\n");
    return 0;
}
