ARCHS = arm64 arm64e
TARGET = iphone:clang:latest:14.0

include $(THEOS)/makefiles/common.mk

APPLICATION_NAME = FFExternal

FFExternal_FILES = main.mm Memory.cpp aimbot.cpp esp.cpp
FFExternal_FRAMEWORKS = UIKit Foundation AVFoundation
FFExternal_CFLAGS = -fobjc-arc -std=c++17 -Wno-deprecated-declarations -Wno-unused-variable
FFExternal_CODESIGN_FLAGS = -SEntitlements.plist

include $(THEOS_MAKE_PATH)/application.mk
