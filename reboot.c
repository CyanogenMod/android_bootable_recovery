#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/reboot.h>
#include <unistd.h>
#include <cutils/properties.h>

int reboot_main(int argc, char *argv[])
{
    int ret;
    int nosync = 0;
    int poweroff = 0;
    int force = 0;

    opterr = 0;
    do {
        int c;

        c = getopt(argc, argv, "npf");
        
        if (c == EOF) {
            break;
        }
        
        switch (c) {
        case 'n':
            nosync = 1;
            break;
        case 'p':
            poweroff = 1;
            break;
    	case 'f':
	        force = 1;
	        break;
        case '?':
            fprintf(stderr, "usage: %s [-n] [-p] [rebootcommand]\n", argv[0]);
            exit(EXIT_FAILURE);
        }
    } while (1);

    if(argc > optind + 1) {
        fprintf(stderr, "%s: too many arguments\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    if(!nosync)
        sync();

    if(force || argc > optind) {
        if(poweroff)
            ret = __reboot(LINUX_REBOOT_MAGIC1, LINUX_REBOOT_MAGIC2, LINUX_REBOOT_CMD_POWER_OFF, NULL);
        else if(argc > optind)
            ret = __reboot(LINUX_REBOOT_MAGIC1, LINUX_REBOOT_MAGIC2, LINUX_REBOOT_CMD_RESTART2, argv[optind]);
        else
            ret = reboot(RB_AUTOBOOT);
    } else {
        if(poweroff) {
            property_set("ctl.start", "poweroff");
            ret = 0;
        } else {
            property_set("ctl.start", "reboot");
            ret = 0;
        }
    }

    if(ret < 0) {
        perror("reboot");
        exit(EXIT_FAILURE);
    }
    fprintf(stderr, "reboot returned\n");
    return 0;
}
