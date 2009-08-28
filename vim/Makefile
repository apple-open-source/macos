##
# Makefile for vim
##

# Project info
Project  = vim
#CommonNoInstallSource = YES

Extra_CC_Flags = -mdynamic-no-pic
Extra_Configure_Flags = --enable-cscope --enable-gui=no --without-x --enable-multibyte --disable-darwin
GnuAfterInstall = install-plist symlink

lazy_install_source:: shadow_source

# It's a GNU Source project
include $(MAKEFILEPATH)/CoreOS/ReleaseControl/GNUSource.make

Install_Target  = install
Install_Prefix = /usr
RC_Install_Prefix = $(Install_Prefix)

# Automatic Extract & Patch
AEP            = YES
AEP_Project    = $(Project)
AEP_Version    = 7.2
AEP_ProjVers   = $(AEP_Project)-$(AEP_Version)
AEP_Filename   = $(AEP_ProjVers).tar.bz2
AEP_Extras_Filename   = $(AEP_ProjVers)-extra.tar.gz
AEP_ExtractDir = vim72
AEP_Patches    = build.diff xattr.diff ex_cmds.c.diff ex_cmds2.c.diff \
	ex_docmd.c.diff fileio.c.diff globals.h.diff main.c.diff \
	move.c.diff normal.c.diff option.c.diff option.h.diff \
	os_unix.c.diff undo.c.diff memline.c.diff \
	ex_cmds.c.diff2 edit.c.diff normal.c.diff2 normal.c.diff3 \
	ops.c.diff no-fortify.diff xxd.c.diff ex_z.diff
VIM_Patches	= $(wildcard patches/7.2.*)

ifeq ($(suffix $(AEP_Filename)),.bz2)
AEP_ExtractOption = j
else
AEP_ExtractOption = z
endif

# Extract the source.
install_source::
ifeq ($(AEP),YES)
	$(TAR) -C $(SRCROOT) -$(AEP_ExtractOption)xf $(SRCROOT)/$(AEP_Filename)
	$(TAR) -C $(SRCROOT) -zxf $(SRCROOT)/$(AEP_Extras_Filename)
	$(RMDIR) $(SRCROOT)/$(AEP_Project)
	$(MV) $(SRCROOT)/$(AEP_ExtractDir) $(SRCROOT)/$(AEP_Project)
	for patchfile in $(AEP_Patches); do \
		(cd $(SRCROOT)/$(Project) && patch -p1 < $(SRCROOT)/patches/$$patchfile) || exit 1; \
	done
	for patchfile in $(VIM_Patches); do \
		(cd $(SRCROOT)/$(Project) && patch -p0 < $(SRCROOT)/$$patchfile) || exit 1; \
	done
	rm $(SRCROOT)/vim/src/os_beos.rsrc
endif

OSV = $(DSTROOT)/usr/local/OpenSourceVersions
OSL = $(DSTROOT)/usr/local/OpenSourceLicenses

install-plist:
	$(MKDIR) $(OSV)
	$(INSTALL_FILE) $(SRCROOT)/$(Project).plist $(OSV)/$(Project).plist
	$(MKDIR) $(OSL)
	$(INSTALL_FILE) $(Sources)/runtime/doc/uganda.txt $(OSL)/$(Project).txt

symlink:
	ln -s vim $(DSTROOT)/usr/bin/vi
	ln -s vim.1 $(DSTROOT)/usr/share/man/man1/vi.1
	install -d $(DSTROOT)/usr/share/vim
	install -c -m 644 $(SRCROOT)/vimrc $(DSTROOT)/usr/share/vim
