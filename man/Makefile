##
# Makefile for man
##

# Project info
Project         = man
GnuAfterInstall = strip-man link-manpath install-plist fix-perms install-ff

install:: shadow_source

include $(MAKEFILEPATH)/CoreOS/ReleaseControl/GNUSource.make

# Not quite like other GNU projects...
Configure_Flags = -d -prefix="$(Install_Prefix)" \
                  -confdir="$(ETCDIR)" \
                  -compatibility_mode_for_colored_groff
Extra_Make_Flags = CFLAGS="$(RC_CFLAGS)" LDFLAGS="$(RC_CFLAGS)" LIBS=-lxcselect
Install_Flags   = DESTDIR="$(DSTROOT)"
Install_Target  = install

# Extract the source.
install_source::

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

FF      = $(DSTROOT)/System/Library/FeatureFlags/Domain/

install-ff:
	$(MKDIR) $(FF)
	$(INSTALL_FILE) $(SRCROOT)/ff-$(Project).plist $(FF)/$(Project).plist
