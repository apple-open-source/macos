##
# Makefile for procmail
##

# Project info
Project           = procmail
UserType          = Administration
ToolType          = Services
Extra_Environment = LDFLAGS0="" LOCKINGTEST="/tmp" \
		    BASENAME="$(USRDIR)" MANDIR=$(MANDIR)

# It's a 3rd Party Source project
include $(MAKEFILEPATH)/CoreOS/ReleaseControl/Common.make

Install_Flags = BASENAME="$(DSTROOT)$(USRDIR)" MANDIR=$(DSTROOT)$(MANDIR)

lazy_install_source:: shadow_source

build::
	@echo "Building $(Project)..."
	$(_v) $(MAKE) -C $(BuildDirectory)/$(Project) $(Environment)

install::
	@echo "Installing $(Project)..."
	$(_v) $(MAKE) -C $(BuildDirectory)/$(Project) $(Environment) $(Install_Flags) install install-suid
	$(_v) cd $(DSTROOT)$(USRBINDIR) && strip *
