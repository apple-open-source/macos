##
# Makefile for tcsh
##

# Project info
Project               = tcsh
UserType              = Administration
ToolType              = Commands
Extra_CC_Flags        = -D_PATH_TCSHELL='\"/bin/tcsh\"' -Wno-precomp
Extra_Configure_Flags = --bindir="/bin"
Extra_Install_Flags   = DESTBIN="$(DSTROOT)/bin"
GnuAfterInstall       = install-links install-rc

# It's a GNU Source project
include $(MAKEFILEPATH)/CoreOS/ReleaseControl/GNUSource.make

install-rc:
	$(_v) $(INSTALL_DIRECTORY) $(DSTROOT)$(ETCDIR)
	$(_v) echo "source /usr/share/init/tcsh/rc"     > $(DSTROOT)$(ETCDIR)/csh.cshrc
	$(_v) echo "source /usr/share/init/tcsh/login"  > $(DSTROOT)$(ETCDIR)/csh.login
	$(_v) echo "source /usr/share/init/tcsh/logout" > $(DSTROOT)$(ETCDIR)/csh.logout

install-links:
	$(_v) $(INSTALL_DIRECTORY) $(DSTROOT)$(BINDIR)
	$(_v) $(LN) -f $(DSTROOT)$(BINDIR)/tcsh $(DSTROOT)$(BINDIR)/csh
