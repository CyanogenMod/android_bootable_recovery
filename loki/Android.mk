LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)
LOCAL_SRC_FILES := loki_flash.c loki_patch.c loki_recovery.c
LOCAL_MODULE := libloki_recovery
# to be able to include common.h as it includes <fs_mgr.h>
# to do: move <fs_mgr.h> outside common.h
LOCAL_C_INCLUDES += system/core/fs_mgr/include
LOCAL_MODULE_TAGS := eng
include $(BUILD_STATIC_LIBRARY)
