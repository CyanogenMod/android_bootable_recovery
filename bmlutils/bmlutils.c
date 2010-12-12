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

int cmd_bml_restore_raw_partition(const char *partition, const char *filename)
{
    printf("bml restore\n");
    int fd = open("/dev/block/bml7", O_RDWR | O_LARGEFILE);
    return ioctl(fd, BML_UNLOCK_ALL, 0);
    
    char tmp[PATH_MAX];
    sprintf("dd if=%s of=/dev/block/bml7 bs=4096", filename);
    return __system(tmp);
}

int cmd_bml_backup_raw_partition(const char *partition, const char *filename)
{
    char tmp[PATH_MAX];
    sprintf("dd of=%s if=/dev/block/bml7 bs=4096", filename);
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
