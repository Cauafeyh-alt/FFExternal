TARGET := iphone:clang:latest:14.0
ARCHS = arm64 arm64e

include $(THEOS)/makefiles/common.mk

APPLICATION_NAME = FFExternal

FFExternal_FILES = main.mm hack.cpp aimbot.cpp esp.cpp Memory.cpp
FFExternal_FRAMEWORKS = UIKit AVFoundation QuartzCore
FFExternal_CFLAGS = -std=c++17 -fobjc-arc -I. -Wno-deprecated-declarations -fno-modules
FFExternal_LDFLAGS = -lc++ -lpthread
FFExternal_CODESIGN_FLAGS = -SEntitlements.plist
FFExternal_INSTALL_PATH = /Applications

include $(THEOS_MAKE_PATH)/application.mk
