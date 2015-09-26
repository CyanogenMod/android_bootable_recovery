LOCAL_PATH := $(call my-dir)

common_cflags :=

common_src_files := graphics.c graphics_adf.c graphics_fbdev.c events.c \
	resources.c

common_c_includes := \
    external/libpng\
    external/zlib

common_additional_dependencies :=

common_whole_static_libraries := libadf

common_cflags += -std=gnu11

ifeq ($(subst ",,$(TARGET_RECOVERY_PIXEL_FORMAT)),ABGR_8888)
  common_cflags += -DRECOVERY_ABGR
endif
ifeq ($(subst ",,$(TARGET_RECOVERY_PIXEL_FORMAT)),RGBX_8888)
  common_cflags += -DRECOVERY_RGBX
endif

ifeq ($(subst ",,$(TARGET_RECOVERY_PIXEL_FORMAT)),BGRA_8888)
  common_cflags += -DRECOVERY_BGRA
endif

ifneq ($(TARGET_RECOVERY_OVERSCAN_PERCENT),)
  common_cflags += -DOVERSCAN_PERCENT=$(TARGET_RECOVERY_OVERSCAN_PERCENT)
else
  common_cflags += -DOVERSCAN_PERCENT=0
endif

ifneq ($(BOARD_RECOVERY_NEEDS_FBIOPAN_DISPLAY),)
  common_cflags += -DBOARD_RECOVERY_NEEDS_FBIOPAN_DISPLAY
endif

include $(CLEAR_VARS)
LOCAL_MODULE := libminui
LOCAL_SRC_FILES := $(common_src_files)
LOCAL_ADDITIONAL_DEPENDENCIES := $(common_additional_dependencies)
LOCAL_C_INCLUDES += $(common_c_includes)
LOCAL_CFLAGS := $(common_cflags)
LOCAL_WHOLE_STATIC_LIBRARIES := $(common_whole_static_libraries)
include $(BUILD_STATIC_LIBRARY)


include $(CLEAR_VARS)
LOCAL_MODULE := libminui
LOCAL_SRC_FILES := $(common_src_files)
LOCAL_ADDITIONAL_DEPENDENCIES := $(common_additional_dependencies)
LOCAL_C_INCLUDES += $(common_c_includes)
LOCAL_SHARED_LIBRARIES := libpng
LOCAL_CFLAGS += $(common_cflags) -DSHARED_MINUI
LOCAL_WHOLE_STATIC_LIBRARIES := $(common_whole_static_libraries)
include $(BUILD_SHARED_LIBRARY)
