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
extern "C" {
#include "minadbd/fuse_adb_provider.h"
#include "fuse_sideload.h"
}

static RecoveryUI* ui = NULL;
static pthread_t sideload_thread;

static void
set_usb_driver(bool enabled) {
    int fd = open("/sys/class/android_usb/android0/enable", O_WRONLY);
    if (fd < 0) {
        ui->Print("failed to open driver control: %s\n", strerror(errno));
        return;
    }
    if (write(fd, enabled ? "1" : "0", 1) < 0) {
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
    char value[PROPERTY_VALUE_MAX+1];
    int len = property_get("ro.debuggable", value, NULL);
    if (len == 1 && value[0] == '1') {
        ui->Print("Restarting adbd...\n");
        set_usb_driver(true);
        property_set("ctl.start", "adbd");
    }
}

struct sideload_data {
    int*        wipe_cache;
    const char* install_file;
    bool        joined;
    bool        canceled;
    int         result;

    pthread_mutex_t cancel_mutex;
    pthread_cond_t  cancel_cond;
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

    // FUSE_SIDELOAD_HOST_PATHNAME will start to exist once the host
    // connects and starts serving a package.  Poll for its
    // appearance.  (Note that inotify doesn't work with FUSE.)
    int result;
    int status;
    struct stat st;
    for (int i = 0; i < ADB_INSTALL_TIMEOUT; ++i) {
        if (kill(child, 0) != 0) {
            result = INSTALL_ERROR;
            break;
        }

        if (stat(FUSE_SIDELOAD_HOST_PATHNAME, &st) != 0) {
            int err = errno;
            printf("%s: stat sideload pathname returned errno=%d (%s)\n", __func__, errno, strerror(errno));
            if (err == ENOENT && i < ADB_INSTALL_TIMEOUT-1) {
                printf("%s: sleeping\n", __func__);

                struct timeval now;
                struct timespec timeout;
                gettimeofday(&now, NULL);
                timeout.tv_sec = now.tv_sec;
                timeout.tv_nsec = now.tv_usec * 1000;
                timeout.tv_sec += 1;

                pthread_mutex_lock(&sideload_data.cancel_mutex);
                pthread_cond_timedwait(&sideload_data.cancel_cond, &sideload_data.cancel_mutex, &timeout);
                pthread_mutex_unlock(&sideload_data.cancel_mutex);

                if (sideload_data.canceled) {
                    result = INSTALL_NONE;
                    kill(child, SIGKILL);
                    break;
                } else {
                    continue;
                }
            } else {
                ui->Print("\nTimed out waiting for package.\n\n", strerror(errno));
                result = INSTALL_ERROR;
                kill(child, SIGKILL);
                break;
            }
        }

        result = install_package(FUSE_SIDELOAD_HOST_PATHNAME,
                                 sideload_data.wipe_cache,
                                 sideload_data.install_file,
                                 false);
        break;
    }

    sideload_data.result = result;

    // Calling stat() on this magic filename signals the minadbd
    // subprocess to shut down.
    stat(FUSE_SIDELOAD_HOST_EXIT_PATHNAME, &st);

    // TODO(dougz): there should be a way to cancel waiting for a
    // package (by pushing some button combo on the device).  For now
    // you just have to 'adb sideload' a file that's not a valid
    // package, like "/dev/null".
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
start_sideload(RecoveryUI* ui_, int* wipe_cache, const char* install_file) {
    ui = ui_;

    stop_adbd();
    set_usb_driver(true);

    ui->Print("\n\nNow send the package you want to apply\n"
              "to the device with \"adb sideload <filename>\"...\n");

    sideload_data.wipe_cache = wipe_cache;
    sideload_data.install_file = install_file;
    sideload_data.joined = false;
    sideload_data.result = 0;
    sideload_data.canceled = false;
    pthread_mutex_init(&sideload_data.cancel_mutex, NULL);
    pthread_cond_init(&sideload_data.cancel_cond, NULL);

    pthread_create(&sideload_thread, NULL, &adb_sideload_thread, NULL);
}

void wait_sideload() {
    if (!sideload_data.joined) {
        pthread_join(sideload_thread, NULL);
        sideload_data.joined = true;
    }
}

int stop_sideload() {
    set_perf_mode(true);

    // Calling stat() on this magic filename signals the minadbd
    // subprocess to shut down.
    struct stat st;
    stat(FUSE_SIDELOAD_HOST_EXIT_PATHNAME, &st);

    pthread_mutex_lock(&sideload_data.cancel_mutex);
    sideload_data.canceled = true;
    pthread_cond_signal(&sideload_data.cancel_cond);
    pthread_mutex_unlock(&sideload_data.cancel_mutex);

    if (!sideload_data.joined) {
        pthread_join(sideload_thread, NULL);
        sideload_data.joined = true;
    }
    ui->FlushKeys();

    maybe_restart_adbd();

    set_perf_mode(false);

    return sideload_data.result;
}
