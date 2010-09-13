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

int write_raw_image(const char* partition, const char* filename) {
    char tmp[PATH_MAX];
    if (0 != strcmp("boot", partition)) {
        return -1;
    }
    sprintf(tmp, "/sbin/redbend_ua restore %s %s", filename, BOARD_BOOT_DEVICE);
    return __system(tmp);
}

int read_raw_image(const char* partition, const char* filename) {
    char tmp[PATH_MAX];
    sprintf(tmp, "dd if=/dev/block/bml7 of=%s", filename);
    return __system(tmp);
}
