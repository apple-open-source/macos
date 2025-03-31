.PHONY: all installhdrs install $(DEVELOPER_DIR)/AppleInternal/Makefiles/darwintest/Makefile.targets $(DEVELOPER_DIR)/AppleInternal/Makefiles/darwintest/Makefile.common Makefile

PROJECT := Libnotify
TEST_DIR := tests/
MIG:=SDKROOT=$(SDKROOT) $(shell xcrun -sdk "$(TARGETSDK)" -find mig)
OTHER_LDFLAGS := -ldarwintest

DEVELOPER_DIR ?= /Applications/Xcode.app/Contents/Developer/

rnserver:
	$(MIG) $(CFLAGS) \
		-user /dev/null \
		-header $(OBJROOT)/rn.h \
		-sheader $(OBJROOT)/rnserver.h \
		-server $(OBJROOT)/rnserver.c \
		rnserver.defs
install-rnserver: ;

notify_register_mach_port: rnserver
notify_register_mach_port: CODE_SIGN_ENTITLEMENTS = notify_register_mach_port.entitlements
notify_register_mach_port: OTHER_LDFLAGS += -ldarwintest_utils -lpthread
notify_register_mach_port: OTHER_CFLAGS += $(OBJROOT)/rnserver.c -I $(OBJROOT)

notify_introspect: CODE_SIGN_ENTITLEMENTS = notify_introspect.entitlements
notify_restrict_post: CODE_SIGN_ENTITLEMENTS = notify_restrict_post.entitlements

ifneq ($(wildcard $(DEVELOPER_DIR)/AppleInternal/Makefiles/darwintest/Makefile.common),)

include $(DEVELOPER_DIR)/AppleInternal/Makefiles/darwintest/Makefile.common

EXCLUDED_SOURCES := \
	random_test.c \
	notify_control.c \
	notify_test_helper.c \
	notify_loopback_test_helper.c \


include $(DEVELOPER_DIR)/AppleInternal/Makefiles/darwintest/Makefile.targets


# fall back if we can't find the Makefile to avoid an error in that case
else
all:
installhdrs:
install:
	mkdir -p $(DSTROOT)/AppleInternal/CoreOS/tests/$(PROJECT)
endif
