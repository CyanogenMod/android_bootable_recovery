LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)


LOCAL_MODULE := su.recovery
LOCAL_MODULE_TAGS := eng debug
LOCAL_FORCE_STATIC_EXECUTABLE := true
LOCAL_STATIC_LIBRARIES := libc liblog libcutils
LOCAL_C_INCLUDES := external/sqlite/dist
LOCAL_SRC_FILES := ../../../external/koush/Superuser/Superuser/jni/su/su.c ../../../external/koush/Superuser/Superuser/jni/su/daemon.c ../../../external/koush/Superuser/Superuser/jni/su/activity.c ../../../external/koush/Superuser/Superuser/jni/su/utils.c dbstub.c
LOCAL_CFLAGS := -DSQLITE_OMIT_LOAD_EXTENSION -DREQUESTOR=\"$(SUPERUSER_PACKAGE)\"
ifdef SUPERUSER_PACKAGE_PREFIX
  LOCAL_CFLAGS += -DREQUESTOR_PREFIX=\"$(SUPERUSER_PACKAGE_PREFIX)\"
endif
LOCAL_MODULE_CLASS := RECOVERY_EXECUTABLES
LOCAL_MODULE_PATH := $(TARGET_RECOVERY_ROOT_OUT)/sbin
include $(BUILD_EXECUTABLE)

include $(CLEAR_VARS)
LOCAL_MODULE := install-su.sh
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_CLASS := RECOVERY_EXECUTABLES
LOCAL_MODULE_PATH := $(TARGET_RECOVERY_ROOT_OUT)/sbin
LOCAL_SRC_FILES := $(LOCAL_MODULE)
include $(BUILD_PREBUILT)

include $(CLEAR_VARS)
LOCAL_MODULE := run-su-daemon.sh
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_CLASS := RECOVERY_EXECUTABLES
LOCAL_MODULE_PATH := $(TARGET_RECOVERY_ROOT_OUT)/sbin
LOCAL_SRC_FILES := ../../../external/koush/Superuser/Superuser/assets/install-recovery.sh
include $(BUILD_PREBUILT)
