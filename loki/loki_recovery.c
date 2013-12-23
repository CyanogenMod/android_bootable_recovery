/*
 * loki_patch
 *
 * A utility to patch unsigned boot and recovery images to make
 * them suitable for booting on the AT&T/Verizon Samsung
 * Galaxy S4, Galaxy Stellar, and various locked LG devices
 *
 * by Dan Rosenberg (@djrbliss)
 * modified for use in recovery by Seth Shelnutt, adapted by PhilZ
 *
 */

#include <stdio.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include "loki.h"
#include "../common.h"

int loki_check_partition(char *partition)
{
    int ifd;
    void *orig;
    struct stat st;
    struct boot_img_hdr *hdr;
    struct loki_hdr *loki_hdr;
    char *buf;

    ifd = open(partition, O_RDONLY);
    if (ifd < 0) {
        LOGE("[-] Failed to open %s for reading.\n", partition);
        return 1;
    }

    /* Map the image to be flashed */
    if (fstat(ifd, &st)) {
        LOGE("[-] fstat() failed.\n");
        return 1;
    }

    orig = mmap(0, (st.st_size + 0x2000 + 0xfff) & ~0xfff, PROT_READ, MAP_PRIVATE, ifd, 0);
    if (orig == MAP_FAILED) {
        LOGE("[-] Failed to mmap Loki image.\n");
        return 1;
    }

    hdr = orig;
    loki_hdr = orig + 0x400;

    /* Verify this is a Loki image */
    if (memcmp(loki_hdr->magic, "LOKI", 4)) {
        ui_print("%s needs lokifying.\n", partition);
        return 1;
    }
    else {
        return 0;
    }
}

int loki_check()
{
    int ret = 0;
    if (loki_check_partition(BOOT_PARTITION))
    {
        if ((ret = loki_patch("boot", BOOT_PARTITION)) != 0) {
            LOGE("Error loki-ifying the boot image.\n");
        }
        else if ((ret = loki_flash("boot")) != 0) {
            LOGE("Error loki-flashing the boot image.\n");
        }
    }

    if (!ret && loki_check_partition(RECOVERY_PARTITION))
    {
        if ((ret = loki_patch("recovery", RECOVERY_PARTITION)) != 0) {
            LOGE("Error loki-ifying the recovery image.\n");
        }
        else if ((ret = loki_flash("recovery")) != 0) {
            LOGE("Error loki-flashing the recovery image.\n");
        }
    }

    return ret;
}
