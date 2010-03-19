ifneq ($(TARGET_SIMULATOR),true)
ifeq ($(TARGET_ARCH),arm)

LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

LOCAL_SRC_FILES := \
	mtdutils.c \
	mounts.c

LOCAL_MODULE := libmtdutils

include $(BUILD_STATIC_LIBRARY)

include $(CLEAR_VARS)
LOCAL_SRC_FILES := flash_image.c
LOCAL_MODULE := flash_image
LOCAL_MODULE_TAGS := eng
LOCAL_STATIC_LIBRARIES := libmtdutils
LOCAL_SHARED_LIBRARIES := libcutils libc
include $(BUILD_EXECUTABLE)

include $(CLEAR_VARS)
LOCAL_SRC_FILES := dump_image.c
LOCAL_MODULE := dump_image
LOCAL_MODULE_TAGS := eng
LOCAL_STATIC_LIBRARIES := libmtdutils
LOCAL_SHARED_LIBRARIES := libcutils libc
include $(BUILD_EXECUTABLE)

include $(CLEAR_VARS)
LOCAL_SRC_FILES := erase_image.c
LOCAL_MODULE := erase_image
LOCAL_MODULE_TAGS := eng
LOCAL_STATIC_LIBRARIES := libmtdutils
LOCAL_SHARED_LIBRARIES := libcutils libc
include $(BUILD_EXECUTABLE)

include $(CLEAR_VARS)
LOCAL_SRC_FILES := flash_image.c
LOCAL_MODULE := libflash_image
LOCAL_CFLAGS += -Dmain=flash_image_main
include $(BUILD_STATIC_LIBRARY)

include $(CLEAR_VARS)
LOCAL_SRC_FILES := dump_image.c
LOCAL_MODULE := libdump_image
LOCAL_CFLAGS += -Dmain=dump_image_main
include $(BUILD_STATIC_LIBRARY)

include $(CLEAR_VARS)
LOCAL_SRC_FILES := erase_image.c
LOCAL_MODULE := liberase_image
LOCAL_CFLAGS += -Dmain=erase_image_main
include $(BUILD_STATIC_LIBRARY)

endif	# TARGET_ARCH == arm
endif	# !TARGET_SIMULATOR
