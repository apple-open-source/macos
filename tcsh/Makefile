##
# Makefile for tcsh
##

# Project info
Project               = tcsh
UserType              = Administration
ToolType              = Commands
Extra_CC_Flags        = -D_PATH_TCSHELL='\"/bin/tcsh\"' -no-cpp-precomp
Extra_Configure_Flags = --bindir="/bin"
Extra_Install_Flags   = DESTBIN="$(DSTROOT)/bin"
GnuAfterInstall       = install-links install-rc

# It's a GNU Source project
include $(MAKEFILEPATH)/CoreOS/ReleaseControl/GNUSource.make

Install_Target = install install.man

install-rc:
	$(_v) $(INSTALL_DIRECTORY) $(DSTROOT)$(ETCDIR)
	$(_v) $(INSTALL_DIRECTORY) $(DSTROOT)/usr/share/tcsh/examples
	$(_v) $(INSTALL) -c -m 0644 $(SRCROOT)/$(Project)/init/README $(DSTROOT)/usr/share/tcsh/examples
	$(_v) $(INSTALL) -c -m 0644 $(SRCROOT)/$(Project)/init/aliases $(DSTROOT)/usr/share/tcsh/examples
	$(_v) $(INSTALL) -c -m 0644 $(SRCROOT)/$(Project)/init/completions $(DSTROOT)/usr/share/tcsh/examples
	$(_v) $(INSTALL) -c -m 0644 $(SRCROOT)/$(Project)/init/environment $(DSTROOT)/usr/share/tcsh/examples
	$(_v) $(INSTALL) -c -m 0644 $(SRCROOT)/$(Project)/init/login $(DSTROOT)/usr/share/tcsh/examples
	$(_v) $(INSTALL) -c -m 0644 $(SRCROOT)/$(Project)/init/logout $(DSTROOT)/usr/share/tcsh/examples
	$(_v) $(INSTALL) -c -m 0644 $(SRCROOT)/$(Project)/init/rc $(DSTROOT)/usr/share/tcsh/examples
	$(_v) $(INSTALL) -c -m 0644 $(SRCROOT)/$(Project)/init/tcsh.defaults $(DSTROOT)/usr/share/tcsh/examples
	$(_V) $(INSTALL) -c -m 0644 $(SRCROOT)/csh.cshrc $(DSTROOT)/$(ETCDIR)/
	$(_V) $(INSTALL) -c -m 0644 $(SRCROOT)/csh.login $(DSTROOT)/$(ETCDIR)/
	$(_V) $(INSTALL) -c -m 0644 $(SRCROOT)/csh.logout $(DSTROOT)/$(ETCDIR)/

install-links:
	$(_v) $(INSTALL_DIRECTORY) $(DSTROOT)$(BINDIR)
	$(_v) $(LN) -f $(DSTROOT)$(BINDIR)/tcsh $(DSTROOT)$(BINDIR)/csh
