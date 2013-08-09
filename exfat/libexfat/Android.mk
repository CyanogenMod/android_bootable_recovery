LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE := libexfat.recovery
LOCAL_MODULE_TAGS := optional
LOCAL_CFLAGS = -D_FILE_OFFSET_BITS=64
LOCAL_SRC_FILES := \
    ../../../../external/exfat/libexfat/cluster.c \
    ../../../../external/exfat/libexfat/io.c \
    ../../../../external/exfat/libexfat/log.c \
    ../../../../external/exfat/libexfat/lookup.c \
    ../../../../external/exfat/libexfat/mount.c \
    ../../../../external/exfat/libexfat/node.c \
    ../../../../external/exfat/libexfat/time.c \
    ../../../../external/exfat/libexfat/utf.c \
    ../../../../external/exfat/libexfat/utils.c
LOCAL_C_INCLUDES += \
    external/exfat/libexfat
LOCAL_STATIC_LIBRARIES += libc

include $(BUILD_STATIC_LIBRARY)
