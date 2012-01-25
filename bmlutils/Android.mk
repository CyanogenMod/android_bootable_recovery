LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

BOARD_RECOVERY_DEFINES := BOARD_BML_BOOT BOARD_BML_RECOVERY

$(foreach board_define,$(BOARD_RECOVERY_DEFINES), \
  $(if $($(board_define)), \
    $(eval LOCAL_CFLAGS += -D$(board_define)=\"$($(board_define))\") \
  ) \
  )

ifneq ($(BOARD_CUSTOM_RECOVERY_BML_FORMAT),)
  LOCAL_SRC_FILES += $(BOARD_CUSTOM_RECOVERY_BML_FORMAT)
else
  LOCAL_SRC_FILES += bmlutils.c
endif
LOCAL_MODULE := libbmlutils
LOCAL_MODULE_TAGS := eng
include $(BUILD_STATIC_LIBRARY)
