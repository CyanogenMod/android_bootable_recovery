LOCAL_PATH := $(call my-dir)

# This is a multi-call static binary which contains the
# GPL filesystem tools.

include $(CLEAR_VARS)
LOCAL_MODULE := fstools
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_PATH := $(TARGET_RECOVERY_ROOT_OUT)/sbin
LOCAL_SRC_FILES := fstools.cpp
LOCAL_FORCE_STATIC_EXECUTABLE := true

LOCAL_WHOLE_STATIC_LIBRARIES += \
	libfuse_static

LOCAL_WHOLE_STATIC_LIBRARIES += \
	libexfat_static \
	libexfat_fsck_static \
	libexfat_mkfs_static \
	libexfat_mount_static 

LOCAL_WHOLE_STATIC_LIBRARIES += \
	libntfs-3g_static \
	libntfs3g_fsck_static \
	libntfs3g_mkfs_main \
	libntfs3g_mount_static

LOCAL_WHOLE_STATIC_LIBRARIES += \
	libext2fs \
	libe2fsck_static \
	libmke2fs_static \
	libtune2fs

LOCAL_WHOLE_STATIC_LIBRARIES += \
	libf2fs_static \
	libf2fs_fsck_static \
	libf2fs_mkfs_static

LOCAL_WHOLE_STATIC_LIBRARIES += \
	libsgdisk_static

LOCAL_STATIC_LIBRARIES := \
	libext2_blkid \
	libext2_uuid \
	libext2_profile \
	libext2_quota \
	libext2_com_err \
	libext2_e2p \
	libc++_static \
	libc \
	libm

FSTOOLS_LINKS := \
	e2fsck mke2fs tune2fs fsck.ext4 mkfs.ext4 \
	fsck.exfat mkfs.exfat mount.exfat \
	fsck.ntfs mkfs.ntfs mount.ntfs \
	mkfs.f2fs fsck.f2fs

LOCAL_POST_INSTALL_CMD := \
    $(hide) $(foreach t,$(FSTOOLS_LINKS),ln -sf fstools $(TARGET_RECOVERY_ROOT_OUT)/sbin/$(t);)
include $(BUILD_EXECUTABLE)

