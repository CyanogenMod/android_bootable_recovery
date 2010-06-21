ifneq ($(TARGET_SIMULATOR),true)
ifeq ($(TARGET_ARCH),arm)

LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

commands_recovery_local_path := $(LOCAL_PATH)
# LOCAL_CPP_EXTENSION := .c

LOCAL_SRC_FILES := \
	extendedcommands.c \
	nandroid.c \
	legacy.c \
	commands.c \
	recovery.c \
	bootloader.c \
	firmware.c \
	install.c \
	roots.c \
	ui.c \
	verifier.c

LOCAL_SRC_FILES += test_roots.c

LOCAL_MODULE := recovery

LOCAL_FORCE_STATIC_EXECUTABLE := true

RECOVERY_VERSION := ClockworkMod Recovery v2.0.1.0
LOCAL_CFLAGS := -DRECOVERY_VERSION="$(RECOVERY_VERSION)"
RECOVERY_API_VERSION := 2
LOCAL_CFLAGS += -DRECOVERY_API_VERSION=$(RECOVERY_API_VERSION)

ifeq ($(BOARD_HAS_NO_SELECT_BUTTON),true)
  LOCAL_CFLAGS += -DKEY_POWER_IS_SELECT_ITEM
endif

ifdef BOARD_SDCARD_DEVICE_PRIMARY
  LOCAL_CFLAGS += -DSDCARD_DEVICE_PRIMARY=\"$(BOARD_SDCARD_DEVICE_PRIMARY)\"
endif

ifdef BOARD_SDCARD_DEVICE_SECONDARY
  LOCAL_CFLAGS += -DSDCARD_DEVICE_SECONDARY=\"$(BOARD_SDCARD_DEVICE_SECONDARY)\"
endif

ifdef BOARD_SDEXT_DEVICE
  LOCAL_CFLAGS += -DSDEXT_DEVICE=\"$(BOARD_SDEXT_DEVICE)\"
endif

ifdef BOARD_SDEXT_FILESYSTEM
  LOCAL_CFLAGS += -DSDEXT_FILESYSTEM=\"$(BOARD_SDEXT_FILESYSTEM)\"
endif

ifdef BOARD_DATA_DEVICE
  LOCAL_CFLAGS += -DDATA_DEVICE=\"$(BOARD_DATA_DEVICE)\"
endif

ifdef BOARD_DATA_FILESYSTEM
  LOCAL_CFLAGS += -DDATA_FILESYSTEM=\"$(BOARD_DATA_FILESYSTEM)\"
endif

ifdef BOARD_CACHE_DEVICE
  LOCAL_CFLAGS += -DCACHE_DEVICE=\"$(BOARD_CACHE_DEVICE)\"
endif

ifdef BOARD_CACHE_FILESYSTEM
  LOCAL_CFLAGS += -DCACHE_FILESYSTEM=\"$(BOARD_CACHE_FILESYSTEM)\"
endif

ifdef BOARD_HAS_DATADATA
  LOCAL_CFLAGS += -DHAS_DATADATA
endif

# This binary is in the recovery ramdisk, which is otherwise a copy of root.
# It gets copied there in config/Makefile.  LOCAL_MODULE_TAGS suppresses
# a (redundant) copy of the binary in /system/bin for user builds.
# TODO: Build the ramdisk image in a more principled way.

LOCAL_MODULE_TAGS := eng

LOCAL_STATIC_LIBRARIES :=
ifeq ($(TARGET_RECOVERY_UI_LIB),)
  LOCAL_SRC_FILES += default_recovery_ui.c
else
  LOCAL_STATIC_LIBRARIES += $(TARGET_RECOVERY_UI_LIB)
endif
LOCAL_STATIC_LIBRARIES += libbusybox libclearsilverregex libmkyaffs2image libunyaffs liberase_image libdump_image libflash_image libmtdutils
LOCAL_STATIC_LIBRARIES += libamend
LOCAL_STATIC_LIBRARIES += libminzip libunz libmtdutils libmincrypt
LOCAL_STATIC_LIBRARIES += libminui libpixelflinger_static libpng libcutils
LOCAL_STATIC_LIBRARIES += libstdc++ libc

include $(BUILD_EXECUTABLE)

RECOVERY_LINKS := amend busybox flash_image dump_image mkyaffs2image unyaffs erase_image
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

include $(commands_recovery_local_path)/amend/Android.mk
include $(commands_recovery_local_path)/minui/Android.mk
include $(commands_recovery_local_path)/minzip/Android.mk
include $(commands_recovery_local_path)/mtdutils/Android.mk
include $(commands_recovery_local_path)/tools/Android.mk
include $(commands_recovery_local_path)/edify/Android.mk
include $(commands_recovery_local_path)/updater/Android.mk
commands_recovery_local_path :=

endif   # TARGET_ARCH == arm
endif	# !TARGET_SIMULATOR

