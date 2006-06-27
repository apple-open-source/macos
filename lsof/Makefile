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
GnuAfterInstall = add_supporting_files
include $(MAKEFILEPATH)/CoreOS/ReleaseControl/GNUSource.make

Install_Target = install-strip

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

LSOF_MAKEFILE   = $(OBJROOT)/Makefile
LSOF_MAKEFILE2  = $(OBJROOT)/lib/Makefile
LSOF_MACHINE_H1 = $(OBJROOT)/dialects/darwin/kmem/machine.h
LSOF_MACHINE_H2 = $(OBJROOT)/dialects/darwin/libproc/machine.h

ConfigStamp2 = $(ConfigStamp)2

configure:: $(ConfigStamp2)

$(ConfigStamp2): $(ConfigStamp)
	$(_v) $(CAT) $(LSOF_MAKEFILE) |			\
		$(SED)	-e 's@^\(DEBUG=\).*@\1 -Os -g@'	\
		> /tmp/build.lsof.$(UNIQUE)
	$(_v) $(MV) -f /tmp/build.lsof.$(UNIQUE) $(LSOF_MAKEFILE)
	$(_v) $(CAT) $(LSOF_MAKEFILE2) |		\
		$(SED)	-e 's@^\(DEBUG=\).*@\1 -Os -g@'	\
		> /tmp/build.lsof.$(UNIQUE)
	$(_v) $(MV) -f /tmp/build.lsof.$(UNIQUE) $(LSOF_MAKEFILE2)
	$(_v) $(CAT) $(LSOF_MACHINE_H1) |					\
		$(SED)	-e 's@^.*\(#define.*HASSECURITY.*1\).*@\1@'		\
			-e 's@^.*\(#define.*HASKERNIDCK.*1\).*@/* \1 */@'	\
		> /tmp/build.lsof.$(UNIQUE)
	$(_v) $(MV) -f /tmp/build.lsof.$(UNIQUE) $(LSOF_MACHINE_H1)
	$(_v) $(CAT) $(LSOF_MACHINE_H2) |					\
		$(SED)	-e 's@^.*\(#define.*HASSECURITY.*1\).*@\1@'		\
			-e 's@^.*\(#define.*HASKERNIDCK.*1\).*@/* \1 */@'	\
		> /tmp/build.lsof.$(UNIQUE)
	$(_v) $(MV) -f /tmp/build.lsof.$(UNIQUE) $(LSOF_MACHINE_H2)
	$(_v) $(TOUCH) $(ConfigStamp2)

# Open Source support files
OSV = ${DSTROOT}/usr/local/OpenSourceVersions
OSL = ${DSTROOT}/usr/local/OpenSourceLicenses

add_supporting_files:
	@echo "Adding Open Source support files"
	$(_v) $(MKDIR) ${OSV}
	$(_v) $(INSTALL_FILE) $(SRCROOT)/${Project}.plist ${OSV}/${Project}.plist
	$(_v) $(MKDIR) ${OSL}
	$(_v) $(CAT) ${SRCROOT}/lsof/00README				\
		| $(SED) -n -e '/^License/,/\*\//p'			\
		> ${OSL}/${Project}.txt
	$(_v) echo ""										>> ${OSL}/${Project}.txt
	$(_v) echo "In addition, the following copyright is included with the files"		>> ${OSL}/${Project}.txt
	$(_v) echo "associated with the \"darwin\" dialect that use the <libproc.h> APIs."	>> ${OSL}/${Project}.txt
	$(_v) echo ""										>> ${OSL}/${Project}.txt
	$(_v) $(CAT) ${SRCROOT}/lsof/dialects/darwin/libproc/dproc.c	\
		| $(SED) -n -e '6,34p'					\
		| $(SED)    -e 's/^/    /'				\
		>> ${OSL}/${Project}.txt
	$(_v) $(CHOWN) $(Install_File_User):$(Install_File_Group) ${OSL}/${Project}.txt
	$(_v) $(CHMOD) $(Install_File_Mode) ${OSL}/${Project}.txt
