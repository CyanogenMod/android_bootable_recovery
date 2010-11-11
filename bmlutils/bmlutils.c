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

int
__system(const char *command);

int restore_raw_partition(const char *partition, const char *filename)
{
    char tmp[PATH_MAX];
    sprintf("dd if=%s of=/dev/block/bml7 bs=4096", filename);
    return __system(tmp);
}

int backup_raw_partition(const char *partition, const char *filename)
{
    char tmp[PATH_MAX];
    sprintf("dd of=%s if=/dev/block/bml7 bs=4096", filename);
    return __system(tmp);
}

int erase_raw_partition(const char *partition)
{
    // TODO: implement raw wipe
    return 0;
}

int erase_partition(const char *partition, const char *filesystem)
{
    return -1;
}

int mount_partition(const char *partition, const char *mount_point, const char *filesystem, int read_only)
{
    return -1;
}

int get_partition_device(const char *partition, char *device)
{
    return -1;
}
