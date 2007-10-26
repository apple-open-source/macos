##
# Makefile for zsh
##

# Project info
Project		      = zsh
UserType	      = Administration
ToolType	      = Commands
Extra_CC_Flags	      = -no-cpp-precomp
Extra_Configure_Flags = --bindir="$(BINDIR)" --with-tcsetpgrp --enable-multibyte
Extra_Install_Flags   = bindir="$(DSTROOT)$(BINDIR)"
GnuAfterInstall	      = post-install install-plist

# It's a GNU Source project
include $(MAKEFILEPATH)/CoreOS/ReleaseControl/GNUSource.make

post-install:
	ln $(DSTROOT)/$(MANDIR)/man1/zsh.1 $(DSTROOT)/$(MANDIR)/man1/zsh-4.3.4.1
	rm -f $(DSTROOT)/$(MANDIR)/man1/zshall.1
	find $(DSTROOT) -type f -perm +111 -exec strip -x '{}' \;
	rm -f $(DSTROOT)/bin/zsh-4.3.4
	ln $(DSTROOT)/bin/zsh $(DSTROOT)/bin/zsh-4.3.4
	rm $(DSTROOT)/usr/share/zsh/4.3.4/scripts/newuser
	mkdir -p $(DSTROOT)/private/etc
	install -m 0444 -o root -g wheel zprofile $(DSTROOT)/private/etc

# Automatic Extract & Patch
AEP	       = YES
AEP_Project    = $(Project)
AEP_Version    = 4.3.4
AEP_ProjVers   = $(AEP_Project)-$(AEP_Version)
AEP_Filename   = $(AEP_ProjVers).tar.bz2
AEP_ExtractDir = $(AEP_ProjVers)
AEP_Patches    = utmpx_ut_user.patch no_strip.patch arg_zero.patch \
		 _groups.patch zsh-Doc.patch

ifeq ($(suffix $(AEP_Filename)),.bz2)
    AEP_ExtractOption = j
else
    AEP_ExtractOption = z
endif

install_source::
ifeq ($(AEP),YES)
	$(TAR) -C $(SRCROOT) -$(AEP_ExtractOption)xf $(SRCROOT)/$(AEP_Filename)
	$(RMDIR) $(SRCROOT)/$(AEP_Project)
	$(MV) $(SRCROOT)/$(AEP_ExtractDir) $(SRCROOT)/$(AEP_Project)
	for patchfile in $(AEP_Patches); do \
	    ( cd $(SRCROOT)/$(Project) && patch -lp0 < $(SRCROOT)/patches/$$patchfile ) || exit 1 ; \
	done
endif

OSV	= $(DSTROOT)/usr/local/OpenSourceVersions
OSL	= $(DSTROOT)/usr/local/OpenSourceLicenses

install-plist:
	$(MKDIR) $(OSV)
	$(INSTALL_FILE) $(SRCROOT)/$(Project).plist $(OSV)/$(Project).plist
	$(MKDIR) $(OSL)
	$(INSTALL_FILE) $(Sources)/LICENCE $(OSL)/$(Project).txt

##---------------------------------------------------------------------
# Patch config.h just after running configure
#
# RLIMIT_RSS is now the save value as the new RLIMIT_AS, which causes
# a duplicate case value.  So we undefine HAVE_RLIMIT_RSS.
##---------------------------------------------------------------------
ConfigStamp2 = $(ConfigStamp)2

configure:: $(ConfigStamp2)

$(ConfigStamp2): $(ConfigStamp)
	$(_v) ed - ${BuildDirectory}/config.h < $(SRCROOT)/patches/config.h.ed
	$(_v) $(TOUCH) $(ConfigStamp2)
