##
# Makefile for iODBC
##

# Configured for Panther (10.3) by default
# To rebuild on Jaguar (10.2) you need to define JAGUAR
#Extra_CC_Flags = -DJAGUAR

# Project info
Project  = iodbc
UserType = Developer
ToolType = Commands
#Install_Prefix = $(USRDIR)

GnuAfterInstall = trashla

# It's a GNU Source project
include $(MAKEFILEPATH)/CoreOS/ReleaseControl/GNUSource.make


#Install_Target = install
Extra_Configure_Flags = --with-iodbc-inidir=/Library/ODBC --disable-gui TMPDIR=$(OBJROOT)

trashla:
	@echo "Trashing RC undesirables *.la files"
	rm -f $(RC_Install_Prefix)/lib/*.la

