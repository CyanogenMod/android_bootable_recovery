ifneq ($(TARGET_SIMULATOR),true)
ifeq ($(TARGET_ARCH),arm)

LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)
LOCAL_SRC_FILES := mtdutils.c
LOCAL_MODULE := libmtdutils
include $(BUILD_STATIC_LIBRARY)

ifeq ($(BOARD_USES_BML_OVER_MTD),true)
include $(CLEAR_VARS)
LOCAL_SRC_FILES := bml_over_mtd.c
LOCAL_C_INCLUDES += bootable/recovery/mtdutils
LOCAL_MODULE := libbml_over_mtd
LOCAL_MODULE_TAGS := eng
LOCAL_CFLAGS += -Dmain=bml_over_mtd_main
include $(BUILD_STATIC_LIBRARY)

include $(CLEAR_VARS)
LOCAL_SRC_FILES := bml_over_mtd.c
LOCAL_MODULE := bml_over_mtd
LOCAL_MODULE_TAGS := eng
LOCAL_MODULE_CLASS := UTILITY_EXECUTABLES
LOCAL_MODULE_PATH := $(PRODUCT_OUT)/utilities
LOCAL_UNSTRIPPED_PATH := $(PRODUCT_OUT)/symbols/utilities
LOCAL_MODULE_STEM := bml_over_mtd
LOCAL_C_INCLUDES += bootable/recovery/mtdutils
LOCAL_STATIC_LIBRARIES := libmtdutils libcutils libc
LOCAL_FORCE_STATIC_EXECUTABLE := true
include $(BUILD_EXECUTABLE)
endif

endif	# TARGET_ARCH == arm
endif	# !TARGET_SIMULATOR
