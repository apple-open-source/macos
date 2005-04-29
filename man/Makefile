##
# Makefile for man
##

# Project info
Project         = man
GnuAfterInstall = strip-man link-manpath install-plist

install:: shadow_source

include $(MAKEFILEPATH)/CoreOS/ReleaseControl/GNUSource.make

# Not quite like other GNU projects...
Configure_Flags = -d -prefix="$(Install_Prefix)" \
                  -compatibility_mode_for_colored_groff
Install_Flags   = DESTDIR="$(DSTROOT)"
Install_Target  = install

# Automatic Extract & Patch
AEP            = YES
AEP_Project    = $(Project)
AEP_Version    = 1.5o1
AEP_ProjVers   = $(AEP_Project)-$(AEP_Version)
AEP_Filename   = $(AEP_ProjVers).tar.bz2
AEP_ExtractDir = $(AEP_ProjVers)
AEP_Patches    = Makefile.in.diff \
                 configure.diff \
                 man2html__Makefile.in.diff \
                 src__Makefile.in.diff \
                 src__man-getopt.c.diff \
                 src__man.c.diff \
                 src__man.conf.in.diff \
                 src__manpath.c.diff \
                 src__util.c.diff \
                 PR3857969.diff

ifeq ($(suffix $(AEP_Filename)),.bz2)
AEP_ExtractOption = j
else
AEP_ExtractOption = z
endif

# Extract the source.
install_source::
ifeq ($(AEP),YES)
	$(TAR) -C $(SRCROOT) -$(AEP_ExtractOption)xf $(SRCROOT)/$(AEP_Filename)
	$(RMDIR) $(SRCROOT)/$(Project)
	$(MV) $(SRCROOT)/$(AEP_ExtractDir) $(SRCROOT)/$(Project)
	for patchfile in $(AEP_Patches); do \
		cd $(SRCROOT)/$(Project) && patch -p0 < $(SRCROOT)/patches/$$patchfile; \
	done
endif

strip-man:
	$(STRIP) -x $(DSTROOT)/usr/bin/man

link-manpath:
	$(LN) -s man $(DSTROOT)/usr/bin/manpath
	$(LN) -s man.1 $(DSTROOT)/usr/share/man/man1/manpath.1

OSV     = $(DSTROOT)/usr/local/OpenSourceVersions
OSL     = $(DSTROOT)/usr/local/OpenSourceLicenses

install-plist:
	$(MKDIR) $(OSV)
	$(INSTALL_FILE) $(SRCROOT)/$(Project).plist $(OSV)/$(Project).plist
	$(MKDIR) $(OSL)
	$(INSTALL_FILE) $(Sources)/COPYING $(OSL)/$(Project).txt
