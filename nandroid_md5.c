/*
 * Copyright (C) 2014 The CyanogenMod Project
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

#include <dirent.h>
#include <fcntl.h>
#include <limits.h>
#include <openssl/md5.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <unistd.h>

#include "common.h"
#include "extendedcommands.h"
#include "nandroid.h"
#include "nandroid_md5.h"
#include "recovery_ui.h"

#define MAX_FILES_CHECKED 20
#define HASH_LENGTH 2*MD5_DIGEST_LENGTH
#define BUFSIZE 1024

typedef struct {
    int is_missing;
    char *filename;
} MissingFiles;

static void to_md5_hash(char *str, unsigned char* md) {
    int i;
    for (i = 0; i < MD5_DIGEST_LENGTH; i++)
        sprintf(&str[2*i], "%02x", (unsigned int)md[i]);
    str[HASH_LENGTH] = '\0';
}

static int calculate_md5(char *str, const char *path) {
    FILE *fd;
    fd = fopen(path, "r");
    if (fd != NULL) {
        MD5_CTX c;
        size_t i;
        static unsigned char buf[BUFSIZE];
        unsigned char md5dig[MD5_DIGEST_LENGTH];
        MD5_Init(&c);
        while ((i = fread(buf, 1, BUFSIZE, fd)) > 0)
            MD5_Update(&c, buf, i);
        MD5_Final(&(md5dig[0]), &c);
        fclose(fd);
        to_md5_hash(str, md5dig);
    } else {
       return 1;
    }

    return 0;
}

static int is_selected_for_restore(const char *file, const unsigned char flags) {
    int check_boot = ((flags & NANDROID_BOOT) == NANDROID_BOOT);
    int check_system = ((flags & NANDROID_SYSTEM) == NANDROID_SYSTEM);
    int check_data = ((flags & NANDROID_DATA) == NANDROID_DATA);
    int check_cache = ((flags & NANDROID_CACHE) == NANDROID_CACHE);
    int check_sdext = ((flags & NANDROID_SDEXT) == NANDROID_SDEXT);
    int check_wimax = ((flags & NANDROID_WIMAX) == NANDROID_WIMAX);

    if (check_boot && strstr(file, "boot"))
        return 1;
    if (check_system && strstr(file, "system"))
        return 1;
    if (check_data && (strstr(file, "data") || strstr(file, "android_secure")))
        return 1;
    if (check_cache && strstr(file, "cache"))
        return 1;
    if (check_sdext && strstr(file, "sd-ext"))
        return 1;
    if (check_wimax && strstr(file, "wimax"))
        return 1;

    return 0;
}

int nandroid_backup_md5_gen(const char *backup_path) {
    DIR *dp;
    FILE *fd;
    int i = 0;
    int j = 0;
    int len = 0;
    int ret = 0;
    int filecount = 0;

    // Dynamically allocated, free them at the end
    char *filenames[MAX_FILES_CHECKED];
    char *filepaths[MAX_FILES_CHECKED];

    char path[PATH_MAX];
    snprintf(path, PATH_MAX, "%s", backup_path);
    len = strlen(path);
    if (path[len-1] == '/')
        path[len-1] = '\0';

    ui_print("Generating MD5 checksums...\n");

    // Read files available in backup path
    dp = opendir(path);
    if (dp != NULL) {
        struct dirent *ep;
        while ((ep = readdir(dp)) && i < MAX_FILES_CHECKED) {
            if (strcmp(ep->d_name, ".") != 0 && strcmp(ep->d_name, "..") != 0
                    && strcmp(ep->d_name, "recovery.log") != 0
                    && strcmp(ep->d_name, "nandroid.md5") != 0) {
                len = strlen(ep->d_name);
                filenames[i] = malloc(sizeof(char[len+1]));
                snprintf(filenames[i], len+1, "%s", ep->d_name);

                len += 1 + strlen(path);
                filepaths[i] = malloc(sizeof(char[len+1]));
                snprintf(filepaths[i], len+1, "%s/%s", path, ep->d_name);
                i++;
            }
        }
        closedir(dp);
        filecount = i;
    }
    if (filecount == 0) {
        ret = -1;
        LOGE("No files found in %s for MD5 generation\n", path);
        goto out;
    }

    // Prepare backup_path/nandroid.md5 for writing
    char md5path[PATH_MAX];
    snprintf(md5path, PATH_MAX, "%s/%s", path, "nandroid.md5");
    fd = fopen(md5path, "w");
    if (fd == NULL) {
        ret = -1;
        LOGE("Unable to create nandroid.md5\n");
        goto out;
    }

    // Generate MD5s and save to nandroid.md5
    char md5calc[HASH_LENGTH+1];
    char tmp[PATH_MAX];
    for (i = 0; i < filecount; i++) {
        if (calculate_md5(md5calc, filepaths[i]) != 0) {
            LOGE("Unable to generate MD5 for %s\n", filenames[i]);
            // Attempt to continue for other files
        } else {
            snprintf(tmp, PATH_MAX, "%s  %s\n", md5calc, filenames[i]);
            fputs(tmp, fd);
        }
    }
    fclose(fd);
    ui_print("MD5 checksums generated\n");

out:
    for (i = 0; i < filecount; i++) {
        free(filenames[i]);
        free(filepaths[i]);
    }

    return ret;
}

int nandroid_restore_md5_check(const char *backup_path, unsigned char flags) {
    DIR *dp;
    FILE *fd;
    int i = 0;
    int j = 0;
    int len = 0;
    int ret = 0;
    int filecount = 0;
    int md5count = 0;
    int use_ui = is_ui_initialized();

    if (empty_nandroid_bitmask(flags)) {
        LOGE("Nothing selected for restore.\n");
        return -1;
    }

    // Dynamically allocated, free them at the end
    char *filenames[MAX_FILES_CHECKED];
    char *filepaths[MAX_FILES_CHECKED];
    char *md5files[MAX_FILES_CHECKED];
    char *md5hashes[MAX_FILES_CHECKED];
    int mf_allocated = 0; // for MissingFiles *mf
    int mm_allocated = 0; // for MissingFiles *mm

    char path[PATH_MAX];
    snprintf(path, PATH_MAX, "%s", backup_path);
    len = strlen(path);
    if (path[len-1] == '/')
        path[len-1] = '\0';

    ui_print("Checking MD5 sums...\n");

    // Read files available in backup path
    dp = opendir(path);
    if (dp != NULL) {
        struct dirent *ep;
        while ((ep = readdir(dp)) && i < MAX_FILES_CHECKED) {
            if (is_selected_for_restore(ep->d_name, flags)) {
                len = strlen(ep->d_name);
                filenames[i] = malloc(sizeof(char[len+1]));
                snprintf(filenames[i], len+1, "%s", ep->d_name);

                len += 1 + strlen(path);
                filepaths[i] = malloc(sizeof(char[len+1]));
                snprintf(filepaths[i], len+1, "%s/%s", path, ep->d_name);
                i++;
            }
        }
        closedir(dp);
        filecount = i;
    }
    if (filecount == 0) {
        ret = -1;
        LOGE("No backup files found in %s\n", path);
        goto out;
    }

    // Read files + hashes available in nandroid.md5
    i = 0;
    char md5path[PATH_MAX];
    snprintf(md5path, PATH_MAX, "%s/%s", path, "nandroid.md5");
    fd = fopen(md5path, "r");
    if (fd != NULL) {
        char tmp[PATH_MAX];
        while (fgets(tmp, PATH_MAX, fd) && i < MAX_FILES_CHECKED) {
            if (tmp[strlen(tmp)-1] == '\n')
                tmp[strlen(tmp)-1] = '\0';
            if (is_selected_for_restore(tmp, flags)) {
                md5hashes[i] = malloc(sizeof(char[HASH_LENGTH+1]));
                snprintf(md5hashes[i], HASH_LENGTH+1, "%s", tmp);

                // md5 hash is followed by two spaces
                len = strlen(tmp) - (HASH_LENGTH+2);
                md5files[i] = malloc(sizeof(char[len+1]));
                snprintf(md5files[i], len+1, "%s", &tmp[HASH_LENGTH+2]);
                i++;
            }
        }
        fclose(fd);
        md5count = i;
    }

#if DEBUG_MD5_CHECKER
    LOGI("[MD5] backup_path: %s\n", path);
    for (i = 0; i < filecount; i++) {
        LOGI("[MD5] files in path: %s\n", filenames[i]);
    }
    for (i = 0; i < md5count; i++) {
        LOGI("[MD5] reference: %s: %s\n", md5files[i], md5hashes[i]);
    }
#endif

    // Cross-reference files in directory to those in nandroid.md5
    // Mark files that are in nandroid.md5 but not in directory (potentially fatal)
    int foundfile;
    int totalmissing = 0;
    MissingFiles *mm = malloc(md5count * sizeof(MissingFiles));
    mm_allocated = 1;
    for (i = 0; i < md5count; i++) {
        foundfile = 0;
        for (j = 0; j < filecount; j++) {
            if (strcmp(md5files[i], filenames[j]) == 0) {
                mm[i] = (MissingFiles){ 0, "" };
                foundfile = 1;
            }
        }
        if (!foundfile) {
            mm[i] = (MissingFiles){ 1, md5files[i] };
            totalmissing++;
        }
    }

    // Warn user of failure for missing files
    if (totalmissing && use_ui) {
        int uiback = ui_is_showing_back_button();
        ui_set_showing_back_button(0);

        const char* headers[totalmissing+8];
        headers[0] = "Backup files are missing:";
        int hi = 1;
        for (i = 0; i < md5count; i++) {
            if (mm[i].is_missing)
                headers[hi++] = mm[i].filename;
        }
        headers[totalmissing+1] = "";
        headers[totalmissing+2] = "Attempting to restore any";
        headers[totalmissing+3] = "of these will fail.";
        headers[totalmissing+4] = "";
        headers[totalmissing+5] = "Continue anyways?";
        headers[totalmissing+6] = "";
        headers[totalmissing+7] = NULL;

        static char* list[] = { "Yes", "No", NULL };

        int chosen_item = get_menu_selection(headers, list, 0, 0);
        if (chosen_item == 0) {
            ui_set_showing_back_button(uiback);
        } else {
            ret = -1;
            ui_set_showing_back_button(uiback);
            LOGE("Aborting\n");
            goto out;
        }
    } else if (totalmissing) {
        // No UI, so no prompt possible; fail the restore
        ret = -1;
        LOGE("Backup files are missing:\n");
        for (i = 0; i < md5count; i++) {
            if (mm[i].is_missing)
                LOGE("%s\n", mm[i].filename);
        }
        LOGE("Aborting\n");
        goto out;
    }

    // Cross-reference files in directory to those in nandroid.md5
    // Mark files that are in directory but not in nandroid.md5 (non-fatal)
    totalmissing = 0;
    MissingFiles *mf = malloc(filecount * sizeof(MissingFiles));
    mf_allocated = 1;
    for (i = 0; i < filecount; i++) {
        foundfile = 0;
        // Ignore files that are not scheduled for restore
        if (!is_selected_for_restore(filenames[i], flags)) {
            mf[i] = (MissingFiles){ 0, "" };
            continue;
        }
        for (j = 0; j < md5count; j++) {
            if (strcmp(filenames[i], md5files[j]) == 0) {
                mf[i] = (MissingFiles){ 0, "" };
                foundfile = 1;
            }
        }
        if (!foundfile) {
            mf[i] = (MissingFiles){ 1, filenames[i] };
            totalmissing++;
        }
    }

    // Warn user about missing md5 references for files
    if (totalmissing && use_ui) {
        int uiback = ui_is_showing_back_button();
        ui_set_showing_back_button(0);

        const char* headers[totalmissing+5];
        headers[0] = "Could not find reference MD5 for:";
        int hi = 1;
        for (i = 0; i < filecount; i++) {
            if (mf[i].is_missing)
                headers[hi++] = mf[i].filename;
        }
        headers[totalmissing+1] = "";
        headers[totalmissing+2] = "Continue anyways?";
        headers[totalmissing+3] = "";
        headers[totalmissing+4] = NULL;

        static char* list[] = { "Yes", "No", NULL };

        int chosen_item = get_menu_selection(headers, list, 0, 0);
        if (chosen_item == 0) {
            ui_set_showing_back_button(uiback);
        } else{
            ret = -1;
            ui_set_showing_back_button(uiback);
            LOGE("Aborting\n");
            goto out;
        }
    } else if (totalmissing) {
        // No UI, so no prompt possible; fail the restore
        ret = -1;
        LOGE("Could not find reference MD5 for:\n");
        for (i = 0; i < filecount; i++) {
            if (mf[i].is_missing)
                LOGE("%s\n", mf[i].filename);
        }
        LOGE("Aborting\n");
        goto out;
    }

    // Compare MD5s of non-missing files that are selected for restore
    int md5matches = 0;
    char md5calc[HASH_LENGTH+1];
    for (i = 0; i < filecount; i++) {
        if (!is_selected_for_restore(filenames[i], flags))
            continue;
        for (j = 0; j < md5count; j++) {
            if (strcmp(filenames[i], md5files[j]) == 0) {
                if (calculate_md5(md5calc, filepaths[i]) != 0) {
                    ret = -1;
                    LOGE("Unable to check MD5 of %s\nAborting\n", filenames[i]);
                    goto out;
                }
                if (strcmp(md5calc, md5hashes[j]) != 0) {
                    ret = -1;
                    LOGE("MD5 mismatch for %s\nAborting\n", filenames[i]);
                    goto out;
                } else {
                    md5matches++;
                }
            }
        }
    }
    if (md5matches) {
        ui_print("All MD5 checksums verified\n");
    } else {
        ui_print("No MD5 verification performed\n");
    }

out:
    if (mf_allocated)
        free(mf);
    if (mm_allocated)
        free(mm);
    for (i = 0; i < filecount; i++) {
        free(filenames[i]);
        free(filepaths[i]);
    }
    for (i = 0; i < md5count; i++) {
        free(md5files[i]);
        free(md5hashes[i]);
    }

    return ret;
}
