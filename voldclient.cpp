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

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <pthread.h>

#include <string>
#include <sstream>

#include <cutils/properties.h>
#include <cutils/sockets.h>

#include "common.h"
#include "roots.h"
#include "voldclient.h"

#include "VolumeBase.h"
#include "ResponseCode.h"

using namespace android::vold;

VoldClient* vdc = NULL;

static void* threadfunc(void* arg)
{
    VoldClient* self = (VoldClient*)arg;
    self->run();
    return NULL;
}

VoldClient::VoldClient(VoldWatcher* watcher /* = nullptr */) :
    mRunning(false),
    mSock(-1),
    mSockMutex(PTHREAD_MUTEX_INITIALIZER),
    mSockCond(PTHREAD_COND_INITIALIZER),
    mInFlight(0),
    mResult(0),
    mWatcher(watcher),
    mVolumeLock(PTHREAD_RWLOCK_INITIALIZER),
    mVolumeChanged(false),
    mEmulatedStorage(true)
{
}

void VoldClient::start(void)
{
    mRunning = true;
    pthread_create(&mThread, NULL, threadfunc, this);
    while (mSock == -1) {
        sleep(1);
    }
    while (mInFlight != 0) {
        sleep(1);
    }
    LOGI("VoldClient initialized, storage is %s\n",
            vdc->isEmulatedStorage() ? "emulated" : "physical");
}

void VoldClient::stop(void)
{
    if (mRunning) {
        mRunning = false;
        close(mSock);
        mSock = -1;
        void* retval;
        pthread_join(mThread, &retval);
    }
}

VolumeInfo VoldClient::getVolume(const std::string& id)
{
    pthread_rwlock_wrlock(&mVolumeLock);
    VolumeInfo* info = getVolumeLocked(id);
    pthread_rwlock_unlock(&mVolumeLock);
    return *info;
}

bool VoldClient::reset(void)
{
    const char *cmd[2] = { "volume", "reset" };
    return sendCommand(2, cmd);
}

bool VoldClient::mountAll(void)
{
    bool ret = true;
    pthread_rwlock_rdlock(&mVolumeLock);
    for (auto& info : mVolumes) {
        if (info.mState == (int)VolumeBase::State::kUnmounted) {
            if (!volumeMount(info.mId)) {
                ret = false;
            }
        }
    }
    pthread_rwlock_unlock(&mVolumeLock);
    return ret;
}

bool VoldClient::unmountAll(void)
{
    bool ret = true;
    pthread_rwlock_rdlock(&mVolumeLock);
    for (auto& info : mVolumes) {
        if (info.mState == (int)VolumeBase::State::kMounted) {
            if (!volumeUnmount(info.mId)) {
                ret = false;
            }
        }
    }
    pthread_rwlock_unlock(&mVolumeLock);
    return ret;
}

bool VoldClient::volumeMount(const std::string& id)
{
    // Special case for emulated storage
    if (id == "emulated") {
        pthread_rwlock_wrlock(&mVolumeLock);
        VolumeInfo* info = getVolumeLocked(id);
        if (!info) {
            pthread_rwlock_unlock(&mVolumeLock);
            return false;
        }
        info->mPath = "/storage/emulated";
        info->mInternalPath = "/data/media";
        pthread_rwlock_unlock(&mVolumeLock);
        return ensure_path_mounted("/data") == 0;
    }
    const char *cmd[3] = { "volume", "mount", id.c_str() };
    return sendCommand(3, cmd);
}

// NB: can only force or detach, not both
bool VoldClient::volumeUnmount(const std::string& id, bool detach /* = false */)
{
    // Special case for emulated storage
    if (id == "emulated") {
        if (ensure_path_unmounted("/data", detach) != 0) {
            return false;
        }
        return true;
    }
    const char *cmd[4] = { "volume", "unmount", id.c_str(), NULL };
    int cmdlen = 3;
    if (detach) {
        cmd[3] = "detach";
        cmdlen = 4;
    }
    return sendCommand(cmdlen, cmd);
}

bool VoldClient::volumeFormat(const std::string& id)
{
    const char* cmd[3] = { "volume", "format", id.c_str() };
    return sendCommand(3, cmd);
}

void VoldClient::resetVolumeState(void)
{
    pthread_rwlock_wrlock(&mVolumeLock);
    mVolumes.clear();
    mVolumeChanged = false;
    mEmulatedStorage = true;
    pthread_rwlock_unlock(&mVolumeLock);
    if (mWatcher) {
        mWatcher->onVolumeChanged();
    }
    const char *cmd[2] = { "volume", "reset" };
    sendCommand(2, cmd, false);
}

VolumeInfo* VoldClient::getVolumeLocked(const std::string& id)
{
    for (auto iter = mVolumes.begin(); iter != mVolumes.end(); ++iter) {
        if (iter->mId == id) {
            return &(*iter);
        }
    }
    return nullptr;
}

bool VoldClient::sendCommand(unsigned int len, const char** command, bool wait /* = true */)
{
    char line[4096];
    char* p;
    unsigned int i;
    size_t sz;
    bool ret = true;

    p = line;
    p += sprintf(p, "0 "); /* 0 is a (now required) sequence number */
    for (i = 0; i < len; i++) {
        const char* cmd = command[i];
        if (!cmd[0] || !strchr(cmd, ' '))
            p += sprintf(p, "%s", cmd);
        else
            p += sprintf(p, "\"%s\"", cmd);
        if (i < len - 1)
            *p++ = ' ';
        if (p >= line + sizeof(line)) {
            LOGE("vold command line too long\n");
            exit(1);
        }
    }

    // only one writer at a time
    pthread_mutex_lock(&mSockMutex);
    if (write(mSock, line, (p - line) + 1) < 0) {
        LOGE("Unable to send command to vold!\n");
        pthread_mutex_unlock(&mSockMutex);
        return false;
    }
    ++mInFlight;

    if (wait) {
        while (mInFlight) {
            // wait for completion
            pthread_cond_wait(&mSockCond, &mSockMutex);
        }
        ret = (mResult >= 200 && mResult < 300);
    }
    pthread_mutex_unlock(&mSockMutex);

    return ret;
}

void VoldClient::handleCommandOkay(void)
{
    bool changed = false;
    pthread_rwlock_wrlock(&mVolumeLock);
    if (mVolumeChanged) {
        mVolumeChanged = false;
        changed = true;
    }
    pthread_rwlock_unlock(&mVolumeLock);
    if (changed) {
        mWatcher->onVolumeChanged();
    }
}

void VoldClient::handleVolumeCreated(const std::string& id, const std::string& type,
        const std::string& disk, const std::string& guid)
{
    pthread_rwlock_wrlock(&mVolumeLock);
    // Ignore emulated storage if primary storage is physical
    if (id == "emulated") {
        char value[PROPERTY_VALUE_MAX];
        property_get("ro.vold.primary_physical", value, "0");
        if (value[0] == '1' || value[0] == 'y' || !strcmp(value, "true")) {
            mEmulatedStorage = false;
            return;
        }
        mEmulatedStorage = true;
    }
    VolumeInfo info;
    info.mId = id;
    mVolumes.push_back(info);
    pthread_rwlock_unlock(&mVolumeLock);
}

void VoldClient::handleVolumeStateChanged(const std::string& id, const std::string& state)
{
    pthread_rwlock_wrlock(&mVolumeLock);
    auto info = getVolumeLocked(id);
    if (info) {
        info->mState = atoi(state.c_str());
    }
    pthread_rwlock_unlock(&mVolumeLock);
}

void VoldClient::handleVolumeFsLabelChanged(const std::string& id, const std::string& label)
{
    pthread_rwlock_wrlock(&mVolumeLock);
    auto info = getVolumeLocked(id);
    if (info) {
        info->mLabel = label;
    }
    pthread_rwlock_unlock(&mVolumeLock);
}

void VoldClient::handleVolumePathChanged(const std::string& id, const std::string& path)
{
    pthread_rwlock_wrlock(&mVolumeLock);
    auto info = getVolumeLocked(id);
    if (info) {
        info->mPath = path;
    }
    pthread_rwlock_unlock(&mVolumeLock);
}

void VoldClient::handleVolumeInternalPathChanged(const std::string& id, const std::string& path)
{
    pthread_rwlock_wrlock(&mVolumeLock);
    auto info = getVolumeLocked(id);
    if (info) {
        info->mInternalPath = path;
    }
    pthread_rwlock_unlock(&mVolumeLock);
}

void VoldClient::handleVolumeDestroyed(const std::string& id)
{
    pthread_rwlock_wrlock(&mVolumeLock);
    for (auto iter = mVolumes.begin(); iter != mVolumes.end(); ++iter) {
        if (iter->mId == id) {
            mVolumes.erase(iter);
            break;
        }
    }
    pthread_rwlock_unlock(&mVolumeLock);
}

static std::vector<std::string> split(const std::string& line)
{
    std::vector<std::string> tokens;
    const char* tok = line.c_str();

    while (*tok) {
        unsigned int toklen;
        const char* next;
        if (*tok == '"') {
            ++tok;
            const char* q = strchr(tok, '"');
            if (!q) {
                LOGE("vold line <%s> malformed\n", line.c_str());
                exit(1);
            }
            toklen = q - tok;
            next = q + 1;
            if (*next) {
                if (*next != ' ') {
                    LOGE("vold line <%s> malformed\n", line.c_str());
                    exit(0);
                }
                ++next;
            }
        }
        else {
            next = strchr(tok, ' ');
            if (next) {
                toklen = next - tok;
                ++next;
            }
            else {
                toklen = strlen(tok);
                next = tok + toklen;
            }
        }
        tokens.push_back(std::string(tok, toklen));
        tok = next;
    }

    return tokens;
}

void VoldClient::dispatch(const std::string& line)
{
    std::vector<std::string> tokens = split(line);

    switch (mResult) {
    case ResponseCode::CommandOkay:
        handleCommandOkay();
        break;
    case ResponseCode::VolumeCreated:
        handleVolumeCreated(tokens[1], tokens[2], tokens[3], tokens[4]);
        break;
    case ResponseCode::VolumeStateChanged:
        handleVolumeStateChanged(tokens[1], tokens[2]);
        break;
    case ResponseCode::VolumeFsLabelChanged:
        handleVolumeFsLabelChanged(tokens[1], tokens[2]);
        break;
    case ResponseCode::VolumePathChanged:
        handleVolumePathChanged(tokens[1], tokens[2]);
        break;
    case ResponseCode::VolumeInternalPathChanged:
        handleVolumeInternalPathChanged(tokens[1], tokens[2]);
        break;
    case ResponseCode::VolumeDestroyed:
        handleVolumeDestroyed(tokens[1]);
        break;
    }
}

void VoldClient::run(void)
{
    LOGI("VoldClient thread starting\n");
    while (mRunning) {
        if (mSock == -1) {
            LOGI("Connecting to Vold...\n");
            mSock = socket_local_client("vold", ANDROID_SOCKET_NAMESPACE_RESERVED, SOCK_STREAM);
            if (mSock == -1) {
                sleep(1);
                continue;
            }
            resetVolumeState();
        }

        int rc;

        struct timeval tv;
        fd_set rfds;

        memset(&tv, 0, sizeof(tv));
        tv.tv_usec = 100 * 1000;
        FD_ZERO(&rfds);
        FD_SET(mSock, &rfds);

        rc = select(mSock + 1, &rfds, NULL, NULL, &tv);
        if (rc <= 0) {
            if (rc < 0 && errno != EINTR) {
                LOGE("vdc: error in select (%s)\n", strerror(errno));
                close(mSock);
                mSock = -1;
            }
            continue;
        }

        char buf[4096];
        memset(buf, 0, sizeof(buf));
        rc = read(mSock, buf, sizeof(buf) - 1);
        if (rc <= 0) {
            LOGE("vdc: read failed: %s\n", (rc == 0 ? "EOF" : strerror(errno)));
            close(mSock);
            mSock = -1;
            continue;
        }

        // dispatch each line of the response
        int nread = rc;
        int off = 0;
        while (off < nread) {
            char* eol = (char*)memchr(buf + off, 0, nread - off);
            if (!eol) {
                break;
            }
            mResult = atoi(buf + off);
            dispatch(std::string(buf + off));
            if (mResult >= 200 && mResult < 600) {
                pthread_mutex_lock(&mSockMutex);
                --mInFlight;
                pthread_cond_signal(&mSockCond);
                pthread_mutex_unlock(&mSockMutex);
            }
            off = (eol - buf) + 1;
        }
    }
}
