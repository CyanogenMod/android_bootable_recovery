/*
 * loki_patch
 *
 * A utility to patch unsigned boot and recovery images to make
 * them suitable for booting on the AT&T/Verizon Samsung
 * Galaxy S4, Galaxy Stellar, and various locked LG devices
 *
 * by Dan Rosenberg (@djrbliss)
 * modified for use in recovery by Seth Shelnutt
 *
 */
#include <stdio.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include "compact_loki.h"
#include "common.h"

#define VERSION "2.0"

#define BOOT_MAGIC_SIZE 8
#define BOOT_NAME_SIZE 16
#define BOOT_ARGS_SIZE 512

struct boot_img_hdr {
	unsigned char magic[BOOT_MAGIC_SIZE];
	unsigned kernel_size;	/* size in bytes */
	unsigned kernel_addr;	/* physical load addr */
	unsigned ramdisk_size;	/* size in bytes */
	unsigned ramdisk_addr;	/* physical load addr */
	unsigned second_size;	/* size in bytes */
	unsigned second_addr;	/* physical load addr */
	unsigned tags_addr;		/* physical addr for kernel tags */
	unsigned page_size;		/* flash page size we assume */
	unsigned dt_size;		/* device_tree in bytes */
	unsigned unused;		/* future expansion: should be 0 */
	unsigned char name[BOOT_NAME_SIZE];	/* asciiz product name */
	unsigned char cmdline[BOOT_ARGS_SIZE];
	unsigned id[8];			/* timestamp / checksum / sha1 / etc */
};

struct loki_hdr {
	unsigned char magic[4];		/* 0x494b4f4c */
	unsigned int recovery;		/* 0 = boot.img, 1 = recovery.img */
	unsigned char build[128];	/* Build number */
	unsigned int orig_kernel_size;
	unsigned int orig_ramdisk_size;
	unsigned int ramdisk_addr;
};

struct target {
	char *vendor;
	char *device;
	char *build;
	unsigned long check_sigs;
	unsigned long hdr;
	int lg;
};

struct target targets[] = {
	{
		.vendor = "AT&T",
		.device = "Samsung Galaxy S4",
		.build = "JDQ39.I337UCUAMDB or JDQ39.I337UCUAMDL",
		.check_sigs = 0x88e0ff98,
		.hdr = 0x88f3bafc,
		.lg = 0,
	},
	{
		.vendor = "Verizon",
		.device = "Samsung Galaxy S4",
		.build = "JDQ39.I545VRUAMDK",
		.check_sigs = 0x88e0fe98,
		.hdr = 0x88f372fc,
		.lg = 0,
	},
	{
		.vendor = "DoCoMo",
		.device = "Samsung Galaxy S4",
		.build = "JDQ39.SC04EOMUAMDI",
		.check_sigs = 0x88e0fcd8,
		.hdr = 0x88f0b2fc,
		.lg = 0,
	},
	{
		.vendor = "Verizon",
		.device = "Samsung Galaxy Stellar",
		.build = "IMM76D.I200VRALH2",
		.check_sigs = 0x88e0f5c0,
		.hdr = 0x88ed32e0,
		.lg = 0,
	},
	{
		.vendor = "Verizon",
		.device = "Samsung Galaxy Stellar",
		.build = "JZO54K.I200VRBMA1",
		.check_sigs = 0x88e101ac,
		.hdr = 0x88ed72e0,
		.lg = 0,
	},
	{
		.vendor = "DoCoMo",
		.device = "LG Optimus G",
		.build = "L01E20b",
		.check_sigs = 0x88F10E48,
		.hdr = 0x88F54418,
		.lg = 1,
	},
	{
		.vendor = "AT&T or HK",
		.device = "LG Optimus G Pro",
		.build = "E98010g or E98810b",
		.check_sigs = 0x88f11084,
		.hdr = 0x88f54418,
		.lg = 1,
	},
	{
		.vendor = "KT, LGU, or SKT",
		.device = "LG Optimus G Pro",
		.build = "F240K10o, F240L10v, or F240S10w",
		.check_sigs = 0x88f110b8,
		.hdr = 0x88f54418,
		.lg = 1,
	},
	{
		.vendor = "KT, LGU, or SKT",
		.device = "LG Optimus LTE 2",
		.build = "F160K20g, F160L20f, F160LV20d, or F160S20f",
		.check_sigs = 0x88f10864,
		.hdr = 0x88f802b8,
		.lg = 1,
	},
	{
		.vendor = "MetroPCS",
		.device = "LG Spirit",
		.build = "MS87010a_05",
		.check_sigs = 0x88f0e634,
		.hdr = 0x88f68194,
		.lg = 1,
	},
	{
		.vendor = "MetroPCS",
		.device = "LG Motion",
		.build = "MS77010f_01",
		.check_sigs = 0x88f1015c,
		.hdr = 0x88f58194,
		.lg = 1,
	},
	{
		.vendor = "Verizon",
		.device = "LG Lucid 2",
		.build = "VS87010B_12",
		.check_sigs = 0x88f10adc,
		.hdr = 0x88f702bc,
		.lg = 1,
	},
	{
		.vendor = "Verizon",
		.device = "LG Spectrum 2",
		.build = "VS93021B_05",
		.check_sigs = 0x88f10c10,
		.hdr = 0x88f84514,
		.lg = 1,
	},
	{
		.vendor = "Boost Mobile",
		.device = "LG Optimus F7",
		.build = "LG870ZV4_06",
		.check_sigs = 0x88f11714,
		.hdr = 0x88f842ac,
		.lg = 1,
	},
	{
		.vendor = "Virgin Mobile",
		.device = "LG Optimus F3",
		.build = "LS720ZV5",
		.check_sigs = 0x88f108f0,
		.hdr = 0x88f854f4,
		.lg = 1,
	},
};

#define PATTERN1 "\xf0\xb5\x8f\xb0\x06\x46\xf0\xf7"
#define PATTERN2 "\xf0\xb5\x8f\xb0\x07\x46\xf0\xf7"
#define PATTERN3 "\x2d\xe9\xf0\x41\x86\xb0\xf1\xf7"
#define PATTERN4 "\x2d\xe9\xf0\x4f\xad\xf5\xc6\x6d"
#define PATTERN5 "\x2d\xe9\xf0\x4f\xad\xf5\x21\x7d"
#define PATTERN6 "\x2d\xe9\xf0\x4f\xf3\xb0\x05\x46"

#define ABOOT_BASE_SAMSUNG 0x88dfffd8
#define ABOOT_BASE_LG 0x88efffd8
#define ABOOT_BASE_G2 0xf7fffd8

unsigned char patch[] =
"\xfe\xb5"
"\x0b\x4d"
"\xa8\x6a"
"\xab\x68"
"\x98\x42"
"\x0e\xd0"
"\xee\x69"
"\x09\x4c"
"\xef\x6a"
"\x07\xf5\x80\x57"
"\x0f\xce"
"\x0f\xc4"
"\x10\x3f"
"\xfb\xdc"
"\xa8\x6a"
"\x04\x49"
"\xea\x6a"
"\xa8\x60"
"\x69\x61"
"\x2a\x61"
"\x00\x20"
"\xfe\xbd"
"\x00\x00"
"\xff\xff\xff\xff"		/* Replace with header address */
"\x00\x00\x20\x82";

int loki_patch_shellcode(unsigned int addr)
{

	unsigned int i;
	unsigned int *ptr;

	for (i = 0; i < sizeof(patch); i++) {
		ptr = (unsigned int *)&patch[i];
		if (*ptr == 0xffffffff) {
			*ptr = addr;
			return 0;
		}
		else if (*ptr == addr) {
			return 0;
		}
	}

	return -1;
}

int loki_patch(char *partition, char *partitionPath)
{

	int ifd, ofd, aboot_fd, pos, i, recovery, offset, fake_size;
	unsigned int orig_ramdisk_size, orig_kernel_size, page_kernel_size, page_ramdisk_size, page_size, page_mask;
	unsigned long target, aboot_base;
	void *orig, *aboot, *ptr;
	struct target *tgt;
	struct stat st;
	struct boot_img_hdr *hdr;
	struct loki_hdr *loki_hdr;
	char *buf;

	if (!strcmp(partition, "boot")) {
		recovery = 0;
	} else if (!strcmp(partition, "recovery")) {
		recovery = 1;
	} else {
		ui_print("[+] First argument must be \"boot\" or \"recovery\".\n");
		return 1;
	}

	/* Open input files */
	aboot_fd = open("/dev/block/platform/msm_sdcc.1/by-name/aboot", O_RDONLY);
	if (aboot_fd < 0) {
		ui_print("[-] Failed to open %s for reading.\n", "/dev/block/platform/msm_sdcc.1/by-name/aboot");
		return 1;
	}

	ifd = open(partitionPath, O_RDONLY);
	if (ifd < 0) {
		ui_print("[-] Failed to open %s for reading.\n", partitionPath);
		return 1;
	}

	ofd = open(Loki_Image, O_WRONLY|O_CREAT|O_TRUNC, 0644);
	if (ofd < 0) {
		ui_print("[-] Failed to open %s for writing.\n", Loki_Image);
		return 1;
	}

	/* Find the signature checking function via pattern matching */
	if (fstat(aboot_fd, &st)) {
		ui_print("[-] fstat() failed.\n");
		return 1;
	}

	/* Verify the to-be-patched address matches the known code pattern */
	aboot = mmap(0, (524288 + 0xfff) & ~0xfff, PROT_READ, MAP_PRIVATE, aboot_fd, 0);
	if (aboot == MAP_FAILED) {
		printf("[-] Failed to mmap aboot.\n");
		return 1;
	}

	target = 0;

	for (ptr = aboot; ptr < aboot + 524288 - 0x1000; ptr++) {
		if (!memcmp(ptr, PATTERN1, 8) ||
			!memcmp(ptr, PATTERN2, 8) ||
			!memcmp(ptr, PATTERN3, 8)) {

			aboot_base = ABOOT_BASE_SAMSUNG;
			target = (unsigned long)ptr - (unsigned long)aboot + aboot_base;
			break;
		}

		if (!memcmp(ptr, PATTERN4, 8)) {

			aboot_base = ABOOT_BASE_LG;
			target = (unsigned long)ptr - (unsigned long)aboot + aboot_base;
			break;
		}
	}

	if (!target) {
		ui_print("[-] Failed to find function to patch.\n");
		return 1;
	}

	tgt = NULL;

	for (i = 0; i < (sizeof(targets)/sizeof(targets[0])); i++) {
		if (targets[i].check_sigs == target) {
			tgt = &targets[i];
			break;
		}
	}

	if (!tgt) {
		ui_print("[-] Unsupported aboot image.\n");
		return 1;
	}

	if (loki_patch_shellcode(tgt->hdr) < 0) {
		ui_print("[-] Failed to patch shellcode.\n");
		return 1;
	}

	/* Map the original boot/recovery image */
	if (fstat(ifd, &st)) {
		ui_print("[-] fstat() failed.\n");
		return 1;
	}

	orig = mmap(0, (25165824 + 0x2000 + 0xfff) & ~0xfff, PROT_READ|PROT_WRITE, MAP_PRIVATE, ifd, 0);
	if (orig == MAP_FAILED) {
		ui_print("[-] Failed to mmap input file.\n");
		return 1;
	}

	hdr = orig;
	loki_hdr = orig + 0x400;

	if (!memcmp(loki_hdr->magic, "LOKI", 4)) {
		ui_print("[-] Input file is already a Loki image.\n");

		/* Copy the entire file to the output transparently */
		if (write(ofd, orig, 25165824) != 25165824) {
			ui_print("[-] Failed to copy Loki image.\n");
			return 1;
		}

		ui_print("[+] Copied Loki image to %s.\n", Loki_Image);

		return 0;
	}

	/* Set the Loki header */
	memcpy(loki_hdr->magic, "LOKI", 4);
	loki_hdr->recovery = recovery;
	strncpy(loki_hdr->build, tgt->build, sizeof(loki_hdr->build) - 1);

	page_size = hdr->page_size;
	page_mask = hdr->page_size - 1;

	orig_kernel_size = hdr->kernel_size;
	orig_ramdisk_size = hdr->ramdisk_size;

	/* Store the original values in uses fields of the header */
	hdr->dt_size = orig_kernel_size;
	hdr->unused = orig_ramdisk_size;
	hdr->second_addr = hdr->kernel_addr + ((hdr->kernel_size + page_mask) & ~page_mask);

	/* Ramdisk must be aligned to a page boundary */
	hdr->kernel_size = ((hdr->kernel_size + page_mask) & ~page_mask) + hdr->ramdisk_size;

	/* Guarantee 16-byte alignment */
	offset = tgt->check_sigs & 0xf;

	hdr->ramdisk_addr = tgt->check_sigs - offset;

	if (tgt->lg) {
		fake_size = page_size;
		hdr->ramdisk_size = page_size;
	}
	else {
		fake_size = 0x200;
		hdr->ramdisk_size = 0;
	}

	/* Write the image header */
	if (write(ofd, orig, page_size) != page_size) {
		ui_print("[-] Failed to write header to output file.\n");
		return 1;
	}

	page_kernel_size = (orig_kernel_size + page_mask) & ~page_mask;

	/* Write the kernel */
	if (write(ofd, orig + page_size, page_kernel_size) != page_kernel_size) {
		ui_print("[-] Failed to write kernel to output file.\n");
		return 1;
	}

	page_ramdisk_size = (orig_ramdisk_size + page_mask) & ~page_mask;

	/* Write the ramdisk */
	if (write(ofd, orig + page_size + page_kernel_size, page_ramdisk_size) != page_ramdisk_size) {
		ui_print("[-] Failed to write ramdisk to output file.\n");
		return 1;
	}

	/* Write fake_size bytes of original code to the output */
	buf = malloc(fake_size);
	if (!buf) {
		ui_print("[-] Out of memory.\n");
		return 1;
	}

	lseek(aboot_fd, tgt->check_sigs - aboot_base - offset, SEEK_SET);
	read(aboot_fd, buf, fake_size);

	if (write(ofd, buf, fake_size) != fake_size) {
		ui_print("[-] Failed to write original aboot code to output file.\n");
		return 1;
	}

	pos = lseek(ofd, 0, SEEK_CUR);
	lseek(ofd, pos - (fake_size - offset), SEEK_SET);

	/* Write the patch */
	if (write(ofd, patch, sizeof(patch)) != sizeof(patch)) {
		ui_print("[-] Failed to write patch to output file.\n");
		return 1;
	}

	close(ifd);
	close(ofd);
	close(aboot_fd);

	return 0;
}

int loki_check(){
    if(loki_check_partition(BOOT_PARTITION)){
        if(loki_patch("boot","/dev/block/platform/msm_sdcc.1/by-name/boot")){
             ui_set_background(BACKGROUND_ICON_ERROR);
             ui_print("Error loki-ifying the boot image.\n");
             return 1;
        }
        if(loki_flash("/dev/block/platform/msm_sdcc.1/by-name/boot")){
             ui_set_background(BACKGROUND_ICON_ERROR);
             ui_print("Error loki-flashing the boot image.\n");
             return 1;
        }
    }
    if(loki_check_partition(RECOVERY_PARTITION)){
        if(loki_patch("recovery","/dev/block/platform/msm_sdcc.1/by-name/recovery")){
             ui_set_background(BACKGROUND_ICON_ERROR);
             ui_print("Error loki-ifying the recovery image.\n");
             return 1;
        }
        if(loki_flash("/dev/block/platform/msm_sdcc.1/by-name/recovery")){
             ui_set_background(BACKGROUND_ICON_ERROR);
             ui_print("Error loki-flashing the recovery image.\n");
             return 1;
        }
    }

    return 0;
}

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
		ui_print("[-] Failed to open %s for reading.\n", partition);
		return 1;
	}

	/* Map the image to be flashed */
	if (fstat(ifd, &st)) {
		ui_print("[-] fstat() failed.\n");
		return 1;
	}

	orig = mmap(0, (25165824 + 0x2000 + 0xfff) & ~0xfff, PROT_READ, MAP_PRIVATE, ifd, 0);
	if (orig == MAP_FAILED) {
		ui_print("[-] Failed to mmap Loki image.\n");
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

int loki_flash(char *partition)
{

	int ifd, aboot_fd, ofd, recovery, offs, match;
	void *orig, *aboot, *patch;
	struct stat st;
	struct boot_img_hdr *hdr;
	struct loki_hdr *loki_hdr;
	char prop[256], outfile[1024], buf[4096];


	if (!strcmp(partition, "/dev/block/platform/msm_sdcc.1/by-name/boot")) {
		recovery = 0;
	} else if (!strcmp(partition, "/dev/block/platform/msm_sdcc.1/by-name/recovery")) {
		recovery = 1;
	} else {
		ui_print("[+] First argument must be \"boot\" or \"recovery\".\n");
		return 1;
	}

	/* Verify input file */
	aboot_fd = open("/dev/block/platform/msm_sdcc.1/by-name/aboot", O_RDONLY);
	if (aboot_fd < 0) {
		ui_print("[-] Failed to open aboot for reading.\n");
		return 1;
	}

	ifd = open(Loki_Image, O_RDONLY);
	if (ifd < 0) {
		ui_print("[-] Failed to open %s for reading.\n", Loki_Image);
		return 1;
	}

	/* Map the image to be flashed */
	if (fstat(ifd, &st)) {
		ui_print("[-] fstat() failed.\n");
		return 1;
	}

	orig = mmap(0, (25165824 + 0x2000 + 0xfff) & ~0xfff, PROT_READ, MAP_PRIVATE, ifd, 0);
	if (orig == MAP_FAILED) {
		ui_print("[-] Failed to mmap Loki image.\n");
		return 1;
	}

	hdr = orig;
	loki_hdr = orig + 0x400;

	/* Verify this is a Loki image */
	if (memcmp(loki_hdr->magic, "LOKI", 4)) {
		ui_print("[-] Input file is not a Loki image.\n");
		return 1;
	}

	/* Verify this is the right type of image */
	if (loki_hdr->recovery != recovery) {
		ui_print("[-] Loki image is not a %s image.\n", recovery ? "recovery" : "boot");
		return 1;
	}

	/* Verify the to-be-patched address matches the known code pattern */
	aboot = mmap(0, 0x40000, PROT_READ, MAP_PRIVATE, aboot_fd, 0);
	if (aboot == MAP_FAILED) {
		ui_print("[-] Failed to mmap aboot.\n");
		return 1;
	}

	match = 0;

	for (offs = 0; offs < 0x10; offs += 0x4) {

		if (hdr->ramdisk_addr < ABOOT_BASE_SAMSUNG)
			patch = hdr->ramdisk_addr - ABOOT_BASE_G2 + aboot + offs;
		else if (hdr->ramdisk_addr < ABOOT_BASE_LG)
			patch = hdr->ramdisk_addr - ABOOT_BASE_SAMSUNG + aboot + offs;
		else
			patch = hdr->ramdisk_addr - ABOOT_BASE_LG + aboot + offs;

		if (patch < aboot || patch > aboot + 0x40000 - 8) {
			ui_print("[-] Invalid .lok file.\n");
			return 1;
		}

		if (!memcmp(patch, PATTERN1, 8) ||
			!memcmp(patch, PATTERN2, 8) ||
			!memcmp(patch, PATTERN3, 8) ||
			!memcmp(patch, PATTERN4, 8) ||
			!memcmp(patch, PATTERN5, 8) ||
			!memcmp(patch, PATTERN6, 8)) {

			match = 1;
			break;
		}
	}

	if (!match) {
		ui_print("[-] Loki aboot version does not match device.\n");
		return 1;
	}

	ui_print("[+] Loki validation passed, flashing image.\n");

	snprintf(outfile, sizeof(outfile),
			 "/dev/block/platform/msm_sdcc.1/by-name/%s",
			 recovery ? "recovery" : "boot");

	ofd = open(outfile, O_WRONLY);
	if (ofd < 0) {
		ui_print("[-] Failed to open output block device.\n");
		return 1;
	}

	if (write(ofd, orig, st.st_size) != st.st_size) {
		ui_print("[-] Failed to write to block device.\n");
		return 1;
	}

	ui_print("[+] Loki flashing complete!\n");

	close(ifd);
	close(aboot_fd);
	close(ofd);

	return 0;
}

