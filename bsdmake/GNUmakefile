##
# Makefile for BSD make
##

# Project info
Project  = bsdmake
UserType = Developer
ToolType = Commands

BSD_After_Install = munge_name

# It's a BSD project
include $(MAKEFILEPATH)/CoreOS/ReleaseControl/BSD.make

BSDMAKE += -m $(SRCROOT)/mk

munge_name:
	$(_v) $(MV) $(DSTROOT)$(USRBINDIR)/make $(DSTROOT)$(USRBINDIR)/bsdmake
	$(_v) $(MV) $(DSTROOT)$(MANDIR)/man1/make.1.gz $(DSTROOT)$(MANDIR)/man1/bsdmake.1.gz

BSD_install:: build
	@echo "Installing $(Project)/mk..."
	$(_v) umask $(Install_Mask) ;					\
		cd mk && $(Environment) MAKEOBJDIR="$(SRCROOT)/mk"	\
			 $(BSDMAKE) $(Install_Environment) BINDIR="$(SHAREDIR)" $(Install_Target)
