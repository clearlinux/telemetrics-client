LOCAL_PATH := $(call my-dir)

################
# build hprobe #
################

include $(CLEAR_VARS)

LOCAL_MODULE := hprobe
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_OWNER := intel
LOCAL_MODULE_CLASS := EXECUTABLES
LOCAL_SHARED_LIBRARIES := libtelemetry
LOCAL_LDLIBS := -llog
LOCAL_CFLAGS += -Wno-unused-parameter
LOCAL_SRC_FILES := \
	hello.c

LOCAL_C_INCLUDES := $(LOCAL_PATH) $(LOCAL_PATH)/.. $(LOCAL_PATH)/../..
LOCAL_PROPRIETARY_MODULE := true
$(LOCAL_MODULE): $(LOCAL_DEPENDENCY_HEAD)

include $(BUILD_EXECUTABLE)

####################
# build crashprobe #
####################

# include $(CLEAR_VARS)
#
# LOCAL_MODULE := crashprobe
# LOCAL_MODULE_TAGS := optional
# LOCAL_MODULE_OWNER := intel
# LOCAL_MODULE_CLASS := EXECUTABLES
# LOCAL_SHARED_LIBRARIES := \
# 	libtelemetry \
# 	libtelem_shared
#
# LOCAL_STATIC_LIBRARIES := libelf
# LOCAL_LDLIBS := -llog
# LOCAL_CFLAGS += -Wno-unused-parameter
# LOCAL_SRC_FILES := \
# 	crash_probe.c \
# 	../nica/nc-string.c
# LOCAL_C_INCLUDES := $(LOCAL_PATH) $(LOCAL_PATH)/.. $(LOCAL_PATH)/../../android
# LOCAL_PROPRIETARY_MODULE := true
# $(LOCAL_MODULE): $(LOCAL_DEPENDENCY_HEAD)
#
# include $(BUILD_EXECUTABLE)

##########################
# build telem-record-gen #
##########################

include $(CLEAR_VARS)

LOCAL_MODULE := telem-record-gen
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_OWNER := intel
LOCAL_MODULE_CLASS := EXECUTABLES
LOCAL_SHARED_LIBRARIES := libtelemetry
LOCAL_LDLIBS := -llog
LOCAL_CFLAGS += -Wno-unused-parameter
LOCAL_SRC_FILES := \
	telem_record_gen.c

LOCAL_C_INCLUDES := $(LOCAL_PATH) $(LOCAL_PATH)/.. $(LOCAL_PATH)/../..
LOCAL_PROPRIETARY_MODULE := true
$(LOCAL_MODULE): $(LOCAL_DEPENDENCY_HEAD)

include $(BUILD_EXECUTABLE)

#####################
# build klogscanner #
#####################

include $(CLEAR_VARS)

LOCAL_MODULE := klogscanner
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_OWNER := intel
LOCAL_MODULE_CLASS := EXECUTABLES
LOCAL_SHARED_LIBRARIES := libtelemetry libtelem_shared
LOCAL_LDFLAGS := $(AM_LDFLAGS) -pie
LOCAL_LDLIBS := -llog
LOCAL_CFLAGS += -Wno-unused-parameter -Wno-missing-field-initializers
LOCAL_SRC_FILES := \
	klog_main.c \
	klog_scanner.c \
	../nica/nc-string.c \
	oops_parser.c

LOCAL_C_INCLUDES := $(LOCAL_PATH) $(LOCAL_PATH)/.. $(LOCAL_PATH)/../..
LOCAL_PROPRIETARY_MODULE := true
$(LOCAL_MODULE): $(LOCAL_DEPENDENCY_HEAD)

include $(BUILD_EXECUTABLE)


#####################
# build pstoreprobe #
#####################

pstoredir:="/sys/fs/pstore"
include $(CLEAR_VARS)

LOCAL_MODULE := pstoreprobe
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_OWNER := intel
LOCAL_MODULE_CLASS := EXECUTABLES
LOCAL_SHARED_LIBRARIES := libtelemetry libtelem_shared
LOCAL_CFLAGS += -DPSTOREDIR='$(pstoredir)'
LOCAL_LDFLAGS := $(AM_LDFLAGS) -pie
LOCAL_LDLIBS := -llog
LOCAL_CFLAGS += -Wno-unused-parameter -Wno-missing-field-initializers
LOCAL_SRC_FILES := \
	pstore_probe.c \
	../nica/nc-string.c \
	oops_parser.c

LOCAL_C_INCLUDES := $(LOCAL_PATH) $(LOCAL_PATH)/.. $(LOCAL_PATH)/../..
LOCAL_PROPRIETARY_MODULE := true
$(LOCAL_MODULE): $(LOCAL_DEPENDENCY_HEAD)

include $(BUILD_EXECUTABLE)

#####################
# build pythonprobe #
#####################

include $(CLEAR_VARS)

LOCAL_MODULE := pythonprobe
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_OWNER := intel
LOCAL_MODULE_CLASS := EXECUTABLES
LOCAL_SHARED_LIBRARIES := libtelemetry
LOCAL_LDFLAGS := $(AM_LDFLAGS) -pie
LOCAL_LDLIBS := -llog
LOCAL_CFLAGS += -Wno-unused-parameter
LOCAL_SRC_FILES := \
	python-probe.c

LOCAL_C_INCLUDES := $(LOCAL_PATH) $(LOCAL_PATH)/.. $(LOCAL_PATH)/../..
LOCAL_PROPRIETARY_MODULE := true
$(LOCAL_MODULE): $(LOCAL_DEPENDENCY_HEAD)

include $(BUILD_EXECUTABLE)

#####################
# build pstoreclean #
#####################

include $(CLEAR_VARS)

LOCAL_MODULE := pstoreclean
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_OWNER := intel
LOCAL_MODULE_CLASS := EXECUTABLES
LOCAL_SHARED_LIBRARIES := libtelemetry libtelem_shared
LOCAL_LDFLAGS := $(AM_LDFLAGS) -pie
LOCAL_CFLAGS += -DPSTOREDIR='$(pstoredir)'
LOCAL_LDLIBS := -llog
LOCAL_CFLAGS += -Wno-unused-parameter
LOCAL_SRC_FILES := \
	pstore_clean.c

LOCAL_C_INCLUDES := $(LOCAL_PATH) $(LOCAL_PATH)/.. $(LOCAL_PATH)/../..
LOCAL_PROPRIETARY_MODULE := true
$(LOCAL_MODULE): $(LOCAL_DEPENDENCY_HEAD)

include $(BUILD_EXECUTABLE)

###################
# build bertprobe #
###################

include $(CLEAR_VARS)

LOCAL_MODULE := bertprobe
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_OWNER := intel
LOCAL_MODULE_CLASS := EXECUTABLES
LOCAL_SHARED_LIBRARIES := libtelemetry
LOCAL_LDFLAGS := $(AM_LDFLAGS) -pie
LOCAL_LDLIBS := -llog
LOCAL_CFLAGS += -Wno-unused-parameter
LOCAL_SRC_FILES := \
	bert_probe.c \
	../nica/b64enc.c

LOCAL_C_INCLUDES := $(LOCAL_PATH) $(LOCAL_PATH)/.. $(LOCAL_PATH)/../..
LOCAL_PROPRIETARY_MODULE := true
$(LOCAL_MODULE): $(LOCAL_DEPENDENCY_HEAD)

include $(BUILD_EXECUTABLE)
