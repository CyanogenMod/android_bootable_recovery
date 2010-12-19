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

extern int __system(const char *command);
#define BML_UNLOCK_ALL				0x8A29		///< unlock all partition RO -> RW

static int restore_internal(const char* bml, const char* filename)
{
    char buf[4096];
    int dstfd, srcfd, bytes_read, bytes_written, total_read = 0;
    if (filename == NULL)
        srcfd = 0;
    else {
        srcfd = open(filename, O_RDONLY | O_LARGEFILE);
        if (srcfd < 0)
            return 2;
    }
    dstfd = open(bml, O_RDWR | O_LARGEFILE);
    if (dstfd < 0)
        return 3;
    if (ioctl(dstfd, BML_UNLOCK_ALL, 0))
        return 4;
    do {
        total_read += bytes_read = read(srcfd, buf, 4096);
        if (!bytes_read)
            break;
        if (bytes_read < 4096)
            memset(&buf[bytes_read], 0, 4096 - bytes_read);
        if (write(dstfd, buf, 4096) < 4096)
            return 5;
    } while(bytes_read == 4096);
    
    close(dstfd);
    close(srcfd);
    
    return 0;
}

int cmd_bml_restore_raw_partition(const char *partition, const char *filename)
{
    char *bml;
    if (strcmp(partition, "boot") == 0 || strcmp(partition, "recovery") == 0)
        bml = "/dev/block/bml7";
    else
        return 6;

    int ret = restore_internal("/dev/block/bml7", filename);
    if (ret != 0)
        return ret;

    ret = restore_internal("/dev/block/bml8", filename);
    return ret;
}

int cmd_bml_backup_raw_partition(const char *partition, const char *filename)
{
    char tmp[PATH_MAX];
    sprintf(tmp, "dd of=%s if=/dev/block/bml7 bs=4096", filename);
    return __system(tmp);
}

int cmd_bml_erase_raw_partition(const char *partition)
{
    // TODO: implement raw wipe
    return 0;
}

int cmd_bml_erase_partition(const char *partition, const char *filesystem)
{
    return -1;
}

int cmd_bml_mount_partition(const char *partition, const char *mount_point, const char *filesystem, int read_only)
{
    return -1;
}

int cmd_bml_get_partition_device(const char *partition, char *device)
{
    return -1;
}
