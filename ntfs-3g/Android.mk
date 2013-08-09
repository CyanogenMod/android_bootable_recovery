# libfuse lite
LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)
LOCAL_SRC_FILES := \
    ../../../external/ntfs-3g/libfuse-lite/fuse.c \
    ../../../external/ntfs-3g/libfuse-lite/fusermount.c \
    ../../../external/ntfs-3g/libfuse-lite/fuse_kern_chan.c \
    ../../../external/ntfs-3g/libfuse-lite/fuse_loop.c \
    ../../../external/ntfs-3g/libfuse-lite/fuse_lowlevel.c \
    ../../../external/ntfs-3g/libfuse-lite/fuse_opt.c \
    ../../../external/ntfs-3g/libfuse-lite/fuse_session.c \
    ../../../external/ntfs-3g/libfuse-lite/fuse_signals.c \
    ../../../external/ntfs-3g/libfuse-lite/helper.c \
    ../../../external/ntfs-3g/libfuse-lite/mount.c \
    ../../../external/ntfs-3g/libfuse-lite/mount_util.c \
    ../../../external/ntfs-3g/androidglue/statvfs.c
LOCAL_C_INCLUDES := \
    external/ntfs-3g \
    external/ntfs-3g/include/fuse-lite \
    external/ntfs-3g/androidglue/include
LOCAL_CFLAGS := -O2 -g -W -Wall -D_LARGEFILE_SOURCE -D_FILE_OFFSET_BITS=64  -DHAVE_CONFIG_H
LOCAL_MODULE := libfuse-lite.recovery
LOCAL_MODULE_TAGS := eng
LOCAL_STATIC_LIBRARIES := libc libcutils
include $(BUILD_STATIC_LIBRARY)


# libntfs-3g
include $(CLEAR_VARS)
LOCAL_SRC_FILES := \
    ../../../external/ntfs-3g/libntfs-3g/acls.c \
    ../../../external/ntfs-3g/libntfs-3g/attrib.c \
    ../../../external/ntfs-3g/libntfs-3g/attrlist.c \
    ../../../external/ntfs-3g/libntfs-3g/bitmap.c \
    ../../../external/ntfs-3g/libntfs-3g/bootsect.c \
    ../../../external/ntfs-3g/libntfs-3g/cache.c \
    ../../../external/ntfs-3g/libntfs-3g/collate.c \
    ../../../external/ntfs-3g/libntfs-3g/compat.c \
    ../../../external/ntfs-3g/libntfs-3g/compress.c \
    ../../../external/ntfs-3g/libntfs-3g/debug.c \
    ../../../external/ntfs-3g/libntfs-3g/device.c \
    ../../../external/ntfs-3g/libntfs-3g/dir.c \
    ../../../external/ntfs-3g/libntfs-3g/efs.c \
    ../../../external/ntfs-3g/libntfs-3g/index.c \
    ../../../external/ntfs-3g/libntfs-3g/inode.c \
    ../../../external/ntfs-3g/libntfs-3g/lcnalloc.c \
    ../../../external/ntfs-3g/libntfs-3g/logfile.c \
    ../../../external/ntfs-3g/libntfs-3g/logging.c \
    ../../../external/ntfs-3g/libntfs-3g/mft.c \
    ../../../external/ntfs-3g/libntfs-3g/misc.c \
    ../../../external/ntfs-3g/libntfs-3g/mst.c \
    ../../../external/ntfs-3g/libntfs-3g/object_id.c \
    ../../../external/ntfs-3g/libntfs-3g/reparse.c \
    ../../../external/ntfs-3g/libntfs-3g/runlist.c \
    ../../../external/ntfs-3g/libntfs-3g/security.c \
    ../../../external/ntfs-3g/libntfs-3g/unistr.c \
    ../../../external/ntfs-3g/libntfs-3g/unix_io.c \
    ../../../external/ntfs-3g/libntfs-3g/volume.c \
    ../../../external/ntfs-3g/libntfs-3g/realpath.c
LOCAL_C_INCLUDES := \
    external/ntfs-3g \
    external/ntfs-3g/include/fuse-lite \
    external/ntfs-3g/include/ntfs-3g
LOCAL_CFLAGS := -O2 -g -W -Wall -D_LARGEFILE_SOURCE -D_FILE_OFFSET_BITS=64  -DHAVE_CONFIG_H
LOCAL_MODULE := libntfs-3g.recovery
LOCAL_MODULE_TAGS := eng
LOCAL_STATIC_LIBRARIES := libc libcutils
include $(BUILD_STATIC_LIBRARY)


# ntfs-3g
include $(CLEAR_VARS)
LOCAL_SRC_FILES := \
    ../../../external/ntfs-3g/src/ntfs-3g.c \
    ../../../external/ntfs-3g/src/ntfs-3g_common.c
LOCAL_C_INCLUDES := \
    external/ntfs-3g \
    external/ntfs-3g/include/fuse-lite \
    external/ntfs-3g/include/ntfs-3g \
    external/ntfs-3g/androidglue/include \
    external/ntfs-3g/src
LOCAL_CFLAGS := -O2 -g -W -Wall -D_LARGEFILE_SOURCE -D_FILE_OFFSET_BITS=64 -DHAVE_CONFIG_H
LOCAL_MODULE := mount.ntfs-3g
LOCAL_MODULE_CLASS := RECOVERY_EXECUTABLES
LOCAL_MODULE_PATH := $(TARGET_RECOVERY_ROOT_OUT)/sbin
LOCAL_MODULE_TAGS := eng
LOCAL_STATIC_LIBRARIES := libc libfuse-lite.recovery libntfs-3g.recovery
LOCAL_FORCE_STATIC_EXECUTABLE := true
include $(BUILD_EXECUTABLE)

# ntfsprogs - mkntfs
include $(CLEAR_VARS)
LOCAL_SRC_FILES := \
    ../../../external/ntfs-3g/ntfsprogs/attrdef.c \
    ../../../external/ntfs-3g/ntfsprogs/boot.c \
    ../../../external/ntfs-3g/ntfsprogs/sd.c \
    ../../../external/ntfs-3g/ntfsprogs/mkntfs.c \
    ../../../external/ntfs-3g/ntfsprogs/utils.c
LOCAL_C_INCLUDES := \
    external/ntfs-3g \
    external/ntfs-3g/include/fuse-lite \
    external/ntfs-3g/include/ntfs-3g \
    external/ntfs-3g/androidglue/include \
    external/ntfs-3g/ntfsprogs \
    external/e2fsprogs/lib
LOCAL_CFLAGS := -O2 -g -W -Wall -D_LARGEFILE_SOURCE -D_FILE_OFFSET_BITS=64 -DHAVE_CONFIG_H
LOCAL_MODULE := mk_ntfs
LOCAL_MODULE_CLASS := RECOVERY_EXECUTABLES
LOCAL_MODULE_PATH := $(TARGET_RECOVERY_ROOT_OUT)/sbin
LOCAL_MODULE_TAGS := eng
LOCAL_STATIC_LIBRARIES := libc libfuse-lite.recovery libntfs-3g.recovery
LOCAL_FORCE_STATIC_EXECUTABLE := true
include $(BUILD_EXECUTABLE)
