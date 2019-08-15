
LOCAL_PATH := $(call my-dir)

TELEM_PATH := $(LOCAL_PATH)
IN_PATH = $(TELEM_PATH)/configure.ac

LOCAL_DEPENDENCY_HEAD := \
	$(LOCAL_PATH)/android/utils_android.h \
	$(LOCAL_PATH)/src/log.h

############################################
#Build telemetrics library: libtelem_shared#
############################################
include $(CLEAR_VARS)

LOCAL_MODULE := libtelem_shared
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_OWNER := intel
LOCAL_MODULE_CLASS := SHARED_LIBRARIES
LOCAL_PROPRIETARY_MODULE := true
LOCAL_SRC_FILES := \
	src/util.c \
	src/configuration.c \
	src/nica/inifile.c \
	src/nica/hashmap.c \
	src/nica/b64enc.c \
	src/common.c

LOCAL_C_INCLUDES := \
	$(LOCAL_PATH) \
	$(LOCAL_PATH)/src \
	$(LOCAL_PATH)/src/nica

LOCAL_SHARED_LIBRARIES := \
	libc \
	libcutils

GEN := $(local-generated-sources-dir)/config_android.h
$(GEN): GEN_TOOL := $(shell $(TELEM_PATH)/config_gen.sh $(IN_PATH))
$(GEN): $(GEN_TOOL)
	$(transform-generated-source)

LOCAL_GENERATE_SOURCE += $(GEN)
LOCAL_LDLIBS := -llog
LOCAL_CFLAGS += -Wno-unused-parameter
$(LOCAL_MODULE): $(LOCAL_DEPENDENCY_HEAD)

include $(BUILD_SHARED_LIBRARY)

#########################################
#Build telemetrics library: libtelemetry#
#########################################
include $(CLEAR_VARS)
LOCAL_MODULE := libtelemetry
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_OWNER := intel
LOCAL_PROPRIETARY_MODULE := true
LOCAL_SRC_FILES := \
	src/telemetry.c

LOCAL_C_INCLUDES := \
	$(LOCAL_PATH) \
	$(LOCAL_PATH)/src

LOCAL_SHARED_LIBRARIES := \
	libtelem_shared

LOCAL_LDLIBS := -llog
LOCAL_CFLAGS += -Wno-unused-parameter -Wno-pointer-bool-conversion -Wno-unknown-warning-option -Wno-format
$(LOCAL_MODULE): $(LOCAL_DEPENDENCY_HEAD)

include $(BUILD_SHARED_LIBRARY)

#########################################
# Build the telemetry daemon: telemprobd#
#########################################
include $(CLEAR_VARS)
LOCAL_MODULE := telemprobd
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_OWNER := intel
LOCAL_PROPRIETARY_MODULE := true

LOCAL_SRC_FILES := \
	src/probe.c \
	src/telemdaemon.c \
	src/journal/journal.c

LOCAL_C_INCLUDES := \
	$(LOCAL_PATH) \
	$(LOCAL_PATH)/src \
	$(LOCAL_PATH)/src/journal

LOCAL_SHARED_LIBRARIES := \
	$(CURL_LIBS) \
	libcurl \
	libtelem_shared \
	libtelemetry \
	libc \
	libcutils

LOCAL_LDLIBS := -llog
LOCAL_CFLAGS += -Wno-unused-parameter
$(LOCAL_MODULE): $(LOCAL_DEPENDENCY_HEAD)

include $(BUILD_EXECUTABLE)

#########################################
# Build the telemetry daemon: telempostd#
#########################################
include $(CLEAR_VARS)

LOCAL_MODULE := telempostd
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_OWNER := intel
LOCAL_PROPRIETARY_MODULE := true

LOCAL_SRC_FILES := \
	src/post.c \
	src/telempostdaemon.c \
	src/journal/journal.c \
	src/spool.c \
	src/retention.c \
	src/iorecord.c

LOCAL_C_INCLUDES := \
	$(LOCAL_PATH) \
	$(LOCAL_PATH)/src \
	$(LOCAL_PATH)/src/journal

LOCAL_SHARED_LIBRARIES := \
	$(CURL_LIBS) \
	libcurl \
	libtelem_shared \
	libtelemetry \
	libc \
	libcutils

LOCAL_LDLIBS := -llog
LOCAL_CFLAGS += -Wno-unused-parameter -Wno-incompatible-pointer-types
$(LOCAL_MODULE): $(LOCAL_DEPENDENCY_HEAD)

include $(BUILD_EXECUTABLE)

##############################
# Build all telemetry probes #
##############################
ifeq ($(wildcard $(LOCAL_PATH)/src/probes/probes.mk),)
else
include $(LOCAL_PATH)/src/probes/probes.mk
endif
