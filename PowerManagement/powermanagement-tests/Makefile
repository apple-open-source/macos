PROJECT := PowerManagement
TEST_DIR := powermanagement-tests
 
 
include $(DEVELOPER_DIR)/AppleInternal/Makefiles/darwintest/Makefile.common
include $(DEVELOPER_DIR)/AppleInternal/Makefiles/darwintest/Makefile.targets

OTHER_LDFLAGS += -framework Foundation
OTHER_LDFLAGS += -framework IOKit

BATS_PLISTS = $(SRCROOT)/../pmconfigd/powerd-test/BATS.plist
CODE_SIGN_ENTITLEMENTS := tests-entitlements.plist
