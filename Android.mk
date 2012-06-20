ifneq ($(TARGET_SIMULATOR),true)
ifeq ($(TARGET_ARCH),arm)

LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

commands_recovery_local_path := $(LOCAL_PATH)
# LOCAL_CPP_EXTENSION := .c

LOCAL_SRC_FILES := \
    recovery.c \
    bootloader.c \
    install.c \
    roots.c \
    ui.c \
    verifier.c \
    encryptedfs_provisioning.c \
    mounts.c \
    extendedcommands.c \
    nandroid.c \
    ../../system/core/toolbox/reboot.c \
    firmware.c \
    edifyscripting.c \
    setprop.c

ADDITIONAL_RECOVERY_FILES := $(shell echo $$ADDITIONAL_RECOVERY_FILES)
LOCAL_SRC_FILES += $(ADDITIONAL_RECOVERY_FILES)

LOCAL_MODULE := recovery

LOCAL_FORCE_STATIC_EXECUTABLE := true

ifdef I_AM_KOUSH
RECOVERY_NAME := ClockworkMod Recovery
LOCAL_CFLAGS += -DI_AM_KOUSH
else
RECOVERY_NAME := CWM-based Recovery
endif

RECOVERY_VERSION := $(RECOVERY_NAME) v5.0.2.8

LOCAL_CFLAGS += -DRECOVERY_VERSION="$(RECOVERY_VERSION)"
RECOVERY_API_VERSION := 2
LOCAL_CFLAGS += -DRECOVERY_API_VERSION=$(RECOVERY_API_VERSION)

BOARD_RECOVERY_DEFINES := BOARD_HAS_NO_SELECT_BUTTON BOARD_HAS_SMALL_RECOVERY BOARD_LDPI_RECOVERY BOARD_UMS_LUNFILE BOARD_RECOVERY_ALWAYS_WIPES BOARD_RECOVERY_HANDLES_MOUNT

$(foreach board_define,$(BOARD_RECOVERY_DEFINES), \
  $(if $($(board_define)), \
    $(eval LOCAL_CFLAGS += -D$(board_define)=\"$($(board_define))\") \
  ) \
  )

# This binary is in the recovery ramdisk, which is otherwise a copy of root.
# It gets copied there in config/Makefile.  LOCAL_MODULE_TAGS suppresses
# a (redundant) copy of the binary in /system/bin for user builds.
# TODO: Build the ramdisk image in a more principled way.

LOCAL_MODULE_TAGS := eng

LOCAL_STATIC_LIBRARIES :=
ifeq ($(BOARD_CUSTOM_RECOVERY_KEYMAPPING),)
  LOCAL_SRC_FILES += default_recovery_ui.c
else
  LOCAL_SRC_FILES += $(BOARD_CUSTOM_RECOVERY_KEYMAPPING)
endif

LOCAL_STATIC_LIBRARIES += librebootrecovery
LOCAL_STATIC_LIBRARIES += libext4_utils libz
LOCAL_STATIC_LIBRARIES += libminzip libunz libmincrypt

LOCAL_STATIC_LIBRARIES += libedify libbusybox libclearsilverregex libmkyaffs2image libunyaffs liberase_image libdump_image libflash_image

LOCAL_STATIC_LIBRARIES += libcrecovery libflashutils libmtdutils libmmcutils libbmlutils 

ifeq ($(BOARD_USES_BML_OVER_MTD),true)
LOCAL_STATIC_LIBRARIES += libbml_over_mtd
endif

LOCAL_STATIC_LIBRARIES += libminui libpixelflinger_static libpng libcutils
LOCAL_STATIC_LIBRARIES += libstdc++ libc

LOCAL_C_INCLUDES += system/extras/ext4_utils

include $(BUILD_EXECUTABLE)

RECOVERY_LINKS := edify busybox flash_image dump_image mkyaffs2image unyaffs erase_image nandroid reboot volume setprop

# nc is provided by external/netcat
RECOVERY_SYMLINKS := $(addprefix $(TARGET_RECOVERY_ROOT_OUT)/sbin/,$(RECOVERY_LINKS))
$(RECOVERY_SYMLINKS): RECOVERY_BINARY := $(LOCAL_MODULE)
$(RECOVERY_SYMLINKS): $(LOCAL_INSTALLED_MODULE)
	@echo "Symlink: $@ -> $(RECOVERY_BINARY)"
	@mkdir -p $(dir $@)
	@rm -rf $@
	$(hide) ln -sf $(RECOVERY_BINARY) $@

ALL_DEFAULT_INSTALLED_MODULES += $(RECOVERY_SYMLINKS)

# Now let's do recovery symlinks
BUSYBOX_LINKS := $(shell cat external/busybox/busybox-minimal.links)
ifndef BOARD_HAS_SMALL_RECOVERY
exclude := tune2fs
ifeq ($(BOARD_HAS_LARGE_FILESYSTEM),true)
exclude += mke2fs
endif
endif
RECOVERY_BUSYBOX_SYMLINKS := $(addprefix $(TARGET_RECOVERY_ROOT_OUT)/sbin/,$(filter-out $(exclude),$(notdir $(BUSYBOX_LINKS))))
$(RECOVERY_BUSYBOX_SYMLINKS): BUSYBOX_BINARY := busybox
$(RECOVERY_BUSYBOX_SYMLINKS): $(LOCAL_INSTALLED_MODULE)
	@echo "Symlink: $@ -> $(BUSYBOX_BINARY)"
	@mkdir -p $(dir $@)
	@rm -rf $@
	$(hide) ln -sf $(BUSYBOX_BINARY) $@

ALL_DEFAULT_INSTALLED_MODULES += $(RECOVERY_BUSYBOX_SYMLINKS)

include $(CLEAR_VARS)
LOCAL_MODULE := nandroid-md5.sh
LOCAL_MODULE_TAGS := eng
LOCAL_MODULE_CLASS := RECOVERY_EXECUTABLES
LOCAL_MODULE_PATH := $(TARGET_RECOVERY_ROOT_OUT)/sbin
LOCAL_SRC_FILES := nandroid-md5.sh
include $(BUILD_PREBUILT)

include $(CLEAR_VARS)
LOCAL_MODULE := killrecovery.sh
LOCAL_MODULE_TAGS := eng
LOCAL_MODULE_CLASS := RECOVERY_EXECUTABLES
LOCAL_MODULE_PATH := $(TARGET_RECOVERY_ROOT_OUT)/sbin
LOCAL_SRC_FILES := killrecovery.sh
include $(BUILD_PREBUILT)

include $(CLEAR_VARS)

LOCAL_SRC_FILES := verifier_test.c verifier.c

LOCAL_MODULE := verifier_test

LOCAL_FORCE_STATIC_EXECUTABLE := true

LOCAL_MODULE_TAGS := tests

LOCAL_STATIC_LIBRARIES := libmincrypt libcutils libstdc++ libc

include $(BUILD_EXECUTABLE)

include $(commands_recovery_local_path)/dedupe/Android.mk

include $(commands_recovery_local_path)/bmlutils/Android.mk
include $(commands_recovery_local_path)/flashutils/Android.mk
include $(commands_recovery_local_path)/libcrecovery/Android.mk
include $(commands_recovery_local_path)/minui/Android.mk
include $(commands_recovery_local_path)/minzip/Android.mk
include $(commands_recovery_local_path)/mtdutils/Android.mk
include $(commands_recovery_local_path)/mmcutils/Android.mk
include $(commands_recovery_local_path)/tools/Android.mk
include $(commands_recovery_local_path)/edify/Android.mk
include $(commands_recovery_local_path)/updater/Android.mk
include $(commands_recovery_local_path)/applypatch/Android.mk
include $(commands_recovery_local_path)/utilities/Android.mk
commands_recovery_local_path :=

endif   # TARGET_ARCH == arm
endif    # !TARGET_SIMULATOR

