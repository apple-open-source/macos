##
# Makefile for groff
##

# Project info
Project             = groff
UserType            = Administration
ToolType            = Commands
Extra_CC_Flags      = -no-cpp-precomp
Extra_Environment   = LIBM="" manroot="$(MANDIR)"
Extra_Install_Flags = INSTALL_PROGRAM="$(INSTALL) -c -s"	\
		      manroot="$(DSTROOT)$(MANDIR)"
GnuAfterInstall     = remove-dir install-man

# It's a GNU Source project
include $(MAKEFILEPATH)/CoreOS/ReleaseControl/GNUSource.make

Install_Target = install

remove-dir :
	rm $(DSTROOT)/usr/share/info/dir

# we install the legacy mdoc.7 man page
# as well as link groff_mdoc to the legacy mdoc.samples.7 page
install-man:
	install -d $(DSTROOT)/usr/share/man/man7
	install -m 444 $(SRCROOT)/mdoc.7 $(DSTROOT)$(MANDIR)/man7/mdoc.7
	$(LN) $(DSTROOT)$(MANDIR)/man7/groff_mdoc.7 $(DSTROOT)$(MANDIR)/man7/mdoc.samples.7
