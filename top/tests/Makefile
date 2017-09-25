PROJECT = top
TEST_DIR = tests/

ifeq ($(DEVELOPER_DIR),)
DEVELOPER_DIR := $(shell xcode-select -p)
endif

test_top: OTHER_LDFLAGS += -ldarwintest_utils

include $(DEVELOPER_DIR)/AppleInternal/Makefiles/darwintest/Makefile.common

include $(DEVELOPER_DIR)/AppleInternal/Makefiles/darwintest/Makefile.targets
