LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

LOCAL_SRC_FILES := graphics.c events.c resources.c

LOCAL_C_INCLUDES +=\
    external/libpng\
    external/zlib

ifneq ($(BOARD_LDPI_RECOVERY),)
    LOCAL_CFLAGS += -DBOARD_LDPI_RECOVERY='"$(BOARD_LDPI_RECOVERY)"'
endif

LOCAL_MODULE := libminui

include $(BUILD_STATIC_LIBRARY)
