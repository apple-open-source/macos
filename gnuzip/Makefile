##
# Makefile for gzip
##

# Project info
Project           = gzip
UserType          = Administration
ToolType          = Commands
Extra_Environment = ZCAT=gzcat G=""
GnuAfterInstall   = gnu_after_install

# It's a GNU Source project
include $(MAKEFILEPATH)/CoreOS/ReleaseControl/GNUSource.make

gnu_after_install::
	$(_v) $(LN) -f $(RC_Install_Prefix)/bin/gzcat $(RC_Install_Prefix)/bin/zcat
	$(_v) $(LN) -f $(RC_Install_Prefix)/share/man/man1/gzcat.1 $(RC_Install_Prefix)/share/man/man1/zcat.1
