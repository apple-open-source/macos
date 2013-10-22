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
Configure           = '$(TOPSRCROOT)/env_no_rc_trace' '$(BuildDirectory)'/Configure
Extra_Environment   = HTMLDIR='$(Install_HTML)' AR='$(TOPSRCROOT)/fix/ar.sh'
Extra_Install_Flags = HTMLDIR='$(RC_Install_HTML)' HTMLROOT='$(Install_HTML)'
GnuAfterInstall     = zap-sitedirs make-extra-links
ifeq "$(RC_XBS)" "YES"
GnuNoBuild	    = YES
endif

# It's a GNU Source project
# Well, not really but we can make it work.
include $(MAKEFILEPATH)/CoreOS/ReleaseControl/GNUSource.make

# Override compiler setting from GNUSource.make to use those set in GNUmakefile
override CC = $(MY_CC)

Install_Target  = install
CC_Optimize     = 
Configure_Flags = -ds -e -Dprefix='$(Install_Prefix)' -Dccflags='$(CFLAGS)' -Dldflags='$(LDFLAGS)' -Dman3ext=3pm -Duseithreads -Duseshrplib -Dinc_version_list=none -Dcc=$(MY_CC)

##---------------------------------------------------------------------
# Patch config.h, Makefile and others just after running configure
##---------------------------------------------------------------------
ConfigStamp2 = $(ConfigStamp)2

configure:: $(ConfigStamp2)

$(ConfigStamp2): $(ConfigStamp)
	$(_v) sed -e 's/@PREPENDFILE@/$(PREPENDFILE)/g' \
	    -e 's/@APPENDFILE@/$(APPENDFILE)/g' \
	    -e 's/@NONLP@/$(NONLP)/g' \
	    -e 's,@UPDATESLIB@,$(UPDATESLIB),g' \
	    -e 's/@VERSION@/$(ENV_PERL_BASE_VERSION)/g' \
	    '$(SRCROOT)/fix/config.h.ed' \
	| ed - '${BuildDirectory}/config.h'
	$(_v) ed - '${BuildDirectory}/Makefile' < '$(SRCROOT)/fix/Makefile.ed'
	$(_v) ed - '${BuildDirectory}/GNUmakefile' < '$(SRCROOT)/fix/Makefile.ed'
	$(_v) ed - '${BuildDirectory}/cflags' < '$(SRCROOT)/fix/cflags.ed'
	$(_v) sed 's/@VERSION@/$(ENV_PERL_BASE_VERSION)/g' $(SRCROOT)/fix/perlmain.c.ed | ed - ${BuildDirectory}/perlmain.c
	$(_v) $(TOUCH) $(ConfigStamp2)

zap-sitedirs:
	$(_v) $(RMDIR) '$(DSTROOT)$(NSLOCALDIR)$(NSLIBRARYSUBDIR)'

make-extra-links:
	$(_v) $(LN) '$(DSTROOT)/usr/share/man/man1/perl.1' '$(DSTROOT)/usr/share/man/man1/perl$(ENV_PERL_BASE_VERSION).1'
	$(_v) $(LN) '$(DSTROOT)/usr/share/man/man1/perl.1' '$(DSTROOT)/usr/share/man/man1/perl$(ENV_PERL_VERSION).1'
