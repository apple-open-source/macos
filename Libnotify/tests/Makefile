PROJECT := Libnotify
TEST_DIR := tests/

DEVELOPER_DIR ?= /Applications/Xcode.app/Contents/Developer/

ifneq ($(wildcard $(DEVELOPER_DIR)/AppleInternal/Makefiles/Makefile.common),)

include $(DEVELOPER_DIR)/AppleInternal/Makefiles/Makefile.common

TARGETS := notify_disable_test notify_many_dups dispatch_cancel_in_block notify_register_signal
OTHER_TARGETS :=

include $(DEVELOPER_DIR)/AppleInternal/Makefiles/Makefile.targets


# fall back if we can't find the Makefile to avoid an error in that case
else
all:
installhdrs:
install:
	mkdir -p $(DSTROOT)/AppleInternal/CoreOS/tests/$(PROJECT)
endif
