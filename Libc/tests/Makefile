PROJECT := Libc
TEST_DIR := tests/

ifeq ($(DEVELOPER_DIR),)
    DEVELOPER_DIR := $(shell xcode-select -p)
endif

include $(DEVELOPER_DIR)/AppleInternal/Makefiles/darwintest/Makefile.common

OTHER_CFLAGS := -DDARWINTEST -Weverything -Wno-vla -Wno-missing-field-initializers -Wno-padded -Wno-gnu-flexible-array-initializer --std=gnu11 -Wno-gnu-empty-initializer
OTHER_LDFLAGS := -ldarwintest_utils

nxheap: OTHER_CFLAGS += -Wno-cast-align
strlcat: OTHER_CFLAGS += -Wno-pointer-arith
psort: OTHER_CFLAGS += -Wno-cast-qual -Wno-sign-conversion
net: OTHER_CFLAGS += -Wno-sign-conversion -Wno-cast-align -Wno-incompatible-pointer-types-discards-qualifiers -Wno-sign-compare

include $(DEVELOPER_DIR)/AppleInternal/Makefiles/darwintest/Makefile.targets
