##
# Makefile for freeradius
##

# Project info
Project               = freeradius
ProjectName           = freeradius
UserType              = Administrator
ToolType              = Commands

Extra_CC_Flags        = -fno-common -gfull -I${OBJROOT}/libltdl
Extra_LD_Flags        = -L${OBJROOT}/libltdl
#Extra_Environment     = AR=${SRCROOT}/ar.sh

GnuNoChown      = YES
#GnuAfterInstall = fixup-dstroot

# It's a GNU Source project
include /Developer/Makefiles/CoreOS/ReleaseControl/GNUSource.make

Extra_Configure_Flags = --enable-static --disable-shared
#Extra_Configure_Flags += --prefix=${DSTROOT}/private --sysconfdir=${DSTROOT}/private/etc
#Used_Configure_Flags = --enable-static --disable-shared --prefix=/usr --sysconfdir=/private/etc --localstatedir=/private/var --mandir=/usr/share/man

Extra_CC_Flags        += -F/System/Library/PrivateFrameworks
Extra_LD_Libraries    += -framework DirectoryService

Install_Flags         = DESTDIR=$(DSTROOT)

#Install_Target = install
Install_Target = build

build::
	$(_v) $(MAKE) -C ${BuildDirectory}/libltdl $(Environment)
	$(_v) $(MAKE) -C ${BuildDirectory}/src $(Environment)
	$(_v) $(MAKE) -C ${BuildDirectory}/raddb $(Environment)
	$(_v) $(MAKE) -C ${BuildDirectory}/scripts $(Environment)
	$(_v) $(MAKE) -C ${BuildDirectory}/doc $(Environment)

#fixup-dstroot:
#	$(_v) mkdir -p $(DSTROOT)/private
#	$(_v) mv    $(DSTROOT)/etc $(DSTROOT)/private
#	$(_v) rmdir $(DSTROOT)/var/empty
#	$(_v) rmdir $(DSTROOT)/var
