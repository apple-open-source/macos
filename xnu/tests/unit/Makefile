# example of how to run this from XNU root, see README.md:
#   make -C tests/unit SDKROOT=macosx.internal example_test_osfmk

PROJECT := xnu/unit_tests

ifdef BASEDSTROOT
override DSTROOT = $(BASEDSTROOT)
endif

DEVELOPER_DIR ?= $(shell xcode-select -p)
INVALID_ARCHS = $(filter-out arm64%,$(ARCH_CONFIGS))

.DEFAULT_GOAL := install

ORIG_SYMROOT := $(SYMROOT)

ifdef OBJROOT
XNU_OBJROOT := $(OBJROOT)
else
XNU_OBJROOT = $(XNU_ROOT)/BUILD/obj
endif

include $(DEVELOPER_DIR)/AppleInternal/Makefiles/darwintest/Makefile.common

ifndef KERNEL_CONFIG
KERNEL_CONFIG = development
endif
ifndef PRODUCT_CONFIG
PRODUCT_CONFIG = j414c
endif

# find the name of the XNU library
XNU_ROOT := $(abspath $(SRCROOT)/../..)
KERNEL_CONFIG_UPPER := $(shell echo $(KERNEL_CONFIG) | tr '[:lower:]' '[:upper:]')
XNU_DETAILS := $(shell $(SRCROOT)/tools/get_target_details.py $(SDKROOT) $(PRODUCT_CONFIG))
XNU_ARCH := $(word 1,$(XNU_DETAILS))
XNU_KERNEL_PLATFORM := $(word 2,$(XNU_DETAILS))
XNU_KERNEL_PLATFORM_UPPER := $(shell echo $(XNU_KERNEL_PLATFORM) | tr '[:lower:]' '[:upper:]')
XNU_FILE_NAME_PREFIX := $(word 3,$(XNU_DETAILS))

XNU_BUILD_DIR := $(XNU_OBJROOT)/$(KERNEL_CONFIG_UPPER)_$(XNU_ARCH)_$(XNU_KERNEL_PLATFORM_UPPER)
XNU_LIB_FILE_BASE := lib$(XNU_FILE_NAME_PREFIX).$(KERNEL_CONFIG).$(XNU_KERNEL_PLATFORM)
XNU_LIB_FILE := $(XNU_BUILD_DIR)/$(XNU_LIB_FILE_BASE).a

# avoid annoyances
OTHER_CFLAGS += -Wno-missing-prototypes -Wno-unused-parameter -Wno-missing-variable-declarations
# darwintest config
DT_CFLAGS = -UT_NAMESPACE_PREFIX -DT_NAMESPACE_PREFIX=xnu -DT_LEAKS_DISABLE=1 -DBUILD_NO_STD_HEADERS -I$(DT_SYMLINKS_DIR)
OTHER_CFLAGS += $(DT_CFLAGS)
OTHER_CXXFLAGS += $(DT_CFLAGS)
OTHER_LDFLAGS += -ldarwintest_utils
# interpose header
INTERPOSE_CFLAGS = -I$(SDKROOT)/usr/local/include/mach-o
OTHER_CFLAGS += $(INTERPOSE_CFLAGS)
# we build with clang but XNU contains C++ so we add this manually
OTHER_LDFLAGS += -lc++

_v = $(if $(filter YES,$(or $(VERBOSE),$(RC_XBS))),,@)

LD := "$(shell xcrun -sdk "$(SDKROOT)" -find ld)"
DYLD_INFO := "$(shell xcrun -sdk "$(SDKROOT)" -find dyld_info)"
LIBTOOL := "$(shell xcrun -sdk "$(SDKROOT)" -find libtool)"

XNU_MAKE_FLAGS = BUILD_LTO=0 PRODUCT_CONFIGS=$(PRODUCT_CONFIG) KERNEL_CONFIGS=$(KERNEL_CONFIG)
XNU_CFLAGS_EXTRA =
COVERAGE_FLAGS =

ifeq ($(BUILD_CODE_COVERAGE),1)
# make XNU build coverage
XNU_MAKE_FLAGS += BUILD_CODE_COVERAGE=1
# make mocks code not mock some coverage related llvm functions
COVERAGE_FLAGS += -D__BUILDING_FOR_COVERAGE__=1
endif # BUILD_CODE_COVERAGE

BUILD_SANITIZERS = 0
SANITIZERS_FLAGS =
ATTACH_SANITIZERS_SOURCES =
SANCOV_FLAG = -fsanitize-coverage=bb,no-prune,trace-pc-guard

ifeq ($(FIBERS_PREEMPTION),1)
# trace-loads,trace-stores depends on either trace-pc-guard or libfuzzer instrumentation
BUILD_SANCOV = 1
# compile with memory operations instrumentation
XNU_CFLAGS_EXTRA += -fsanitize-coverage=trace-loads,trace-stores
# make mocks code aware of sanitizers runtime being linked
SANITIZERS_FLAGS += -D__BUILDING_WITH_SANCOV_LOAD_STORES__=1
endif # FIBERS_PREEMPTION

ifeq ($(BUILD_ASAN),1)
BUILD_SANITIZERS = 1
# compile XNU with asan
# TODO: enable globals instrumentation and write a proper ignorelist for problematic global vars
XNU_CFLAGS_EXTRA += -fsanitize=address -mllvm -asan-globals=0
# make mocks code aware of sanitizers runtime being linked
SANITIZERS_FLAGS += -D__BUILDING_WITH_ASAN__=1
endif # BUILD_ASAN

ifeq ($(BUILD_UBSAN),1)
BUILD_SANITIZERS = 1
# compile XNU with ubsan
# TODO: add more ubsan support
XNU_CFLAGS_EXTRA += -fsanitize=signed-integer-overflow,shift,pointer-overflow,bounds,object-size,vla-bound,builtin
# make mocks code aware of sanitizers runtime being linked
SANITIZERS_FLAGS += -D__BUILDING_WITH_UBSAN__=1
endif # BUILD_UBSAN

ifeq ($(BUILD_TSAN),1)
BUILD_SANITIZERS = 1
# compile XNU with tsan
XNU_CFLAGS_EXTRA += -fsanitize=thread
# make mocks code aware of sanitizers runtime being linked
SANITIZERS_FLAGS += -D__BUILDING_WITH_TSAN__=1
endif # BUILD_TSAN

# SanitizerCoverage is used for preemption simulation
ifeq ($(BUILD_SANCOV),1)
BUILD_SANITIZERS = 1
# compile XNU with bb sancov
XNU_CFLAGS_EXTRA += $(SANCOV_FLAG) -fsanitize-coverage-ignorelist=$(SRCROOT)/tools/sanitizers-ignorelist
# make mocks code aware of sanitizers runtime being linked
SANITIZERS_FLAGS += -D__BUILDING_WITH_SANCOV__=1
endif # BUILD_SANCOV

ifeq ($(BUILD_SANITIZERS),1)
# include the ignorelist to disable instrumentation of some file/functions https://clang.llvm.org/docs/SanitizerSpecialCaseList.html
XNU_CFLAGS_EXTRA += -fsanitize-ignorelist=$(SRCROOT)/tools/sanitizers-ignorelist
# make mocks code aware of sanitizers runtime being linked
SANITIZERS_FLAGS += -D__BUILDING_WITH_SANITIZER__=1
ATTACH_SANITIZERS_SOURCES += mocks/san_attached.c
endif # BUILD_SANITIZERS

ifneq ($(strip $(XNU_CFLAGS_EXTRA)),)
# add CFLAGS_EXTRA to the XNU make flags if any, wrap between "" as XNU_CFLAGS_EXTRA contains spaces and replace inner " with \"
XNU_MAKE_FLAGS += CFLAGS_EXTRA="$(subst ",\",$(XNU_CFLAGS_EXTRA))"
endif

# sources for the mocks .dylib which overrides functions from XNU
MOCK_SOURCES = mocks/mock_alloc.c \
			   mocks/mock_misc.c \
			   mocks/mock_pmap.c \
			   mocks/mock_thread.c \
			   mocks/mock_unimpl.c \
			   mocks/mock_cpu.c \
			   mocks/unit_test_utils.c \
			   mocks/fibers/random.c \
			   mocks/fibers/fibers.c \
			   mocks/fibers/mutex.c \
			   mocks/fibers/condition.c \
			   mocks/fibers/rwlock.c \
			   mocks/fibers/checker.c

# sources for the mocks that are linked into the same .dylib as the XNU static lib
# fake_kinit.c needs to be first because it contains the initialization entry point
ATTACH_MOCK_SOURCES = mocks/fake_kinit.c \
                      mocks/mock_3rd_party.c  \
                      mocks/mock_mem.c \
                      mocks/mock_attached.c \
                      $(ATTACH_SANITIZERS_SOURCES)

# sources that are added to the compilation of every test target
TEST_SIDE_SOURCES = mocks/dt_proxy.c

# --------------------- individual tests customization ----------------------------

INCLUDED_TEST_SOURCE_DIRS += example_dir


# ---------------------------------------------------------------------------------
# we don't want to pass our arguments to the XNU makefile since that would confuse it, but we do want to pass any
# -j argument if it existed
unexport SRCROOT
unexport SDKROOT
unexport MAKEFLAGS
MKJOBS = $(filter --jobs=%,$(MAKEFLAGS))

.FORCE:
ifeq ($(SKIP_XNU),)
# The XNU make needs to run every time since if anything changed in XNU, we want to rebuild the tests
# This extra wait time can be skipped buy adding SKIP_XNU=1 to the make command line
$(XNU_LIB_FILE): .FORCE
	SDKROOT=$(SDKROOT) $(MAKE) -C $(XNU_ROOT) RC_ProjectName=xnu_libraries XNU_LibAllFiles=1 XNU_LibFlavour=UNITTEST SDKROOT=$(SDKROOT) $(XNU_MAKE_FLAGS) $(MKJOBS)
endif # SKIP_XNU

# Structure of unit-test linking:
# Mocking of XNU functions relies on the interposable functions mechanism which requires the interposed and interposable
# functions to be in different .dylibs
# - tester (executable)
#    - compiles tester.c
#    - statically linked to libdarwintest.a
#    - statically linked to libside.a
#    - dynamically linked to libmocks.dylib
#    - dynamically linked to libkernel.xxx.dylib
# - libside.a
#    - compiles mocks/dt_proxy.c  ($(TEST_SIDE_SOURCES)) which bridges PT_xxx calls from libmocks and libkernel
#      to libdarwintest. This is needed since the test executable is the only mach-o which links to libdarwintest
# - libmocks.dylib
#    - compiles mocks/mock_xxx.c - $(MOCK_SOURCES) files which contain T_MOCK() definitions to override functions from XNU
#      via the interposable functions mechanism.
#    - dynamically linked with libkernel.xxx.dylib
# - libkernel.xxx.dylib
#    - compiles mocks/xxx.c - $(ATTACH_MOCK_SOURCES) files which contain dependencies needed for linking the XNU static library
#    - statically linked to libkernel.xxx.prelinked.a
#    - makes all functions interposable so that internal calls are routed to mocks
# - libkernel.xxx.prelinked.a
#    - This is the same content as libkernel.xxx.a, passed through "ld -r" so that some symbols that are also
#      defined in libc can be unexported, so that anything outside XNU (i.e. mock and darwintest code) doesn't end up
#      calling them. e.g get_pid()
# - libkernel.xxx.a
#    - This is the static lib produced by XNU make which contains all of the XNU code.

# flags that .c files under MODULE (osfmk/bsd) are built with. MODULE should be defined per target
MODULE_CFLAGS = $(shell $(SRCROOT)/tools/quote_defines.py $(XNU_BUILD_DIR)/$(MODULE)/$(KERNEL_CONFIG_UPPER)/.CFLAGS) -I$(XNU_BUILD_DIR)/$(MODULE)/$(KERNEL_CONFIG_UPPER) $(MODULE_FLAG) $(SANITIZERS_FLAGS)
MODULE_CXXFLAGS = $(shell $(SRCROOT)/tools/quote_defines.py $(XNU_BUILD_DIR)/$(MODULE)/$(KERNEL_CONFIG_UPPER)/.CXXFLAGS) -I$(XNU_BUILD_DIR)/$(MODULE)/$(KERNEL_CONFIG_UPPER) $(MODULE_FLAG) $(SANITIZERS_FLAGS)
OSFMK_CFLAGS = $(shell $(SRCROOT)/tools/quote_defines.py $(XNU_BUILD_DIR)/osfmk/$(KERNEL_CONFIG_UPPER)/.CFLAGS) -I$(XNU_BUILD_DIR)/osfmk/$(KERNEL_CONFIG_UPPER)
MOCKS_CFLAGS = $(OSFMK_CFLAGS) $(INTERPOSE_CFLAGS) $(COVERAGE_FLAGS) $(SANITIZERS_FLAGS)

# We need to link the darwintest headers from an empty folder and include from there since we can't add their
# original folder as -I since that would allow XNU headers to include SDK headers.
DT_SYMLINKS_DIR := $(OBJROOT)/darwintest_headers
DT_ORIG_DIR := $(SDKROOT)/usr/local/include
# Target to check if the sdk changed. content of the file is the path to the last sdk used for building
$(OBJROOT)/sdk_updated: .FORCE
	$(_v)if [ ! -f $(OBJROOT)/sdk_updated ] || [ "$$(cat $(OBJROOT)/sdk_updated)" != "$(SDKROOT)" ]; then \
	    echo $(SDKROOT) > $(OBJROOT)/sdk_updated; \
	fi
# Do we need to update the darwintest headers symlinks?
$(DT_SYMLINKS_DIR): $(DT_ORIG_DIR)/darwintest.h $(OBJROOT)/sdk_updated
	$(_v)mkdir -p $(DT_SYMLINKS_DIR)
	$(_v)for file in $(DT_ORIG_DIR)/darwintest*; do ln -sfn "$${file}" $(DT_SYMLINKS_DIR); done

# The LD invocation below can't get the arch and platform from the prelinked.a file so we need this empty object
# to carry this information
ARCH_DEF_OBJ := $(OBJROOT)/arch_def.o
$(ARCH_DEF_OBJ): $(XNU_LIB_FILE)
	$(_v)$(CC) -c -x c /dev/null -o $@ $(CFLAGS)

SPTM_LIB := $(SDKROOT)/usr/local/lib/kernel/platform/libsptm_xnu.a
# this creates a dummy executable with libsptm_xnu.a so that the xbs dependency analysis know to build xnu_unittests after libsptm was installed
DUMMY_SPTM_EXE := $(OBJROOT)/ut_dummy_sptm
$(DUMMY_SPTM_EXE): $(SPTM_LIB) $(OBJROOT)/sdk_updated $(ARCH_DEF_OBJ)
	$(_v)echo "int start() { return 0; }" > $(OBJROOT)/dummy_sptm_start.c
	$(_v)$(CC) $(CFLAGS) $(OBJROOT)/dummy_sptm_start.c -Wl,-e,_start -Wl,-pie -nostdlib -Wl,-kernel -static -Wl,-force_load,$(SPTM_LIB) -o $@

XNU_LIB_PRELINKED := $(OBJROOT)/$(XNU_LIB_FILE_BASE).prelinked.a
$(XNU_LIB_PRELINKED): $(XNU_LIB_FILE) $(SRCROOT)/tools/xnu_lib.unexport $(ARCH_DEF_OBJ)
	$(_v)$(LD) -r $(ARCH_DEF_OBJ) -all_load $(XNU_LIB_FILE) -o $@ -unexported_symbols_list $(SRCROOT)/tools/xnu_lib.unexport

XNU_LIB_DYLIB := $(SYMROOT)/$(XNU_LIB_FILE_BASE).dylib
$(XNU_LIB_DYLIB): $(XNU_LIB_PRELINKED) $(ATTACH_MOCK_SOURCES) $(DT_SYMLINKS_DIR)
	$(_v)$(CC) $(MOCKS_CFLAGS) $(CFLAGS) -dynamiclib $(ATTACH_MOCK_SOURCES) -Wl,-all_load,$(XNU_LIB_PRELINKED) -lc++ -Wl,-undefined,dynamic_lookup -Wl,-interposable -install_name @rpath/$(XNU_LIB_FILE_BASE).dylib  -o $@

# Do we need to update the unimplemented functions mock?
$(OBJROOT)/func_unimpl.inc: $(XNU_LIB_DYLIB)
	$(_v)echo "// Generated from undefined imports from: $(XNU_LIB_DYLIB)" > $@
	$(_v)$(DYLD_INFO) -imports $(XNU_LIB_DYLIB) | grep "flat-namespace" | awk '{print "UNIMPLEMENTED(" substr($$2, 2) ")"}' >> $@
# The xnu dylib is linked with -undefined dynamic_lookup so that it's able to find undefined symbols at load time
# These are symbols that come from libsptm_xnu.a and libTightbeam.a. They still need to have an implementation for load
# to succeeds so this gets the list of undefined symbols and defines them in the mocks dylib
# use awk to discard the first underscore prefix of each symbol and wrap with UNIMPLEMENTED() macro expected in mock_unimpl.c

MOCKS_DYLIB := $(SYMROOT)/libmocks.dylib
$(MOCKS_DYLIB): $(MOCK_SOURCES) $(XNU_LIB_DYLIB) $(OBJROOT)/func_unimpl.inc
	$(_v)$(CC) $(MOCKS_CFLAGS) $(CFLAGS) -I$(OBJROOT) $(XNU_LIB_DYLIB) -dynamiclib -MJ $@.json $(MOCK_SOURCES) $(XNU_LIB_DYLIB) -install_name @rpath/libmocks.dylib -o $@
# -install_name makes the test executables which link to these .dylibs find them in the same folder rather than the
# folder the .dylibs happen to be built at. The path is relative to rpath and rpath is defined by the executable
# itself to be to root of test executables, see below

SIDE_LIB := $(OBJROOT)/libside.a
TEST_SIDE_OBJ := $(foreach source,$(TEST_SIDE_SOURCES),$(OBJROOT)/$(notdir $(basename $(source))).o)
$(TEST_SIDE_OBJ): $(TEST_SIDE_SOURCES) $(DT_SYMLINKS_DIR) $(XNU_LIB_DYLIB)
	$(_v)$(CC) $(MOCKS_CFLAGS) $(CFLAGS) $(DT_CFLAGS) -c $< -o $@
$(SIDE_LIB): $(TEST_SIDE_OBJ)
	$(_v)$(LIBTOOL) -static $< -o $@

# this creates a shell script for running all the unit-tests on-desk (build all using target 'install'
.PHONY: run_unittests
install: run_unittests
RUN_UT_SCRIPT := $(SYMROOT)/run_unittests.sh
RUN_UT_LIST := $(SYMROOT)/run_unittests.targets
run_unittests: $(RUN_UT_SCRIPT)
$(RUN_UT_SCRIPT): $(SRCROOT)/tools/make_run_unittests.py
	$(SRCROOT)/tools/make_run_unittests.py "$(TEST_TARGETS)" $@ $(RUN_UT_LIST)
	chmod a+x $@

# inform the dt makefile that these need to be installed along with the test executables
CUSTOM_TARGETS += $(XNU_LIB_DYLIB) $(MOCKS_DYLIB) $(DUMMY_SPTM_EXE) $(RUN_UT_SCRIPT)
install-$(XNU_LIB_DYLIB): $(XNU_LIB_DYLIB)
	mkdir -p $(INSTALLDIR)
	cp $< $(INSTALLDIR)/
install-$(MOCKS_DYLIB): $(MOCKS_DYLIB)
	mkdir -p $(INSTALLDIR)
	cp $< $(INSTALLDIR)/
install-$(DUMMY_SPTM_EXE): $(DUMMY_SPTM_EXE)
	echo "not copying $(DUMMY_SPTM_EXE)"
install-$(RUN_UT_SCRIPT): $(RUN_UT_SCRIPT)
	mkdir -p $(INSTALLDIR)
	cp $< $(INSTALLDIR)/
	cp $(RUN_UT_LIST) $(INSTALLDIR)/

OTHER_LDFLAGS += $(XNU_LIB_DYLIB) $(MOCKS_DYLIB) -Wl,-force_load,$(SIDE_LIB)


include $(DEVELOPER_DIR)/AppleInternal/Makefiles/darwintest/Makefile.targets

# Every unit-test target needs to define a target-specific variable named UT_MODULE so that MODULE_CFLAGS is defined
# This is parsed from the .c file that needs to have a line like: #define UT_MODULE osfmk
# The clang invocation may produce errors due to missing include folders but it still generates the #defines list.
define assign_module
$(1): MODULE ?= $(shell for f in $(1).c $(1).cpp; do \
    [ -f $$f ] && $(CC) -E -dM $$f 2>/dev/null | grep -m1 '^#define UT_MODULE ' | awk '{print $$3}' && break; \
done)
endef
$(foreach target, $(TEST_TARGETS), $(eval $(call assign_module, $(target), $(target))))
# if no UT_MODULE was found, this will trigger an error in std_safe.h
MODULE_FLAG = -DUT_MODULE=$(if $(strip $(MODULE)),$(MODULE),-1)

# All test targets depend on the XNU lib
$(TEST_TARGETS): $(XNU_LIB_DYLIB) $(MOCKS_DYLIB) $(SIDE_LIB)

# if the test executalbe is in a sub-folder, it's rpath needs to point back to the root of all
# test executables. This is done by appending as many ".." to @executable_path as there are levels of sub-directories
define make_rpath
@executable_path$(shell \
  dir=$$(dirname "$1"); \
  if [ "$$dir" != "." ]; then \
    echo "$$dir" | sed 's|[^/][^/]*|..|g' | sed 's|^|/|'; \
  fi)
endef
# this sets the CFLAGS for building the test to be the same as files in it's UT_MODULE
$(TEST_TARGETS): OTHER_CFLAGS += $(MODULE_CFLAGS) -MJ $(OBJROOT)/$(subst /,__,$@).json -rpath $(call make_rpath,$@)
# C++ files get both CFLAGS and CXXFLAGS
$(TEST_TARGETS): OTHER_CXXFLAGS += $(MODULE_CXXFLAGS) $(MODULE_CFLAGS) -MJ $(OBJROOT)/$(subst /,__,$@).json -rpath $(call make_rpath,$@)

# This is for creating a new version of $(XNU_ROOT)/compile_commands.json that includes
# the test and mock files. It's used by IDEs for understanding the code
.PHONY: cmds_json proj_xcode proj_vscode proj_clion
cmds_json:
	$(SRCROOT)/tools/merge_cmds_json.py $(XNU_ROOT) $(XNU_BUILD_DIR) $(OBJROOT)

proj_xcode: cmds_json
	$(SRCROOT)/tools/generate_ut_proj.py xcode
proj_vscode: cmds_json
	$(SRCROOT)/tools/generate_ut_proj.py vscode
proj_clion: cmds_json
	$(SRCROOT)/tools/generate_ut_proj.py clion

