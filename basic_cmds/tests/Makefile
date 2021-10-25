PROJECT := basic_cmds
TEST_DIR := tests

ifeq ($(DEVELOPER_DIR),)
	DEVELOPER_DIR := $(shell xcode-select -p)
endif

include $(DEVELOPER_DIR)/AppleInternal/Makefiles/darwintest/Makefile.common

test_base64: OTHER_CFLAGS+= -I../uuencode ../uuencode/base64.c

include $(DEVELOPER_DIR)/AppleInternal/Makefiles/darwintest/Makefile.targets
