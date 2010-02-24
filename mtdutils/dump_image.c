/*
 * Copyright (C) 2008 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/ioctl.h>

#include "cutils/log.h"
#include "mtdutils.h"

#ifdef LOG_TAG
#undef LOG_TAG
#endif

#define LOG_TAG "dump_image"

#define BLOCK_SIZE	2048
#define SPARE_SIZE	(BLOCK_SIZE >> 5)

static void die(const char *msg, ...) {
    int err = errno;
    va_list args;
    va_start(args, msg);
    char buf[1024];
    vsnprintf(buf, sizeof(buf), msg, args);
    va_end(args);

    if (err != 0) {
        strlcat(buf, ": ", sizeof(buf));
        strlcat(buf, strerror(err), sizeof(buf));
    }

    fprintf(stderr, "%s\n", buf);
    exit(1);
}

/* Read a flash partition and write it to an image file. */

int main(int argc, char **argv)
{
    ssize_t (*read_func) (MtdReadContext *, char *, size_t);
    MtdReadContext *in;
    const MtdPartition *partition;
    char buf[BLOCK_SIZE + SPARE_SIZE];
    size_t partition_size;
    size_t read_size;
    size_t total;
    int fd;
    int wrote;
    int len;

    if (argc != 3) {
        fprintf(stderr, "usage: %s partition file.img\n", argv[0]);
        return 2;
    }

    if (mtd_scan_partitions() <= 0)
    	die("error scanning partitions");

    partition = mtd_find_partition_by_name(argv[1]);
    if (partition == NULL)
   	 die("can't find %s partition", argv[1]);

    if (mtd_partition_info(partition, &partition_size, NULL, NULL)) {
   	 die("can't get info of partition %s", argv[1]);
    }

    if (!strcmp(argv[2], "-")) {
	fd = fileno(stdout);
    } else {
	fd = open(argv[2], O_WRONLY|O_CREAT|O_TRUNC, 0666);
    }

    if (fd < 0)
    	die("error opening %s", argv[2]);

    in = mtd_read_partition(partition);
    if (in == NULL) {
    	close(fd);
	unlink(argv[2]);
        die("error opening %s: %s\n", argv[1], strerror(errno));
    }

    total = 0;
    while ((len = mtd_read_data(in, buf, BLOCK_SIZE)) > 0) {
        wrote = write(fd, buf, len);
        if (wrote != len) {
    		close(fd);
		unlink(argv[2]);
		die("error writing %s", argv[2]);
	}
	total += BLOCK_SIZE;
    }

    mtd_read_close(in);

    if (close(fd)) {
	unlink(argv[2]);
    	die("error closing %s", argv[2]);
    }

    return 0;
}
