LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

LOCAL_SRC_FILES := events.c resources.c
ifneq ($(BOARD_CUSTOM_GRAPHICS),)
  LOCAL_SRC_FILES += $(BOARD_CUSTOM_GRAPHICS)
else
  LOCAL_SRC_FILES += graphics.c
endif

LOCAL_C_INCLUDES +=\
    external/libpng\
    external/zlib

LOCAL_MODULE := libminui

ifeq ($(TARGET_RECOVERY_PIXEL_FORMAT),"RGBX_8888")
  LOCAL_CFLAGS += -DRECOVERY_RGBX
endif
ifeq ($(TARGET_RECOVERY_PIXEL_FORMAT),"BGRA_8888")
  LOCAL_CFLAGS += -DRECOVERY_BGRA
endif

ifeq ($(BOARD_XHDPI_RECOVERY),true)
  LOCAL_CFLAGS += -DBOARD_XHDPI_RECOVERY
endif
ifeq ($(BOARD_HDPI_RECOVERY),true)
  LOCAL_CFLAGS += -DBOARD_HDPI_RECOVERY
endif
ifeq ($(BOARD_MDPI_RECOVERY),true)
  LOCAL_CFLAGS += -DBOARD_MDPI_RECOVERY
endif
ifeq ($(BOARD_LDPI_RECOVERY),true)
  LOCAL_CFLAGS += -DBOARD_LDPI_RECOVERY
endif

include $(BUILD_STATIC_LIBRARY)
