##
# Makefile for expect
##

# Project info
Project               = expect
ToolType              = Commands
Extra_Configure_Flags = --with-tclinclude=/usr/local/include --with-tcl=/usr/lib
GnuAfterInstall       = strip

# It's a GNU Source project
include $(MAKEFILEPATH)/CoreOS/ReleaseControl/GNUSource.make

Install_Target         = install-binaries install-doc

strip:
	$(_v) $(STRIP) $(DSTROOT)/usr/bin/expect
