PROJECT := libmalloc
TEST_DIR := tests/

DEVELOPER_DIR ?= $(shell xcode-select -p)

include $(DEVELOPER_DIR)/AppleInternal/Makefiles/darwintest/Makefile.common

TRACE_FILES := \
	$(notdir $(wildcard $(SRCROOT)/../traces/*.mtrace))

# add trace files without the traces/ prefix, fex:
#   exclude_this_file.mtrace
EXCLUDED_TRACE_FILES :=

MALLOC_EXCLAVES := $(if $(findstring Exclave,$(PLATFORM)),YES,NO)
MALLOC_EXCLAVECORE := $(if $(findstring ExclaveCore,$(PLATFORM)),YES,NO)
MALLOC_EXCLAVEKIT := $(if $(findstring ExclaveKit,$(PLATFORM)),YES,NO)

ifeq ($(MALLOC_EXCLAVES),NO)
OTHER_TEST_TARGETS = \
	$(addprefix nano-trace-replay_, $(basename $(filter-out $(EXCLUDED_TRACE_FILES), $(TRACE_FILES))))
endif

BATS_PLISTS_EXTRACT = \
	$(patsubst %,$(SYMROOT)/%.plist,$(OTHER_TEST_TARGETS))

EXCLUDED_SOURCES := \
	nano_trace_replay.c
CXX := $(shell $(XCRUN) -sdk "$(TARGETSDK)" -find clang++)

WARNING_CFLAGS := -Wno-format-invalid-specifier -Wno-format-extra-args
OTHER_CFLAGS += \
	-DDARWINTEST \
	-DOS_ATOMIC_CONFIG_MEMORY_ORDER_DEPENDENCY=1 \
	-DT_META_TAG_XZONE=T_META_TAG\(\"xzone\"\) \
	-DT_META_TAG_XZONE_ONLY=T_META_TAG\(\"xzone_only\"\) \
	-DT_META_TAG_NANO_ON_XZONE=T_META_TAG\(\"nano_on_xzone\"\) \
	-I$(SDKROOT)/System/Library/Frameworks/System.framework/PrivateHeaders \
	-I$(SRCROOT)/../private \
	-I$(SRCROOT)/../include \
	$(WARNING_CFLAGS)

ifeq ($(MALLOC_EXCLAVES),NO)
OTHER_CFLAGS += -fno-builtin -fno-typed-memory-operations
endif

ifeq ($(MALLOC_EXCLAVEKIT),YES)
INSTALLPATH := System/ExclaveKit/usr/local/bin
OTHER_CFLAGS += -DL4_CONFIG_PLAT_EVP
OTHER_CFLAGS += -isystem $(SDKROOT)/System/ExclaveKit/usr/local/standalone/cL4-future/include
OTHER_LDFLAGS += -L $(SDKROOT)/System/ExclaveKit/usr/local/lib
OTHER_LDFLAGS += -lc++

CUSTOM_TARGETS += libmalloc-exclavekit-darwintests
CUSTOM_TARGETS += libmalloc_exclavekit.plist
endif

ifeq ($(MALLOC_EXCLAVECORE),YES)
INSTALLPATH := System/ExclaveCore/usr/local/bin
OTHER_CFLAGS += -isystem $(SDKROOT)/System/ExclaveCore/usr/local/standalone/cL4-future/include
OTHER_LDFLAGS += -L $(SDKROOT)/System/ExclaveCore/usr/local/lib
OTHER_LDFLAGS += \
	-nostartfiles \
	-pagezero_size 0x0 \
	-seg1addr 0x07FFC000 \
	-static \
	-e _start \
	-Wl,-threaded_starts_section \
	-dead_strip \
	-fapple-link-rtlib \
	-lc++ \
	-lc++abi \
	-llibc \
	-llibc_plat_cl4_standalone_malloc \
	-lm \
	-lmalloc \
	-lpmm \
	-lvas \
	-lvas-core \
	-lxrt

CUSTOM_TARGETS += libmalloc-exclavecore-darwintests
CUSTOM_TARGETS += libmalloc_exclavecore.plist
endif

# For minirunner support only
EXCLUDED_SOURCES += exclavecore_support.c

ifeq ($(MALLOC_EXCLAVES),YES)
# In order to not generate any standalone test exectuables, we need TEST_TARGETS
# to be defined but empty, which the recursive expansion assignment to an
# undefined variable here does
TEST_TARGETS=$(UNDEFINED_VARIABLE)

OTHER_CFLAGS += -I ../src

# PGM specific tests
EXCLUDED_SOURCES += \
	pgm_allocator.c \
	pgm_integration.c \
	pgm_internals.c \
	pgm_zone_api.c \
	stack_trace_test.c

# Magazine Malloc and Nano specific tests
EXCLUDED_SOURCES += \
	magazine_malloc.c \
	magazine_medium_test.c \
	magazine_small_test.c \
	magazine_rack.c \
	magazine_tiny_test.c \
	region_cookie_test.c \
	nano_tests.c

# Malloc zones are largely unsupported on exclaves (no malloc_create_zone or
# malloc_num_zones)
EXCLUDED_SOURCES += \
	zone_names.c \
	malloc_create_purgeable_zone.c \
	malloc_create_zone_tests.c \
	malloc_zone_unregister_test.c

# Exclaves don't support environment variables
EXCLUDED_SOURCES += scribble_tests.c

# Exclaves don't support blocks, so the introspector doesn't run
EXCLUDED_SOURCES += introspect_tests.c

# Exclaves doesn't have fork
EXCLUDED_SOURCES += xzone_corruption.c

# These tests reboot the system with bootargs set, doesn't apply to exclaves
EXCLUDED_SOURCES += xzone.c

# Exclaves should always be built with TMO
EXCLUDED_SOURCES += malloc_type_callsite.c

# Darwin specific behaviours
EXCLUDED_SOURCES += \
	asan.c \
	tsan.c \
	memory_pressure.c \
	malloc_msl.c \
	madvise.c

# Exclaves don't support dispatch_apply, which this perf test relies on
EXCLUDED_SOURCES += basic_malloc_free_perf.c

# These two tests both include C files to test submodules, which causes symbol
# conflicts in the statically linked exclavecore tests
ifeq ($(MALLOC_EXCLAVECORE),YES)
EXCLUDED_SOURCES += \
	mfm_test.c \
	xzone_metapool_tests.c
endif # MALLOC_EXCLAVECORE
else
OTHER_CFLAGS += \
	-DOS_UNFAIR_LOCK_INLINE=1 \
	-lCrashReporterClient
endif # MALLOC_EXCLAVES

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

pgm_integration: OTHER_CFLAGS += -F $(PRIVATE_FRAMEWORKS)
pgm_integration: OTHER_CFLAGS += -framework MallocStackLogging
pgm_integration: OTHER_LDFLAGS += -F $(PRIVATE_FRAMEWORKS)
pgm_integration: OTHER_LDFLAGS += -framework MallocStackLogging

ifeq ($(MALLOC_EXCLAVES),NO)
malloc_with_options_test: OTHER_LDFLAGS += -F $(PRIVATE_FRAMEWORKS)
malloc_with_options_test: OTHER_LDFLAGS += -framework ktrace
endif
malloc_with_options_test: OTHER_CFLAGS += -I../src

madvise: OTHER_CFLAGS += -I../src
malloc_claimed_address_tests: OTHER_CFLAGS += -I../src
scribble_tests: OTHER_CFLAGS += -I../src
malloc_size_test: OTHER_CFLAGS += -I../src

xzone: OTHER_LDFLAGS += -ldarwintest_utils

# -fno-builtin also disables TMO
#
# Build this test with no explicit TMO options to validate that it gets TMO
# automatically on platforms where that's expected

printer_tests: OTHER_LDFLAGS += -lobjc -fobjc-arc -ldarwintest_utils

MALLOC_TYPE_SWIFT_OBJS := $(addsuffix .o,$(addprefix malloc_type_swift_,$(ARCH_CONFIGS)))
MALLOC_TYPE_SYM_SWIFT_OBJS := $(addprefix $(SYMROOT)/,$(MALLOC_TYPE_SWIFT_OBJS))

malloc_type: OTHER_CFLAGS := $(filter-out -fno-builtin -fno-typed-memory-operations,$(OTHER_CFLAGS))
malloc_type: OTHER_CFLAGS += $(SYMROOT)/malloc_type_cpp.o $(MALLOC_TYPE_SYM_SWIFT_OBJS) $(SYMROOT)/malloc_type_objc.o
malloc_type: $(SYMROOT)/malloc_type_cpp.o $(MALLOC_TYPE_SYM_SWIFT_OBJS) $(SYMROOT)/malloc_type_objc.o
malloc_type: OTHER_LDFLAGS += -lc++ -L $(TOOLCHAIN_DIR)/usr/lib/swift/

$(SYMROOT)/malloc_type_objc.o: malloc_type_objc.m
	$(CC) $(CFLAGS) $(OBJCFLAGS) -c $< -o $@

$(SYMROOT)/malloc_type_cpp.o: malloc_type_cpp.cpp
	$(CXX) $(OTHER_CXXFLAGS) $(CXXFLAGS) -c $< -o $@

SWIFTC := "$(shell $(XCRUN) -sdk "$(TARGETSDK)" -find swiftc)"
SWIFT_OBJ_FLAGS := -emit-object -parse-as-library -import-objc-header xzone_swift_bridge.h

$(SYMROOT)/malloc_type_swift.swift: malloc_type_swift.swift.i
	$(CC) -x c -P -E $< -o $@

$(SYMROOT)/malloc_type_swift_%.o: $(SYMROOT)/malloc_type_swift.swift
	$(SWIFTC) -target $*-$(subst -target unknown-,,$(DEPLOYMENT_TARGET_FLAGS)) $(SWIFT_OBJ_FLAGS) $< -o $@

EXCLUDED_SOURCES += malloc_type_cpp.cpp malloc_type_callsite_cpp.cpp malloc_type_swift.swift.i malloc_type_objc.m

$(SYMROOT)/malloc_type_callsite_cpp.o: malloc_type_callsite_cpp.cpp
	$(CXX) $(OTHER_CXXFLAGS) $(CXXFLAGS) -fno-typed-memory-operations -fno-typed-cxx-new-delete -c $< -o $@

malloc_type_callsite: $(SYMROOT)/malloc_type_callsite_cpp.o
malloc_type_callsite: OTHER_CFLAGS += $(SYMROOT)/malloc_type_callsite_cpp.o
malloc_type_callsite: OTHER_LDFLAGS += -lc++

ifeq ($(MALLOC_EXCLAVES),NO)
# BATS test plist handling
#  - Create separate plist to re-run test suite with PGM
#  - Modify plist (add tests) to run general tests again with xzone malloc enabled
project_test_configs: bats.plist
	mkdir -p $(DSTROOT)/AppleInternal/CoreOS/BATS/unit_tests/
	xcrun -sdk $(SDKROOT) python3 ./create_pgm_plist.py $(SYMROOT)/bats.plist $(SYMROOT)/libmalloc_pgm.plist
	cp $(SYMROOT)/libmalloc_pgm.plist $(DSTROOT)/AppleInternal/CoreOS/BATS/unit_tests/
	xcrun -sdk $(SDKROOT) python3 ./add_xzone_tests.py $(SYMROOT)/bats.plist --platform $(PLATFORM)

.PHONY: project_test_configs

# Ensure that we add the xzone tests before bats.plist is installed to the
# dstroot
install: project_test_configs
endif

.DEFAULT_GOAL := all

include $(DEVELOPER_DIR)/AppleInternal/Makefiles/darwintest/Makefile.targets

ifeq ($(MALLOC_EXCLAVEKIT),YES)
libmalloc-exclavekit-darwintests: $(SOURCES_c) $(SYMROOT)/malloc_type_cpp.o
	$(CC) $(DT_CFLAGS) $(OTHER_CFLAGS) $(CFLAGS) $(DT_LDFLAGS) $(OTHER_LDFLAGS) $(LDFLAGS) $^ -o $(SYMROOT)/$@
install-libmalloc-exclavekit-darwintests: libmalloc-exclavekit-darwintests
	mkdir -p $(INSTALLDIR)
	cp $(SYMROOT)/$(patsubst install-%,%,$@) $(INSTALLDIR)/
libmalloc_exclavekit.plist: $(PROJECT_DIR)/tests/exclave/libmalloc_exclavekit.plist
	plutil -convert binary1 -o $(SYMROOT)/$@ $<
install-libmalloc_exclavekit.plist: libmalloc_exclavekit.plist
	mkdir -p $(BATS_PLIST_DIR)
	cp $(SYMROOT)/$(patsubst install-%,%,$@) $(BATS_PLIST_DIR)/
endif

ifeq ($(MALLOC_EXCLAVECORE),YES)
libmalloc-exclavecore-darwintests: $(SOURCES_c) exclavecore_support.c $(SYMROOT)/malloc_type_cpp.o
	$(CC) $(DT_CFLAGS) $(OTHER_CFLAGS) $(CFLAGS) $(DT_LDFLAGS) $(OTHER_LDFLAGS) $(LDFLAGS) $^ -o $(SYMROOT)/$@
install-libmalloc-exclavecore-darwintests : libmalloc-exclavecore-darwintests
	mkdir -p $(INSTALLDIR)
	cp $(SYMROOT)/$(patsubst install-%,%,$@) $(INSTALLDIR)/
libmalloc_exclavecore.plist: $(PROJECT_DIR)/tests/exclave/libmalloc_exclavecore.plist
	plutil -convert binary1 -o $(SYMROOT)/$@ $<
install-libmalloc_exclavecore.plist: libmalloc_exclavecore.plist
	mkdir -p $(BATS_PLIST_DIR)
	cp $(SYMROOT)/$(patsubst install-%,%,$@) $(BATS_PLIST_DIR)/
endif
