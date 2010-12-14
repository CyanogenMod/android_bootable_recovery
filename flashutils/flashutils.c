#include <signal.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>

#include "flashutils/flashutils.h"

int the_flash_type = UNKNOWN;

int device_flash_type()
{
    if (the_flash_type == UNKNOWN) {
        if (access("/dev/block/bml1", F_OK) == 0) {
            the_flash_type = BML;
        } else if (access("/proc/emmc", F_OK) == 0) {
            the_flash_type = MMC;
        } else if (access("/proc/mtd", F_OK) == 0) {
            the_flash_type = MTD;
        } else {
            the_flash_type = UNSUPPORTED;
        }
    }
    return the_flash_type;
}

char* get_default_filesystem()
{
    return device_flash_type() == MMC ? "ext3" : "yaffs2";
}

// This was pulled from bionic: The default system command always looks
// for shell in /system/bin/sh. This is bad.
#define _PATH_BSHELL "/sbin/sh"

extern char **environ;
int
__system(const char *command)
{
  pid_t pid;
    sig_t intsave, quitsave;
    sigset_t mask, omask;
    int pstat;
    char *argp[] = {"sh", "-c", NULL, NULL};

    if (!command)        /* just checking... */
        return(1);

    argp[2] = (char *)command;

    sigemptyset(&mask);
    sigaddset(&mask, SIGCHLD);
    sigprocmask(SIG_BLOCK, &mask, &omask);
    switch (pid = vfork()) {
    case -1:            /* error */
        sigprocmask(SIG_SETMASK, &omask, NULL);
        return(-1);
    case 0:                /* child */
        sigprocmask(SIG_SETMASK, &omask, NULL);
        execve(_PATH_BSHELL, argp, environ);
    _exit(127);
  }

    intsave = (sig_t)  bsd_signal(SIGINT, SIG_IGN);
    quitsave = (sig_t) bsd_signal(SIGQUIT, SIG_IGN);
    pid = waitpid(pid, (int *)&pstat, 0);
    sigprocmask(SIG_SETMASK, &omask, NULL);
    (void)bsd_signal(SIGINT, intsave);
    (void)bsd_signal(SIGQUIT, quitsave);
    return (pid == -1 ? -1 : pstat);
}

int restore_raw_partition(const char *partition, const char *filename)
{
    int type = device_flash_type();
    switch (type) {
        case MTD:
            return cmd_mtd_restore_raw_partition(partition, filename);
        case MMC:
            return cmd_mmc_restore_raw_partition(partition, filename);
        case BML:
            return cmd_bml_restore_raw_partition(partition, filename);
        default:
            return -1;
    }
}

int backup_raw_partition(const char *partition, const char *filename)
{
    int type = device_flash_type();
    switch (type) {
        case MTD:
            return cmd_mtd_backup_raw_partition(partition, filename);
        case MMC:
            return cmd_mmc_backup_raw_partition(partition, filename);
        case BML:
            return cmd_bml_backup_raw_partition(partition, filename);
        default:
            return -1;
    }
}

int erase_raw_partition(const char *partition)
{
    int type = device_flash_type();
    switch (type) {
        case MTD:
            return cmd_mtd_erase_raw_partition(partition);
        case MMC:
            return cmd_mmc_erase_raw_partition(partition);
        case BML:
            return cmd_bml_erase_raw_partition(partition);
        default:
            return -1;
    }
}

int erase_partition(const char *partition, const char *filesystem)
{
    int type = device_flash_type();
    switch (type) {
        case MTD:
            return cmd_mtd_erase_partition(partition, filesystem);
        case MMC:
            return cmd_mmc_erase_partition(partition, filesystem);
        case BML:
            return cmd_bml_erase_partition(partition, filesystem);
        default:
            return -1;
    }
}

int mount_partition(const char *partition, const char *mount_point, const char *filesystem, int read_only)
{
    int type = device_flash_type();
    switch (type) {
        case MTD:
            return cmd_mtd_mount_partition(partition, mount_point, filesystem, read_only);
        case MMC:
            return cmd_mmc_mount_partition(partition, mount_point, filesystem, read_only);
        case BML:
            return cmd_bml_mount_partition(partition, mount_point, filesystem, read_only);
        default:
            return -1;
    }
}

int get_partition_device(const char *partition, char *device)
{
    int type = device_flash_type();
    switch (type) {
        case MTD:
            return cmd_mtd_get_partition_device(partition, device);
        case MMC:
            return cmd_mmc_get_partition_device(partition, device);
        case BML:
            return cmd_bml_get_partition_device(partition, device);
        default:
            return -1;
    }
}
