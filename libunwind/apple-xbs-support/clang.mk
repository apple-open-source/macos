# apple-xbs-support/clang.mk
# Apple Internal B&I makefile for clang.

################################################################################
# Apple clang default B&I configuration.
################################################################################

Clang_Use_Assertions   := 0
Clang_Use_Optimized    := 1
# FIXME: remove, stop hardcoding it.
Clang_Version          := 11.0.0

ifeq ($(RC_ProjectName),clang) # Use LTO for clang but not clang_device
Clang_Enable_LTO := THIN
else
Clang_Enable_LTO := 0
endif

################################################################################
# Apple clang XBS targets.
################################################################################

# Default target.
all: help

help:
	@echo "usage: make [{VARIABLE=VALUE}*] <target>"
	@echo
	@echo "The Apple Clang makefile is primarily intended for use with XBS."
	@echo
	@echo "Supported B&I related targets are:"
	@echo "  installsrc    -- Copy source files from the current" \
	      "directory to the SRCROOT."
	@echo "  clean         -- Does nothing, just for XBS support."
	@echo "  installhdrs   -- Does nothing, just for XBS support."
	@echo "  install       -- Alias for install-clang."
	@echo "  install-clang -- Build the Apple Clang compiler."

# Default is to build Clang.
install: clang

# Install source uses the shared helper, but also fetches PGO data during
# submission to a B&I train.
installsrc-paths :=    \
    llvm               \
    clang-tools-extra  \
    compiler-rt        \
    libcxx             \
    clang
include apple-xbs-support/helpers/installsrc.mk

# FIXME: Fetch PGO data if we can.
installsrc: installsrc-helper

# The clean target is run after installing sources, but we do nothing because
# the expectation is that we will just avoid copying in cruft during the
# installsrc phase.
clean:

# We do not need to do anything for the install headers phase.
installhdrs:

################################################################################
# Apple clang build targets.
################################################################################

# The clang build target invokes CMake and ninja in the `build_clang` script.
BUILD_CLANG = $(SRCROOT)/clang/utils/buildit/build_clang

# FIXME (Alex): export the Git version.
clang: $(OBJROOT) $(SYMROOT) $(DSTROOT)
	cd $(OBJROOT) && \
		export LLVM_REPOSITORY=$(LLVM_REPOSITORY) && \
		$(BUILD_CLANG) $(Clang_Use_Assertions) $(Clang_Use_Optimized) $(Clang_Version) $(Clang_Enable_LTO)

clang-libs: $(OBJROOT) $(SYMROOT) $(DSTROOT)
	cd $(OBJROOT) && \
		export LLVM_REPOSITORY=$(LLVM_REPOSITORY) && \
		$(BUILD_CLANG) $(Clang_Use_Assertions) $(Clang_Use_Optimized) $(Clang_Version) $(Clang_Enable_LTO) build_libs

BUILD_COMPILER_RT = $(SRCROOT)/compiler-rt/utils/buildit/build_compiler_rt

compiler-rt: $(OBJROOT) $(SYMROOT) $(DSTROOT)
	cd $(OBJROOT) && \
		$(BUILD_COMPILER_RT) $(Clang_Use_Assertions) $(Clang_Use_Optimized) $(Clang_Version)

COMPILER_RT_DYLIBS = $(filter-out %sim_dynamic.dylib,$(wildcard $(RC_EMBEDDEDPROJECT_DIR)/clang_compiler_rt/$(DT_TOOLCHAIN_DIR)/usr/lib/clang/$(Clang_Version)/lib/darwin/*_dynamic.dylib))
ifeq ($(findstring OSX,$(DT_TOOLCHAIN_DIR)),)
    OS_DYLIBS = $(filter-out %osx_dynamic.dylib,$(COMPILER_RT_DYLIBS))
else
    OS_DYLIBS = $(COMPILER_RT_DYLIBS)
endif

compiler-rt-os:
	mkdir -p $(DSTROOT)/usr/local/lib/sanitizers/
	ditto $(OS_DYLIBS) $(DSTROOT)/usr/local/lib/sanitizers/
