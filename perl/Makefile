##
# Makefile for perl
##
# Wilfredo Sanchez | wsanchez@apple.com
# I wish they'd just use autoconf. This is hairy.
##

# Project info
Project             = perl
UserType            = Developer
ToolType            = Commands
Configure           = $(BuildDirectory)/Configure
Extra_Environment   = HTMLDIR="$(Install_HTML)"						\
		      AR="$(SRCROOT)/ar.sh"  DYLD_LIBRARY_PATH=$(BuildDirectory)
Extra_Install_Flags = HTMLDIR="$(RC_Install_HTML)" HTMLROOT=$(Install_HTML)
GnuAfterInstall     = undo-dstroot-hack zap-sitedirs
Extra_CC_Flags      = -Wno-precomp

# It's a GNU Source project
# Well, not really but we can make it work.
include $(MAKEFILEPATH)/CoreOS/ReleaseControl/GNUSource.make

Install_Target  = install-strip
CC_Optimize     = 
Extra_CC_Flags  = 
Configure_Flags = -ds -e -Dprefix='$(Install_Prefix)' -Dccflags='$(CFLAGS)' -Dldflags='$(LDFLAGS)'

##
# Eit. Getting perl to compile outside of the source tree is a severe pain.
# I got some of it working, but it's pretty bothersome, so we'll cheat here and
# create a 'shadow tree' instead and build there.
##
lazy_install_source:: shadow_source

##
# Eit again.
# There's no easy way (well I don't see one, offhand) to sneak $(DSTROOT) into the
# location variables at "make install" time because make doesn't do the install.
# So we create config.sh, and then do a string replace for the install locations
# in there, which are meant for AFS, or we'd have configure options for it maybe.
# But we don't, so we hack some more.
##
UNIQUE := $(shell echo $$$$)

CONFIGSH = $(OBJROOT)/config.sh

ConfigStamp2 = $(ConfigStamp)2

configure:: $(ConfigStamp2)

$(ConfigStamp2): $(ConfigStamp)
	$(_v) $(CAT) $(CONFIGSH) |					\
	      $(SED) -e 's@^\(install[^=]*='\''\)@\1$(DSTROOT)@'	\
	             -e 's@^\(installstyle='\''\)$(DSTROOT)@\1@'	\
	             -e 's@^\(installusrbinperl='\''\)$(DSTROOT)@\1@'	\
	      > /tmp/build.perl.$(UNIQUE)
	$(_v) $(MV) /tmp/build.perl.$(UNIQUE) $(CONFIGSH)
	$(_v) $(TOUCH) $(ConfigStamp2)

##
# Triple eit.
# Having whacked values in config.sh effects Config.pm, and we don't want $(DSTROOT) in there.
# So we need to undo the damage once it is installed.
#
# Quad-eit:
# We may be building perl fat here, but a system may only have thin libraries installed.
# So now we need to remove the -arch build flags from Config.pm so you can build modules
# on those systems.  This means modules are build thin by default.
##
MINIPERL = export DYLD_LIBRARY_PATH="$(BuildDirectory)"; "$(BuildDirectory)/miniperl"
CONFIGPM = $(DSTROOT)/System/Library/Perl/$$($(MINIPERL) -v | grep 'This is perl' | awk '{print $$7}')/Config.pm

undo-dstroot-hack:
	$(_v) echo "Fixing $(CONFIGPM)"
	$(_v) $(CAT) $(CONFIGPM) | $(SED) 's@^\(install[^=]*='\''\)$(DSTROOT)@\1@' > /tmp/build.perl.$(UNIQUE)
	$(_v) $(MINIPERL) -i -pe 's|-arch\s+\S+\s*||g' /tmp/build.perl.$(UNIQUE)
	$(_v) $(MV) -f /tmp/build.perl.$(UNIQUE) $(CONFIGPM)

zap-sitedirs:
	$(_v) $(RMDIR) $(DSTROOT)$(NSLOCALDIR)$(NSLIBRARYSUBDIR)
