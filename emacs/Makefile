##
# Makefile for emacs
##

Extra_CC_Flags = -no-cpp-precomp
Extra_LD_Flags = -Wl,-headerpad,0x1000
Extra_Configure_Flags = --without-x

# Project info
Project  = emacs
UserType = Developer
ToolType = Commands
CommonNoInstallSource = YES
GnuAfterInstall = remove-dir install-dumpemacs

# It's a GNU Source project
include $(MAKEFILEPATH)/CoreOS/ReleaseControl/GNUSource.make

# Regenerate the .elc files after copying the source, since RCS string
# substitution has corrupted several of them.  This is an unavoidable result of
# storing the sources in cvs.

installsrc : CC_Archs = 
installsrc :
	if test ! -d $(SRCROOT) ; then mkdir -p $(SRCROOT); fi;
	tar cf - . | (cd $(SRCROOT) ; tar xfp -)
	for i in `find $(SRCROOT) | grep "CVS$$"` ; do \
		if test -d $$i ; then \
			rm -rf $$i; \
		fi; \
	done
	$(SHELL) -ec \
	'cd $(SRCROOT)/emacs; \
	$(Environment) $(Configure) $(Configure_Flags); \
	$(MAKE) bootstrap; \
	$(MAKE) distclean'

remove-dir :
	rm $(DSTROOT)/usr/share/info/dir

install-dumpemacs: $(SRCROOT)/dumpemacs
	$(INSTALL) -o root -g wheel -m 555 $(SRCROOT)/dumpemacs $(DSTROOT)/usr/libexec/
