##
# Makefile for GNU m4
##

# Project info
Project		= m4
ProjectName	= gm4
UserType	= Developer
ToolType	= Commands
Install_HTML	= $(NSDEVELOPERDIR)/Documentation/DeveloperTools/m4
GnuAfterInstall	= after_install

# It's a GNU Source project
include $(MAKEFILEPATH)/CoreOS/ReleaseControl/GNUSource.make

# Install gm4 for compatibilty with previous releases.
after_install :
	ln $(DSTROOT)/usr/bin/m4 $(DSTROOT)/usr/bin/gm4
