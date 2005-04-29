##
# Makefile for vim
##

# Project info
Project  = vim
#CommonNoInstallSource = YES

Extra_CC_Flags = -no-cpp-precomp
Extra_Configure_Flags = --enable-cscope --enable-gui=no --without-x --enable-multibyte
GnuAfterInstall = symlink

lazy_install_source:: shadow_source

# It's a GNU Source project
include $(MAKEFILEPATH)/CoreOS/ReleaseControl/GNUSource.make

Install_Target  = install
Install_Prefix = /usr
RC_Install_Prefix = $(Install_Prefix)

symlink:
	ln -s vim $(DSTROOT)/usr/bin/vi
	ln -s vim.1 $(DSTROOT)/usr/share/man/man1/vi.1
	install -d $(DSTROOT)/usr/share/vim
	install -c -m 644 $(SRCROOT)/vimrc $(DSTROOT)/usr/share/vim
