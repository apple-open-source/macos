##
# Makefile for man
##

# Project info
Project         = man
GnuAfterInstall = strip-man link-manpath install-plist fix-perms

install:: shadow_source

include $(MAKEFILEPATH)/CoreOS/ReleaseControl/GNUSource.make

# Not quite like other GNU projects...
Configure_Flags = -d -prefix="$(Install_Prefix)" \
                  -confdir="$(ETCDIR)" \
                  -compatibility_mode_for_colored_groff
Extra_Make_Flags = LIBS=-lxcselect
Install_Flags   = DESTDIR="$(DSTROOT)"
Install_Target  = install

# Automatic Extract & Patch
AEP            = YES
AEP_Project    = $(Project)
AEP_Version    = 1.6c
AEP_ProjVers   = $(AEP_Project)-$(AEP_Version)
AEP_Filename   = $(AEP_ProjVers).tar.gz
AEP_ExtractDir = $(AEP_ProjVers)
AEP_Patches    = Makefile.in.diff \
                 configure.diff \
                 man__Makefile.in.diff \
                 src__Makefile.in.diff \
                 src__man-getopt.c.diff \
                 src__man.c.diff \
                 src__man.conf.in.diff \
                 src__manpath.c.diff \
                 src__util.c.diff \
                 PR3845474.diff \
                 PR3857969.diff \
                 PR3939085.diff \
                 PR4006198.diff \
                 PR4062483.diff \
                 PR4076593.diff \
                 PR4121764.diff \
                 PR4302566.diff \
                 PR4670363.diff \
                 PR5291011.diff \
                 PR5024303.diff \
                 PR11291804-xcode.diff \
                 PR13528825.diff

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
		(cd $(SRCROOT)/$(Project) && patch -p0 -F0 < $(SRCROOT)/patches/$$patchfile) || exit 1; \
	done
endif

strip-man:
	$(STRIP) -x $(DSTROOT)/usr/bin/man

link-manpath:
	$(LN) -s man $(DSTROOT)/usr/bin/manpath
	$(LN) -s man.1 $(DSTROOT)/usr/share/man/man1/manpath.1

fix-perms:
	@for prog in apropos man whatis; do \
		$(CHMOD) $(Install_Program_Mode) $(DSTROOT)/usr/bin/$${prog}; \
		$(CHMOD) $(Install_File_Mode) $(DSTROOT)/usr/share/man/man1/$${prog}.1; \
	done

OSV     = $(DSTROOT)/usr/local/OpenSourceVersions
OSL     = $(DSTROOT)/usr/local/OpenSourceLicenses

install-plist:
	$(MKDIR) $(OSV)
	$(INSTALL_FILE) $(SRCROOT)/$(Project).plist $(OSV)/$(Project).plist
	$(MKDIR) $(OSL)
	$(INSTALL_FILE) $(Sources)/COPYING $(OSL)/$(Project).txt
