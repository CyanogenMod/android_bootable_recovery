#ifndef __LOKI_H_
#define __LOKI_H_

#define VERSION "2.1"

#define BOOT_MAGIC_SIZE 8
#define BOOT_NAME_SIZE 16
#define BOOT_ARGS_SIZE 512

#define BOOT_PARTITION      "/dev/block/platform/msm_sdcc.1/by-name/boot"
#define RECOVERY_PARTITION  "/dev/block/platform/msm_sdcc.1/by-name/recovery"
#define ABOOT_PARTITION     "/dev/block/platform/msm_sdcc.1/by-name/aboot"

#define PATTERN1 "\xf0\xb5\x8f\xb0\x06\x46\xf0\xf7"
#define PATTERN2 "\xf0\xb5\x8f\xb0\x07\x46\xf0\xf7"
#define PATTERN3 "\x2d\xe9\xf0\x41\x86\xb0\xf1\xf7"
#define PATTERN4 "\x2d\xe9\xf0\x4f\xad\xf5\xc6\x6d"
#define PATTERN5 "\x2d\xe9\xf0\x4f\xad\xf5\x21\x7d"
#define PATTERN6 "\x2d\xe9\xf0\x4f\xf3\xb0\x05\x46"

#define ABOOT_BASE_SAMSUNG 0x88dfffd8
#define ABOOT_BASE_LG 0x88efffd8
#define ABOOT_BASE_G2 0xf7fffd8

struct boot_img_hdr {
    unsigned char magic[BOOT_MAGIC_SIZE];
    unsigned kernel_size;   /* size in bytes */
    unsigned kernel_addr;   /* physical load addr */
    unsigned ramdisk_size;  /* size in bytes */
    unsigned ramdisk_addr;  /* physical load addr */
    unsigned second_size;   /* size in bytes */
    unsigned second_addr;   /* physical load addr */
    unsigned tags_addr;     /* physical addr for kernel tags */
    unsigned page_size;     /* flash page size we assume */
    unsigned dt_size;       /* device_tree in bytes */
    unsigned unused;        /* future expansion: should be 0 */
    unsigned char name[BOOT_NAME_SIZE];    /* asciiz product name */
    unsigned char cmdline[BOOT_ARGS_SIZE];
    unsigned id[8];         /* timestamp / checksum / sha1 / etc */
};

struct loki_hdr {
    unsigned char magic[4];     /* 0x494b4f4c */
    unsigned int recovery;      /* 0 = boot.img, 1 = recovery.img */
    char build[128];   /* Build number */

    unsigned int orig_kernel_size;
    unsigned int orig_ramdisk_size;
    unsigned int ramdisk_addr;
};

int loki_patch(const char* partition_label, const char* aboot_image, const char* in_image, const char* out_image);
int loki_flash(const char* partition_label, const char* loki_image);
int loki_find(const char* aboot_image);

#endif //__LOKI_H_
