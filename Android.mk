ifneq ($(TARGET_SIMULATOR),true)
ifeq ($(TARGET_ARCH),arm)

LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

commands_recovery_local_path := $(LOCAL_PATH)
# LOCAL_CPP_EXTENSION := .c

LOCAL_SRC_FILES := \
    mounts.c \
	extendedcommands.c \
	nandroid.c \
	legacy.c \
	commands.c \
	recovery.c \
	install.c \
	roots.c \
	ui.c \
	verifier.c

LOCAL_SRC_FILES += \
    reboot.c \
    setprop.c

ifndef BOARD_HAS_NO_MISC_PARTITION
    LOCAL_SRC_FILES += \
        firmware.c \
        bootloader.c
endif

ifdef BOARD_HIJACK_RECOVERY_PATH
    LOCAL_CFLAGS += -DBOARD_HIJACK_RECOVERY_PATH=\"$(BOARD_HIJACK_RECOVERY_PATH)\"
endif

LOCAL_SRC_FILES += test_roots.c

LOCAL_MODULE := recovery

LOCAL_FORCE_STATIC_EXECUTABLE := true

RECOVERY_VERSION := ClockworkMod Recovery v2.5.1.8
LOCAL_CFLAGS += -DRECOVERY_VERSION="$(RECOVERY_VERSION)"
RECOVERY_API_VERSION := 2
LOCAL_CFLAGS += -DRECOVERY_API_VERSION=$(RECOVERY_API_VERSION)

BOARD_RECOVERY_DEFINES := BOARD_HAS_NO_SELECT_BUTTON BOARD_SDCARD_DEVICE_PRIMARY BOARD_SDCARD_DEVICE_SECONDARY BOARD_SDEXT_DEVICE BOARD_SDEXT_FILESYSTEM BOARD_DATA_DEVICE BOARD_DATA_FILESYSTEM BOARD_DATADATA_DEVICE BOARD_DATADATA_FILESYSTEM BOARD_CACHE_DEVICE BOARD_CACHE_FILESYSTEM BOARD_SYSTEM_DEVICE BOARD_SYSTEM_FILESYSTEM BOARD_HAS_DATADATA BOARD_DATA_FILESYSTEM_OPTIONS BOARD_DATADATA_FILESYSTEM_OPTIONS BOARD_CACHE_FILESYSTEM_OPTIONS BOARD_SYSTEM_FILESYSTEM_OPTIONS BOARD_HAS_MTD_CACHE BOARD_USES_BMLUTILS BOARD_USES_MMCUTILS BOARD_HAS_SMALL_RECOVERY BOARD_LDPI_RECOVERY BOARD_RECOVERY_IGNORE_BOOTABLES BOARD_HAS_NO_MISC_PARTITION BOARD_HAS_SDCARD_INTERNAL BOARD_SDCARD_DEVICE_INTERNAL

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
LOCAL_STATIC_LIBRARIES += libbusybox libclearsilverregex libmkyaffs2image libunyaffs liberase_image libdump_image libflash_image

LOCAL_STATIC_LIBRARIES += libflashutils libmtdutils libmmcutils libbmlutils

LOCAL_STATIC_LIBRARIES += libamend
LOCAL_STATIC_LIBRARIES += libminzip libunz libmincrypt
LOCAL_STATIC_LIBRARIES += libminui libpixelflinger_static libpng libcutils
LOCAL_STATIC_LIBRARIES += libstdc++ libc

include $(BUILD_EXECUTABLE)

RECOVERY_LINKS := amend busybox flash_image dump_image mkyaffs2image unyaffs erase_image nandroid reboot

# nc is provided by external/netcat
SYMLINKS := $(addprefix $(TARGET_RECOVERY_ROOT_OUT)/sbin/,$(RECOVERY_LINKS))
$(SYMLINKS): RECOVERY_BINARY := $(LOCAL_MODULE)
$(SYMLINKS): $(LOCAL_INSTALLED_MODULE)
	@echo "Symlink: $@ -> $(RECOVERY_BINARY)"
	@mkdir -p $(dir $@)
	@rm -rf $@
	$(hide) ln -sf $(RECOVERY_BINARY) $@

ALL_DEFAULT_INSTALLED_MODULES += $(SYMLINKS)

# Now let's do recovery symlinks
BUSYBOX_LINKS := $(shell cat external/busybox/busybox-minimal.links)
SYMLINKS := $(addprefix $(TARGET_RECOVERY_ROOT_OUT)/sbin/,$(filter-out $(exclude),$(notdir $(BUSYBOX_LINKS))))
$(SYMLINKS): BUSYBOX_BINARY := busybox
$(SYMLINKS): $(LOCAL_INSTALLED_MODULE)
	@echo "Symlink: $@ -> $(BUSYBOX_BINARY)"
	@mkdir -p $(dir $@)
	@rm -rf $@
	$(hide) ln -sf $(BUSYBOX_BINARY) $@

ALL_DEFAULT_INSTALLED_MODULES += $(SYMLINKS)

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


include $(commands_recovery_local_path)/amend/Android.mk
include $(commands_recovery_local_path)/bmlutils/Android.mk
include $(commands_recovery_local_path)/flashutils/Android.mk
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

