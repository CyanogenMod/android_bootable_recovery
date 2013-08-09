LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE := mkexfatfs
LOCAL_MODULE_CLASS := RECOVERY_EXECUTABLES
LOCAL_MODULE_PATH := $(TARGET_RECOVERY_ROOT_OUT)/sbin
LOCAL_MODULE_TAGS := optional
LOCAL_CFLAGS = -D_FILE_OFFSET_BITS=64
LOCAL_SRC_FILES := \
    ../../../../external/exfat/mkfs/cbm.c \
    ../../../../external/exfat/mkfs/fat.c \
    ../../../../external/exfat/mkfs/main.c \
    ../../../../external/exfat/mkfs/mkexfat.c \
    ../../../../external/exfat/mkfs/rootdir.c \
    ../../../../external/exfat/mkfs/uct.c \
    ../../../../external/exfat/mkfs/uctc.c \
    ../../../../external/exfat/mkfs/vbr.c
LOCAL_C_INCLUDES += \
    external/exfat/mkfs \
    external/exfat/libexfat \
    external/fuse/include
LOCAL_STATIC_LIBRARIES += libz libc libexfat.recovery libfuse.recovery
LOCAL_FORCE_STATIC_EXECUTABLE := true

include $(BUILD_EXECUTABLE)
