PROJECT := Libc
TEST_DIR := tests/

CUSTOM_TARGETS += osvariantutil subsystem_test_helper thread_stack_pcs_helper

ifeq ($(DEVELOPER_DIR),)
    DEVELOPER_DIR := $(shell xcode-select -p)
endif

include $(DEVELOPER_DIR)/AppleInternal/Makefiles/darwintest/Makefile.common

ifneq ($(PLATFORM),MacOSX)
EXCLUDED_SOURCES += locale.c
EXCLUDED_SOURCES += os_boot_mode.c
else
# All of FreeBSD's libc tests are going to be ATF tests, and ATF isn't currently
# available outside of macOS.  We'll exclude them from the build and also not
# include them in the testing .plist until we have some non-ATF test to add to
# it.
BATS_PLISTS += $(SRCROOT)/FreeBSD/freebsd.plist
endif
BATS_PLISTS += $(SRCROOT)/regex/regex.plist

FRAMEWORK_CFLAGS := $(patsubst %,-iframework %,$(SYSTEM_FRAMEWORK_SEARCH_PATHS))
WARNING_CFLAGS := -Weverything \
	-Wno-vla -Wno-missing-field-initializers -Wno-padded \
	-Wno-gnu-flexible-array-initializer -Wno-gnu-empty-initializer \
	-Wno-partial-availability -Wno-used-but-marked-unused \
	-Wno-reserved-id-macro -fmacro-backtrace-limit=0 \
	-Wno-c++98-compat -Wno-extra-semi -Wno-language-extension-token
OTHER_CFLAGS := -DDARWINTEST --std=gnu11 $(FRAMEWORK_CFLAGS) $(WARNING_CFLAGS) -I$(SDK_SYSTEM_FRAMEWORK_HEADERS) -L/usr/lib/system -lsystem_collections
DT_LDFLAGS += -ldarwintest_utils
ASAN_DYLIB_PATH := /usr/appleinternal/lib/sanitizers/

OTHER_LDFLAGS := -umbrella System -L/usr/lib/system -lsystem_collections

fbounds_safety_assumes: OTHER_CFLAGS += -fbounds-safety
fbounds_safety_includes: OTHER_CFLAGS += -fbounds-safety -D__LIBC_STAGED_BOUNDS_SAFETY_ATTRIBUTES
abort_tests: OTHER_CFLAGS += -fstack-check
strtod: OTHER_CFLAGS += -Wno-float-equal -Wno-ignored-pragmas
strtof: OTHER_CFLAGS += -Wno-float-equal -Wno-ignored-pragmas
strtold: OTHER_CFLAGS += -Wno-float-equal -Wno-ignored-pragmas
nxheap: OTHER_CFLAGS += -Wno-cast-align
strlcat: OTHER_CFLAGS += -Wno-pointer-arith
psort: OTHER_CFLAGS += -Wno-cast-qual -Wno-sign-conversion
net: OTHER_CFLAGS += -Wno-sign-conversion -Wno-cast-align -Wno-incompatible-pointer-types-discards-qualifiers -Wno-sign-compare
printf: OTHER_CFLAGS += -Wno-format-nonliteral
strlcpy: OTHER_CFLAGS += -D_FORTIFY_SOURCE=0
realpath_edge: OTHER_CFLAGS += -fsanitize=address -I../fbsdcompat
realpath_edge: OTHER_LDFLAGS += -Wl,-rpath -Wl,$(ASAN_DYLIB_PATH)
qsort freebsd_qsort: OTHER_CFLAGS += -Wno-unused-function
ifeq ($(PLATFORM),MacOSX)
qsort_perf: OTHER_CFLAGS += -Wno-sign-compare -Wno-sign-conversion -Wno-cast-align -Wno-shorten-64-to-32
else
EXCLUDED_SOURCES += qsort_perf.c
endif
os_variant: OTHER_CFLAGS += -DOS_CRASH_ENABLE_EXPERIMENTAL_LIBTRACE

osvariantutil: OTHER_CFLAGS += -Wno-gnu-statement-expression -Wno-covered-switch-default -DOS_CRASH_ENABLE_EXPERIMENTAL_LIBTRACE

ifeq ($(PLATFORM),MacOSX)
include $(SRCROOT)/FreeBSD/gen/Makefile.targets
include $(SRCROOT)/FreeBSD/string/Makefile.targets
endif
include $(SRCROOT)/regex/Makefile.targets

include $(DEVELOPER_DIR)/AppleInternal/Makefiles/darwintest/Makefile.targets

ifeq ($(PLATFORM),MacOSX)
include $(SRCROOT)/FreeBSD/gen/Makefile
include $(SRCROOT)/FreeBSD/string/Makefile
endif
include $(SRCROOT)/regex/Makefile

OSVARIANTUTIL_INSTALLDIR = $(DSTROOT)/usr/local/bin

install-osvariantutil: osvariantutil
	mkdir -p $(OSVARIANTUTIL_INSTALLDIR)
	cp $(SYMROOT)/osvariantutil $(OSVARIANTUTIL_INSTALLDIR)/
ifeq ($(DWARF_DSYM_FILE_SHOULD_ACCOMPANY_PRODUCT),YES)
	cp -R $(SYMROOT)/osvariantutil.dSYM $(OSVARIANTUTIL_INSTALLDIR)/
endif

subsystem_test: subsystem_test.c subsystem_test-entitlements.plist
subsystem_test: CODE_SIGN_ENTITLEMENTS=subsystem_test-entitlements.plist

install-subsystem_test_helper: subsystem_test_helper
	mkdir -p $(INSTALLDIR)
	cp $(SYMROOT)/subsystem_test_helper $(INSTALLDIR)/
ifeq ($(DWARF_DSYM_FILE_SHOULD_ACCOMPANY_PRODUCT),YES)
	cp -R $(SYMROOT)/subsystem_test_helper.dSYM $(INSTALLDIR)/
endif

thread_stack_pcs: CODE_SIGN_ENTITLEMENTS=get_task_allow_entitlement.plist

thread_stack_pcs_helper: OTHER_LDFLAGS += -dynamiclib

install-thread_stack_pcs_helper: thread_stack_pcs_helper
	mkdir -p $(INSTALLDIR)
	cp $(SYMROOT)/thread_stack_pcs_helper $(INSTALLDIR)/
ifeq ($(DWARF_DSYM_FILE_SHOULD_ACCOMPANY_PRODUCT),YES)
	cp -R $(SYMROOT)/thread_stack_pcs_helper.dSYM $(INSTALLDIR)/
endif

