LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)
LOCAL_MODULE_TAGS:= optional

LOCAL_CFLAGS += -DCONFIG_UNISOC
LOCAL_CFLAGS += -DUSE_NDK
LOCAL_CFLAGS += -DCONFIG_QCOM

LOCAL_CFLAGS += -DCONFIG_ZTE

LOCAL_CFLAGS += -DCONFIG_EIGENCOMM

LOCAL_C_INCLUDES = $(wildcard $(LOCAL_PATH)/misc_code/ $(LOCAL_PATH)/zte_code/ $(LOCAL_PATH)/qcom_code/ $(LOCAL_PATH)/unisoc_code/ $(LOCAL_PATH)/zte_code/config $(LOCAL_PATH)/qcom_code/config)

SRC_LIST := $(wildcard $(LOCAL_PATH)/misc_code/*.c $(LOCAL_PATH)/*.c)
SRC_LIST +=$(wildcard $(LOCAL_PATH)/zte_code/*.c)
SRC_LIST +=$(wildcard $(LOCAL_PATH)/qcom_code/*.c)
SRC_LIST +=$(wildcard $(LOCAL_PATH)/eigencomm_code/*.c)
SRC_LIST +=$(wildcard $(LOCAL_PATH)/unisoc_code/*.c)

LOCAL_SRC_FILES:= $(SRC_LIST:$(LOCAL_PATH)/%=%)

LOCAL_CFLAGS += -Wall
LOCAL_CFLAGS +=  -Wno-unused-result \
                 -Wno-unused-variable \
                 -Wno-sign-compare \
                 -Wno-pointer-sign \
                 -Wno-unused-function \
                 -Wno-unused-parameter \
                 -Wno-implicit-function-declaration \
                 -Wno-unused-result \
                 -Wno-comment \
                 -Wno-unused-value \
                 -Wno-incompatible-pointer-types \
                 -Wno-format  \
                 -Wno-for-loop-analysis \
                 -Wno-sometimes-uninitialized \
                 -Wno-parentheses-equality \
                 -Wno-uninitialized \
                 -Wno-return-stack-address \
                 -Wno-self-assign \
                 -Wno-unused-label \
                 -Wno-enum-conversion \

LOCAL_MODULE:= log_tool


include $(BUILD_EXECUTABLE)