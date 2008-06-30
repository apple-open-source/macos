##
# cxxfilt Makefile
##

# The --target is because bfd doesn't actually work on powerpc-darwin.
# libiberty doesn't care about the target, but c++filt does, in one
# very small way: it needs to know whether or not the target prepends an
# underscore.  It turns out that this target does prepend an underscore.
FAKE_TARGET=mn10300-elf

# Project info
Project               = cxxfilt
UserType              = Developer
ToolType              = Commands
Install_Prefix	      = /usr/local
Extra_Configure_Flags = --target=$(FAKE_TARGET) \
			--without-target-subdir --enable-install-libiberty \
			--disable-nls $(HOST_TARGET_FLAGS)
Extra_CC_Flags        = -mdynamic-no-pic
GnuAfterInstall       = post-install install-plist

# It's a GNU Source project
include ./GNUSource.make

# Automatic Extract & Patch
AEP            = YES
AEP_Project    = binutils
AEP_Version    = 070207
AEP_ProjVers   = $(AEP_Project)-$(AEP_Version)
AEP_Filename   = $(AEP_ProjVers).tar.bz2
AEP_ExtractDir = $(AEP_ProjVers)
AEP_Patches    = libiberty-demangle-5046344.patch libiberty-printf.patch

ifeq ($(suffix $(AEP_Filename)),.bz2)
AEP_ExtractOption = j
else
AEP_ExtractOption = z
endif

Install_Target = install

# Extract the source.
install_source::
ifeq ($(AEP),YES)
	$(TAR) -C $(SRCROOT) -$(AEP_ExtractOption)xf $(SRCROOT)/$(AEP_Filename)
	$(RMDIR) $(SRCROOT)/$(Project)
	$(MV) $(SRCROOT)/$(AEP_ExtractDir) $(SRCROOT)/$(Project)
	$(RM) -r $(SRCROOT)/$(Project)/{gas,ld,gprof}
	for patchfile in $(AEP_Patches); do \
		cd $(SRCROOT)/$(Project) && patch -p0 < $(SRCROOT)/patches/$$patchfile; \
	done
endif

post-install:
	$(MKDIR) $(DSTROOT)/usr/bin
	$(STRIP) $(DSTROOT)/usr/local/bin/$(FAKE_TARGET)-c++filt \
		-o $(DSTROOT)/usr/bin/c++filt
	$(MV) $(DSTROOT)/usr/share/man/man1/$(FAKE_TARGET)-c++filt.1 \
		$(DSTROOT)/usr/share/man/man1/c++filt.1
	$(RM) -r $(DSTROOT)/usr/share/info $(DSTROOT)/usr/local/bin
	$(RM) -r $(DSTROOT)/usr/local/$(FAKE_TARGET)
	$(RM) $(DSTROOT)/usr/share/man/man1/$(FAKE_TARGET)-*

OSV = $(DSTROOT)/usr/local/OpenSourceVersions
OSL = $(DSTROOT)/usr/local/OpenSourceLicenses

install-plist:
	$(MKDIR) $(OSV)
	$(INSTALL_FILE) $(SRCROOT)/$(Project).plist $(OSV)/$(Project).plist
	$(MKDIR) $(OSL)
	$(INSTALL_FILE) $(Sources)/COPYING $(OSL)/$(Project).txt
