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

#include "libcrecovery/common.h"

#include "bootloader.h"
#include "common.h"
#include "cutils/properties.h"
#include "firmware.h"
#include "install.h"
#include "minui/minui.h"
#include "minzip/DirUtil.h"
#include "roots.h"
#include "recovery_ui.h"

#include <sys/vfs.h>

#include "extendedcommands.h"
#include "nandroid.h"
#include "mounts.h"

#include "flashutils/flashutils.h"
#include <libgen.h>

void nandroid_generate_timestamp_path(char* backup_path)
{
    time_t t = time(NULL);
    struct tm *tmp = localtime(&t);
    if (tmp == NULL)
    {
        struct timeval tp;
        gettimeofday(&tp, NULL);
        sprintf(backup_path, "/sdcard/clockworkmod/backup/%ld", tp.tv_sec);
    }
    else
    {
        strftime(backup_path, PATH_MAX, "/sdcard/clockworkmod/backup/%F.%H.%M.%S", tmp);
    }
}

static void ensure_directory(const char* dir) {
    char tmp[PATH_MAX];
    sprintf(tmp, "mkdir -p %s ; chmod 777 %s", dir, dir);
    __system(tmp);
}

static int print_and_error(const char* message) {
    ui_print("%s\n", message);
    return 1;
}

static int nandroid_backup_bitfield = 0;
#define NANDROID_FIELD_DEDUPE_CLEARED_SPACE 1
static int nandroid_files_total = 0;
static int nandroid_files_count = 0;
static void nandroid_callback(const char* filename)
{
    if (filename == NULL)
        return;
    const char* justfile = basename(filename);
    char tmp[PATH_MAX];
    strcpy(tmp, justfile);
    if (tmp[strlen(tmp) - 1] == '\n')
        tmp[strlen(tmp) - 1] = '\0';
    tmp[ui_get_text_cols() - 1] = '\0';
    nandroid_files_count++;
    ui_increment_frame();
    ui_nice_print("%s\n", tmp);
    if (!ui_was_niced() && nandroid_files_total != 0)
        ui_set_progress((float)nandroid_files_count / (float)nandroid_files_total);
    if (!ui_was_niced())
        ui_delete_line();
}

static void compute_directory_stats(const char* directory)
{
    char tmp[PATH_MAX];
    sprintf(tmp, "find %s | %s wc -l > /tmp/dircount", directory, strcmp(directory, "/data") == 0 && is_data_media() ? "grep -v /data/media |" : "");
    __system(tmp);
    char count_text[100];
    FILE* f = fopen("/tmp/dircount", "r");
    fread(count_text, 1, sizeof(count_text), f);
    fclose(f);
    nandroid_files_count = 0;
    nandroid_files_total = atoi(count_text);
    ui_reset_progress();
    ui_show_progress(1, 0);
}

typedef void (*file_event_callback)(const char* filename);
typedef int (*nandroid_backup_handler)(const char* backup_path, const char* backup_file_image, int callback);

static int mkyaffs2image_wrapper(const char* backup_path, const char* backup_file_image, int callback) {
    char tmp[PATH_MAX];
    sprintf(tmp, "cd %s ; mkyaffs2image . %s.img ; exit $?", backup_path, backup_file_image);

    FILE *fp = __popen(tmp, "r");
    if (fp == NULL) {
        ui_print("Unable to execute mkyaffs2image.\n");
        return -1;
    }

    while (fgets(tmp, PATH_MAX, fp) != NULL) {
        tmp[PATH_MAX - 1] = '\0';
        if (callback)
            nandroid_callback(tmp);
    }

    return __pclose(fp);
}

static int do_tar_compress(char* command, int callback) {
    char buf[PATH_MAX];

    FILE *fp = __popen(command, "r");
    if (fp == NULL) {
        ui_print("Unable to execute tar command!\n");
        return -1;
    }

    while (fgets(buf, PATH_MAX, fp) != NULL) {
        buf[PATH_MAX - 1] = '\0';
        if (callback)
            nandroid_callback(buf);
    }

    return __pclose(fp);
}

static int tar_compress_wrapper(const char* backup_path, const char* backup_file_image, int callback) {
    char tmp[PATH_MAX];
    sprintf(tmp, "cd $(dirname %s) ; touch %s.tar ; (tar cv --exclude=data/data/com.google.android.music/files/* %s $(basename %s) | split -a 1 -b 1000000000 /proc/self/fd/0 %s.tar.) 2> /proc/self/fd/1 ; exit $?", backup_path, backup_file_image, strcmp(backup_path, "/data") == 0 && is_data_media() ? "--exclude 'media'" : "", backup_path, backup_file_image);

    return do_tar_compress(tmp, callback);
}

static int tar_gzip_compress_wrapper(const char* backup_path, const char* backup_file_image, int callback) {
    char tmp[PATH_MAX];
    sprintf(tmp, "cd $(dirname %s) ; touch %s.tar.gz ; (tar cv --exclude=data/data/com.google.android.music/files/* %s $(basename %s) | pigz -c | split -a 1 -b 1000000000 /proc/self/fd/0 %s.tar.gz.) 2> /proc/self/fd/1 ; exit $?", backup_path, backup_file_image, strcmp(backup_path, "/data") == 0 && is_data_media() ? "--exclude 'media'" : "", backup_path, backup_file_image);

    return do_tar_compress(tmp, callback);
}

static int tar_dump_wrapper(const char* backup_path, const char* backup_file_image, int callback) {
    char tmp[PATH_MAX];
    sprintf(tmp, "cd $(dirname %s); tar cv --exclude=data/data/com.google.android.music/files/* %s $(basename %s) 2> /dev/null | cat", backup_path, strcmp(backup_path, "/data") == 0 && is_data_media() ? "--exclude 'media'" : "", backup_path);

    return __system(tmp);
}

void nandroid_dedupe_gc(const char* blob_dir) {
    char backup_dir[PATH_MAX];
    strcpy(backup_dir, blob_dir);
    char *d = dirname(backup_dir);
    strcpy(backup_dir, d);
    strcat(backup_dir, "/backup");
    ui_print("Freeing space...\n");
    char tmp[PATH_MAX];
    sprintf(tmp, "dedupe gc %s $(find %s -name '*.dup')", blob_dir, backup_dir);
    __system(tmp);
    ui_print("Done freeing space.\n");
}

static int dedupe_compress_wrapper(const char* backup_path, const char* backup_file_image, int callback) {
    char tmp[PATH_MAX];
    char blob_dir[PATH_MAX];
    strcpy(blob_dir, backup_file_image);
    char *d = dirname(blob_dir);
    strcpy(blob_dir, d);
    d = dirname(blob_dir);
    strcpy(blob_dir, d);
    d = dirname(blob_dir);
    strcpy(blob_dir, d);
    strcat(blob_dir, "/blobs");
    ensure_directory(blob_dir);

    if (!(nandroid_backup_bitfield & NANDROID_FIELD_DEDUPE_CLEARED_SPACE)) {
        nandroid_backup_bitfield |= NANDROID_FIELD_DEDUPE_CLEARED_SPACE;
        nandroid_dedupe_gc(blob_dir);
    }

    sprintf(tmp, "dedupe c %s %s %s.dup %s", backup_path, blob_dir, backup_file_image, strcmp(backup_path, "/data") == 0 && is_data_media() ? "./media" : "");

    FILE *fp = __popen(tmp, "r");
    if (fp == NULL) {
        ui_print("Unable to execute dedupe.\n");
        return -1;
    }

    while (fgets(tmp, PATH_MAX, fp) != NULL) {
        tmp[PATH_MAX - 1] = '\0';
        if (callback)
            nandroid_callback(tmp);
    }

    return __pclose(fp);
}

static nandroid_backup_handler default_backup_handler = tar_compress_wrapper;
static char forced_backup_format[5] = "";
void nandroid_force_backup_format(const char* fmt) {
    strcpy(forced_backup_format, fmt);
}
static void refresh_default_backup_handler() {
    char fmt[5];
    if (strlen(forced_backup_format) > 0) {
        strcpy(fmt, forced_backup_format);
    }
    else {
        ensure_path_mounted(get_primary_storage_path());
        FILE* f = fopen(NANDROID_BACKUP_FORMAT_FILE, "r");
        if (NULL == f) {
            default_backup_handler = tar_compress_wrapper;
            return;
        }
        fread(fmt, 1, sizeof(fmt), f);
        fclose(f);
    }
    fmt[3] = '\0';
    if (0 == strcmp(fmt, "dup"))
        default_backup_handler = dedupe_compress_wrapper;
    else if (0 == strcmp(fmt, "tgz"))
        default_backup_handler = tar_gzip_compress_wrapper;
    else if (0 == strcmp(fmt, "tar"))
        default_backup_handler = tar_compress_wrapper;
    else
        default_backup_handler = tar_compress_wrapper;
}

unsigned nandroid_get_default_backup_format() {
    refresh_default_backup_handler();
    if (default_backup_handler == dedupe_compress_wrapper) {
        return NANDROID_BACKUP_FORMAT_DUP;
    } else if (default_backup_handler == tar_gzip_compress_wrapper) {
        return NANDROID_BACKUP_FORMAT_TGZ;
    } else {
        return NANDROID_BACKUP_FORMAT_TAR;
    }
}

static nandroid_backup_handler get_backup_handler(const char *backup_path) {
    Volume *v = volume_for_path(backup_path);
    if (v == NULL) {
        ui_print("Unable to find volume.\n");
        return NULL;
    }
    const MountedVolume *mv = find_mounted_volume_by_mount_point(v->mount_point);
    if (mv == NULL) {
        ui_print("Unable to find mounted volume: %s\n", v->mount_point);
        return NULL;
    }

    if (strcmp(backup_path, "/data") == 0 && is_data_media()) {
        return default_backup_handler;
    }

    if (strlen(forced_backup_format) > 0)
        return default_backup_handler;

    // cwr5, we prefer dedupe for everything except yaffs2
    if (strcmp("yaffs2", mv->filesystem) == 0) {
        return mkyaffs2image_wrapper;
    }

    return default_backup_handler;
}

int nandroid_backup_partition_extended(const char* backup_path, const char* mount_point, int umount_when_finished) {

    int ret = 0;
    char name[PATH_MAX];
    char tmp[PATH_MAX];
    strcpy(name, basename(mount_point));

    struct stat file_info;
    int callback = stat("/sdcard/clockworkmod/.hidenandroidprogress", &file_info) != 0;

    ui_print("Backing up %s...\n", name);
    if (0 != (ret = ensure_path_mounted(mount_point) != 0)) {
        ui_print("Can't mount %s!\n", mount_point);
        return ret;
    }
    compute_directory_stats(mount_point);
    scan_mounted_volumes();
    Volume *v = volume_for_path(mount_point);
    const MountedVolume *mv = NULL;
    if (v != NULL)
        mv = find_mounted_volume_by_mount_point(v->mount_point);

    if (strcmp(backup_path, "-") == 0)
        sprintf(tmp, "/proc/self/fd/1");
    else if (mv == NULL || mv->filesystem == NULL)
        sprintf(tmp, "%s/%s.auto", backup_path, name);
    else
        sprintf(tmp, "%s/%s.%s", backup_path, name, mv->filesystem);
    nandroid_backup_handler backup_handler = get_backup_handler(mount_point);

    if (backup_handler == NULL) {
        ui_print("Error finding an appropriate backup handler.\n");
        return -2;
    }
    ret = backup_handler(mount_point, tmp, callback);
    if (umount_when_finished) {
        ensure_path_unmounted(mount_point);
    }
    if (0 != ret) {
        ui_print("Error while making a backup image of %s!\n", mount_point);
        return ret;
    }
    ui_print("Backup of %s completed.\n", name);
    return 0;
}

int nandroid_backup_partition(const char* backup_path, const char* root) {
    Volume *vol = volume_for_path(root);
    // make sure the volume exists before attempting anything...
    if (vol == NULL || vol->fs_type == NULL)
        return 0;

    // see if we need a raw backup (mtd)
    char tmp[PATH_MAX];
    int ret;
    if (strcmp(vol->fs_type, "mtd") == 0 ||
            strcmp(vol->fs_type, "bml") == 0 ||
            strcmp(vol->fs_type, "emmc") == 0) {
        const char* name = basename(root);
        if (strcmp(backup_path, "-") == 0)
            strcpy(tmp, "/proc/self/fd/1");
        else
            sprintf(tmp, "%s/%s.img", backup_path, name);

        ui_print("Backing up %s image...\n", name);
        if (0 != (ret = backup_raw_partition(vol->fs_type, vol->blk_device, tmp))) {
            ui_print("Error while backing up %s image!", name);
            return ret;
        }

        ui_print("Backup of %s image completed.\n", name);
        return 0;
    }

    return nandroid_backup_partition_extended(backup_path, root, 1);
}

int nandroid_backup(const char* backup_path)
{
    nandroid_backup_bitfield = 0;
    ui_set_background(BACKGROUND_ICON_INSTALLING);
    refresh_default_backup_handler();

    if (ensure_path_mounted(backup_path) != 0) {
        return print_and_error("Can't mount backup path.\n");
    }

    Volume* volume;
    if (is_data_media_volume_path(backup_path))
        volume = volume_for_path("/data");
    else
        volume = volume_for_path(backup_path);
    if (NULL == volume)
        return print_and_error("Unable to find volume for backup path.\n");
    int ret;
    struct statfs sfs;
    struct stat s;
    if (NULL != volume) {
        if (0 != (ret = statfs(volume->mount_point, &sfs)))
            return print_and_error("Unable to stat backup path.\n");
        uint64_t bavail = sfs.f_bavail;
        uint64_t bsize = sfs.f_bsize;
        uint64_t sdcard_free = bavail * bsize;
        uint64_t sdcard_free_mb = sdcard_free / (uint64_t)(1024 * 1024);
        ui_print("SD Card space free: %lluMB\n", sdcard_free_mb);
        if (sdcard_free_mb < 150)
            ui_print("There may not be enough free space to complete backup... continuing...\n");
    }
    char tmp[PATH_MAX];
    ensure_directory(backup_path);

    if (0 != (ret = nandroid_backup_partition(backup_path, "/boot")))
        return ret;

    if (0 != (ret = nandroid_backup_partition(backup_path, "/recovery")))
        return ret;

    Volume *vol = volume_for_path("/wimax");
    if (vol != NULL && 0 == stat(vol->blk_device, &s))
    {
        char serialno[PROPERTY_VALUE_MAX];
        ui_print("Backing up WiMAX...\n");
        serialno[0] = 0;
        property_get("ro.serialno", serialno, "");
        sprintf(tmp, "%s/wimax.%s.img", backup_path, serialno);
        ret = backup_raw_partition(vol->fs_type, vol->blk_device, tmp);
        if (0 != ret)
            return print_and_error("Error while dumping WiMAX image!\n");
    }

    if (0 != (ret = nandroid_backup_partition(backup_path, "/system")))
        return ret;

    if (0 != (ret = nandroid_backup_partition(backup_path, "/data")))
        return ret;

    if (has_datadata()) {
        if (0 != (ret = nandroid_backup_partition(backup_path, "/datadata")))
            return ret;
    }

    if (is_data_media() || 0 != stat(get_android_secure_path(), &s)) {
        ui_print("No .android_secure found. Skipping backup of applications on external storage.\n");
    }
    else {
        if (0 != (ret = nandroid_backup_partition_extended(backup_path, get_android_secure_path(), 0)))
            return ret;
    }

    if (0 != (ret = nandroid_backup_partition_extended(backup_path, "/cache", 0)))
        return ret;

    vol = volume_for_path("/sd-ext");
    if (vol == NULL || 0 != stat(vol->blk_device, &s))
    {
        LOGI("No sd-ext found. Skipping backup of sd-ext.\n");
    }
    else
    {
        if (0 != ensure_path_mounted("/sd-ext"))
            LOGI("Could not mount sd-ext. sd-ext backup may not be supported on this device. Skipping backup of sd-ext.\n");
        else if (0 != (ret = nandroid_backup_partition(backup_path, "/sd-ext")))
            return ret;
    }

    ui_print("Generating md5 sum...\n");
    sprintf(tmp, "nandroid-md5.sh %s", backup_path);
    if (0 != (ret = __system(tmp))) {
        ui_print("Error while generating md5 sum!\n");
        return ret;
    }

    sprintf(tmp, "cp /tmp/recovery.log %s/recovery.log", backup_path);
    __system(tmp);

    sprintf(tmp, "chmod -R 777 %s ; chmod -R u+r,u+w,g+r,g+w,o+r,o+w /sdcard/clockworkmod ; chmod u+x,g+x,o+x /sdcard/clockworkmod/backup ; chmod u+x,g+x,o+x /sdcard/clockworkmod/blobs", backup_path);
    __system(tmp);
    sync();
    ui_set_background(BACKGROUND_ICON_NONE);
    ui_reset_progress();
    ui_print("\nBackup complete!\n");
    return 0;
}

int nandroid_dump(const char* partition) {
    // silence our ui_print statements and other logging
    ui_set_log_stdout(0);

    nandroid_backup_bitfield = 0;
    refresh_default_backup_handler();

    // override our default to be the basic tar dumper
    default_backup_handler = tar_dump_wrapper;

    if (strcmp(partition, "boot") == 0) {
        return __system("dump_image boot /proc/self/fd/1 | cat");
    }

    if (strcmp(partition, "recovery") == 0) {
        return __system("dump_image recovery /proc/self/fd/1 | cat");
    }

    if (strcmp(partition, "data") == 0) {
        return nandroid_backup_partition("-", "/data");
    }

    if (strcmp(partition, "system") == 0) {
        return nandroid_backup_partition("-", "/system");
    }

    return 1;
}

typedef int (*nandroid_restore_handler)(const char* backup_file_image, const char* backup_path, int callback);

static int unyaffs_wrapper(const char* backup_file_image, const char* backup_path, int callback) {
    char tmp[PATH_MAX];
    sprintf(tmp, "cd %s ; unyaffs %s ; exit $?", backup_path, backup_file_image);
    FILE *fp = __popen(tmp, "r");
    if (fp == NULL) {
        ui_print("Unable to execute unyaffs.\n");
        return -1;
    }

    while (fgets(tmp, PATH_MAX, fp) != NULL) {
        tmp[PATH_MAX - 1] = '\0';
        if (callback)
            nandroid_callback(tmp);
    }

    return __pclose(fp);
}

static int do_tar_extract(char* command, int callback) {
    char buf[PATH_MAX];

    FILE *fp = __popen(command, "r");
    if (fp == NULL) {
        ui_print("Unable to execute tar command.\n");
        return -1;
    }

    while (fgets(buf, PATH_MAX, fp) != NULL) {
        buf[PATH_MAX - 1] = '\0';
        if (callback)
            nandroid_callback(buf);
    }

    return __pclose(fp);
}

static int tar_gzip_extract_wrapper(const char* backup_file_image, const char* backup_path, int callback) {
    char tmp[PATH_MAX];
    sprintf(tmp, "cd $(dirname %s) ; cat %s* | pigz -d -c | tar xv ; exit $?", backup_path, backup_file_image);

    return do_tar_extract(tmp, callback);
}

static int tar_extract_wrapper(const char* backup_file_image, const char* backup_path, int callback) {
    char tmp[PATH_MAX];
    sprintf(tmp, "cd $(dirname %s) ; cat %s* | tar xv ; exit $?", backup_path, backup_file_image);

    return do_tar_extract(tmp, callback);
}

static int dedupe_extract_wrapper(const char* backup_file_image, const char* backup_path, int callback) {
    char tmp[PATH_MAX];
    char blob_dir[PATH_MAX];
    strcpy(blob_dir, backup_file_image);
    char *bd = dirname(blob_dir);
    strcpy(blob_dir, bd);
    bd = dirname(blob_dir);
    strcpy(blob_dir, bd);
    bd = dirname(blob_dir);
    sprintf(tmp, "dedupe x %s %s/blobs %s; exit $?", backup_file_image, bd, backup_path);

    char path[PATH_MAX];
    FILE *fp = __popen(tmp, "r");
    if (fp == NULL) {
        ui_print("Unable to execute dedupe.\n");
        return -1;
    }

    while (fgets(path, PATH_MAX, fp) != NULL) {
        if (callback)
            nandroid_callback(path);
    }

    return __pclose(fp);
}

static int tar_undump_wrapper(const char* backup_file_image, const char* backup_path, int callback) {
    char tmp[PATH_MAX];
    sprintf(tmp, "cd $(dirname %s) ; tar xv ", backup_path);

    return __system(tmp);
}

static nandroid_restore_handler get_restore_handler(const char *backup_path) {
    Volume *v = volume_for_path(backup_path);
    if (v == NULL) {
        ui_print("Unable to find volume.\n");
        return NULL;
    }
    scan_mounted_volumes();
    const MountedVolume *mv = find_mounted_volume_by_mount_point(v->mount_point);
    if (mv == NULL) {
        ui_print("Unable to find mounted volume: %s\n", v->mount_point);
        return NULL;
    }

    if (strcmp(backup_path, "/data") == 0 && is_data_media()) {
        return tar_extract_wrapper;
    }

    // cwr 5, we prefer tar for everything unless it is yaffs2
    char str[255];
    char* partition;
    property_get("ro.cwm.prefer_tar", str, "false");
    if (strcmp("true", str) != 0) {
        return unyaffs_wrapper;
    }

    if (strcmp("yaffs2", mv->filesystem) == 0) {
        return unyaffs_wrapper;
    }

    return tar_extract_wrapper;
}

int nandroid_restore_partition_extended(const char* backup_path, const char* mount_point, int umount_when_finished) {
    int ret = 0;
    char* name = basename(mount_point);

    nandroid_restore_handler restore_handler = NULL;
    const char *filesystems[] = { "yaffs2", "ext2", "ext3", "ext4", "vfat", "rfs", "f2fs", NULL };
    const char* backup_filesystem = NULL;
    Volume *vol = volume_for_path(mount_point);
    const char *device = NULL;
    if (vol != NULL)
        device = vol->blk_device;

    char tmp[PATH_MAX];
    sprintf(tmp, "%s/%s.img", backup_path, name);
    struct stat file_info;
    if (strcmp(backup_path, "-") == 0) {
        if (vol)
            backup_filesystem = vol->fs_type;
        restore_handler = tar_extract_wrapper;
        strcpy(tmp, "/proc/self/fd/0");
    }
    else if (0 != (ret = stat(tmp, &file_info))) {
        // can't find the backup, it may be the new backup format?
        // iterate through the backup types
        printf("couldn't find default\n");
        const char *filesystem;
        int i = 0;
        while ((filesystem = filesystems[i]) != NULL) {
            sprintf(tmp, "%s/%s.%s.img", backup_path, name, filesystem);
            if (0 == (ret = stat(tmp, &file_info))) {
                backup_filesystem = filesystem;
                restore_handler = unyaffs_wrapper;
                break;
            }
            sprintf(tmp, "%s/%s.%s.tar", backup_path, name, filesystem);
            if (0 == (ret = stat(tmp, &file_info))) {
                backup_filesystem = filesystem;
                restore_handler = tar_extract_wrapper;
                break;
            }
            sprintf(tmp, "%s/%s.%s.tar.gz", backup_path, name, filesystem);
            if (0 == (ret = stat(tmp, &file_info))) {
                backup_filesystem = filesystem;
                restore_handler = tar_gzip_extract_wrapper;
                break;
            }
            sprintf(tmp, "%s/%s.%s.dup", backup_path, name, filesystem);
            if (0 == (ret = stat(tmp, &file_info))) {
                backup_filesystem = filesystem;
                restore_handler = dedupe_extract_wrapper;
                break;
            }
            i++;
        }

        if (backup_filesystem == NULL || restore_handler == NULL) {
            ui_print("%s.img not found. Skipping restore of %s.\n", name, mount_point);
            return 0;
        }
        else {
            printf("Found new backup image: %s\n", tmp);
        }
    }

    // If the fs_type of this volume is "auto" or mount_point is /data
    // and is_data_media, let's revert
    // to using a rm -rf, rather than trying to do a
    // ext3/ext4/whatever format.
    // This is because some phones (like DroidX) will freak out if you
    // reformat the /system or /data partitions, and not boot due to
    // a locked bootloader.
    // Other devices, like the Galaxy Nexus, XOOM, and Galaxy Tab 10.1
    // have a /sdcard symlinked to /data/media.
    // Or of volume does not exist (.android_secure), just rm -rf.
    if (vol == NULL || 0 == strcmp(vol->fs_type, "auto"))
        backup_filesystem = NULL;
    if (0 == strcmp(vol->mount_point, "/data") && is_data_media())
        backup_filesystem = NULL;

    ensure_directory(mount_point);

    int callback = stat("/sdcard/clockworkmod/.hidenandroidprogress", &file_info) != 0;

    ui_print("Restoring %s...\n", name);
    if (backup_filesystem == NULL) {
        if (0 != (ret = format_volume(mount_point))) {
            ui_print("Error while formatting %s!\n", mount_point);
            return ret;
        }
    }
    else if (0 != (ret = format_device(device, mount_point, backup_filesystem))) {
        ui_print("Error while formatting %s!\n", mount_point);
        return ret;
    }

    if (0 != (ret = ensure_path_mounted(mount_point))) {
        ui_print("Can't mount %s!\n", mount_point);
        return ret;
    }

    if (restore_handler == NULL)
        restore_handler = get_restore_handler(mount_point);

    // override restore handler for undump
    if (strcmp(backup_path, "-") == 0) {
        restore_handler = tar_undump_wrapper;
    }

    if (restore_handler == NULL) {
        ui_print("Error finding an appropriate restore handler.\n");
        return -2;
    }

    if (0 != (ret = restore_handler(tmp, mount_point, callback))) {
        ui_print("Error while restoring %s!\n", mount_point);
        return ret;
    }

    if (umount_when_finished) {
        ensure_path_unmounted(mount_point);
    }

    return 0;
}

int nandroid_restore_partition(const char* backup_path, const char* root) {
    Volume *vol = volume_for_path(root);
    // make sure the volume exists...
    if (vol == NULL || vol->fs_type == NULL)
        return 0;

    // see if we need a raw restore (mtd)
    char tmp[PATH_MAX];
    if (strcmp(vol->fs_type, "mtd") == 0 ||
            strcmp(vol->fs_type, "bml") == 0 ||
            strcmp(vol->fs_type, "emmc") == 0) {
        int ret;
        const char* name = basename(root);
        ui_print("Erasing %s before restore...\n", name);
        if (0 != (ret = format_volume(root))) {
            ui_print("Error while erasing %s image!", name);
            return ret;
        }

        if (strcmp(backup_path, "-") == 0)
            strcpy(tmp, backup_path);
        else
            sprintf(tmp, "%s%s.img", backup_path, root);

        ui_print("Restoring %s image...\n", name);
        if (0 != (ret = restore_raw_partition(vol->fs_type, vol->blk_device, tmp))) {
            ui_print("Error while flashing %s image!\n", name);
            return ret;
        }
        return 0;
    }
    return nandroid_restore_partition_extended(backup_path, root, 1);
}

int nandroid_restore(const char* backup_path, int restore_boot, int restore_system, int restore_data, int restore_cache, int restore_sdext, int restore_wimax)
{
    ui_set_background(BACKGROUND_ICON_INSTALLING);
    ui_show_indeterminate_progress();
    nandroid_files_total = 0;

    if (ensure_path_mounted(backup_path) != 0)
        return print_and_error("Can't mount backup path\n");

    char tmp[PATH_MAX];

    ui_print("Checking MD5 sums...\n");
    sprintf(tmp, "cd %s && md5sum -c nandroid.md5", backup_path);
    if (0 != __system(tmp))
        return print_and_error("MD5 mismatch!\n");

    int ret;

    if (restore_boot && NULL != volume_for_path("/boot") && 0 != (ret = nandroid_restore_partition(backup_path, "/boot")))
        return ret;

    struct stat s;
    Volume *vol = volume_for_path("/wimax");
    if (restore_wimax && vol != NULL && 0 == stat(vol->blk_device, &s))
    {
        char serialno[PROPERTY_VALUE_MAX];

        serialno[0] = 0;
        property_get("ro.serialno", serialno, "");
        sprintf(tmp, "%s/wimax.%s.img", backup_path, serialno);

        struct stat st;
        if (0 != stat(tmp, &st))
        {
            ui_print("WARNING: WiMAX partition exists, but nandroid\n");
            ui_print("         backup does not contain WiMAX image.\n");
            ui_print("         You should create a new backup to\n");
            ui_print("         protect your WiMAX keys.\n");
        }
        else
        {
            ui_print("Erasing WiMAX before restore...\n");
            if (0 != (ret = format_volume("/wimax")))
                return print_and_error("Error while formatting wimax!\n");
            ui_print("Restoring WiMAX image...\n");
            if (0 != (ret = restore_raw_partition(vol->fs_type, vol->blk_device, tmp)))
                return ret;
        }
    }

    if (restore_system && 0 != (ret = nandroid_restore_partition(backup_path, "/system")))
        return ret;

    if (restore_data && 0 != (ret = nandroid_restore_partition(backup_path, "/data")))
        return ret;

    if (has_datadata()) {
        if (restore_data && 0 != (ret = nandroid_restore_partition(backup_path, "/datadata")))
            return ret;
    }

    if (restore_data && 0 != (ret = nandroid_restore_partition_extended(backup_path, get_android_secure_path(), 0)))
        return ret;

    if (restore_cache && 0 != (ret = nandroid_restore_partition_extended(backup_path, "/cache", 0)))
        return ret;

    if (restore_sdext && 0 != (ret = nandroid_restore_partition(backup_path, "/sd-ext")))
        return ret;

    sync();
    ui_set_background(BACKGROUND_ICON_NONE);
    ui_reset_progress();
    ui_print("\nRestore complete!\n");
    return 0;
}

int nandroid_undump(const char* partition) {
    nandroid_files_total = 0;

    int ret;

    if (strcmp(partition, "boot") == 0) {
        return __system("flash_image boot /proc/self/fd/0");
    }

    if (strcmp(partition, "recovery") == 0) {
        if(0 != (ret = nandroid_restore_partition("-", "/recovery")))
            return ret;
    }

    if (strcmp(partition, "system") == 0) {
        if(0 != (ret = nandroid_restore_partition("-", "/system")))
            return ret;
    }

    if (strcmp(partition, "data") == 0) {
        if(0 != (ret = nandroid_restore_partition("-", "/data")))
            return ret;
    }

    sync();
    return 0;
}

int nandroid_usage()
{
    printf("Usage: nandroid backup\n");
    printf("Usage: nandroid restore <directory>\n");
    printf("Usage: nandroid dump <partition>\n");
    printf("Usage: nandroid undump <partition>\n");
    return 1;
}

static int bu_usage() {
    printf("Usage: bu <fd> backup partition\n");
    printf("Usage: Prior to restore:\n");
    printf("Usage: echo -n <partition> > /tmp/ro.bu.restore\n");
    printf("Usage: bu <fd> restore\n");
    return 1;
}

int bu_main(int argc, char** argv) {
    load_volume_table();

    if (strcmp(argv[2], "backup") == 0) {
        if (argc != 4) {
            return bu_usage();
        }

        int fd = atoi(argv[1]);
        char* partition = argv[3];

        if (fd != STDOUT_FILENO) {
            dup2(fd, STDOUT_FILENO);
            close(fd);
        }

        // fprintf(stderr, "%d %d %s\n", fd, STDOUT_FILENO, argv[3]);
        int ret = nandroid_dump(partition);
        sleep(10);
        return ret;
    }
    else if (strcmp(argv[2], "restore") == 0) {
        if (argc != 3) {
            return bu_usage();
        }

        int fd = atoi(argv[1]);
        if (fd != STDIN_FILENO) {
            dup2(fd, STDIN_FILENO);
            close(fd);
        }
        char partition[100];
        FILE* f = fopen("/tmp/ro.bu.restore", "r");
        fread(partition, 1, sizeof(partition), f);
        fclose(f);

        // fprintf(stderr, "%d %d %s\n", fd, STDIN_FILENO, argv[3]);
        return nandroid_undump(partition);
    }

    return bu_usage();
}

int nandroid_main(int argc, char** argv)
{
    load_volume_table();
    char backup_path[PATH_MAX];

    if (argc > 3 || argc < 2)
        return nandroid_usage();

    if (strcmp("backup", argv[1]) == 0)
    {
        if (argc != 2)
            return nandroid_usage();

        nandroid_generate_timestamp_path(backup_path);
        return nandroid_backup(backup_path);
    }

    if (strcmp("restore", argv[1]) == 0)
    {
        if (argc != 3)
            return nandroid_usage();
        return nandroid_restore(argv[2], 1, 1, 1, 1, 1, 0);
    }

    if (strcmp("dump", argv[1]) == 0)
    {
        if (argc != 3)
            return nandroid_usage();
        return nandroid_dump(argv[2]);
    }

    if (strcmp("undump", argv[1]) == 0)
    {
        if (argc != 3)
            return nandroid_usage();
        return nandroid_undump(argv[2]);
    }

    return nandroid_usage();
}
