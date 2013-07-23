LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)
LOCAL_MODULE := parted
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_CLASS := RECOVERY_EXECUTABLES
LOCAL_MODULE_PATH := $(TARGET_RECOVERY_ROOT_OUT)/sbin
LOCAL_SRC_FILES := $(LOCAL_MODULE)
include $(BUILD_PREBUILT)

include $(CLEAR_VARS)
LOCAL_MODULE := sdparted
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_CLASS := RECOVERY_EXECUTABLES
LOCAL_MODULE_PATH := $(TARGET_RECOVERY_ROOT_OUT)/sbin
LOCAL_SRC_FILES := $(LOCAL_MODULE)
include $(BUILD_PREBUILT)

ifneq ($(TARGET_RECOVERY_FSTAB),)
  BOARD_RECOVERY_RFS_CHECK := $(shell grep rfs $(TARGET_RECOVERY_FSTAB))
else
  BOARD_RECOVERY_RFS_CHECK := $(shell grep rfs $(TARGET_DEVICE_DIR)/recovery.fstab)
endif

ifneq ($(BOARD_RECOVERY_RFS_CHECK),)
include $(CLEAR_VARS)
LOCAL_MODULE := fat.format
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_CLASS := RECOVERY_EXECUTABLES
LOCAL_MODULE_PATH := $(TARGET_RECOVERY_ROOT_OUT)/sbin
LOCAL_SRC_FILES := $(LOCAL_MODULE)
include $(BUILD_PREBUILT)

endif

include $(CLEAR_VARS)
LOCAL_STATIC_LIBRARIES := libz
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE := libminizip
LOCAL_CFLAGS := -Dmain=minizip_main -D__ANDROID__ -DIOAPI_NO_64
LOCAL_C_INCLUDES := external/zlib
LOCAL_SRC_FILES := ../../../external/zlib/src/contrib/minizip/minizip.c ../../../external/zlib/src/contrib/minizip/zip.c ../../../external/zlib/src/contrib/minizip/ioapi.c
include $(BUILD_STATIC_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE := libmake_ext4fs
LOCAL_CFLAGS := -Dmain=make_ext4fs_main
LOCAL_SRC_FILES := ../../../system/extras/ext4_utils/make_ext4fs_main.c
include $(BUILD_STATIC_LIBRARY)
