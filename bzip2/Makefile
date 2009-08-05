#
# xbs-compatible Makefile for bzip2.
#

Project             = bzip2
GnuNoConfigure      = YES
Extra_CC_Flags      = -no-cpp-precomp -D_FILE_OFFSET_BITS=64
Extra_Install_Flags = PREFIX=$(RC_Install_Prefix)
GnuAfterInstall     = strip-binaries fix-manpages install-plist

BZIP2_VERSION = 1.0.5
Extra_Environment = BZIP2_VERSION=$(BZIP2_VERSION)

install:: shadow_source

include $(MAKEFILEPATH)/CoreOS/ReleaseControl/GNUSource.make

Install_Target      = install

strip-binaries:
	for binary in bunzip2 bzcat bzip2recover bzip2; do \
		file=$(DSTROOT)/usr/bin/$${binary}; \
		echo $(CP) $${file} $(SYMROOT); \
		$(CP) $${file} $(SYMROOT); \
		echo $(STRIP) -x $${file}; \
		$(STRIP) -x $${file}; \
		for arch in ppc64 x86_64; do \
			echo lipo -remove $${arch} -output $${file} $${file}; \
			lipo -remove $${arch} -output $${file} $${file} || true; \
		done \
	done
	$(CP) $(DSTROOT)/usr/local/lib/libbz2.a $(SYMROOT)
	$(STRIP) -x $(DSTROOT)/usr/local/lib/libbz2.a
	$(CP) $(DSTROOT)/usr/lib/libbz2.$(BZIP2_VERSION).dylib $(SYMROOT)
	$(STRIP) -x $(DSTROOT)/usr/lib/libbz2.$(BZIP2_VERSION).dylib

fix-manpages:
	$(MKDIR) $(DSTROOT)/usr/share
	$(MV) $(DSTROOT)/usr/man $(DSTROOT)/usr/share
	$(LN) $(DSTROOT)/usr/share/man/man1/bzip2.1 $(DSTROOT)/usr/share/man/man1/bunzip2.1
	$(LN) $(DSTROOT)/usr/share/man/man1/bzip2.1 $(DSTROOT)/usr/share/man/man1/bzcat.1
	$(LN) $(DSTROOT)/usr/share/man/man1/bzip2.1 $(DSTROOT)/usr/share/man/man1/bzip2recover.1

OSV	= $(DSTROOT)/usr/local/OpenSourceVersions
OSL	= $(DSTROOT)/usr/local/OpenSourceLicenses

install-plist:
	$(MKDIR) $(OSV)
	$(INSTALL_FILE) $(SRCROOT)/$(Project).plist $(OSV)/$(Project).plist
	$(MKDIR) $(OSL)
	$(INSTALL_FILE) $(Sources)/LICENSE $(OSL)/$(Project).txt

# Automatic Extract & Patch
AEP            = YES
AEP_Project    = $(Project)
AEP_Version    = $(BZIP2_VERSION)
AEP_ProjVers   = $(AEP_Project)-$(AEP_Version)
AEP_Filename   = $(AEP_ProjVers).tar.gz
AEP_ExtractDir = $(AEP_ProjVers)
AEP_Patches    = EA.diff dylib.diff nopic.diff bzgrep.diff

ifeq ($(suffix $(AEP_Filename)),.bz2)
AEP_ExtractOption = j
else
AEP_ExtractOption = z
endif

# Extract the source.
install_source::
ifeq ($(AEP),YES)
	$(TAR) -C $(SRCROOT) -$(AEP_ExtractOption)xf $(SRCROOT)/$(AEP_Filename)
	$(RMDIR) $(SRCROOT)/$(AEP_Project)
	$(MV) $(SRCROOT)/$(AEP_ExtractDir) $(SRCROOT)/$(AEP_Project)
	for patchfile in $(AEP_Patches); do \
		cd $(SRCROOT)/$(Project) && patch -p0 < $(SRCROOT)/patches/$$patchfile || exit 1; \
	done
endif
