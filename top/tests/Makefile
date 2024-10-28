PROJECT = top
TEST_DIR = tests/

ifeq ($(DEVELOPER_DIR),)
DEVELOPER_DIR := $(shell xcode-select -p)
endif

test_top: OTHER_LDFLAGS += -ldarwintest_utils -ltop -framework CoreFoundation -framework IOKit
test_top: CODE_SIGN_ENTITLEMENTS = test_top_entitlements.plist

include $(DEVELOPER_DIR)/AppleInternal/Makefiles/darwintest/Makefile.common

include $(DEVELOPER_DIR)/AppleInternal/Makefiles/darwintest/Makefile.targets
