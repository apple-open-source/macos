PROJECT := libedit
TEST_DIR := tests

ifeq ($(DEVELOPER_DIR),)
    DEVELOPER_DIR := $(shell xcode-select -p)
endif

include $(DEVELOPER_DIR)/AppleInternal/Makefiles/darwintest/Makefile.common
include $(DEVELOPER_DIR)/AppleInternal/Makefiles/darwintest/Makefile.targets

OTHER_CFLAGS += -fsanitize=address
OTHER_LDFLAGS += -ledit
