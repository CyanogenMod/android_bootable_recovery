LOCAL_PATH := $(call my-dir)

common_cflags :=

common_src_files := graphics.c graphics_fbdev.c events.c resources.c

common_c_includes := \
    external/libpng\
    external/zlib

ifeq ($(call is-vendor-board-platform,QCOM),true)
  common_additional_dependencies := $(TARGET_OUT_INTERMEDIATES)/KERNEL_OBJ/usr
  common_c_includes += $(TARGET_OUT_INTERMEDIATES)/KERNEL_OBJ/usr/include
  common_src_files += graphics_overlay.c
  common_cflags += -DMSMFB_OVERLAY
endif

ifneq ($(TARGET_RECOVERY_NO_MSM_BSP), true)
ifeq ($(TARGET_USES_QCOM_BSP), true)
    common_cflags += -DMSM_BSP
endif
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

ifneq ($(BOARD_USE_CUSTOM_RECOVERY_FONT),)
  common_cflags += -DRECOVERY_FONT=$(BOARD_USE_CUSTOM_RECOVERY_FONT)
endif


include $(CLEAR_VARS)
LOCAL_MODULE := libminui
LOCAL_SRC_FILES := $(common_src_files)
LOCAL_ADDITIONAL_DEPENDENCIES := $(common_additional_dependencies)
LOCAL_C_INCLUDES := $(common_c_includes)
LOCAL_CFLAGS := $(common_cflags)
include $(BUILD_STATIC_LIBRARY)


include $(CLEAR_VARS)
LOCAL_MODULE := libminui
LOCAL_ARM_MODE:= arm
LOCAL_SRC_FILES := $(common_src_files)
LOCAL_ADDITIONAL_DEPENDENCIES := $(common_additional_dependencies)
LOCAL_C_INCLUDES := $(common_c_includes)
LOCAL_SHARED_LIBRARIES := libpng libpixelflinger
LOCAL_CFLAGS += $(common_cflags) -DSHARED_MINUI
include $(BUILD_SHARED_LIBRARY)
