PROJECT := libmalloc
TEST_DIR := tests/

DEVELOPER_DIR ?= $(shell xcode-select -p)

include $(DEVELOPER_DIR)/AppleInternal/Makefiles/darwintest/Makefile.common

TRACE_FILES := \
	$(notdir $(wildcard $(SRCROOT)/../traces/*.mtrace))

# add trace files without the traces/ prefix, fex:
#   exclude_this_file.mtrace
EXCLUDED_TRACE_FILES :=

OTHER_TEST_TARGETS = \
	$(addprefix nano-trace-replay_, $(basename $(filter-out $(EXCLUDED_TRACE_FILES), $(TRACE_FILES)))) 

BATS_PLISTS_EXTRACT = \
	$(patsubst %,$(SYMROOT)/%.plist,$(OTHER_TEST_TARGETS))

EXCLUDED_SOURCES := \
	nano_trace_replay.c
CXX := $(shell $(XCRUN) -sdk "$(TARGETSDK)" -find clang++)

WARNING_CFLAGS := -Wno-format-invalid-specifier -Wno-format-extra-args
OTHER_CFLAGS += \
	-DDARWINTEST \
	-DOS_ATOMIC_CONFIG_MEMORY_ORDER_DEPENDENCY=1 \
	-DOS_UNFAIR_LOCK_INLINE=1 \
	-DT_META_TAG_XZONE=T_META_TAG\(\"xzone\"\) \
	-DT_META_TAG_XZONE_ONLY=T_META_TAG\(\"xzone_only\"\) \
	-fno-builtin \
	-fno-typed-memory-operations \
	-lCrashReporterClient \
	-I$(SDKROOT)/System/Library/Frameworks/System.framework/PrivateHeaders \
	-I$(SRCROOT)/../private \
	-I$(SRCROOT)/../include \
	$(WARNING_CFLAGS)

PRIVATE_FRAMEWORKS = $(SDKROOT)/System/Library/PrivateFrameworks
PERFDATA_FRAMEWORK = perfdata

nano-trace-replay_%: $(SRCROOT)/nano_trace_replay.c
	$(CC) \
		$(CFLAGS) $(DT_CFLAGS) $(OTHER_CFLAGS) \
		$(LDFLAGS) $(DT_LDFLAGS) $(OTHER_LDFLAGS) \
		-DTRACE_NAME="$(patsubst nano-trace-replay_%,%,$(notdir $@))" \
		$(patsubst _Photos_puja,-DTEST_TIMEOUT=3000,$(findstring _Photos_puja,$(notdir $@))) \
		-o $@ \
		$(SRCROOT)/nano_trace_replay.c

BATS_PLISTS = $(BATS_PLISTS_EXTRACT)
BATS_PLISTS += $(PROJECT_DIR)/tests/xctests/BATS.plist

$(BATS_PLISTS_EXTRACT): %.plist : %
	$(EXTRACTMETA) extract -i /$(INSTALLPATH)/$(notdir $<) -b $(SYMROOT)/$(notdir $<) -o $@
	@plutil -convert binary1 $@

SANITIZER_DYLIB_PATH := /usr/appleinternal/lib/sanitizers/
asan: OTHER_CFLAGS += -fsanitize=address
asan: OTHER_LDFLAGS += -Wl,-rpath -Wl,$(SANITIZER_DYLIB_PATH)

ifeq ($(Embedded),NO)
tsan: CFLAGS := $(filter-out $(ARCH_FLAGS),$(CFLAGS)) -arch x86_64 -fsanitize=thread
tsan: OTHER_LDFLAGS += -Wl,-rpath -Wl,$(SANITIZER_DYLIB_PATH)
else
EXCLUDED_SOURCES += tsan.c
endif

bounds_test: OTHER_CFLAGS += -fbounds-attributes

malloc_msl: OTHER_CFLAGS += -F $(PRIVATE_FRAMEWORKS)
malloc_msl: OTHER_CFLAGS += -framework MallocStackLogging
malloc_msl: OTHER_LDFLAGS += -F $(PRIVATE_FRAMEWORKS)
malloc_msl: OTHER_LDFLAGS += -framework MallocStackLogging

malloc_with_options_test: OTHER_LDFLAGS += -F $(PRIVATE_FRAMEWORKS)
malloc_with_options_test: OTHER_LDFLAGS += -framework ktrace
malloc_with_options_test: OTHER_CFLAGS += -I../src

madvise: OTHER_CFLAGS += -I../src
malloc_claimed_address_tests: OTHER_CFLAGS += -I../src
scribble_tests: OTHER_CFLAGS += -I../src
malloc_size_test: OTHER_CFLAGS += -I../src

xzone: OTHER_LDFLAGS += -ldarwintest_utils

malloc_type: OTHER_CFLAGS += -ftyped-memory-operations

EXCLUDED_SOURCES += malloc_type_callsite_cpp.cpp

malloc_type_callsite_cpp.o: malloc_type_callsite_cpp.cpp
	$(CXX) $(OTHER_CXXFLAGS) $(CXXFLAGS) -fno-typed-memory-operations -fno-typed-cxx-new-delete -c $< -o $(SYMROOT)/$@

malloc_type_callsite: malloc_type_callsite_cpp.o
malloc_type_callsite: OTHER_CFLAGS += $(SYMROOT)/malloc_type_callsite_cpp.o
malloc_type_callsite: OTHER_LDFLAGS += -lc++

# BATS test plist handling
#  - Modify plist (add tests) to run general tests again with xzone malloc enabled
project_test_configs: bats.plist
	cp ./libmalloc_pgm.plist $(DSTROOT)/AppleInternal/CoreOS/BATS/unit_tests/
	xcrun -sdk $(SDKROOT) python3 ./add_xzone_tests.py $(SYMROOT)/bats.plist --platform $(PLATFORM)

.PHONY: project_test_configs

# Ensure that we add the xzone tests before bats.plist is installed to the
# dstroot
install: project_test_configs

.DEFAULT_GOAL := all

include $(DEVELOPER_DIR)/AppleInternal/Makefiles/darwintest/Makefile.targets
