##
# Makefile for sasl
##

# Project info
Project               = cyrus_sasl
ToolType              = Services
UserType              = Administration
Configure             = $(Sources)/configure
#Extra_CC_Flags        = -fno-common -Wno-precomp
Extra_Configure_Flags = --includedir="$(USRDIR)/local/include"
Extra_Environment     = SERVER_BINDIR="$(LIBEXECDIR)"
GnuAfterInstall       = strip

# It's a GNU Source project
include $(MAKEFILEPATH)/CoreOS/ReleaseControl/GNUSource.make

Install_Flags  = DESTDIR=$(DSTROOT)
Install_Target = install

    
strip:
	$(_v) find $(DSTROOT)$(USRLIBDIR) -type f | xargs strip -S
	$(_v) find $(DSTROOT)/usr/sbin -type f | xargs strip -S

    