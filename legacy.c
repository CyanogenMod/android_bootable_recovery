/*
 * Copyright (C) 2007 The Android Open Source Project
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

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

#include "common.h"
#include "install.h"
#include "mincrypt/rsa.h"
#include "minui/minui.h"
#include "minzip/SysUtil.h"
#include "minzip/Zip.h"
#include "mtdutils/mtdutils.h"
#include "roots.h"
#include "verifier.h"
#include "firmware.h"

#include "amend/amend.h"
#include "common.h"
#include "install.h"
#include "mincrypt/rsa.h"
#include "minui/minui.h"
#include "minzip/SysUtil.h"
#include "minzip/Zip.h"
#include "mounts.h"
#include "mtdutils/mtdutils.h"
#include "roots.h"
#include "verifier.h"

static int read_data(ZipArchive *zip, const ZipEntry *entry,
        char** ppData, int* pLength) {
    int len = (int)mzGetZipEntryUncompLen(entry);
    if (len <= 0) {
        LOGE("Bad data length %d\n", len);
        return -1;
    }
    char *data = malloc(len + 1);
    if (data == NULL) {
        LOGE("Can't allocate %d bytes for data\n", len + 1);
        return -2;
    }
    bool ok = mzReadZipEntry(zip, entry, data, len);
    if (!ok) {
        LOGE("Error while reading data\n");
        free(data);
        return -3;
    }
    data[len] = '\0';     // not necessary, but just to be safe
    *ppData = data;
    if (pLength) {
        *pLength = len;
    }
    return 0;
}

int
handle_update_script(ZipArchive *zip, const ZipEntry *update_script_entry)
{
    /* Read the entire script into a buffer.
     */
    int script_len;
    char* script_data;
    if (read_data(zip, update_script_entry, &script_data, &script_len) < 0) {
        LOGE("Can't read update script\n");
        return INSTALL_UPDATE_SCRIPT_MISSING;
    }

    /* Parse the script.  Note that the script and parse tree are never freed.
     */
    const AmCommandList *commands = parseAmendScript(script_data, script_len);
    if (commands == NULL) {
        LOGE("Syntax error in update script\n");
        return INSTALL_ERROR;
    } else {
        UnterminatedString name = mzGetZipEntryFileName(update_script_entry);
        LOGI("Parsed %.*s\n", name.len, name.str);
    }

    /* Execute the script.
     */
    int ret = execCommandList((ExecContext *)1, commands);
    if (ret != 0) {
        int num = ret;
        char *line, *next = script_data;
        while (next != NULL && ret-- > 0) {
            line = next;
            next = memchr(line, '\n', script_data + script_len - line);
            if (next != NULL) *next++ = '\0';
        }
        LOGE("Failure at line %d:\n%s\n", num, next ? line : "(not found)");
        return INSTALL_ERROR;
    }

    ui_print("Installation complete.\n");
    return INSTALL_SUCCESS;
}

#define ASSUMED_UPDATE_SCRIPT_NAME  "META-INF/com/google/android/update-script"

const ZipEntry *
find_update_script(ZipArchive *zip)
{
//TODO: Get the location of this script from the MANIFEST.MF file
    return mzFindZipEntry(zip, ASSUMED_UPDATE_SCRIPT_NAME);
}
