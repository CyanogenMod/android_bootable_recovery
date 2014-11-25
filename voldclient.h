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
#ifndef _VOLD_CLIENT_H
#define _VOLD_CLIENT_H

#include <sys/types.h>
#include <pthread.h>

#include <string>
#include <vector>

class VoldWatcher {
public:
    virtual ~VoldWatcher(void) {}
    virtual void onVolumeChanged(void) = 0;
};

class VolumeInfo {
public:
    std::string     mId;
    std::string     mLabel;
    std::string     mPath;
    std::string     mInternalPath;
    int             mState;
};

class VoldClient
{
public:
    VoldClient(VoldWatcher* watcher = nullptr);

    void start(void);
    void stop(void);

    std::vector<VolumeInfo> getVolumes(void) { return mVolumes; }
    VolumeInfo getVolume(const std::string& id);

    bool isEmulatedStorage(void) { return mEmulatedStorage; }

    bool reset(void);
    bool mountAll(void);
    bool unmountAll(void);

    bool volumeMount(const std::string& id);
    bool volumeUnmount(const std::string& id, bool detach = false);
    bool volumeFormat(const std::string& id);
    bool volumeAvailable(const std::string& id);

private:
    void resetVolumeState(void);

    VolumeInfo* getVolumeLocked(const std::string& id);
    bool sendCommand(unsigned int len, const char** command, bool wait = true);

    void handleCommandOkay(void);
    void handleVolumeCreated(const std::string& id, const std::string& type,
            const std::string& disk, const std::string& guid);
    void handleVolumeStateChanged(const std::string& id, const std::string& state);
    void handleVolumeFsLabelChanged(const std::string& id, const std::string& label);
    void handleVolumePathChanged(const std::string& id, const std::string& path);
    void handleVolumeInternalPathChanged(const std::string& id, const std::string& path);
    void handleVolumeDestroyed(const std::string& id);

    void dispatch(const std::string& line);

public:
    void run(void); // INTERNAL

private:
    bool                        mRunning;
    int                         mSock;
    pthread_t                   mThread;
    pthread_mutex_t             mSockMutex;
    pthread_cond_t              mSockCond;
    unsigned int                mInFlight;
    unsigned int                mResult;
    VoldWatcher*                mWatcher;
    pthread_rwlock_t            mVolumeLock;
    bool                        mVolumeChanged;
    bool                        mEmulatedStorage;
    std::vector<VolumeInfo>     mVolumes;
};

extern VoldClient* vdc;

#endif

