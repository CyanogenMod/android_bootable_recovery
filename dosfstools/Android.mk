# initial release taken from TWRP sources
# updated to latest 3.0.16 sources
# only use mkdosfs for now

LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)
LOCAL_SRC_FILES := src/mkdosfs.c
LOCAL_C_INCLUDES := $(KERNEL_HEADERS)
LOCAL_CFLAGS += -D_USING_BIONIC_
LOCAL_STATIC_LIBRARIES := libc
LOCAL_FORCE_STATIC_EXECUTABLE := true
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE = mkdosfs
LOCAL_MODULE_CLASS := RECOVERY_EXECUTABLES
LOCAL_MODULE_PATH := $(TARGET_RECOVERY_ROOT_OUT)/sbin
include $(BUILD_EXECUTABLE)
