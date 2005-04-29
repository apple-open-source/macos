##
# CoreOS Makefile for zip
##

Project		= zip
UserType	= Developer
ToolType	= Commands

GnuNoConfigure	= YES
GnuNoBuild	= YES

GnuAfterInstall	= destroot manpages ossinfo

OSLICENSES	= /usr/local/OpenSourceLicenses
PATCHFILES	= zip23-CAN-2004-1010-patch.txt canversion-patch.txt

include $(MAKEFILEPATH)/CoreOS/ReleaseControl/GNUSource.make

Install_Target	= -f unix/Makefile generic
Extra_CC_Flags  = -mdynamic-no-pic
CC_Optimize     =       # project handles this
Extra_Install_Flags	=	LOCAL_ZIP="$(CC_Flags)" \
		BIND="$(CC) $(LDFLAGS)" CC="$(CC) -fno-builtin"
Environment		=	$(Extra_Environment)

build:: install_source_local patch_source_local

destroot:
	$(MAKE) -C $(BuildDirectory) $(Environment) $(Install_Flags) \
		-f unix/Makefile install \
		MANDIR=$(RC_Install_Man)/'man$$(manext)'

manpages:
	$(INSTALL_FILE) $(SRCROOT)/manpage_alias $(RC_Install_Man)/man1/zipcloak.1
	$(INSTALL_FILE) $(SRCROOT)/manpage_alias $(RC_Install_Man)/man1/zipnote.1
	$(INSTALL_FILE) $(SRCROOT)/manpage_alias $(RC_Install_Man)/man1/zipsplit.1

ossinfo:
	$(MKDIR) $(DSTROOT)/$(OSLICENSES)
	$(INSTALL_FILE) $(SRCROOT)/$(Project)/LICENSE $(DSTROOT)/$(OSLICENSES)/$(Project).txt

install_source_local:
	cd $(Project) && $(PAX) -rw . $(BuildDirectory)

patch_source_local: install_source_local
	for p in $(PATCHFILES); do						\
		cd $(BuildDirectory) && patch -f -p1 --posix < $(SRCROOT)/$${p};	\
	done
