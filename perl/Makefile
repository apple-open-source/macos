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
Configure           = '$(SRCROOT)/env_no_rc_trace' '$(BuildDirectory)'/Configure
Extra_Environment   = HTMLDIR='$(Install_HTML)'						\
		      AR='$(SRCROOT)/ar.sh'  DYLD_LIBRARY_PATH='$(BuildDirectory)'
Extra_Install_Flags = HTMLDIR='$(RC_Install_HTML)' HTMLROOT='$(Install_HTML)'
GnuAfterInstall     = fix-dstroot zap-sitedirs link-man-page
Extra_CC_Flags      = -Wno-precomp
ifeq "$(RC_XBS)" "YES"
GnuNoBuild	    = YES
endif

# It's a GNU Source project
# Well, not really but we can make it work.
include $(MAKEFILEPATH)/CoreOS/ReleaseControl/GNUSource.make

Install_Target  = install-strip
CC_Optimize     = 
Extra_CC_Flags  = 
Configure_Flags = -ds -e -Dprefix='$(Install_Prefix)' -Dccflags='$(CFLAGS)' -Dldflags='$(LDFLAGS)' -Dman3ext=3pm -Duseithreads -Duseshrplib

##---------------------------------------------------------------------
# Patch pyconfig.h just after running configure
#
# Makefile.ed is used to workaround a bug in dyld (3661976).  It can be
# removed when dyld is fixed.
##---------------------------------------------------------------------
ConfigStamp2 = $(ConfigStamp)2

configure:: $(ConfigStamp2)

$(ConfigStamp2): $(ConfigStamp)
	$(_v) sed -e 's/@PREPENDFILE@/$(PREPENDFILE)/' \
	    -e 's/@APPENDFILE@/$(APPENDFILE)/' \
	    -e 's/@VERSION@/$(_VERSION)/' < '$(SRCROOT)/fix/config.h.ed' | \
	    ed - '${BuildDirectory}/config.h'
	$(_v) ed - '${BuildDirectory}/Makefile' < '$(SRCROOT)/fix/Makefile.ed'
	$(_v) ed - '${BuildDirectory}/GNUmakefile' < '$(SRCROOT)/fix/Makefile.ed'
	$(_v) $(TOUCH) $(ConfigStamp2)

##--------------------------------------------------------------------------
# We need to strip $(DSTROOT) from Config.pm and .packlist.
#
# We may be building perl fat here, but a system may only have thin libraries
# installed.  So now we need to remove the -arch build flags from Config.pm so
# you can build modules on those systems.  This means modules are build thin
# by default.
#
# We do both of these things in the fix-dstroot.pl script
#
# Setting DYLD_IGNORE_PREBINDING is used to workaround a bug in dyld (3661976).
# It can be removed when dyld is fixed.
##--------------------------------------------------------------------------
MINIPERL = DYLD_IGNORE_PREBINDING=all DYLD_LIBRARY_PATH='$(BuildDirectory)' '$(BuildDirectory)/miniperl'

fix-dstroot:
	$(_v) $(MINIPERL) -I'$(BuildDirectory)/lib' '$(SRCROOT)/fix-dstroot.pl' '$(DSTROOT)'

zap-sitedirs:
	$(_v) $(RMDIR) '$(DSTROOT)$(NSLOCALDIR)$(NSLIBRARYSUBDIR)'

link-man-page:
	$(_v) $(LN) '$(DSTROOT)/usr/share/man/man1/perl.1' '$(DSTROOT)/usr/share/man/man1/perl$(_VERSION).1'
