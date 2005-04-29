##
# Makefile for CVS
##

# Project info
Project		      = cvs
Extra_Configure_Flags = --with-gssapi
UserType	      = Developer
ToolType	      = Commands
GnuAfterInstall	      = install-man-pages clobber-bogus-dirfile

# It's a GNU Source project
include $(MAKEFILEPATH)/CoreOS/ReleaseControl/GNUSource.make

MANPAGES = man/rcs2log.1 

install-man-pages:
	install -d $(DSTROOT)/usr/share/man/man1
	install -c -m 444 $(SRCROOT)/$(MANPAGES) $(DSTROOT)/usr/share/man/man1/

clobber-bogus-dirfile:
	rm -f $(DSTROOT)/usr/share/info/dir

OSV	= $(DSTROOT)/usr/local/OpenSourceVersions
OSL	= $(DSTROOT)/usr/local/OpenSourceLicenses

install-plist:
	$(MKDIR) $(OSV)
	$(INSTALL_FILE) $(SRCROOT)/$(Project).plist $(OSV)/$(Project).plist
	$(MKDIR) $(OSL)
	$(INSTALL_FILE) $(Sources)/COPYING $(OSL)/$(Project).txt

# Automatic Extract & Patch
AEP	       = YES
AEP_Project    = $(Project)
AEP_Version    = 1.11.18
AEP_ProjVers   = $(AEP_Project)-$(AEP_Version)
AEP_Filename   = $(AEP_ProjVers).tar.bz2
AEP_ExtractDir = $(AEP_ProjVers)
AEP_Patches    = error_msg.diff readonlyfs.diff

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
		cd $(SRCROOT)/$(Project) && patch -p0 < $(SRCROOT)/patches/$$patchfile; \
	done
endif
