LOCAL_PATH := $(call my-dir)

# libloki
include $(CLEAR_VARS)
LOCAL_SRC_FILES := loki_flash.c loki_patch.c
LOCAL_MODULE := libloki_static
LOCAL_MODULE_TAGS := eng
include $(BUILD_STATIC_LIBRARY)

# build static binary
include $(CLEAR_VARS)
LOCAL_SRC_FILES := loki_flash.c loki_patch.c loki_find.c main.c
LOCAL_MODULE := loki_tool_static
LOCAL_MODULE_STEM := loki_tool
LOCAL_MODULE_TAGS := eng
# LOCAL_MODULE_PATH := $(TARGET_RECOVERY_ROOT_OUT)/sbin
LOCAL_STATIC_LIBRARIES := libc
LOCAL_FORCE_STATIC_EXECUTABLE := true
include $(BUILD_EXECUTABLE)
