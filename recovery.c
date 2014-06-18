/*
 * Copyright (C) 2007 The Android Open Source Project
 * Copyright (c) 2010, Code Aurora Forum. All rights reserved.
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
#include <getopt.h>
#include <limits.h>
#include <linux/input.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>

#include "bootloader.h"
#include "common.h"
#include "cutils/properties.h"
#include "cutils/android_reboot.h"
#include "install.h"
#include "minui/minui.h"
#include "minzip/DirUtil.h"
#include "roots.h"
#include "recovery_ui.h"

#include "adb_install.h"
#include "minadbd/adb.h"

#include "firmware.h"
#include "extendedcommands.h"
#include "flashutils/flashutils.h"
#include "dedupe/dedupe.h"
#include "voldclient/voldclient.h"

#include "recovery_cmds.h"

struct selabel_handle *sehandle = NULL;

static const struct option OPTIONS[] = {
  { "send_intent", required_argument, NULL, 's' },
  { "update_package", required_argument, NULL, 'u' },
  { "headless", no_argument, NULL, 'h' },
  { "wipe_data", no_argument, NULL, 'w' },
  { "wipe_cache", no_argument, NULL, 'c' },
  { "show_text", no_argument, NULL, 't' },
  { "sideload", no_argument, NULL, 'l' },
  { NULL, 0, NULL, 0 },
};

#define LAST_LOG_FILE "/cache/recovery/last_log"
static const char *CACHE_LOG_DIR = "/cache/recovery";
static const char *COMMAND_FILE = "/cache/recovery/command";
static const char *INTENT_FILE = "/cache/recovery/intent";
static const char *LOG_FILE = "/cache/recovery/log";
static const char *LAST_INSTALL_FILE = "/cache/recovery/last_install";
static const char *CACHE_ROOT = "/cache";
static const char *TEMPORARY_LOG_FILE = "/tmp/recovery.log";
static const char *TEMPORARY_INSTALL_FILE = "/tmp/last_install";
static const char *SIDELOAD_TEMP_DIR = "/tmp/sideload";

extern UIParameters ui_parameters;    // from ui.c

/*
 * The recovery tool communicates with the main system through /cache files.
 *   /cache/recovery/command - INPUT - command line for tool, one arg per line
 *   /cache/recovery/log - OUTPUT - combined log file from recovery run(s)
 *   /cache/recovery/intent - OUTPUT - intent that was passed in
 *
 * The arguments which may be supplied in the recovery.command file:
 *   --send_intent=anystring - write the text out to recovery.intent
 *   --update_package=path - verify install an OTA package file
 *   --wipe_data - erase user data (and cache), then reboot
 *   --wipe_cache - wipe cache (but not user data), then reboot
 *   --set_encrypted_filesystem=on|off - enables / diasables encrypted fs
 *
 * After completing, we remove /cache/recovery/command and reboot.
 * Arguments may also be supplied in the bootloader control block (BCB).
 * These important scenarios must be safely restartable at any point:
 *
 * FACTORY RESET
 * 1. user selects "factory reset"
 * 2. main system writes "--wipe_data" to /cache/recovery/command
 * 3. main system reboots into recovery
 * 4. get_args() writes BCB with "boot-recovery" and "--wipe_data"
 *    -- after this, rebooting will restart the erase --
 * 5. erase_volume() reformats /data
 * 6. erase_volume() reformats /cache
 * 7. finish_recovery() erases BCB
 *    -- after this, rebooting will restart the main system --
 * 8. main() calls reboot() to boot main system
 *
 * OTA INSTALL
 * 1. main system downloads OTA package to /cache/some-filename.zip
 * 2. main system writes "--update_package=/cache/some-filename.zip"
 * 3. main system reboots into recovery
 * 4. get_args() writes BCB with "boot-recovery" and "--update_package=..."
 *    -- after this, rebooting will attempt to reinstall the update --
 * 5. install_package() attempts to install the update
 *    NOTE: the package install must itself be restartable from any point
 * 6. finish_recovery() erases BCB
 *    -- after this, rebooting will (try to) restart the main system --
 * 7. ** if install failed **
 *    7a. prompt_and_wait() shows an error icon and waits for the user
 *    7b; the user reboots (pulling the battery, etc) into the main system
 * 8. main() calls maybe_install_firmware_update()
 *    ** if the update contained radio/hboot firmware **:
 *    8a. m_i_f_u() writes BCB with "boot-recovery" and "--wipe_cache"
 *        -- after this, rebooting will reformat cache & restart main system --
 *    8b. m_i_f_u() writes firmware image into raw cache partition
 *    8c. m_i_f_u() writes BCB with "update-radio/hboot" and "--wipe_cache"
 *        -- after this, rebooting will attempt to reinstall firmware --
 *    8d. bootloader tries to flash firmware
 *    8e. bootloader writes BCB with "boot-recovery" (keeping "--wipe_cache")
 *        -- after this, rebooting will reformat cache & restart main system --
 *    8f. erase_volume() reformats /cache
 *    8g. finish_recovery() erases BCB
 *        -- after this, rebooting will (try to) restart the main system --
 * 9. main() calls reboot() to boot main system
 */

static const int MAX_ARG_LENGTH = 4096;
static const int MAX_ARGS = 100;

// open a given path, mounting partitions as necessary
FILE*
fopen_path(const char *path, const char *mode) {
    if (ensure_path_mounted(path) != 0) {
        LOGE("Can't mount %s\n", path);
        return NULL;
    }

    // When writing, try to create the containing directory, if necessary.
    // Use generous permissions, the system (init.rc) will reset them.
    if (strchr("wa", mode[0])) dirCreateHierarchy(path, 0777, NULL, 1, sehandle);

    FILE *fp = fopen(path, mode);
    if (fp == NULL && path != COMMAND_FILE) LOGE("Can't open %s\n", path);
    return fp;
}

// close a file, log an error if the error indicator is set
static void
check_and_fclose(FILE *fp, const char *name) {
    fflush(fp);
    if (ferror(fp)) LOGE("Error in %s\n(%s)\n", name, strerror(errno));
    fclose(fp);
}

// command line args come from, in decreasing precedence:
//   - the actual command line
//   - the bootloader control block (one per line, after "recovery")
//   - the contents of COMMAND_FILE (one per line)
static void
get_args(int *argc, char ***argv) {
    struct bootloader_message boot;
    memset(&boot, 0, sizeof(boot));
    get_bootloader_message(&boot);  // this may fail, leaving a zeroed structure

    if (boot.command[0] != 0 && boot.command[0] != 255) {
        LOGI("Boot command: %.*s\n", sizeof(boot.command), boot.command);
    }

    if (boot.status[0] != 0 && boot.status[0] != 255) {
        LOGI("Boot status: %.*s\n", sizeof(boot.status), boot.status);
    }

    struct stat file_info;

    // --- if arguments weren't supplied, look in the bootloader control block
    if (*argc <= 1 && 0 != stat("/tmp/.ignorebootmessage", &file_info)) {
        boot.recovery[sizeof(boot.recovery) - 1] = '\0';  // Ensure termination
        const char *arg = strtok(boot.recovery, "\n");
        if (arg != NULL && !strcmp(arg, "recovery")) {
            *argv = (char **) malloc(sizeof(char *) * MAX_ARGS);
            (*argv)[0] = strdup(arg);
            for (*argc = 1; *argc < MAX_ARGS; ++*argc) {
                if ((arg = strtok(NULL, "\n")) == NULL) break;
                (*argv)[*argc] = strdup(arg);
            }
            LOGI("Got arguments from boot message\n");
        } else if (boot.recovery[0] != 0 && boot.recovery[0] != 255) {
            LOGE("Bad boot message\n\"%.20s\"\n", boot.recovery);
        }
    }

    // --- if that doesn't work, try the command file
    if (*argc <= 1) {
        FILE *fp = fopen_path(COMMAND_FILE, "r");
        if (fp != NULL) {
            char *token;
            char *argv0 = (*argv)[0];
            *argv = (char **) malloc(sizeof(char *) * MAX_ARGS);
            (*argv)[0] = argv0;  // use the same program name

            char buf[MAX_ARG_LENGTH];
            for (*argc = 1; *argc < MAX_ARGS; ++*argc) {
                if (!fgets(buf, sizeof(buf), fp)) break;
                token = strtok(buf, "\r\n");
                if (token != NULL) {
                    (*argv)[*argc] = strdup(token);  // Strip newline.
                } else {
                    --*argc;
                }
            }

            check_and_fclose(fp, COMMAND_FILE);
            LOGI("Got arguments from %s\n", COMMAND_FILE);
        }
    }

    // --> write the arguments we have back into the bootloader control block
    // always boot into recovery after this (until finish_recovery() is called)
    strlcpy(boot.command, "boot-recovery", sizeof(boot.command));
    strlcpy(boot.recovery, "recovery\n", sizeof(boot.recovery));
    int i;
    for (i = 1; i < *argc; ++i) {
        strlcat(boot.recovery, (*argv)[i], sizeof(boot.recovery));
        strlcat(boot.recovery, "\n", sizeof(boot.recovery));
    }
    set_bootloader_message(&boot);
}

void
set_sdcard_update_bootloader_message() {
    struct bootloader_message boot;
    memset(&boot, 0, sizeof(boot));
    strlcpy(boot.command, "boot-recovery", sizeof(boot.command));
    strlcpy(boot.recovery, "recovery\n", sizeof(boot.recovery));
    set_bootloader_message(&boot);
}

// How much of the temp log we have copied to the copy in cache.
static long tmplog_offset = 0;

static void
copy_log_file(const char* source, const char* destination, int append) {
    FILE *log = fopen_path(destination, append ? "a" : "w");
    if (log == NULL) {
        LOGE("Can't open %s\n", destination);
    } else {
        FILE *tmplog = fopen(source, "r");
        if (tmplog != NULL) {
            if (append) {
                fseek(tmplog, tmplog_offset, SEEK_SET);  // Since last write
            }
            char buf[4096];
            while (fgets(buf, sizeof(buf), tmplog)) fputs(buf, log);
            if (append) {
                tmplog_offset = ftell(tmplog);
            }
            check_and_fclose(tmplog, source);
        }
        check_and_fclose(log, destination);
    }
}

// Rename last_log -> last_log.1 -> last_log.2 -> ... -> last_log.$max
// Overwrites any existing last_log.$max.
static void
rotate_last_logs(int max) {
    char oldfn[256];
    char newfn[256];

    int i;
    for (i = max-1; i >= 0; --i) {
        snprintf(oldfn, sizeof(oldfn), (i==0) ? LAST_LOG_FILE : (LAST_LOG_FILE ".%d"), i);
        snprintf(newfn, sizeof(newfn), LAST_LOG_FILE ".%d", i+1);
        // ignore errors
        rename(oldfn, newfn);
    }
}

static void
copy_logs() {
    // Copy logs to cache so the system can find out what happened.
    copy_log_file(TEMPORARY_LOG_FILE, LOG_FILE, true);
    copy_log_file(TEMPORARY_LOG_FILE, LAST_LOG_FILE, false);
    copy_log_file(TEMPORARY_INSTALL_FILE, LAST_INSTALL_FILE, false);
    chmod(LOG_FILE, 0600);
    chown(LOG_FILE, 1000, 1000);   // system user
    chmod(LAST_LOG_FILE, 0640);
    chmod(LAST_INSTALL_FILE, 0644);
    sync();
}

// clear the recovery command and prepare to boot a (hopefully working) system,
// copy our log file to cache as well (for the system to read), and
// record any intent we were asked to communicate back to the system.
// this function is idempotent: call it as many times as you like.
static void
finish_recovery(const char *send_intent) {
    // By this point, we're ready to return to the main system...
    if (send_intent != NULL) {
        FILE *fp = fopen_path(INTENT_FILE, "w");
        if (fp == NULL) {
            LOGE("Can't open %s\n", INTENT_FILE);
        } else {
            fputs(send_intent, fp);
            check_and_fclose(fp, INTENT_FILE);
        }
    }

    copy_logs();

    // Reset to normal system boot so recovery won't cycle indefinitely.
    struct bootloader_message boot;
    memset(&boot, 0, sizeof(boot));
    set_bootloader_message(&boot);

    // Remove the command file, so recovery won't repeat indefinitely.
    if (ensure_path_mounted(COMMAND_FILE) != 0 ||
        (unlink(COMMAND_FILE) && errno != ENOENT)) {
        LOGW("Can't unlink %s\n", COMMAND_FILE);
    }

    sync();  // For good measure.
}

typedef struct _saved_log_file {
    char* name;
    struct stat st;
    unsigned char* data;
    struct _saved_log_file* next;
} saved_log_file;

static int
erase_volume(const char *volume) {
    bool is_cache = (strcmp(volume, CACHE_ROOT) == 0);

    ui_set_background(BACKGROUND_ICON_INSTALLING);
    ui_show_indeterminate_progress();

    saved_log_file* head = NULL;

    if (is_cache) {
        // If we're reformatting /cache, we load any
        // "/cache/recovery/last*" files into memory, so we can restore
        // them after the reformat.

        ensure_path_mounted(volume);

        DIR* d;
        struct dirent* de;
        d = opendir(CACHE_LOG_DIR);
        if (d) {
            char path[PATH_MAX];
            strcpy(path, CACHE_LOG_DIR);
            strcat(path, "/");
            int path_len = strlen(path);
            while ((de = readdir(d)) != NULL) {
                if (strncmp(de->d_name, "last", 4) == 0) {
                    saved_log_file* p = (saved_log_file*) malloc(sizeof(saved_log_file));
                    strcpy(path+path_len, de->d_name);
                    p->name = strdup(path);
                    if (stat(path, &(p->st)) == 0) {
                        // truncate files to 512kb
                        if (p->st.st_size > (1 << 19)) {
                            p->st.st_size = 1 << 19;
                        }
                        p->data = (unsigned char*) malloc(p->st.st_size);
                        FILE* f = fopen(path, "rb");
                        fread(p->data, 1, p->st.st_size, f);
                        fclose(f);
                        p->next = head;
                        head = p;
                    } else {
                        free(p);
                    }
                }
            }
            closedir(d);
        } else {
            if (errno != ENOENT) {
                printf("opendir failed: %s\n", strerror(errno));
            }
        }
    }

    ui_print("Formatting %s...\n", volume);

    ensure_path_unmounted(volume);
    int result = format_volume(volume);

    if (is_cache) {
        while (head) {
            FILE* f = fopen_path(head->name, "wb");
            if (f) {
                fwrite(head->data, 1, head->st.st_size, f);
                fclose(f);
                chmod(head->name, head->st.st_mode);
                chown(head->name, head->st.st_uid, head->st.st_gid);
            }
            free(head->name);
            free(head->data);
            saved_log_file* temp = head->next;
            free(head);
            head = temp;
        }

        // Any part of the log we'd copied to cache is now gone.
        // Reset the pointer so we copy from the beginning of the temp
        // log.
        tmplog_offset = 0;
        copy_logs();
    }

    ui_set_background(BACKGROUND_ICON_CLOCKWORK);
    ui_reset_progress();
    return result;
}

static char*
copy_sideloaded_package(const char* original_path) {
  if (ensure_path_mounted(original_path) != 0) {
    LOGE("Can't mount %s\n", original_path);
    return NULL;
  }

  if (ensure_path_mounted(SIDELOAD_TEMP_DIR) != 0) {
    LOGE("Can't mount %s\n", SIDELOAD_TEMP_DIR);
    return NULL;
  }

  if (mkdir(SIDELOAD_TEMP_DIR, 0700) != 0) {
    if (errno != EEXIST) {
      LOGE("Can't mkdir %s (%s)\n", SIDELOAD_TEMP_DIR, strerror(errno));
      return NULL;
    }
  }

  // verify that SIDELOAD_TEMP_DIR is exactly what we expect: a
  // directory, owned by root, readable and writable only by root.
  struct stat st;
  if (stat(SIDELOAD_TEMP_DIR, &st) != 0) {
    LOGE("failed to stat %s (%s)\n", SIDELOAD_TEMP_DIR, strerror(errno));
    return NULL;
  }
  if (!S_ISDIR(st.st_mode)) {
    LOGE("%s isn't a directory\n", SIDELOAD_TEMP_DIR);
    return NULL;
  }
  if ((st.st_mode & 0777) != 0700) {
    LOGE("%s has perms %o\n", SIDELOAD_TEMP_DIR, st.st_mode);
    return NULL;
  }
  if (st.st_uid != 0) {
    LOGE("%s owned by %lu; not root\n", SIDELOAD_TEMP_DIR, st.st_uid);
    return NULL;
  }

  char copy_path[PATH_MAX];
  strcpy(copy_path, SIDELOAD_TEMP_DIR);
  strcat(copy_path, "/package.zip");

  char* buffer = malloc(BUFSIZ);
  if (buffer == NULL) {
    LOGE("Failed to allocate buffer\n");
    return NULL;
  }

  size_t read;
  FILE* fin = fopen(original_path, "rb");
  if (fin == NULL) {
    LOGE("Failed to open %s (%s)\n", original_path, strerror(errno));
    return NULL;
  }
  FILE* fout = fopen(copy_path, "wb");
  if (fout == NULL) {
    LOGE("Failed to open %s (%s)\n", copy_path, strerror(errno));
    return NULL;
  }

  while ((read = fread(buffer, 1, BUFSIZ, fin)) > 0) {
    if (fwrite(buffer, 1, read, fout) != read) {
      LOGE("Short write of %s (%s)\n", copy_path, strerror(errno));
      return NULL;
    }
  }

  free(buffer);

  if (fclose(fout) != 0) {
    LOGE("Failed to close %s (%s)\n", copy_path, strerror(errno));
    return NULL;
  }

  if (fclose(fin) != 0) {
    LOGE("Failed to close %s (%s)\n", original_path, strerror(errno));
    return NULL;
  }

  // "adb push" is happy to overwrite read-only files when it's
  // running as root, but we'll try anyway.
  if (chmod(copy_path, 0400) != 0) {
    LOGE("Failed to chmod %s (%s)\n", copy_path, strerror(errno));
    return NULL;
  }

  return strdup(copy_path);
}

static const char**
prepend_title(const char** headers) {
    const char* title[] = { EXPAND(RECOVERY_VERSION),
                      "",
                      NULL };

    // count the number of lines in our title, plus the
    // caller-provided headers.
    int count = 0;
    const char** p;
    for (p = title; *p; ++p, ++count);
    for (p = headers; *p; ++p, ++count);

    const char** new_headers = malloc((count+1) * sizeof(const char*));
    const char** h = new_headers;
    for (p = title; *p; ++p, ++h) *h = *p;
    for (p = headers; *p; ++p, ++h) *h = *p;
    *h = NULL;

    return new_headers;
}

int
get_menu_selection(const char** headers, char** items, int menu_only,
                   int initial_selection) {
    // throw away keys pressed previously, so user doesn't
    // accidentally trigger menu items.
    ui_clear_key_queue();

    int item_count = ui_start_menu(headers, items, initial_selection);
    int selected = initial_selection;
    int chosen_item = -1; // NO_ACTION
    int wrap_count = 0;

    while (chosen_item < 0 && chosen_item != GO_BACK) {
        int key = ui_wait_key();
        int visible = ui_text_visible();

        if (key == -1) {   // ui_wait_key() timed out
            if (ui_text_ever_visible()) {
                continue;
            } else {
                LOGI("timed out waiting for key input; rebooting.\n");
                ui_end_menu();
                return ITEM_REBOOT;
            }
        }
        else if (key == -2) {   // we are returning from ui_cancel_wait_key(): trigger a GO_BACK
            return GO_BACK;
        }
        else if (key == -3) {   // an USB device was plugged in (returning from ui_wait_key())
            return REFRESH;
        }

        int action = ui_handle_key(key, visible);

        int old_selected = selected;
        selected = ui_get_selected_item();

        if (action < 0) {
            switch (action) {
                case HIGHLIGHT_UP:
                    --selected;
                    selected = ui_menu_select(selected);
                    break;
                case HIGHLIGHT_DOWN:
                    ++selected;
                    selected = ui_menu_select(selected);
                    break;
                case SELECT_ITEM:
                    chosen_item = selected;
                    if (ui_is_showing_back_button()) {
                        if (chosen_item == item_count) {
                            chosen_item = GO_BACK;
                        }
                    }
                    break;
                case NO_ACTION:
                    break;
                case GO_BACK:
                    chosen_item = GO_BACK;
                    break;
            }
        } else if (!menu_only) {
            chosen_item = action;
        }

        if (abs(selected - old_selected) > 1) {
            wrap_count++;
            if (wrap_count == 5) {
                wrap_count = 0;
                if (ui_get_rainbow_mode()) {
                    ui_set_rainbow_mode(0);
                    ui_print("Rainbow mode disabled\n");
                }
                else {
                    ui_set_rainbow_mode(1);
                    ui_print("Rainbow mode enabled!\n");
                }
            }
        }
    }

    ui_end_menu();
    ui_clear_key_queue();
    return chosen_item;
}

static int compare_string(const void* a, const void* b) {
    return strcmp(*(const char**)a, *(const char**)b);
}

static int
update_directory(const char* path, const char* unmount_when_done) {
    ensure_path_mounted(path);

    const char* MENU_HEADERS[] = { "Choose a package to install:",
                                   path,
                                   "",
                                   NULL };
    DIR* d;
    struct dirent* de;
    d = opendir(path);
    if (d == NULL) {
        LOGE("error opening %s: %s\n", path, strerror(errno));
        if (unmount_when_done != NULL) {
            ensure_path_unmounted(unmount_when_done);
        }
        return 0;
    }

    const char** headers = prepend_title(MENU_HEADERS);

    int d_size = 0;
    int d_alloc = 10;
    char** dirs = malloc(d_alloc * sizeof(char*));
    int z_size = 1;
    int z_alloc = 10;
    char** zips = malloc(z_alloc * sizeof(char*));
    zips[0] = strdup("../");

    while ((de = readdir(d)) != NULL) {
        int name_len = strlen(de->d_name);

        if (de->d_type == DT_DIR) {
            // skip "." and ".." entries
            if (name_len == 1 && de->d_name[0] == '.') continue;
            if (name_len == 2 && de->d_name[0] == '.' &&
                de->d_name[1] == '.') continue;

            if (d_size >= d_alloc) {
                d_alloc *= 2;
                dirs = realloc(dirs, d_alloc * sizeof(char*));
            }
            dirs[d_size] = malloc(name_len + 2);
            strcpy(dirs[d_size], de->d_name);
            dirs[d_size][name_len] = '/';
            dirs[d_size][name_len+1] = '\0';
            ++d_size;
        } else if (de->d_type == DT_REG &&
                   name_len >= 4 &&
                   strncasecmp(de->d_name + (name_len-4), ".zip", 4) == 0) {
            if (z_size >= z_alloc) {
                z_alloc *= 2;
                zips = realloc(zips, z_alloc * sizeof(char*));
            }
            zips[z_size++] = strdup(de->d_name);
        }
    }
    closedir(d);

    qsort(dirs, d_size, sizeof(char*), compare_string);
    qsort(zips, z_size, sizeof(char*), compare_string);

    // append dirs to the zips list
    if (d_size + z_size + 1 > z_alloc) {
        z_alloc = d_size + z_size + 1;
        zips = realloc(zips, z_alloc * sizeof(char*));
    }
    memcpy(zips + z_size, dirs, d_size * sizeof(char*));
    free(dirs);
    z_size += d_size;
    zips[z_size] = NULL;

    int result;
    int chosen_item = 0;
    do {
        chosen_item = get_menu_selection(headers, zips, 1, chosen_item);

        char* item = zips[chosen_item];
        int item_len = strlen(item);
        if (chosen_item == 0) {          // item 0 is always "../"
            // go up but continue browsing (if the caller is update_directory)
            result = -1;
            break;
        } else if (item[item_len-1] == '/') {
            // recurse down into a subdirectory
            char new_path[PATH_MAX];
            strlcpy(new_path, path, PATH_MAX);
            strlcat(new_path, "/", PATH_MAX);
            strlcat(new_path, item, PATH_MAX);
            new_path[strlen(new_path)-1] = '\0';  // truncate the trailing '/'
            result = update_directory(new_path, unmount_when_done);
            if (result >= 0) break;
        } else {
            // selected a zip file:  attempt to install it, and return
            // the status to the caller.
            char new_path[PATH_MAX];
            strlcpy(new_path, path, PATH_MAX);
            strlcat(new_path, "/", PATH_MAX);
            strlcat(new_path, item, PATH_MAX);

            ui_print("\n-- Install %s ...\n", path);
            set_sdcard_update_bootloader_message();
            char* copy = copy_sideloaded_package(new_path);
            if (unmount_when_done != NULL) {
                ensure_path_unmounted(unmount_when_done);
            }
            if (copy) {
                result = install_package(copy);
                free(copy);
            } else {
                result = INSTALL_ERROR;
            }
            break;
        }
    } while (true);

    int i;
    for (i = 0; i < z_size; ++i) free(zips[i]);
    free(zips);
    free(headers);

    if (unmount_when_done != NULL) {
        ensure_path_unmounted(unmount_when_done);
    }
    return result;
}

static void
wipe_data(int confirm) {
    if (confirm && !confirm_selection( "Confirm wipe of all user data?", "Yes - Wipe all user data"))
        return;

    ui_print("\n-- Wiping data...\n");
    device_wipe_data();
    erase_volume("/data");
    erase_volume("/cache");
    if (has_datadata()) {
        erase_volume("/datadata");
    }
    erase_volume("/sd-ext");
    erase_volume(get_android_secure_path());
    ui_print("Data wipe complete.\n");
}

static void headless_wait() {
    ui_show_text(0);
    const char** headers = prepend_title((const char**)MENU_HEADERS);
    for(;;) {
        finish_recovery(NULL);
        get_menu_selection(headers, MENU_ITEMS, 0, 0);
    }
}

int ui_menu_level = 1;
int ui_root_menu = 0;
static void
prompt_and_wait() {
    const char** headers = prepend_title((const char**)MENU_HEADERS);

    for (;;) {
        finish_recovery(NULL);
        ui_reset_progress();

        ui_root_menu = 1;
        // ui_menu_level is a legacy variable that i am keeping around to prevent build breakage.
        ui_menu_level = 0;
        int chosen_item = get_menu_selection(headers, MENU_ITEMS, 0, 0);
        ui_menu_level = 1;
        ui_root_menu = 0;

        // device-specific code may take some action here.  It may
        // return one of the core actions handled in the switch
        // statement below.
        chosen_item = device_perform_action(chosen_item);

        int status;
        int ret = 0;

        for (;;) {
            switch (chosen_item) {
                case ITEM_REBOOT:
                    return;

                case ITEM_WIPE_DATA:
                    wipe_data(ui_text_visible());
                    if (!ui_text_visible()) return;
                    break;

                case ITEM_WIPE_CACHE:
                    if (confirm_selection("Confirm wipe?", "Yes - Wipe Cache"))
                    {
                        ui_print("\n-- Wiping cache...\n");
                        erase_volume("/cache");
                        ui_print("Cache wipe complete.\n");
                        if (!ui_text_visible()) return;
                    }
                    break;

                case ITEM_APPLY_ZIP:
                    ret = show_install_update_menu();
                    break;

                case ITEM_NANDROID:
                    ret = show_nandroid_menu();
                    break;

                case ITEM_PARTITION:
                    ret = show_partition_menu();
                    break;

                case ITEM_ADVANCED:
                    ret = show_advanced_menu();
                    break;
            }
            if (ret == REFRESH) {
                ret = 0;
                continue;
            }
            break;
        }
    }
}

static void
print_property(const char *key, const char *name, void *cookie) {
    printf("%s=%s\n", key, name);
}

static void
setup_adbd() {
    struct stat f;
    static char *key_src = "/data/misc/adb/adb_keys";
    static char *key_dest = "/adb_keys";

    // Mount /data and copy adb_keys to root if it exists
    ensure_path_mounted("/data");
    if (stat(key_src, &f) == 0) {
        FILE *file_src = fopen(key_src, "r");
        if (file_src == NULL) {
            LOGE("Can't open %s\n", key_src);
        } else {
            FILE *file_dest = fopen(key_dest, "w");
            if (file_dest == NULL) {
                LOGE("Can't open %s\n", key_dest);
            } else {
                char buf[4096];
                while (fgets(buf, sizeof(buf), file_src)) fputs(buf, file_dest);
                check_and_fclose(file_dest, key_dest);

                // Enable secure adbd
                property_set("ro.adb.secure", "1");
            }
            check_and_fclose(file_src, key_src);
        }
    }
    preserve_data_media(0);
    ensure_path_unmounted("/data");
    preserve_data_media(1);

    // Trigger (re)start of adb daemon
    property_set("service.adb.root", "1");
}

// call a clean reboot
void reboot_main_system(int cmd, int flags, char *arg) {
    write_recovery_version();

#ifdef BOARD_NATIVE_DUALBOOT
    device_verify_root_and_recovery();
#else
    verify_root_and_recovery();
#endif

    finish_recovery(NULL); // sync() in here
    vold_unmount_all();
    android_reboot(cmd, flags, arg);
}

static int v_changed = 0;
int volumes_changed() {
    int ret = v_changed;
    if (v_changed == 1)
        v_changed = 0;
    return ret;
}

static int handle_volume_hotswap(char* label, char* path) {
    v_changed = 1;
    return 0;
}

static int handle_volume_state_changed(char* label, char* path, int state) {
    int log = -1;
    if (state == State_Checking || state == State_Mounted || state == State_Idle) {
        // do not ever log to screen mount/unmount events for sdcards
        if (strncmp(path, "/storage/sdcard", 15) == 0)
            log = 0;
        else log = 1;
    }
    else if (state == State_Formatting || state == State_Shared) {
            log = 1;
    }

    if (log == 0)
        LOGI("%s: %s\n", path, volume_state_to_string(state));
    else if (log == 1)
        ui_print("%s: %s\n", path, volume_state_to_string(state));

    return 0;
}

static struct vold_callbacks v_callbacks = {
    .state_changed = handle_volume_state_changed,
    .disk_added = handle_volume_hotswap,
    .disk_removed = handle_volume_hotswap,
};

void vold_init() {
    vold_client_start(&v_callbacks, 0);
    vold_set_automount(1);
}

int
main(int argc, char **argv) {

    if (argc == 2 && strcmp(argv[1], "adbd") == 0) {
        adb_main();
        return 0;
    }

    // Recovery needs to install world-readable files, so clear umask
    // set by init
    umask(0);

    char* command = argv[0];
    char* stripped = strrchr(argv[0], '/');
    if (stripped)
        command = stripped + 1;

    if (strcmp(command, "recovery") != 0)
    {
        struct recovery_cmd cmd = get_command(command);
        if (cmd.name)
            return cmd.main_func(argc, argv);

#ifdef BOARD_RECOVERY_HANDLES_MOUNT
        if (!strcmp(command, "mount") && argc == 2)
        {
            load_volume_table();
            return ensure_path_mounted(argv[1]);
        }
#endif
        if (!strcmp(command, "setup_adbd")) {
            load_volume_table();
            setup_adbd();
            return 0;
        }
        if (!strcmp(command, "start")) {
            property_set("ctl.start", argv[1]);
            return 0;
        }
        if (!strcmp(command, "stop")) {
            property_set("ctl.stop", argv[1]);
            return 0;
        }
        return busybox_driver(argc, argv);
    }
    __system("/sbin/postrecoveryboot.sh");

    int is_user_initiated_recovery = 0;
    time_t start = time(NULL);

    // If these fail, there's not really anywhere to complain...
    freopen(TEMPORARY_LOG_FILE, "a", stdout); setbuf(stdout, NULL);
    freopen(TEMPORARY_LOG_FILE, "a", stderr); setbuf(stderr, NULL);
    printf("Starting recovery on %s\n", ctime(&start));

    device_ui_init(&ui_parameters);
    ui_init();
    ui_print(EXPAND(RECOVERY_VERSION)"\n");

#ifdef BOARD_RECOVERY_SWIPE
#ifndef BOARD_TOUCH_RECOVERY
    //display directions for swipe controls
    ui_print("Swipe up/down to change selections.\n");
    ui_print("Swipe to the right for enter.\n");
    ui_print("Swipe to the left for back.\n");
#endif
#endif

    load_volume_table();
    process_volumes();
    vold_init();
    setup_legacy_storage_paths();
    LOGI("Processing arguments.\n");
    ensure_path_mounted(LAST_LOG_FILE);
    rotate_last_logs(10);
    get_args(&argc, &argv);

    int previous_runs = 0;
    const char *send_intent = NULL;
    const char *update_package = NULL;
    int wipe_data = 0, wipe_cache = 0;
    int sideload = 0;
    int headless = 0;

    LOGI("Checking arguments.\n");
    int arg;
    while ((arg = getopt_long(argc, argv, "", OPTIONS, NULL)) != -1) {
        switch (arg) {
        case 'p': previous_runs = atoi(optarg); break;
        case 's': send_intent = optarg; break;
        case 'u': update_package = optarg; break;
        case 'w':
#ifndef BOARD_RECOVERY_ALWAYS_WIPES
        wipe_data = wipe_cache = 1;
#endif
        break;
        case 'h':
            ui_set_background(BACKGROUND_ICON_CID);
            ui_show_text(0);
            headless = 1;
            break;
        case 'c': wipe_cache = 1; break;
        case 't': ui_show_text(1); break;
        case 'l': sideload = 1; break;
        case '?':
            LOGE("Invalid command argument\n");
            continue;
        }
    }

    struct selinux_opt seopts[] = {
      { SELABEL_OPT_PATH, "/file_contexts" }
    };

    sehandle = selabel_open(SELABEL_CTX_FILE, seopts, 1);

    if (!sehandle) {
        fprintf(stderr, "Warning: No file_contexts\n");
        ui_print("Warning:  No file_contexts\n");
    }

    LOGI("device_recovery_start()\n");
    device_recovery_start();

    printf("Command:");
    for (arg = 0; arg < argc; arg++) {
        printf(" \"%s\"", argv[arg]);
    }
    printf("\n");

    if (update_package) {
        // For backwards compatibility on the cache partition only, if
        // we're given an old 'root' path "CACHE:foo", change it to
        // "/cache/foo".
        if (strncmp(update_package, "CACHE:", 6) == 0) {
            int len = strlen(update_package) + 10;
            char* modified_path = malloc(len);
            strlcpy(modified_path, "/cache/", len);
            strlcat(modified_path, update_package+6, len);
            printf("(replacing path \"%s\" with \"%s\")\n",
                   update_package, modified_path);
            update_package = modified_path;
        }
    }
    printf("\n");

    property_list(print_property, NULL);
    printf("\n");

    int status = INSTALL_SUCCESS;

    if (update_package != NULL) {
        status = install_package(update_package);
        if (status != INSTALL_SUCCESS) {
            copy_logs();
            ui_print("Installation aborted.\n");
        }
    } else if (wipe_data) {
        if (device_wipe_data()) status = INSTALL_ERROR;
        preserve_data_media(0);
        if (erase_volume("/data")) status = INSTALL_ERROR;
        preserve_data_media(1);
        if (has_datadata() && erase_volume("/datadata")) status = INSTALL_ERROR;
        if (wipe_cache && erase_volume("/cache")) status = INSTALL_ERROR;
        if (status != INSTALL_SUCCESS) {
            copy_logs();
            ui_print("Data wipe failed.\n");
        }
    } else if (wipe_cache) {
        if (wipe_cache && erase_volume("/cache")) status = INSTALL_ERROR;
        if (status != INSTALL_SUCCESS) {
            copy_logs();
            ui_print("Cache wipe failed.\n");
        }
    } else {
        LOGI("Checking for extendedcommand...\n");
        status = INSTALL_ERROR;  // No command specified
        // we are starting up in user initiated recovery here
        // let's set up some default options
        signature_check_enabled = 0;
        is_user_initiated_recovery = 1;
        if (!headless) {
            ui_set_show_text(1);
            ui_set_background(BACKGROUND_ICON_CLOCKWORK);
        }

        if (extendedcommand_file_exists()) {
            LOGI("Running extendedcommand...\n");
            int ret;
            if (0 == (ret = run_and_remove_extendedcommand())) {
                status = INSTALL_SUCCESS;
                ui_set_show_text(0);
            }
            else {
                handle_failure(ret);
            }
        } else {
            LOGI("Skipping execution of extendedcommand, file not found...\n");
        }
    }

    if (sideload) {
        signature_check_enabled = 0;
        if (!headless)
            ui_set_show_text(1);
        if (0 == apply_from_adb()) {
            status = INSTALL_SUCCESS;
            ui_set_show_text(0);
        }
    }

    if (headless) {
        headless_wait();
    }
    if (status != INSTALL_SUCCESS && !is_user_initiated_recovery) {
        ui_set_show_text(1);
        ui_set_background(BACKGROUND_ICON_ERROR);
    }
    else if (status != INSTALL_SUCCESS || ui_text_visible()) {
        prompt_and_wait();
    }

    // We reach here when in main menu we choose reboot main system or for some wipe commands on start
    // If there is a radio image pending, reboot now to install it.
    maybe_install_firmware_update(send_intent);

    // Otherwise, get ready to boot the main system...
    finish_recovery(send_intent);
    ui_print("Rebooting...\n");
    reboot_main_system(ANDROID_RB_RESTART, 0, 0);

    return EXIT_SUCCESS;
}

void set_perf_mode(int on) {
    property_set("recovery.perf.mode", on ? "1" : "0");
}
