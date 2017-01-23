LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

APP_PLATFORM    := android-15
LOCAL_MODULE    := ray

LOCAL_LDFLAGS += -fPIE -pie

LOCAL_SRC_FILES := \
\
RayMain.cpp \
UctRating.cpp \
Command.cpp \
DynamicKomi.cpp \
GoBoard.cpp \
Gtp.cpp \
Ladder.cpp \
Message.cpp \
Nakade.cpp \
Pattern.cpp \
PatternHash.cpp \
Point.cpp \
Rating.cpp \
Semeai.cpp \
Simulation.cpp \
UctSearch.cpp \
Utility.cpp \
ZobristHash.cpp \

include $(BUILD_EXECUTABLE)
