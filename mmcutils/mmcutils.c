/*
 * Copyright (c) 2010, Code Aurora Forum. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above
 *     copyright notice, this list of conditions and the following
 *     disclaimer in the documentation and/or other materials provided
 *     with the distribution.
 *   * Neither the name of Code Aurora Forum, Inc. nor the names of its
 *     contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
 * IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <dirent.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/reboot.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/mount.h>  // for _IOW, _IOR, mount()

#include "mmcutils.h"

unsigned ext3_count = 0;
char *ext3_partitions[] = {"system", "userdata", "cache", "NONE"};

unsigned vfat_count = 0;
char *vfat_partitions[] = {"modem", "NONE"};

struct MmcPartition {
    char *device_index;
    char *filesystem;
    char *name;
    unsigned dstatus;
    unsigned dtype ;
    unsigned dfirstsec;
    unsigned dsize;
};

typedef struct {
    MmcPartition *partitions;
    int partitions_allocd;
    int partition_count;
} MmcState;

static MmcState g_mmc_state = {
    NULL,   // partitions
    0,      // partitions_allocd
    -1      // partition_count
};

#define MMC_DEVICENAME "/dev/block/mmcblk0"

static void
mmc_partition_name (MmcPartition *mbr, unsigned int type) {
    switch(type)
    {
        char name[64];
        case MMC_BOOT_TYPE:
            sprintf(name,"boot");
            mbr->name = strdup(name);
            break;
        case MMC_EXT3_TYPE:
            if (strcmp("NONE", ext3_partitions[ext3_count])) {
                strcpy((char *)name,(const char *)ext3_partitions[ext3_count]);
                mbr->name = strdup(name);
                ext3_count++;
            }
            mbr->filesystem = strdup("ext3");
            break;
        case MMC_VFAT_TYPE:
            if (strcmp("NONE", vfat_partitions[vfat_count])) {
                strcpy((char *)name,(const char *)vfat_partitions[vfat_count]);
                mbr->name = strdup(name);
                vfat_count++;
            }
            mbr->filesystem = strdup("vfat");
            break;
    };
}

static int
mmc_read_mbr (const char *device, MmcPartition *mbr) {
    FILE *fd;
    unsigned char buffer[512];
    int idx, i;
    unsigned mmc_partition_count = 0;
    unsigned int dtype;
    unsigned int dfirstsec;
    unsigned int EBR_first_sec;
    unsigned int EBR_current_sec;
    int ret = -1;

    fd = fopen(device, "r");
    if(fd == NULL)
    {
        printf("Can't open device: \"%s\"\n", device);
        goto ERROR2;
    }
    if ((fread(buffer, 512, 1, fd)) != 1)
    {
        printf("Can't read device: \"%s\"\n", device);
        goto ERROR1;
    }
    /* Check to see if signature exists */
    if ((buffer[TABLE_SIGNATURE] != 0x55) || \
        (buffer[TABLE_SIGNATURE + 1] != 0xAA))
    {
        printf("Incorrect mbr signatures!\n");
        goto ERROR1;
    }
    idx = TABLE_ENTRY_0;
    for (i = 0; i < 4; i++)
    {
        char device_index[128];

        mbr[mmc_partition_count].dstatus = \
                    buffer[idx + i * TABLE_ENTRY_SIZE + OFFSET_STATUS];
        mbr[mmc_partition_count].dtype   = \
                    buffer[idx + i * TABLE_ENTRY_SIZE + OFFSET_TYPE];
        mbr[mmc_partition_count].dfirstsec = \
                    GET_LWORD_FROM_BYTE(&buffer[idx + \
                                        i * TABLE_ENTRY_SIZE + \
                                        OFFSET_FIRST_SEC]);
        mbr[mmc_partition_count].dsize  = \
                    GET_LWORD_FROM_BYTE(&buffer[idx + \
                                        i * TABLE_ENTRY_SIZE + \
                                        OFFSET_SIZE]);
        dtype  = mbr[mmc_partition_count].dtype;
        dfirstsec = mbr[mmc_partition_count].dfirstsec;
        mmc_partition_name(&mbr[mmc_partition_count], \
                        mbr[mmc_partition_count].dtype);

        sprintf(device_index, "%sp%d", device, (mmc_partition_count+1));
        mbr[mmc_partition_count].device_index = strdup(device_index);

        mmc_partition_count++;
        if (mmc_partition_count == MAX_PARTITIONS)
            goto SUCCESS;
    }

    /* See if the last partition is EBR, if not, parsing is done */
    if (dtype != 0x05)
    {
        goto SUCCESS;
    }

    EBR_first_sec = dfirstsec;
    EBR_current_sec = dfirstsec;

    fseek (fd, (EBR_first_sec * 512), SEEK_SET);
    if ((fread(buffer, 512, 1, fd)) != 1)
        goto ERROR1;

    /* Loop to parse the EBR */
    for (i = 0;; i++)
    {
        char device_index[128];

        if ((buffer[TABLE_SIGNATURE] != 0x55) || (buffer[TABLE_SIGNATURE + 1] != 0xAA))
        {
            break;
        }
        mbr[mmc_partition_count].dstatus = \
                    buffer[TABLE_ENTRY_0 + OFFSET_STATUS];
        mbr[mmc_partition_count].dtype   = \
                    buffer[TABLE_ENTRY_0 + OFFSET_TYPE];
        mbr[mmc_partition_count].dfirstsec = \
                    GET_LWORD_FROM_BYTE(&buffer[TABLE_ENTRY_0 + \
                                        OFFSET_FIRST_SEC])    + \
                                        EBR_current_sec;
        mbr[mmc_partition_count].dsize = \
                    GET_LWORD_FROM_BYTE(&buffer[TABLE_ENTRY_0 + \
                                        OFFSET_SIZE]);
        mmc_partition_name(&mbr[mmc_partition_count], \
                        mbr[mmc_partition_count].dtype);

        sprintf(device_index, "%sp%d", device, (mmc_partition_count+1));
        mbr[mmc_partition_count].device_index = strdup(device_index);

        mmc_partition_count++;
        if (mmc_partition_count == MAX_PARTITIONS)
            goto SUCCESS;

        dfirstsec = GET_LWORD_FROM_BYTE(&buffer[TABLE_ENTRY_1 + OFFSET_FIRST_SEC]);
        if(dfirstsec == 0)
        {
            /* Getting to the end of the EBR tables */
            break;
        }
        /* More EBR to follow - read in the next EBR sector */
        fseek (fd,  ((EBR_first_sec + dfirstsec) * 512), SEEK_SET);
        if ((fread(buffer, 512, 1, fd)) != 1)
            goto ERROR1;

        EBR_current_sec = EBR_first_sec + dfirstsec;
    }

SUCCESS:
    ret = mmc_partition_count;
ERROR1:
    fclose(fd);
ERROR2:
    return ret;
}

int
mmc_scan_partitions() {
    int i;
    ssize_t nbytes;

    if (g_mmc_state.partitions == NULL) {
        const int nump = MAX_PARTITIONS;
        MmcPartition *partitions = malloc(nump * sizeof(*partitions));
        if (partitions == NULL) {
            errno = ENOMEM;
            return -1;
        }
        g_mmc_state.partitions = partitions;
        g_mmc_state.partitions_allocd = nump;
        memset(partitions, 0, nump * sizeof(*partitions));
    }
    g_mmc_state.partition_count = 0;
    ext3_count = 0;
    vfat_count = 0;

    /* Initialize all of the entries to make things easier later.
     * (Lets us handle sparsely-numbered partitions, which
     * may not even be possible.)
     */
    for (i = 0; i < g_mmc_state.partitions_allocd; i++) {
        MmcPartition *p = &g_mmc_state.partitions[i];
        if (p->device_index != NULL) {
            free(p->device_index);
            p->device_index = NULL;
        }
        if (p->name != NULL) {
            free(p->name);
            p->name = NULL;
        }
        if (p->filesystem != NULL) {
            free(p->filesystem);
            p->filesystem = NULL;
        }
    }

    g_mmc_state.partition_count = mmc_read_mbr(MMC_DEVICENAME, g_mmc_state.partitions);
    if(g_mmc_state.partition_count == -1)
    {
        printf("Error in reading mbr!\n");
        // keep "partitions" around so we can free the names on a rescan.
        g_mmc_state.partition_count = -1;
    }
    return g_mmc_state.partition_count;
}

const MmcPartition *
mmc_find_partition_by_name(const char *name)
{
    if (g_mmc_state.partitions != NULL) {
        int i;
        for (i = 0; i < g_mmc_state.partitions_allocd; i++) {
            MmcPartition *p = &g_mmc_state.partitions[i];
            if (p->device_index !=NULL && p->name != NULL) {
                if (strcmp(p->name, name) == 0) {
                    return p;
                }
            }
        }
    }
    return NULL;
}

#define MKE2FS_BIN      "/sbin/mke2fs_static"
#define TUNE2FS_BIN     "/sbin/tune2fs_static"
#define E2FSCK_BIN      "/sbin/e2fsck_static"

static int
run_exec_process ( char **argv) {
    pid_t pid;
    int status;
    pid = fork();
    if (pid == 0) {
        execv(argv[0], argv);
        fprintf(stderr, "E:Can't run (%s)\n",strerror(errno));
        _exit(-1);
    }

    waitpid(pid, &status, 0);
    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
        return 1;
    }
    return 0;
}

int
mmc_format_ext3 (MmcPartition *partition) {
    char device[128];
    strcpy(device, partition->device_index);
    // Run mke2fs
    char *const mke2fs[] = {MKE2FS_BIN, "-j", device, NULL};
    if(run_exec_process(mke2fs))
        return -1;

    // Run tune2fs
    char *const tune2fs[] = {TUNE2FS_BIN, "-C", "1", device, NULL};
    if(run_exec_process(tune2fs))
        return -1;

    // Run e2fsck
    char *const e2fsck[] = {E2FSCK_BIN, "-fy", device, NULL};
    if(run_exec_process(e2fsck))
        return -1;

    return 0;
}

int
mmc_mount_partition(const MmcPartition *partition, const char *mount_point,
        int read_only)
{
    const unsigned long flags = MS_NOATIME | MS_NODEV | MS_NODIRATIME;
    char devname[128];
    int rv = -1;
    strcpy(devname, partition->device_index);
    if (partition->filesystem == NULL) {
        printf("Null filesystem!\n");
        return rv;
    }
    if (!read_only) {
        rv = mount(devname, mount_point, partition->filesystem, flags, NULL);
    }
    if (read_only || rv < 0) {
        rv = mount(devname, mount_point, partition->filesystem, flags | MS_RDONLY, 0);
        if (rv < 0) {
            printf("Failed to mount %s on %s: %s\n",
                    devname, mount_point, strerror(errno));
        } else {
            printf("Mount %s on %s read-only\n", devname, mount_point);
        }
    }
    return rv;
}

int
mmc_raw_copy (const MmcPartition *partition, char *in_file) {
    int ch;
    FILE *in;
    FILE *out;
    int val = 0;
    char buf[512];
    unsigned sz = 0;
    unsigned i;
    int ret = -1;
    char *out_file = partition->device_index;

    in  = fopen ( in_file,  "r" );
    if (in == NULL)
        goto ERROR3;

    out = fopen ( out_file,  "w" );
    if (out == NULL)
        goto ERROR2;

    fseek(in, 0L, SEEK_END);
    sz = ftell(in);
    fseek(in, 0L, SEEK_SET);

    if (sz % 512)
    {
        while ( ( ch = fgetc ( in ) ) != EOF )
            fputc ( ch, out );
    }
    else
    {
        for (i=0; i< (sz/512); i++)
        {
            if ((fread(buf, 512, 1, in)) != 1)
                goto ERROR1;
            if ((fwrite(buf, 512, 1, out)) != 1)
                goto ERROR1;
        }
    }

    fsync(out);
    ret = 0;
ERROR1:
    fclose ( out );
ERROR2:
    fclose ( in );
ERROR3:
    return ret;

}

