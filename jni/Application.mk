APP_PROJECT_PATH := $(call my-dir)/..
APP_MODULES      := ray
APP_ABI          := armeabi
APP_OPTIM        := release
APP_STL := gnustl_static
APP_CPPFLAGS += -std=c++11