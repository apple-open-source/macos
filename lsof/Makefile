##
# Makefile for lsof
##
# Allan Nathanson <ajn@apple.com>
##

# Project info
Project  = lsof
UserType = Administrator
ToolType = Commands

# It's a GNU Source project
# Well, not really but we can make it work.
GnuNoChown = YES
include $(MAKEFILEPATH)/CoreOS/ReleaseControl/GNUSource.make

Install_Target = install-strip
Extra_Install_Flags = "DEBUG=\"-g\""

Configure = $(BuildDirectory)/Configure
Configure_Flags = -n darwin

##
# The "Configure" script wants to create symlinks within the source
# tree. Since we're not supposed to modify the sources we'll create
# a 'shadow tree' and use that directory instead.
##
lazy_install_source:: shadow_source

##
# Change a few of compile time definitions
##

UNIQUE := $(shell echo $$$$)

LSOF_MAKEFILE  = $(OBJROOT)/Makefile
LSOF_MACHINE_H = $(OBJROOT)/dialects/darwin/machine.h

ConfigStamp2 = $(ConfigStamp)2

configure:: $(ConfigStamp2)

$(ConfigStamp2): $(ConfigStamp)
	$(_v) $(CAT) $(LSOF_MAKEFILE) |			\
		$(SED)	-e 's@^\(DEBUG=\).*@\1 -Os@'	\
		> /tmp/build.lsof.$(UNIQUE)
	$(_v) $(MV) -f /tmp/build.lsof.$(UNIQUE) $(LSOF_MAKEFILE)
	$(_v) $(CAT) $(LSOF_MACHINE_H) |					\
		$(SED)	-e 's@^.*\(#define.*HASSECURITY.*1\).*@\1@'		\
			-e 's@^.*\(#define.*HASKERNIDCK.*1\).*@/* \1 */@'	\
		> /tmp/build.lsof.$(UNIQUE)
	$(_v) $(MV) -f /tmp/build.lsof.$(UNIQUE) $(LSOF_MACHINE_H)
	$(_v) $(TOUCH) $(ConfigStamp2)

