LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

LOCAL_SRC_FILES := graphics.c events.c resources.c

LOCAL_C_INCLUDES +=\
    external/libpng\
    external/zlib

LOCAL_MODULE := libminui

ifneq ($(BOARD_RECOVERY_FONT_INCLUDE_HEADER),)
	LOCAL_CFLAGS += -DBOARD_RECOVERY_FONT_INCLUDE_HEADER='"$(BOARD_RECOVERY_FONT_INCLUDE_HEADER)"'
endif

include $(BUILD_STATIC_LIBRARY)
