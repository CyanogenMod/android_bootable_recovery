/*
 * Copyright (C) 2008 The Android Open Source Project
 * Portions Copyright (C) 2010 Magnus Eriksson <packetlss@gmail.com>
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

#include <fcntl.h>
#include <mtd/mtd-user.h>

#include "cutils/log.h"
#include "mtdutils.h"

#ifdef LOG_TAG
#undef LOG_TAG
#endif

#define LOG_TAG "erase_flash"

void die(const char *msg, ...) {
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
    LOGE("%s\n", buf);
    exit(1);
}

/* Erase a mtd partition */

int main(int argc, char **argv) {
    MtdWriteContext *out;
    size_t erased;
    size_t total_size;
    size_t erase_size;

    if (argc != 2) {
        fprintf(stderr, "usage: %s <partition>\n", argv[0]);
        return 2;
    }

    if (mtd_scan_partitions() <= 0) die("error scanning partitions");
    const MtdPartition *partition = mtd_find_partition_by_name(argv[1]);
    if (partition == NULL) die("can't find %s partition", argv[1]);

    out = mtd_write_partition(partition);
    if (out == NULL) die("could not estabilish write context for %s", argv[1]);

    // do the actual erase, -1 = full partition erase
    erased = mtd_erase_blocks(out, -1);

    // erased = bytes erased, if zero, something borked
    if (!erased) die("error erasing %s", argv[1]);

    return 0;
}
