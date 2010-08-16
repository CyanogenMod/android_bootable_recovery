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


#define RECOVERY_MODE_FILE "/cache/.recovery_mode"
#define UPDATE_BINARY BOARD_HIJACK_RECOVERY_PATH"/update-binary"
#define UPDATE_PACKAGE BOARD_HIJACK_RECOVERY_PATH"/recovery.zip"
#define BUSYBOX BOARD_HIJACK_RECOVERY_PATH"/busybox"
#define PREPARE_SCRIPT BOARD_HIJACK_RECOVERY_PATH"/prepare.sh"

// This was pulled from bionic: The default system command always looks
// for shell in /system/bin/sh. This is bad.
#define _PATH_BSHELL BOARD_HIJACK_RECOVERY_PATH"/sh"

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

int main(int argc, char** argv) {
    char* hijacked_executable = argv[0];
    struct stat info;
    
    if (NULL != strstr(hijacked_executable, "hijack")) {
        // no op
        printf("Hijack!\n");
        return 0;
    }

    char cmd[PATH_MAX];
    if (0 == stat(RECOVERY_MODE_FILE, &info)) {
        remove(RECOVERY_MODE_FILE);
        kill(getppid(), SIGKILL);
        sprintf(cmd, "%s 2 0 %s", UPDATE_BINARY, UPDATE_PACKAGE);
        return __system(cmd);
    }
    
    sprintf(cmd, "%s.bin", hijacked_executable);
    int i;
    for (i = 1; i < argc; i++) {
        strcat(cmd, " '");
        strcat(cmd, argv[i]);
        strcat(cmd, "'");
    }
    
    __system(PREPARE_SCRIPT);
    return __system(cmd);
}
