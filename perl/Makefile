##--------------------------------------------------------------------------
# Makefile for perl
##--------------------------------------------------------------------------
# Wilfredo Sanchez | wsanchez@apple.com
# I wish they'd just use autoconf. This is hairy.
# Modified by Edward Moy <emoy@apple.com>
##--------------------------------------------------------------------------

# Project info
Project             = perl
UserType            = Developer
ToolType            = Commands
##--------------------------------------------------------------------------
# env_no_rc_trace is a shell script that removes RC_TRACE_ARCHIVES and
# RC_TRACE_DYLIBS from the environment before exec-ing the argument.
# This is necessary because otherwise B&I logging messages will get into
# the cppsymbols value in Config.pm and break h2ph (3093501).
##--------------------------------------------------------------------------
Configure           = $(SRCROOT)/env_no_rc_trace $(BuildDirectory)/Configure
Extra_Environment   = HTMLDIR="$(Install_HTML)"						\
		      AR="$(SRCROOT)/ar.sh"  DYLD_LIBRARY_PATH=$(BuildDirectory)
Extra_Install_Flags = HTMLDIR="$(RC_Install_HTML)" HTMLROOT=$(Install_HTML)
GnuAfterInstall     = fix-dstroot zap-sitedirs
Extra_CC_Flags      = -Wno-precomp

# It's a GNU Source project
# Well, not really but we can make it work.
include $(MAKEFILEPATH)/CoreOS/ReleaseControl/GNUSource.make

Install_Target  = install-strip
CC_Optimize     = 
Extra_CC_Flags  = 
Configure_Flags = -ds -e -Dprefix='$(Install_Prefix)' -Dccflags='$(CFLAGS)' -Dldflags='$(LDFLAGS)' -Dman3ext=3pm -Duseithreads -Duseshrplib

##--------------------------------------------------------------------------
# We need to strip $(DSTROOT) from Config.pm and .packlist.
#
# We may be building perl fat here, but a system may only have thin libraries
# installed.  So now we need to remove the -arch build flags from Config.pm so
# you can build modules on those systems.  This means modules are build thin
# by default.
#
# We do both of these things in the fix-dstroot.pl script
##--------------------------------------------------------------------------
MINIPERL = DYLD_LIBRARY_PATH="$(BuildDirectory)" "$(BuildDirectory)/miniperl"

fix-dstroot:
	$(_v) $(MINIPERL) -I$(BuildDirectory)/lib $(SRCROOT)/fix-dstroot.pl $(DSTROOT)

zap-sitedirs:
	$(_v) $(RMDIR) $(DSTROOT)$(NSLOCALDIR)$(NSLIBRARYSUBDIR)
