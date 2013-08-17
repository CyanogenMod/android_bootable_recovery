/*
 * Copyright (c) 2013 The CyanogenMod Project
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

#include "voldclient.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <pthread.h>

#include <sys/socket.h>
#include <sys/select.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/un.h>

#include <cutils/sockets.h>
#include <private/android_filesystem_config.h>

#include "common.h"

// writer
static pthread_mutex_t cmd_mutex    = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t  cmd_complete = PTHREAD_COND_INITIALIZER;

// command result status, read with mutex held
static int cmd_result = 0;

// commands currently in flight
static int cmd_inflight = 0;

// socket fd
static int sock = -1;


static int vold_connect() {

    int ret = 1;
    if (sock > 0) {
        return ret;
    }

    // socket connection to vold
    if ((sock = socket_local_client("vold",
                                     ANDROID_SOCKET_NAMESPACE_RESERVED,
                                     SOCK_STREAM)) < 0) {
        LOGE("Error connecting to Vold! (%s)\n", strerror(errno));
        ret = -1;
    } else {
        LOGI("Connected to Vold..\n");
    }
    return ret;
}

static int split(char *str, char **splitstr) {      

    char *p;      
    int i = 0;      

    p = strtok(str, " ");      

    while(p != NULL) {
        splitstr[i] = malloc(strlen(p) + 1);
        if (splitstr[i])
            strcpy(splitstr[i], p);
        i++;
        p = strtok (NULL, " ");       
    }
    
    return i; 
}

extern int vold_dispatch(int code, char** tokens, int len);

static int handle_response(char* response) {

    int code = 0, len = 0, i = 0;
    char *tokens[32] = { NULL };

    len = split(response, tokens);
    code = atoi(tokens[0]);

    if (len) {
        vold_dispatch(code, tokens, len);

        for (i = 0; i < len; i++)
            free(tokens[i]);
    }

    return code;
}

// wait for events and signal waiters when appropriate
static int monitor() {

    char *buffer = malloc(4096);
    int code = 0;

    while(1) {
        fd_set read_fds;
        struct timeval to;
        int rc = 0;

        to.tv_sec = 10;
        to.tv_usec = 0;

        FD_ZERO(&read_fds);
        FD_SET(sock, &read_fds);

        if ((rc = select(sock +1, &read_fds, NULL, NULL, &to)) < 0) {
            LOGE("Error in select (%s)\n", strerror(errno));
            goto out;

        } else if (!rc) {
            continue;

        } else if (FD_ISSET(sock, &read_fds)) {
            memset(buffer, 0, 4096);
            if ((rc = read(sock, buffer, 4096)) <= 0) {
                if (rc == 0)
                    LOGE("Lost connection to Vold - did it crash?\n");
                else
                    LOGE("Error reading data (%s)\n", strerror(errno));
                if (rc == 0)
                    return ECONNRESET;
                goto out;
            }

            int offset = 0;
            int i = 0;

            // dispatch each line of the response
            for (i = 0; i < rc; i++) {
                if (buffer[i] == '\0') {

                    LOGD("%s\n", buffer + offset);
                    code = handle_response(strdup(buffer + offset));

                    if (code >= 200 && code < 600) {
                        // signal the waiter and let it unlock
                        cmd_result = code;
                        pthread_cond_signal(&cmd_complete);
                        pthread_mutex_lock(&cmd_mutex);
                        cmd_inflight--;
                        pthread_mutex_unlock(&cmd_mutex);
                    }
                    offset = i + 1;
                }
            }
        }
    }
out:
    free(buffer);
    return 0;
}

static void *event_thread_func(void* v) {

    // if monitor() returns, it means we lost the connection to vold
    while (1) {

        if (vold_connect()) {
            monitor();

            if (sock)
                close(sock);
        }
        sleep(3);
    }
    return NULL;
}

extern void vold_set_callbacks(struct vold_callbacks* callbacks);
extern void vold_set_automount(int automount);

// start the client thread
void vold_client_start(struct vold_callbacks* callbacks, int automount) {

    if (sock > 0) {
        return;
    }

    vold_set_callbacks(callbacks);
    vold_set_automount(automount);

    pthread_t vold_event_thread;
    pthread_create(&vold_event_thread, NULL, &event_thread_func, NULL);

    vold_scan_volumes(0);
}

// send a command to vold. waits for completion and returns result
// code if wait is 1, otherwise returns zero immediately.
int vold_command(int len, const char** command, int wait) {

    char final_cmd[255] = "0 "; /* 0 is a (now required) sequence number */
    int i;
    size_t sz;
    int ret = 0;

    if (!vold_connect()) {
        return -1;
    }

    for (i = 0; i < len; i++) {
        char *cmp;

        if (!index(command[i], ' '))
            asprintf(&cmp, "%s%s", command[i], (i == (len -1)) ? "" : " ");
        else
            asprintf(&cmp, "\"%s\"%s", command[i], (i == (len -1)) ? "" : " ");

        sz = strlcat(final_cmd, cmp, sizeof(final_cmd));

        if (sz >= sizeof(final_cmd)) {
            LOGE("command syntax error  sz=%d size=%d", sz, sizeof(final_cmd));
            free(cmp);
            return -1;
        }
        free(cmp);
    }

    // only one writer at a time
    pthread_mutex_lock(&cmd_mutex);
    if (write(sock, final_cmd, strlen(final_cmd) + 1) < 0) {
        LOGE("Unable to send command to vold!\n");
        ret = -1;
    }
    cmd_inflight++;

    if (wait) {
        // wait for completion
        pthread_cond_wait(&cmd_complete, &cmd_mutex);
        ret = cmd_result;
    }
    pthread_mutex_unlock(&cmd_mutex);

    return ret;
}
