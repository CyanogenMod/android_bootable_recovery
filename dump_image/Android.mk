ifneq ($(TARGET_SIMULATOR),true)
ifeq ($(TARGET_ARCH),arm)

LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)
LOCAL_SRC_FILES := dump_image.c mtdutils.c ../mtdutils/mounts.c
LOCAL_MODULE := recovery_dump_image
LOCAL_MODULE_TAGS := eng
LOCAL_STATIC_LIBRARIES := libcutils libc
LOCAL_MODULE_STEM := dump_image
LOCAL_FORCE_STATIC_EXECUTABLE := true
include $(BUILD_EXECUTABLE)

endif	# TARGET_ARCH == arm
endif	# !TARGET_SIMULATOR
