TARGET = native

include theos/makefiles/common.mk

TOOL_NAME = bozocache
bozocache_FILES = main.c
bozocache_FRAMEWORKS = CoreFoundation
bozocache_PRIVATE_FRAMEWORKS = MobileDevice

include $(THEOS_MAKE_PATH)/tool.mk
