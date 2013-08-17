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
static pthread_mutex_t vold_write_mutex = PTHREAD_MUTEX_INITIALIZER;

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
        printf("%s", p);
        splitstr[i] = malloc(strlen(p) + 1);
        if (splitstr[i])
            strcpy(splitstr[i], p);
        i++;
        p = strtok (NULL, " ");       
    }
    
    return i; 
}

static int handle_response(char* response) {

    int ret = -1, len = 0, i = 0;
    char *tokens[32] = { NULL };

    len = split(response, tokens);
    
    if (len) {
        ret = vold_dispatch(atoi(tokens[0]), tokens, len);

        for (i = 0; i < len; i++)
            free(tokens[i]);
    }

    return ret;
}

// wait for events, return final response code if oneshot
static int monitor(int oneshot) {

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

                    code = handle_response(strdup(buffer + offset));

                    if (oneshot) {
                        if (code >= 200 && code < 600)
                            goto out;
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

    // if monitor() returns, vold is dead :(
    while (1) {
        monitor(0);

        if (sock)
            close(sock);

        sleep(3);
    }
    return NULL;
}

// start the client thread
void vold_client_start() {

    if (sock > 0) {
        return;
    }

    pthread_t vold_event_thread;
    pthread_create(&vold_event_thread, NULL, &event_thread_func, NULL);
}

// send a command to vold.  returns response code if sync or zero if async
int vold_command(int len, const char** command, int sync) {

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

    // only one writer or sync client at a time
    pthread_mutex_lock(&vold_write_mutex);
    if (write(sock, final_cmd, strlen(final_cmd) + 1) < 0) {
        LOGE("Unable to send command to vold!\n");
        ret = -1;
    }

    if (sync) {
        ret = monitor(1);
    }
    pthread_mutex_unlock(&vold_write_mutex);

    return ret;
}
