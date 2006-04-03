##
# libstdcxx Makefile
##

# The configure script for libstdc++ wants to know which platform
# it's building for, mostly for dyld behaviour
ifeq ($(RC_RELEASE),Jaguar)
DARWIN_VERS=6
else
ifeq ($(RC_RELEASE),Panther)
DARWIN_VERS=7
else
ifeq ($(RC_RELEASE),SUPanWheat)
DARWIN_VERS=7
else
DARWIN_VERS=8
endif
endif
endif


# Project info
Project               = libstdcxx
UserType              = Developer
ToolType              = Libraries
Extra_Configure_Flags = --disable-multilib \
	--build $(BUILD)$(DARWIN_VERS) \
	--host `echo $$arch | $(TRANSLATE_ARCH)`-apple-darwin$(DARWIN_VERS) \
	--target `echo $$arch | $(TRANSLATE_ARCH)`-apple-darwin$(DARWIN_VERS)
GnuAfterInstall       = post-install install-plist
Environment	     += CXX_FOR_TARGET="$(CXX) -arch $$arch" \
			RAW_CXX_FOR_TARGET="$(CC) -arch $$arch -shared-libgcc \
				-nostdinc++" \
			CC_FOR_TARGET="$(CC) -arch $$arch" \
			GCC_FOR_TARGET="$(CC) -arch $$arch" \
			CONFIGURED_AS_FOR_TARGET="$(AS) -arch $$arch" \
			CONFIGURED_LD_FOR_TARGET="$(LD) -arch $$arch" \
			CONFIGURED_NM_FOR_TARGET="nm -arch $$arch" \
			CONFIGURED_AR_FOR_TARGET="$(AR)" \
			CONFIGURED_RANLIB_FOR_TARGET="ranlib"


# It's a GNU Source project
include ./GNUSource.make

# Automatic Extract & Patch
AEP            = YES
AEP_Project    = gcc
AEP_Version    = 4.0.0
AEP_ProjVers   = $(AEP_Project)-$(AEP_Version)
AEP_Filename   = $(AEP_ProjVers).tar.bz2
AEP_ExtractDir = $(AEP_ProjVers)
AEP_Patches    = tilde-in-pathnames.patch emergency-buffer-reduction.patch \
		 keymgr.patch testing-installed.patch align-natural-abi.patch \
		 export-control.patch cross-configury.patch eprintf.patch \
		 testsuite-4.0.1.patch \
		 libtool-jaguar.patch jaguar-semun.patch jaguar-abilimits.patch

ifeq ($(suffix $(AEP_Filename)),.bz2)
AEP_ExtractOption = j
else
AEP_ExtractOption = z
endif

Install_Target = install

ifeq ($(RC_RELEASE),Jaguar)
SYSROOT=-isysroot /Developer/SDKs/MacOSX10.2.8.internal.sdk
CC		     := /usr/bin/gcc $(SYSROOT)
CXX		     := /usr/bin/gcc $(SYSROOT)
Extra_Cxx_Flags	     += -DLIBSTDCXX_FOR_JAGUAR
endif
ifeq ($(RC_RELEASE),Panther)
SYSROOT=-isysroot /Developer/SDKs/MacOSX10.3.internal.sdk
CC		     := /usr/bin/gcc $(SYSROOT)
CXX		     := /usr/bin/gcc $(SYSROOT)
endif
ifeq ($(RC_RELEASE),SUPanWheat)
SYSROOT=-isysroot /Developer/SDKs/MacOSX10.3.9.internal.sdk
CC		     := /usr/bin/gcc $(SYSROOT)
CXX		     := /usr/bin/gcc $(SYSROOT)
endif

# Extract the source.
install_source::
ifeq ($(AEP),YES)
	$(TAR) -C $(SRCROOT) -$(AEP_ExtractOption)xf $(SRCROOT)/$(AEP_Filename)
	$(RMDIR) $(SRCROOT)/$(Project)
	$(MV) $(SRCROOT)/$(AEP_ExtractDir) $(SRCROOT)/$(Project)
	for deldir in libada libcpp libgfortran libjava libobjc zlib \
		      boehm-gc fastjar fixincludes intl ; do \
		$(RMDIR) $(SRCROOT)/$(Project)/$$deldir ; \
	done
	for delconfigdir in gcc ; do \
		$(RM) $(SRCROOT)/$(Project)/$$delconfigdir/configure ; \
	done
	for patchfile in $(AEP_Patches); do \
		cd $(SRCROOT)/$(Project) && \
		patch -p0 < $(SRCROOT)/patches/$$patchfile || exit 1 ;  \
	done
endif

# Rearrange the final destroot to be just the way we want it.
post-install:
	if [ -d $(DSTROOT)/usr/lib/ppc64 ] ; then \
	  install_name_tool -id /usr/lib/libstdc++.6.dylib \
	    $(DSTROOT)/usr/lib/ppc64/libstdc++.6.*.dylib && \
	  for f in `cd $(DSTROOT)/usr/lib/ppc64 && echo *.{dylib,a}` ; do \
	    if [ ! -L $(DSTROOT)/usr/lib/ppc64/$$f ] ; then \
		lipo -create -output $(DSTROOT)/usr/lib/$${f}~ \
		  $(DSTROOT)/usr/lib/$${f} $(DSTROOT)/usr/lib/ppc64/$${f} && \
		mv $(DSTROOT)/usr/lib/$${f}~ $(DSTROOT)/usr/lib/$${f} || \
		exit 1 ; \
	    fi ; \
	  done && \
	  $(RM) -r $(DSTROOT)/usr/lib/ppc64 ; \
	fi
	$(RM) $(DSTROOT)/usr/lib/*.la
	$(RM) $(DSTROOT)/usr/lib/libiberty.a
	$(RM) $(DSTROOT)/usr/lib/libstdc++.dylib
	cp -Rp $(DSTROOT)/usr/lib/*.{a,dylib} $(SYMROOT)/
	strip -x $(DSTROOT)/usr/lib/*.dylib
	strip -X -S $(DSTROOT)/usr/lib/*.a
	$(MKDIR) $(DSTROOT)/usr/local/lib
	mv $(DSTROOT)/usr/lib/libstdc++.a \
		$(DSTROOT)/usr/local/lib/libstdc++-static.a

OSV = $(DSTROOT)/usr/local/OpenSourceVersions
OSL = $(DSTROOT)/usr/local/OpenSourceLicenses

install-plist:
	$(MKDIR) $(OSV)
	$(INSTALL_FILE) $(SRCROOT)/$(Project).plist $(OSV)/$(Project).plist
	$(MKDIR) $(OSL)
	$(INSTALL_FILE) $(Sources)/COPYING $(OSL)/$(Project).txt
