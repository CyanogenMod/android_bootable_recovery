ifneq ($(TARGET_SIMULATOR),true)
ifeq ($(TARGET_ARCH),arm)

LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)
LOCAL_MODULE := recovery_nandroid
LOCAL_MODULE_CLASS := EXECUTABLES
LOCAL_MODULE_PATH := $(TARGET_RECOVERY_ROOT_OUT)/sbin
LOCAL_SRC_FILES := nandroid-mobile.sh
LOCAL_MODULE_STEM := nandroid-mobile.sh
ADDITIONAL_RECOVERY_EXECUTABLES += recovery_nandroid
include $(BUILD_PREBUILT)

include $(CLEAR_VARS)
LOCAL_MODULE := recovery_unyaffs
LOCAL_MODULE_STEM := unyaffs
LOCAL_FORCE_STATIC_EXECUTABLE := true
LOCAL_SRC_FILES := unyaffs.c
LOCAL_STATIC_LIBRARIES := libc libcutils
LOCAL_MODULE_PATH := $(TARGET_RECOVERY_ROOT_OUT)/sbin
include $(BUILD_EXECUTABLE)

endif	# TARGET_ARCH == arm
endif	# !TARGET_SIMULATOR
