##
# Makefile for fetchmail
##

# Project info
Project  = fetchmail
UserType = Administration
ToolType = Services
Extra_Configure_Flags = --disable-nls
GnuAfterInstall = strip remove-fetchmailconf

# It's a GNU Source project
include $(MAKEFILEPATH)/CoreOS/ReleaseControl/GNUSource.make
Install_Target = install

strip::
	$(STRIP) $(DSTROOT)/usr/bin/fetchmail

# fetchmailconf is a python script, and we don't have python.
remove-fetchmailconf:
	$(RM) $(DSTROOT)/usr/bin/fetchmailconf
	$(RM) $(DSTROOT)/usr/share/man/man1/fetchmailconf.1
