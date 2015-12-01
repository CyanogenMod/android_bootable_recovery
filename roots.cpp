/*
 * Copyright (C) 2007 The Android Open Source Project
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

#include <errno.h>
#include <stdlib.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <ctype.h>
#include <fcntl.h>
#include <dirent.h>

#include <fs_mgr.h>
#include "mtdutils/mtdutils.h"
#include "mtdutils/mounts.h"
#include "roots.h"
#include "common.h"
#include "make_ext4fs.h"
extern "C" {
#include "wipe.h"
#include "cryptfs.h"
}

#include "voldclient.h"
#include <blkid/blkid.h>

static struct fstab *fstab = NULL;

extern struct selabel_handle *sehandle;

static int mkdir_p(const char* path, mode_t mode)
{
    char dir[PATH_MAX];
    char* p;
    strcpy(dir, path);
    for (p = strchr(&dir[1], '/'); p != NULL; p = strchr(p+1, '/')) {
        *p = '\0';
        if (mkdir(dir, mode) != 0 && errno != EEXIST) {
            return -1;
        }
        *p = '/';
    }
    if (mkdir(dir, mode) != 0 && errno != EEXIST) {
        return -1;
    }
    return 0;
}

static void write_fstab_entry(Volume *v, FILE *file)
{
    if (NULL != v && strcmp(v->fs_type, "mtd") != 0 && strcmp(v->fs_type, "emmc") != 0
                  && strcmp(v->fs_type, "bml") != 0 && !fs_mgr_is_voldmanaged(v)
                  && strncmp(v->blk_device, "/", 1) == 0
                  && strncmp(v->mount_point, "/", 1) == 0) {

        fprintf(file, "%s ", v->blk_device);
        fprintf(file, "%s ", v->mount_point);
        fprintf(file, "%s ", v->fs_type);
        fprintf(file, "%s 0 0\n", v->fs_options == NULL ? "defaults" : v->fs_options);
    }
}

int get_num_volumes() {
    return fstab->num_entries;
}

Volume* get_device_volumes() {
    return fstab->recs;
}

void load_volume_table()
{
    int i;
    int ret;

    fstab = fs_mgr_read_fstab("/etc/recovery.fstab");
    if (!fstab) {
        LOGE("failed to read /etc/recovery.fstab\n");
        return;
    }

    ret = fs_mgr_add_entry(fstab, "/tmp", "ramdisk", "ramdisk");
    if (ret < 0 ) {
        LOGE("failed to add /tmp entry to fstab\n");
        fs_mgr_free_fstab(fstab);
        fstab = NULL;
        return;
    }

    // Create a boring /etc/fstab so tools like Busybox work
    FILE *file = fopen("/etc/fstab", "w");
    if (file == NULL) {
        LOGW("Unable to create /etc/fstab!\n");
        return;
    }

    printf("recovery filesystem table\n");
    printf("=========================\n");
    for (i = 0; i < fstab->num_entries; ++i) {
        Volume* v = &fstab->recs[i];
        printf("  %d %s %s %s %lld\n", i, v->mount_point, v->fs_type,
               v->blk_device, v->length);

        write_fstab_entry(v, file);
    }

    fclose(file);

    printf("\n");
}

bool volume_is_mountable(Volume *v)
{
    return (fs_mgr_is_voldmanaged(v) ||
            !strcmp(v->fs_type, "yaffs2") ||
            !strcmp(v->fs_type, "ext4") ||
            !strcmp(v->fs_type, "f2fs") ||
            !strcmp(v->fs_type, "vfat"));
}

bool volume_is_readonly(Volume *v)
{
    return (v->flags & MS_RDONLY);
}

bool volume_is_verity(Volume *v)
{
    return fs_mgr_is_verified(v);
}

Volume* volume_for_path(const char* path) {
    Volume *rec = fs_mgr_get_entry_for_mount_point(fstab, path);

    if (rec == NULL)
        return rec;

    if (strcmp(rec->fs_type, "ext4") == 0 || strcmp(rec->fs_type, "f2fs") == 0 ||
            strcmp(rec->fs_type, "vfat") == 0) {
        char *detected_fs_type = blkid_get_tag_value(NULL, "TYPE", rec->blk_device);

        if (detected_fs_type == NULL)
            return rec;

        Volume *fetched_rec = rec;
        while (rec != NULL && strcmp(rec->fs_type, detected_fs_type) != 0)
            rec = fs_mgr_get_entry_for_mount_point_after(rec, fstab, path);

        if (rec == NULL)
            return fetched_rec;
    }

    return rec;
}

Volume* volume_for_label(const char* label) {
    int i;
    for (i = 0; i < get_num_volumes(); i++) {
        Volume* v = get_device_volumes() + i;
        if (v->label && !strcmp(v->label, label)) {
            return v;
        }
    }
    return NULL;
}

// Mount the volume specified by path at the given mount_point.
int ensure_path_mounted_at(const char* path, const char* mount_point, bool force_rw) {
    Volume* v = volume_for_path(path);
    if (v == NULL) {
        LOGE("unknown volume for path [%s]\n", path);
        return -1;
    }
    if (strcmp(v->fs_type, "ramdisk") == 0) {
        // the ramdisk is always mounted.
        return 0;
    }

    int result;
    result = scan_mounted_volumes();
    if (result < 0) {
        LOGE("failed to scan mounted volumes\n");
        return -1;
    }

    if (!mount_point) {
        mount_point = v->mount_point;
    }

    if (!fs_mgr_is_voldmanaged(v)) {
        const MountedVolume* mv =
            find_mounted_volume_by_mount_point(mount_point);
        if (mv) {
            // volume is already mounted
            return 0;
        }
    }

    mkdir_p(mount_point, 0755);  // in case it doesn't already exist

    if (strcmp(v->fs_type, "yaffs2") == 0) {
        // mount an MTD partition as a YAFFS2 filesystem.
        mtd_scan_partitions();
        const MtdPartition* partition;
        partition = mtd_find_partition_by_name(v->blk_device);
        if (partition == NULL) {
            LOGE("failed to find \"%s\" partition to mount at \"%s\"\n",
                 v->blk_device, mount_point);
            return -1;
        }
        return mtd_mount_partition(partition, mount_point, v->fs_type, 0);
    } else if (strcmp(v->fs_type, "ext4") == 0 ||
               strcmp(v->fs_type, "f2fs") == 0 ||
               strcmp(v->fs_type, "squashfs") == 0 ||
               strcmp(v->fs_type, "vfat") == 0) {
        unsigned long mntflags = v->flags;
        if (!force_rw) {
            if ((v->flags & MS_RDONLY) || fs_mgr_is_verified(v)) {
                mntflags |= MS_RDONLY;
            }
        }
        result = mount(v->blk_device, mount_point, v->fs_type,
                       mntflags, v->fs_options);
        if (result == 0) return 0;

        LOGE("failed to mount %s (%s)\n", mount_point, strerror(errno));
        return -1;
    }

    LOGE("unknown fs_type \"%s\" for %s\n", v->fs_type, mount_point);
    return -1;
}

int ensure_volume_mounted(Volume* v, bool force_rw) {
    if (v == NULL) {
        LOGE("cannot mount unknown volume\n");
        return -1;
    }
    return ensure_path_mounted_at(v->mount_point, nullptr, force_rw);
}

int ensure_path_mounted(const char* path, bool force_rw) {
    // Mount at the default mount point.
    return ensure_path_mounted_at(path, nullptr, force_rw);
}

int ensure_path_unmounted(const char* path, bool detach /* = false */) {
    Volume* v;
    if (memcmp(path, "/storage/", 9) == 0) {
        char label[PATH_MAX];
        const char* p = path+9;
        const char* q = strchr(p, '/');
        memset(label, 0, sizeof(label));
        if (q) {
            memcpy(label, p, q-p);
        }
        else {
            strcpy(label, p);
        }
        v = volume_for_label(label);
    }
    else {
        v = volume_for_path(path);
    }
    if (v == NULL) {
        LOGE("unknown volume for path [%s]\n", path);
        return -1;
    }
    return ensure_volume_unmounted(v, detach);
}

int ensure_volume_unmounted(Volume* v, bool detach /* = false */) {
    if (v == NULL) {
        LOGE("cannot unmount unknown volume\n");
        return -1;
    }
    if (strcmp(v->fs_type, "ramdisk") == 0) {
        // the ramdisk is always mounted; you can't unmount it.
        return -1;
    }

    int result;
    result = scan_mounted_volumes();
    if (result < 0) {
        LOGE("failed to scan mounted volumes\n");
        return -1;
    }

    const MountedVolume* mv =
        find_mounted_volume_by_mount_point(v->mount_point);
    if (mv == NULL) {
        // volume is already unmounted
        return 0;
    }

    if (detach) {
        result = unmount_mounted_volume_detach(mv);
    }
    else {
        result = unmount_mounted_volume(mv);
    }

    return result;
}

static int exec_cmd(const char* path, char* const argv[]) {
    int status;
    pid_t child;
    if ((child = vfork()) == 0) {
        execv(path, argv);
        _exit(-1);
    }
    waitpid(child, &status, 0);
    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
        LOGE("%s failed with status %d\n", path, WEXITSTATUS(status));
    }
    return WEXITSTATUS(status);
}

static int rmtree_except(const char* path, const char* except)
{
    char pathbuf[PATH_MAX];
    int rc = 0;
    DIR* dp = opendir(path);
    if (dp == NULL) {
        return -1;
    }
    struct dirent* de;
    while ((de = readdir(dp)) != NULL) {
        if (!strcmp(de->d_name, ".") || !strcmp(de->d_name, ".."))
            continue;
        if (except && !strcmp(de->d_name, except))
            continue;
        struct stat st;
        snprintf(pathbuf, sizeof(pathbuf), "%s/%s", path, de->d_name);
        rc = lstat(pathbuf, &st);
        if (rc != 0) {
            LOGE("Failed to stat %s\n", pathbuf);
            break;
        }
        if (S_ISDIR(st.st_mode)) {
            rc = rmtree_except(pathbuf, NULL);
            if (rc != 0)
                break;
            rc = rmdir(pathbuf);
        }
        else {
            rc = unlink(pathbuf);
        }
        if (rc != 0) {
            LOGI("Failed to remove %s: %s\n", pathbuf, strerror(errno));
            break;
        }
    }
    closedir(dp);
    return rc;
}

int format_volume(const char* volume, bool force) {
    if (strcmp(volume, "media") == 0) {
        if (!vdc->isEmulatedStorage()) {
            return 0;
        }
        if (ensure_path_mounted("/data") != 0) {
            LOGE("format_volume failed to mount /data\n");
            return -1;
        }
        int rc = 0;
        rc = rmtree_except("/data/media", NULL);
        ensure_path_unmounted("/data");
        return rc;
    }

    Volume* v = volume_for_path(volume);
    if (v == NULL) {
        LOGE("unknown volume \"%s\"\n", volume);
        return -1;
    }
    if (strcmp(v->fs_type, "ramdisk") == 0) {
        // you can't format the ramdisk.
        LOGE("can't format_volume \"%s\"", volume);
        return -1;
    }
    if (strcmp(v->mount_point, volume) != 0) {
        LOGE("can't give path \"%s\" to format_volume\n", volume);
        return -1;
    }

    if (!force && strcmp(volume, "/data") == 0 && vdc->isEmulatedStorage()) {
        if (ensure_path_mounted("/data") == 0) {
            // Preserve .layout_version to avoid "nesting bug"
            LOGI("Preserving layout version\n");
            unsigned char layout_buf[256];
            ssize_t layout_buflen = -1;
            int fd;
            fd = open("/data/.layout_version", O_RDONLY);
            if (fd != -1) {
                layout_buflen = read(fd, layout_buf, sizeof(layout_buf));
                close(fd);
            }

            int rc = rmtree_except("/data", "media");

            // Restore .layout_version
            if (layout_buflen > 0) {
                LOGI("Restoring layout version\n");
                fd = open("/data/.layout_version", O_WRONLY | O_CREAT | O_EXCL, 0600);
                if (fd != -1) {
                    write(fd, layout_buf, layout_buflen);
                    close(fd);
                }
            }

            ensure_path_unmounted(volume);

            return rc;
        }
        LOGE("format_volume failed to mount /data, formatting instead\n");
    }

    if (ensure_path_unmounted(volume) != 0) {
        LOGE("format_volume failed to unmount \"%s\"\n", v->mount_point);
        return -1;
    }

    if (fs_mgr_is_voldmanaged(v)) {
        LOGE("can't format vold volume \"%s\"", volume);
        return -1;
    }

    if (strcmp(v->fs_type, "yaffs2") == 0 || strcmp(v->fs_type, "mtd") == 0) {
        mtd_scan_partitions();
        const MtdPartition* partition = mtd_find_partition_by_name(v->blk_device);
        if (partition == NULL) {
            LOGE("format_volume: no MTD partition \"%s\"\n", v->blk_device);
            return -1;
        }

        MtdWriteContext *write = mtd_write_partition(partition);
        if (write == NULL) {
            LOGW("format_volume: can't open MTD \"%s\"\n", v->blk_device);
            return -1;
        } else if (mtd_erase_blocks(write, -1) == (off_t) -1) {
            LOGW("format_volume: can't erase MTD \"%s\"\n", v->blk_device);
            mtd_write_close(write);
            return -1;
        } else if (mtd_write_close(write)) {
            LOGW("format_volume: can't close MTD \"%s\"\n", v->blk_device);
            return -1;
        }
        return 0;
    }

    if (strcmp(v->fs_type, "ext4") == 0 || strcmp(v->fs_type, "f2fs") == 0) {
        // if there's a key_loc that looks like a path, it should be a
        // block device for storing encryption metadata.  wipe it too.
        if (v->key_loc != NULL && v->key_loc[0] == '/') {
            LOGI("wiping %s\n", v->key_loc);
            int fd = open(v->key_loc, O_WRONLY | O_CREAT, 0644);
            if (fd < 0) {
                LOGE("format_volume: failed to open %s\n", v->key_loc);
                return -1;
            }
            wipe_block_device(fd, get_file_size(fd));
            close(fd);
        }

        ssize_t length = 0;
        if (v->length != 0) {
            length = v->length;
        } else if (v->key_loc != NULL && strcmp(v->key_loc, "footer") == 0) {
            length = -CRYPT_FOOTER_OFFSET;
        }
        int result;
        if (strcmp(v->fs_type, "ext4") == 0) {
            result = make_ext4fs(v->blk_device, length, volume, sehandle);
        } else {   /* Has to be f2fs because we checked earlier. */
            char bytes_reserved[20], num_sectors[20];
            const char* f2fs_argv[6] = {"mkfs.f2fs", "-t1"};
            if (length < 0) {
                snprintf(bytes_reserved, sizeof(bytes_reserved), "%zd", -length);
                f2fs_argv[2] = "-r";
                f2fs_argv[3] = bytes_reserved;
                f2fs_argv[4] = v->blk_device;
                f2fs_argv[5] = NULL;
            } else {
                /* num_sectors can be zero which mean whole device space */
                snprintf(num_sectors, sizeof(num_sectors), "%zd", length / 512);
                f2fs_argv[2] = v->blk_device;
                f2fs_argv[3] = num_sectors;
                f2fs_argv[4] = NULL;
            }
            const char *f2fs_path = "/sbin/mkfs.f2fs";

            result = exec_cmd(f2fs_path, (char* const*)f2fs_argv);
        }
        if (result != 0) {
            LOGE("format_volume: make %s failed on %s with %d(%s)\n", v->fs_type, v->blk_device, result, strerror(errno));
            return -1;
        }
        return 0;
    }

    LOGE("format_volume: fs_type \"%s\" unsupported\n", v->fs_type);
    return -1;
}

int setup_install_mounts() {
    if (fstab == NULL) {
        LOGE("can't set up install mounts: no fstab loaded\n");
        return -1;
    }
    for (int i = 0; i < fstab->num_entries; ++i) {
        Volume* v = fstab->recs + i;

        if (strcmp(v->mount_point, "/tmp") == 0 ||
            strcmp(v->mount_point, "/cache") == 0) {
            if (ensure_path_mounted(v->mount_point) != 0) {
                LOGE("failed to mount %s\n", v->mount_point);
                return -1;
            }

        } else {
            // datamedia and anything managed by vold must be unmounted
            // with the detach flag to ensure that FUSE works.
            bool detach = false;
            if (vdc->isEmulatedStorage() && strcmp(v->mount_point, "/data") == 0) {
                detach = true;
            }
            if (ensure_volume_unmounted(v, detach) != 0) {
                LOGE("failed to unmount %s\n", v->mount_point);
                return -1;
            }
        }
    }
    return 0;
}
