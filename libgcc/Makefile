##
# libstdcxx Makefile
##

# Project info
Project               = libgcc
UserType              = Developer
ToolType              = Libraries
# This project builds a compiler, and then the library with the compiler;
# the host of the compiler had better be this system!
DARWIN_VERS=9
Extra_Configure_Flags = --disable-bootstrap --enable-languages=c \
	--build $(BUILD)$(DARWIN_VERS) \
	--host $(BUILD)$(DARWIN_VERS) \
	--target `echo $$arch | $(TRANSLATE_ARCH)`-apple-darwin$(DARWIN_VERS) \
	--with-slibdir="$(Install_Prefix)/lib"
GnuAfterInstall       = post-install install-plist
Environment	     += AS_FOR_TARGET="$(AS) -arch $$arch" \
			LD_FOR_TARGET="$(LD) -arch $$arch" \
			NM_FOR_TARGET="nm" \
			AR_FOR_TARGET="$(AR)" \
			RANLIB_FOR_TARGET="ranlib" \
			STRIP_FOR_TARGET="$(STRIP)" \
			LIPO_FOR_TARGET="$(LIPO)"

# It's a GNU Source project
include ./GNUSource.make

# Override what GNUSource.make puts in Environment, it's not quite
# right for the unusual way we're defining the 'host'.
Environment	     += CC="$(CC)" CXX="$(CXX)" AS="$(AS)" LD="$(LD)" NM=nm

# Speed the build, a lot.
CC_Optimize = -O0
Environment += CFLAGS_FOR_TARGET="-O2 -gdwarf-2"
Environment += LIBGCC2_DEBUG_CFLAGS="-gdwarf-2 -mmacosx-version-min=$(MACOSX_DEPLOYMENT_TARGET)"

# Automatic Extract & Patch
AEP            = YES
AEP_Project    = gcc-core
AEP_Version    = 4.2.0
AEP_ProjVers   = $(AEP_Project)-$(AEP_Version)
AEP_Filename   = $(AEP_ProjVers).tar.bz2
AEP_ExtractDir = gcc-$(AEP_Version)
AEP_Patches    = gcc-darwin-no64fallback.patch gcc-5054233.patch gcc-unwind-cfacontext.patch

ifeq ($(suffix $(AEP_Filename)),.bz2)
AEP_ExtractOption = j
else
AEP_ExtractOption = z
endif

Install_Target = install-gcc

# Extract the source.
install_source::
ifeq ($(AEP),YES)
	$(TAR) -C $(SRCROOT) -$(AEP_ExtractOption)xf $(SRCROOT)/$(AEP_Filename)
	$(RMDIR) $(SRCROOT)/$(Project)
	$(MV) $(SRCROOT)/$(AEP_ExtractDir) $(SRCROOT)/$(Project)
	for deldir in libgomp libssp libmudflap ; do \
		$(RMDIR) $(SRCROOT)/$(Project)/$$deldir ; \
	done
	for patchfile in $(AEP_Patches); do \
		cd $(SRCROOT)/$(Project) && \
		patch -p0 < $(SRCROOT)/patches/$$patchfile || exit 1 ;  \
	done
endif

# Remove the parts of the destroot we don't need
post-install:
	$(RM) -r $(DSTROOT)/usr/{bin,include,libexec,share,lib/gcc}
	dsymutil $(DSTROOT)/usr/lib/libgcc_s.1.dylib
	mv $(DSTROOT)/usr/lib/libgcc_s.1.dylib.dSYM $(SYMROOT)/
	strip -x $(DSTROOT)/usr/lib/libgcc_s.1.dylib
	ln -s libgcc_s.1.dylib  $(DSTROOT)/usr/lib/libgcc_s.1.0.dylib

OSV = $(DSTROOT)/usr/local/OpenSourceVersions
OSL = $(DSTROOT)/usr/local/OpenSourceLicenses

install-plist:
	$(MKDIR) $(OSV)
	$(INSTALL_FILE) $(SRCROOT)/$(Project).plist \
	   $(OSV)/$(RC_ProjectName).plist
	if [ "x$(SDKPFXs)" == x ] ; then \
	  $(MKDIR) $(OSL) && \
	  $(INSTALL_FILE) $(Sources)/COPYING $(OSL)/$(Project).txt || exit 1 ;\
	fi
