ifndef RC_ProjectName
RC_ProjectName = subversion
endif

ifndef DEVELOPER_DIR
DEVELOPER_DIR := $(shell xcode-select -p)
endif

APR_TOOLCHAIN_DIR=$(dir $(shell xcrun --toolchain $(TOOLCHAINS) -f apr-1-config))/..

Project               = subversion
ProjectVersion        = 1.10.6

#-------------------------------------------------------------------------
# build/get-py-info.py appends "-framework Python" to its --link and --libs
# output.  This is wrong, and especially bad when building multi-version
# (e.g., when python 2.6 is the default, and we are building for 2.5,
# "-framework Python" will bring in the 2.6 python dylib, causing crashes).
# So the build_get-py-info.py.diff patch removes the appending of
# "-framework Python".
#-------------------------------------------------------------------------
Patches        = build_get-py-info.py.diff \
                 configure.diff \
                 Makefile.in.diff \
                 spawn.diff \
                 build-outputs.mk.perl.diff \
                 apr14.diff

Extra_Make_Flags = -j $(shell sysctl -n hw.activecpu)
Extra_Cxx_Flags = -stdlib=libc++
Extra_LD_Flags =

ifndef SDKROOT
SDKROOT := $(shell xcrun --sdk macosx.internal --show-sdk-path)
endif

export CODESIGN_ALLOCATE :=  $(shell xcrun -find -sdk $(SDKROOT) codesign_allocate)
CODESIGN = /usr/bin/codesign --force --sign - --timestamp=none

include Makefile.$(RC_ProjectName)

# Extract the source.
install_source::
	$(RMDIR) $(SRCROOT)/$(Project) $(SRCROOT)/$(Project)-$(ProjectVersion)
	$(TAR) -C $(SRCROOT) -xof $(SRCROOT)/$(Project)-$(ProjectVersion).tar.bz2
	$(MV) $(SRCROOT)/$(Project)-$(ProjectVersion) $(SRCROOT)/$(Project)
	@set -x && \
	cd $(SRCROOT)/$(Project) && \
	for patchfile in $(Patches); do \
		patch -p0 -F0 -i $(SRCROOT)/files/$$patchfile || exit 1; \
	done
	ed - $(SRCROOT)/$(Project)/build-outputs.mk < $(SRCROOT)/files/fix-build-outputs.mk.ed

OSV = $(DSTROOT)/usr/local/OpenSourceVersions
OSL = $(DSTROOT)/usr/local/OpenSourceLicenses

install-plist:
	$(MKDIR) $(OSV)
	$(INSTALL_FILE) $(SRCROOT)/$(Project).plist $(OSV)/$(RC_ProjectName).plist
	$(MKDIR) $(OSL)
	$(INSTALL_FILE) $(Sources)/LICENSE $(OSL)/$(RC_ProjectName).txt

# testbots!
testbots:
	$(MAKE) -C $(OBJROOT) check
