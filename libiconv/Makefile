##
# Makefile for libiconv
##

# Project info
Project               = libiconv
UserType              = Administrator
ToolType              = Libraries
Configure	      = $(SRCROOT)/configure-no-trace $(Sources)/configure
Extra_Configure_Flags = --disable-static --enable-extra-encodings
GnuAfterInstall       = strip install-plist

NativeArch		= $(shell arch)
NativeCompile		= $(filter $(NativeArch),$(RC_ARCHS))
ifeq "$(MAKECMDGOALS)" "install"
ifeq "$(NativeCompile)" ""
# if we aren't building the native architecture, set up configure to
# cross-compile, so it doesn't try to run the executable it builds
CONFIGGUESS		:= $(SRCROOT)/$(Project)/build-aux/config.guess
HostName		:= $(shell $(CONFIGGUESS))
TargetName		:= $(shell echo $(HostName) | sed 's/^[^-]*-/cross-/')
Extra_Configure_Flags	+= --host $(HostName) --target $(TargetName)
endif
ifneq "$(SDKROOT)" ""
Extra_CC_Flags		+= -isysroot "$(SDKROOT)"
Extra_LD_Flags		+= -Wl,-syslibroot,"$(SDKROOT)"
endif
endif

# It's a GNU Source project
include $(MAKEFILEPATH)/CoreOS/ReleaseControl/GNUSource.make

# Automatic Extract & Patch
AEP_Project		= $(Project)
AEP_Version		= 1.11
AEP_ProjVers		= $(AEP_Project)-$(AEP_Version)
AEP_Filename		= $(AEP_ProjVers).tar.gz
AEP_ExtractDir		= $(AEP_ProjVers)
AEP_ExtractOption	= z

AEP_Patches		= configure.patch \
			  macroman.patch \
			  macutf8.patch \
			  manpage.patch \
			  options.patch \
			  unix03.patch \
			  iconv-Makefile.patch

# Extract the source.
install_source::
	$(TAR) -C $(SRCROOT) -$(AEP_ExtractOption)xf $(SRCROOT)/$(AEP_Filename)
	$(RMDIR) $(SRCROOT)/$(Project)
	$(MV) $(SRCROOT)/$(AEP_ExtractDir) $(SRCROOT)/$(Project)
	for patchfile in $(AEP_Patches); do \
		(cd $(SRCROOT)/$(Project) && patch -p1 < $(SRCROOT)/patches/$$patchfile) || exit 1; \
	done


Install_Target = install

##---------------------------------------------------------------------
# Patch config.h and lib/config.h just after running configure
##---------------------------------------------------------------------
ConfigStamp2 = $(ConfigStamp)2

configure:: $(ConfigStamp2)

$(ConfigStamp2): $(ConfigStamp)
	ed - $(OBJROOT)/config.h < $(SRCROOT)/config.h.ed
	ed - $(OBJROOT)/lib/config.h < $(SRCROOT)/config.h.ed
	touch $(ConfigStamp2)

strip:
	strip -x $(DSTROOT)/usr/lib/libiconv.2.4.0.dylib
	strip -x $(DSTROOT)/usr/lib/libcharset.1.0.0.dylib
	strip -x $(DSTROOT)/usr/bin/iconv
	rm -f $(DSTROOT)/usr/lib/libiconv.2.dylib
	mv $(DSTROOT)/usr/lib/libiconv.2.4.0.dylib $(DSTROOT)/usr/lib/libiconv.2.dylib
	ln -s libiconv.2.dylib $(DSTROOT)/usr/lib/libiconv.2.4.0.dylib
	rm -f $(DSTROOT)/usr/lib/libcharset.1.dylib
	mv $(DSTROOT)/usr/lib/libcharset.1.0.0.dylib $(DSTROOT)/usr/lib/libcharset.1.dylib
	ln -s libcharset.1.dylib $(DSTROOT)/usr/lib/libcharset.1.0.0.dylib
	rm -rf $(DSTROOT)/Library/Documentation/$(ToolType)/$(Project)
	-rmdir $(DSTROOT)/Library/Documentation/$(ToolType)
	-rmdir $(DSTROOT)/Library/Documentation
	-rmdir $(DSTROOT)/Library

OSV = $(DSTROOT)/usr/local/OpenSourceVersions
OSL = $(DSTROOT)/usr/local/OpenSourceLicenses

install-plist:
	$(MKDIR) $(OSV)
	$(INSTALL_FILE) $(SRCROOT)/$(Project).plist $(OSV)/$(Project).plist
	$(MKDIR) $(OSL)
	$(INSTALL_FILE) $(Sources)/COPYING.LIB $(OSL)/$(Project).txt

