LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

LOCAL_SRC_FILES := graphics.c events.c resources.c

LOCAL_C_INCLUDES +=\
    external/libpng\
    external/zlib

ifneq ($(BOARD_LDPI_RECOVERY),)
    LOCAL_CFLAGS += -DBOARD_LDPI_RECOVERY='"$(BOARD_LDPI_RECOVERY)"'
endif

ifneq ($(BOARD_HAS_JANKY_BACKBUFFER),)
    LOCAL_CFLAGS += -DBOARD_HAS_JANKY_BACKBUFFER
endif

ifeq ($(BOARD_HAS_FLIPPED_SCREEN), true)
    LOCAL_CFLAGS += -DBOARD_HAS_FLIPPED_SCREEN
endif

LOCAL_MODULE := libminui

include $(BUILD_STATIC_LIBRARY)
