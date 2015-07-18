LOCAL_PATH := $(call my-dir)


include $(CLEAR_VARS)
LOCAL_C_INCLUDES += $(OGG_PATH)/include
LOCAL_SRC_FILES := $(OGG_PATH)/src/bitwise.c \
                   $(OGG_PATH)/src/framing.c
LOCAL_MODULE    := libogg
LOCAL_MODULE_TAGS := optional

#LOCAL_SDK_VERSION := 14

include $(BUILD_STATIC_LIBRARY)



#LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)


LOCAL_MODULE_TAGS :=


LOCAL_MODULE        := libffmpeng
