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
#include <limits.h>
#include <stdio.h>
#include "loki.h"
#include "../common.h"
#include "../libcrecovery/common.h"    // __system


#define ABOOT_DUMP_IMAGE    "/tmp/loki_aboot-dump.img"
#define BOOT_DUMP_IMAGE     "/tmp/loki_boot-dump.img"
#define RECOVERY_DUMP_IMAGE "/tmp/loki_recovery-dump.img"
#define LOKI_IMAGE          "/tmp/loki_lokied-image"

static int loki_dump_partitions() {
    char cmd[PATH_MAX];
    int ifd;

    const char *target_partitions[] = { ABOOT_PARTITION, BOOT_PARTITION, RECOVERY_PARTITION, NULL };
    const char *dump_images[] = { ABOOT_DUMP_IMAGE, BOOT_DUMP_IMAGE, RECOVERY_DUMP_IMAGE, NULL };
    int i = 0;

    ui_print("[+] Dumping partitions...\n");
    while (target_partitions[i] != NULL) {
        sprintf(cmd, "dd if=%s of=%s", target_partitions[i], dump_images[i]);
        if (__system(cmd) != 0) {
            LOGE("[-] Failed to dump %s\n", target_partitions[i]);
            return 1;
        }
        ++i;
    }

    return 0;
}

static int needs_loki_patch(const char *partition_image) {
    int ifd;
    void *orig;
    struct stat st;
    struct boot_img_hdr *hdr;
    struct loki_hdr *loki_hdr;
    char *buf;

    ifd = open(partition_image, O_RDONLY);
    if (ifd < 0) {
        LOGE("[-] Failed to open %s for reading.\n", partition_image);
        return 0;
    }

    /* Map the image to be flashed */
    if (fstat(ifd, &st)) {
        LOGE("[-] fstat() failed.\n");
        return 0;
    }

    orig = mmap(0, (st.st_size + 0x2000 + 0xfff) & ~0xfff, PROT_READ, MAP_PRIVATE, ifd, 0);
    if (orig == MAP_FAILED) {
        LOGE("[-] Failed to mmap Loki image.\n");
        return 0;
    }

    hdr = orig;
    loki_hdr = orig + 0x400;

    /* Verify this is a Loki image */
    if (memcmp(loki_hdr->magic, "LOKI", 4)) {
        ui_print("%s needs lokifying.\n", partition_image);
        return 1;
    }

    return 0;
}

int loki_check() {
    const char *target_partitions[] = { "boot", "recovery", NULL };
    const char *dump_images[] = { BOOT_DUMP_IMAGE, RECOVERY_DUMP_IMAGE, NULL };
    int i = 0;
    int ret = 0;

    ret = loki_dump_partitions();
    while (!ret && target_partitions[i] != NULL) {
        if (needs_loki_patch(dump_images[i])) {
            if ((ret = loki_patch(target_partitions[i], ABOOT_DUMP_IMAGE, dump_images[i], LOKI_IMAGE)) != 0)
                LOGE("Error loki-ifying the %s image.\n", target_partitions[i]);
            else if ((ret = loki_flash(target_partitions[i], LOKI_IMAGE)) != 0)
                LOGE("Error loki-flashing the %s image.\n", target_partitions[i]);
        }
        ++ i;
    }

    // free the ramdisk
    remove(ABOOT_DUMP_IMAGE);
    remove(BOOT_DUMP_IMAGE);
    remove(RECOVERY_DUMP_IMAGE);

    return ret;
}
