##
# Makefile for emacs
##

Extra_CC_Flags = -no-cpp-precomp
Extra_Configure_Flags = --without-x

# Project info
Project  = emacs
UserType = Developer
ToolType = Commands
CommonNoInstallSource = YES
GnuAfterInstall = remove-dir

# It's a GNU Source project
include $(MAKEFILEPATH)/CoreOS/ReleaseControl/GNUSource.make

# Emacs cannot be built fat; it requires a forked native build
CC_Archs =

# Regenerate the .elc files after copying the source, since RCS string
# substitution has corrupted several of them.  This is an unavoidable result of
# storing the sources in cvs.
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
