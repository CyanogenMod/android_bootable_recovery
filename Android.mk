# Copyright (C) 2007 The Android Open Source Project
# Copyright (C) 2015 The CyanogenMod Project
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

ifeq ($(call my-dir),$(call project-path-for,recovery))

LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_SRC_FILES := fuse_sideload.cpp
LOCAL_CLANG := true
LOCAL_CFLAGS := -O2 -g -DADB_HOST=0 -Wall -Wno-unused-parameter
LOCAL_CFLAGS += -D_XOPEN_SOURCE -D_GNU_SOURCE

LOCAL_MODULE := libfusesideload

LOCAL_STATIC_LIBRARIES := libcutils libc libcrypto_static
include $(BUILD_STATIC_LIBRARY)

include $(CLEAR_VARS)

LOCAL_SRC_FILES := \
    adb_install.cpp \
    asn1_decoder.cpp \
    bootloader.cpp \
    device.cpp \
    fuse_sdcard_provider.cpp \
    install.cpp \
    recovery.cpp \
    roots.cpp \
    screen_ui.cpp \
    ui.cpp \
    verifier.cpp \
    wear_ui.cpp \
    wear_touch.cpp \

# External tools
LOCAL_SRC_FILES += \
    ../../system/core/toolbox/newfs_msdos.c \
    ../../system/core/toolbox/start.c \
    ../../system/core/toolbox/stop.c

LOCAL_MODULE := recovery

LOCAL_FORCE_STATIC_EXECUTABLE := true

LOCAL_REQUIRED_MODULES := mkfs.f2fs

RECOVERY_API_VERSION := 3
RECOVERY_FSTAB_VERSION := 2
LOCAL_CFLAGS += -DRECOVERY_API_VERSION=$(RECOVERY_API_VERSION)
LOCAL_CFLAGS += -Wno-unused-parameter
LOCAL_CLANG := true

LOCAL_C_INCLUDES += \
    system/vold \
    system/extras/ext4_utils \
    system/core/adb \

LOCAL_STATIC_LIBRARIES := \
    libbatterymonitor \
    libext4_utils_static \
    libmake_ext4fs_static \
    libminizip_static \
    libminiunz_static \
    libsparse_static \
    libfsck_msdos \
    libminipigz_static \
    libzopfli \
    libreboot_static \
    libminzip \
    libz \
    libmtdutils \
    libminadbd \
    libtoybox_driver \
    libmksh_static \
    libfusesideload \
    libminui \
    libpng \
    libfs_mgr \
    libcrypto_static \
    libbase \
    libcutils \
    libutils \
    liblog \
    libselinux \
    libc++_static \
    libm \
    libc \
    libext2_blkid \
    libext2_uuid

LOCAL_HAL_STATIC_LIBRARIES := libhealthd

LOCAL_MODULE_PATH := $(TARGET_RECOVERY_ROOT_OUT)/sbin

ifeq ($(TARGET_USE_MDTP), true)
    LOCAL_CFLAGS += -DUSE_MDTP
endif

ifeq ($(TARGET_RECOVERY_UI_LIB),)
  LOCAL_SRC_FILES += default_device.cpp
else
  LOCAL_STATIC_LIBRARIES += $(TARGET_RECOVERY_UI_LIB)
endif

ifeq ($(BOARD_CACHEIMAGE_PARTITION_SIZE),)
LOCAL_REQUIRED_MODULES := recovery-persist recovery-refresh
endif

LOCAL_C_INCLUDES += system/extras/ext4_utils
LOCAL_C_INCLUDES += external/boringssl/include

ifeq ($(ONE_SHOT_MAKEFILE),)
LOCAL_ADDITIONAL_DEPENDENCIES += \
    fstools \
    recovery_mkshrc

endif

TOYBOX_INSTLIST := $(HOST_OUT_EXECUTABLES)/toybox-instlist
LOCAL_ADDITIONAL_DEPENDENCIES += toybox_recovery_links

# Set up the static symlinks
RECOVERY_TOOLS := \
    gunzip gzip make_ext4fs reboot setup_adbd sh start stop toybox unzip zip
LOCAL_POST_INSTALL_CMD := \
	$(hide) $(foreach t,$(RECOVERY_TOOLS),ln -sf recovery $(TARGET_RECOVERY_ROOT_OUT)/sbin/$(t);)

include $(BUILD_EXECUTABLE)

# Run toybox-instlist and generate the rest of the symlinks
toybox_recovery_links: $(TOYBOX_INSTLIST) recovery
toybox_recovery_links: TOY_LIST=$(shell $(TOYBOX_INSTLIST))
toybox_recovery_links: TOYBOX_BINARY := $(TARGET_RECOVERY_ROOT_OUT)/sbin/toybox
toybox_recovery_links:
	@echo -e ${CL_CYN}"Generate Toybox links:"${CL_RST} $(TOY_LIST)
	@mkdir -p $(TARGET_RECOVERY_ROOT_OUT)/sbin
	$(hide) $(foreach t,$(TOY_LIST),ln -sf toybox $(TARGET_RECOVERY_ROOT_OUT)/sbin/$(t);)

# mkshrc
include $(CLEAR_VARS)
LOCAL_MODULE := recovery_mkshrc
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_CLASS := ETC
LOCAL_MODULE_PATH := $(TARGET_RECOVERY_ROOT_OUT)/etc
LOCAL_SRC_FILES := etc/mkshrc
LOCAL_MODULE_STEM := mkshrc
include $(BUILD_PREBUILT)

# make_ext4fs
include $(CLEAR_VARS)
LOCAL_MODULE := libmake_ext4fs_static
LOCAL_MODULE_TAGS := optional
LOCAL_CFLAGS := -Dmain=make_ext4fs_main
LOCAL_SRC_FILES := \
    ../../system/extras/ext4_utils/make_ext4fs_main.c \
    ../../system/extras/ext4_utils/canned_fs_config.c
include $(BUILD_STATIC_LIBRARY)

# Minizip static library
include $(CLEAR_VARS)
LOCAL_MODULE := libminizip_static
LOCAL_MODULE_TAGS := optional
LOCAL_CFLAGS := -Dmain=minizip_main -D__ANDROID__ -DIOAPI_NO_64
LOCAL_C_INCLUDES := external/zlib
LOCAL_SRC_FILES := \
    ../../external/zlib/src/contrib/minizip/ioapi.c \
    ../../external/zlib/src/contrib/minizip/minizip.c \
    ../../external/zlib/src/contrib/minizip/zip.c
include $(BUILD_STATIC_LIBRARY)

# Miniunz static library
include $(CLEAR_VARS)
LOCAL_MODULE := libminiunz_static
LOCAL_MODULE_TAGS := optional
LOCAL_CFLAGS := -Dmain=miniunz_main -D__ANDROID__ -DIOAPI_NO_64
LOCAL_C_INCLUDES := external/zlib
LOCAL_SRC_FILES := \
    ../../external/zlib/src/contrib/minizip/ioapi.c \
    ../../external/zlib/src/contrib/minizip/miniunz.c \
    ../../external/zlib/src/contrib/minizip/unzip.c
include $(BUILD_STATIC_LIBRARY)

# Reboot static library
include $(CLEAR_VARS)
LOCAL_MODULE := libreboot_static
LOCAL_MODULE_TAGS := optional
LOCAL_CFLAGS := -Dmain=reboot_main
LOCAL_SRC_FILES := ../../system/core/reboot/reboot.c
include $(BUILD_STATIC_LIBRARY)


# recovery-persist (system partition dynamic executable run after /data mounts)
# ===============================
include $(CLEAR_VARS)
LOCAL_SRC_FILES := recovery-persist.cpp
LOCAL_MODULE := recovery-persist
LOCAL_SHARED_LIBRARIES := liblog libbase
LOCAL_CFLAGS := -Werror
LOCAL_INIT_RC := recovery-persist.rc
include $(BUILD_EXECUTABLE)

# recovery-refresh (system partition dynamic executable run at init)
# ===============================
include $(CLEAR_VARS)
LOCAL_SRC_FILES := recovery-refresh.cpp
LOCAL_MODULE := recovery-refresh
LOCAL_SHARED_LIBRARIES := liblog
LOCAL_CFLAGS := -Werror
LOCAL_INIT_RC := recovery-refresh.rc
include $(BUILD_EXECUTABLE)

# All the APIs for testing
include $(CLEAR_VARS)
LOCAL_CLANG := true
LOCAL_MODULE := libverifier
LOCAL_MODULE_TAGS := tests
LOCAL_SRC_FILES := \
    asn1_decoder.cpp \
    verifier.cpp \
    ui.cpp
LOCAL_STATIC_LIBRARIES := libcrypto_static
include $(BUILD_STATIC_LIBRARY)

include $(LOCAL_PATH)/minui/Android.mk \
    $(LOCAL_PATH)/minzip/Android.mk \
    $(LOCAL_PATH)/minadbd/Android.mk \
    $(LOCAL_PATH)/mtdutils/Android.mk \
    $(LOCAL_PATH)/tests/Android.mk \
    $(LOCAL_PATH)/tools/Android.mk \
    $(LOCAL_PATH)/edify/Android.mk \
    $(LOCAL_PATH)/uncrypt/Android.mk \
    $(LOCAL_PATH)/otafault/Android.mk \
    $(LOCAL_PATH)/updater/Android.mk \
    $(LOCAL_PATH)/update_verifier/Android.mk \
    $(LOCAL_PATH)/applypatch/Android.mk \
    $(LOCAL_PATH)/fstools/Android.mk

endif
