/*
 * Copyright (C) 2012 The Android Open Source Project
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

#include <unistd.h>
#include <dirent.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <signal.h>
#include <fcntl.h>

#include "ui.h"
#include "cutils/properties.h"
#include "install.h"
#include "common.h"
#include "adb_install.h"
#include "minadbd/fuse_adb_provider.h"
#include "fuse_sideload.h"

static RecoveryUI* ui = NULL;
static pthread_t sideload_thread;

static void
set_usb_driver(bool enabled) {
    int fd = open("/sys/class/android_usb/android0/enable", O_WRONLY);
    if (fd < 0) {
        ui->Print("failed to open driver control: %s\n", strerror(errno));
        return;
    }
    if (TEMP_FAILURE_RETRY(write(fd, enabled ? "1" : "0", 1)) == -1) {
        ui->Print("failed to set driver control: %s\n", strerror(errno));
    }
    if (close(fd) < 0) {
        ui->Print("failed to close driver control: %s\n", strerror(errno));
    }
}

static void
stop_adbd() {
    property_set("ctl.stop", "adbd");
    set_usb_driver(false);
}


static void
maybe_restart_adbd() {
    if (is_ro_debuggable()) {
        ui->Print("Restarting adbd...\n");
        set_usb_driver(true);
        property_set("ctl.start", "adbd");
    }
}

struct sideload_data {
    bool*       wipe_cache;
    const char* install_file;
    bool        cancel;
    int         result;
};

static struct sideload_data sideload_data;

// How long (in seconds) we wait for the host to start sending us a
// package, before timing out.
#define ADB_INSTALL_TIMEOUT 300

void *adb_sideload_thread(void* v) {
    pid_t child;
    if ((child = fork()) == 0) {
        execl("/sbin/recovery", "recovery", "--adbd", NULL);
        _exit(-1);
    }

    time_t start_time = time(NULL);
    time_t now = start_time;

    // FUSE_SIDELOAD_HOST_PATHNAME will start to exist once the host
    // connects and starts serving a package.  Poll for its
    // appearance.  (Note that inotify doesn't work with FUSE.)
    int result = INSTALL_NONE;
    int status = -1;
    struct stat st;
    while (now - start_time < ADB_INSTALL_TIMEOUT) {
        /*
         * Exit if either:
         *  - The adb child process dies, or
         *  - The ui tells us to cancel
         */
        if (kill(child, 0) != 0) {
            result = INSTALL_ERROR;
            break;
        }

        if (sideload_data.cancel) {
            break;
        }

        status = stat(FUSE_SIDELOAD_HOST_PATHNAME, &st);
        if (status == 0) {
            break;
        }
        if (errno != ENOENT && errno != ENOTCONN) {
            ui->Print("\nError %s waiting for package\n\n", strerror(errno));
            result = INSTALL_ERROR;
            break;
        }

        sleep(1);
        now = time(NULL);
    }

    if (status == 0) {
        // Signal UI thread that we can no longer cancel
        ui->CancelWaitKey();

        result = install_package(FUSE_SIDELOAD_HOST_PATHNAME,
                                 sideload_data.wipe_cache,
                                 sideload_data.install_file,
                                 false);

        sideload_data.result = result;
    }

    // Ensure adb exits
    kill(child, SIGTERM);
    waitpid(child, &status, 0);

    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
        if (WEXITSTATUS(status) == 3) {
            ui->Print("\nYou need adb 1.0.32 or newer to sideload\nto this device.\n\n");
        } else if (!WIFSIGNALED(status)) {
            ui->Print("\n(adbd status %d)\n", WEXITSTATUS(status));
        }
    }

    LOGI("sideload thread finished\n");
    return NULL;
}

void
start_sideload(RecoveryUI* ui_, bool* wipe_cache, const char* install_file) {
    modified_flash = true;

    ui = ui_;

    stop_adbd();
    set_usb_driver(true);

    ui->Print("\n\nNow send the package you want to apply\n"
              "to the device with \"adb sideload <filename>\"...\n");

    sideload_data.wipe_cache = wipe_cache;
    sideload_data.install_file = install_file;
    sideload_data.cancel = false;
    sideload_data.result = INSTALL_NONE;

    pthread_create(&sideload_thread, NULL, &adb_sideload_thread, NULL);
}

void stop_sideload() {
    sideload_data.cancel = true;
}

int wait_sideload() {
    set_perf_mode(true);

    pthread_join(sideload_thread, NULL);

    ui->FlushKeys();

    maybe_restart_adbd();

    set_perf_mode(false);

    return sideload_data.result;
}
