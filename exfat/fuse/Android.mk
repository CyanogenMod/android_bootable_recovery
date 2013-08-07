LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

LOCAL_MODULE := mount.exfat-fuse
LOCAL_MODULE_CLASS := RECOVERY_EXECUTABLES
LOCAL_MODULE_PATH := $(TARGET_RECOVERY_ROOT_OUT)/sbin
LOCAL_MODULE_TAGS := optional
LOCAL_CFLAGS = -D_FILE_OFFSET_BITS=64
LOCAL_SRC_FILES = ../../../../external/exfat/fuse/main.c
LOCAL_C_INCLUDES += \
    external/exfat/fuse \
    external/exfat/libexfat \
    external/fuse/include \
    external/fuse/android
LOCAL_STATIC_LIBRARIES += libz libc libexfat.recovery libfuse.recovery
LOCAL_FORCE_STATIC_EXECUTABLE := true

include $(BUILD_EXECUTABLE)
