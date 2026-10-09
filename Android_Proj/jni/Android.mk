LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE := ka_soft_BarcodeScannerHID
LOCAL_LDLIBS := -llog -landroid
LOCAL_CFLAGS += -fexceptions -std=c++11

LOCAL_C_INCLUDES += $(LOCAL_PATH) $(LOCAL_PATH)/../../../include
LOCAL_SRC_FILES := \
    jnienv.cpp \
    ../../AddInNative.cpp \
    ../../BarcodeScannerHID.cpp \
    ../../ConversionWchar.cpp \
    ../../dllmain.cpp \
    ../../stdafx.cpp

include $(BUILD_SHARED_LIBRARY)
