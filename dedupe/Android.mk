LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_SRC_FILES := dedupe.c driver.c \
    ../../../external/libselinux/src/lsetfilecon.c \
    ../../../external/libselinux/src/lgetfilecon.c

LOCAL_FORCE_STATIC_EXECUTABLE := true
LOCAL_MODULE := dedupe
LOCAL_STATIC_LIBRARIES := libcrypto_static libselinux
LOCAL_C_INCLUDES += external/openssl/include external/libselinux/include
include $(BUILD_HOST_EXECUTABLE)

include $(CLEAR_VARS)
LOCAL_SRC_FILES := dedupe.c
LOCAL_STATIC_LIBRARIES := libcrypto_static libcutils libc libselinux
LOCAL_MODULE := libdedupe
LOCAL_MODULE_TAGS := eng
LOCAL_C_INCLUDES := external/openssl/include external/libselinux/include
include $(BUILD_STATIC_LIBRARY)

include $(CLEAR_VARS)
LOCAL_SRC_FILES := driver.c
LOCAL_STATIC_LIBRARIES := libdedupe libcrypto_static libcutils libc libselinux
LOCAL_MODULE := utility_dedupe
LOCAL_MODULE_TAGS := eng
LOCAL_MODULE_STEM := dedupe
LOCAL_MODULE_CLASS := UTILITY_EXECUTABLES
LOCAL_C_INCLUDES := external/openssl/include external/libselinux/include
LOCAL_UNSTRIPPED_PATH := $(PRODUCT_OUT)/symbols/utilities
LOCAL_MODULE_PATH := $(PRODUCT_OUT)/utilities
LOCAL_FORCE_STATIC_EXECUTABLE := true
include $(BUILD_EXECUTABLE)
