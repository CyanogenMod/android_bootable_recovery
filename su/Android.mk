LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)


LOCAL_MODULE := su.recovery
LOCAL_MODULE_TAGS := eng debug
LOCAL_FORCE_STATIC_EXECUTABLE := true
LOCAL_STATIC_LIBRARIES := libc
LOCAL_C_INCLUDES := external/sqlite/dist
LOCAL_SRC_FILES := ../../../external/koush/Superuser/Superuser/jni/su/su.c ../../../external/koush/Superuser/Superuser/jni/su/activity.c ../../../external/koush/Superuser/Superuser/jni/su/utils.c dbstub.c
LOCAL_CFLAGS := -DSQLITE_OMIT_LOAD_EXTENSION -DREQUESTOR=\"$(SUPERUSER_PACKAGE)\"
ifdef SUPERUSER_PACKAGE_PREFIX
  LOCAL_CFLAGS += -DREQUESTOR_PREFIX=\"$(SUPERUSER_PACKAGE_PREFIX)\"
endif
LOCAL_MODULE_CLASS := RECOVERY_EXECUTABLES
LOCAL_MODULE_PATH := $(TARGET_RECOVERY_ROOT_OUT)/sbin
include $(BUILD_EXECUTABLE)

