# Copyright (C) 2009 The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_SRC_FILES := \
    ../../../external/fuse/android/statvfs.c \
    ../../../external/fuse/lib/buffer.c \
    ../../../external/fuse/lib/cuse_lowlevel.c \
    fuse.c \
    ../../../external/fuse/lib/fuse_kern_chan.c \
    ../../../external/fuse/lib/fuse_loop.c \
    ../../../external/fuse/lib/fuse_loop_mt.c \
    ../../../external/fuse/lib/fuse_lowlevel.c \
    ../../../external/fuse/lib/fuse_mt.c \
    ../../../external/fuse/lib/fuse_opt.c \
    ../../../external/fuse/lib/fuse_session.c \
    ../../../external/fuse/lib/fuse_signals.c \
    ../../../external/fuse/lib/helper.c \
    ../../../external/fuse/lib/mount.c \
    ../../../external/fuse/lib/mount_util.c \
    ../../../external/fuse/lib/ulockmgr.c

LOCAL_C_INCLUDES := \
    external/fuse/android \
    external/fuse/include

LOCAL_STATIC_LIBRARIES := \
    libc

LOCAL_CFLAGS := \
    -D_FILE_OFFSET_BITS=64 \
    -DFUSE_USE_VERSION=26

LOCAL_MODULE := libfuse.recovery
LOCAL_MODULE_TAGS := optional

include $(BUILD_STATIC_LIBRARY)
