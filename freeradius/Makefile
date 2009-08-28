##
# Makefile for freeradius
##

# Project info
Project                = freeradius
ProjectName            = freeradius
ProjectDir			   = $(shell pwd)
UserType               = Administrator
ToolType               = Commands

Extra_CC_Flags         = -fno-common -gdwarf-2
Extra_CC_Flags        += -F/System/Library/PrivateFrameworks -I ${OBJROOT}/src

Extra_LD_Libraries     = -framework DirectoryService

Extra_Configure_Flags  = --disable-static --enable-shared
Extra_Configure_Flags += --srcdir=. --prefix=/usr
Extra_Configure_Flags += --sysconfdir=/private/etc --localstatedir=/private/var
Extra_Configure_Flags += --libdir=/usr/lib/freeradius --includedir=/usr/local/include
Extra_Configure_Flags += --enable-ltdl-install=yes

Extra_Install_Flags   = sysconfdir=$(DSTROOT)/private/etc localstatedir=$(DSTROOT)/private/var
Extra_Install_Flags  += includedir=$(DSTROOT)/usr/local/include
Extra_Install_Flags  += libdir=$(DSTROOT)/usr/lib/freeradius

GnuNoBuild		= YES
GnuAfterInstall = install-plists remove-dirs strip fix-man-pages

lazy_install_source:: full_copy_source

include $(MAKEFILEPATH)/CoreOS/ReleaseControl/GNUSource.make

Install_Target = install

STD_CCFLAGS = $(CC_Debug) $(CC_Other) $(CC_Archs)

# Some of the freeradius makefiles use the 'test -f somefile' which fails
# if the file is a symlink (as it would be for shadow_source).  And since
# the configure scripts depend on the source being in the build directory,
# a full copy of the source is needed in $(OBJROOT).

full_copy_source:
	echo "Creating full copy of sources in the build directory...";
	cd $(Sources) && $(PAX) -rw . $(OBJROOT)


# Override the normal build target because $Environment uses "CFLAGS=..."
# which wipes out all of the "CFLAG+=" statements used in the freeradius
# makefiles.

build::
	umask $(Install_Mask) && cd $(BuildDirectory) && LDFLAGS+="$(Extra_LD_Flags) $(Extra_LD_Libraries)" CFLAGS+="$(Extra_CC_Flags) $(STD_CCFLAGS)" $(MAKE)

install-plists:
	$(MKDIR) -p $(DSTROOT)/usr/local/OpenSourceLicenses
	$(INSTALL) -m 644 $(SRCROOT)/freeradius/LICENSE $(DSTROOT)/usr/local/OpenSourceLicenses/freeradius.txt
	$(MKDIR) -p $(DSTROOT)/usr/local/OpenSourceVersions
	$(INSTALL) -m 644 $(SRCROOT)/freeradius.plist $(DSTROOT)/usr/local/OpenSourceVersions/freeradius.plist

remove-dirs:
	$(RMDIR) $(DSTROOT)/private/var/run

strip:
	$(STRIP) $(DSTROOT)/usr/bin/*
	$(STRIP) -u -r $(DSTROOT)/usr/sbin/*
	$(STRIP) -x $(DSTROOT)/usr/lib/freeradius/*.so
	$(STRIP) -x $(DSTROOT)/usr/lib/freeradius/*.dylib

fix-man-pages:
	cd $(DSTROOT)/usr/share/man/man8 && ln -fs radisud.8.gz checkrad.8.gz
	cd $(DSTROOT)/usr/share/man/man8 && ln -fs radiusd.8.gz rc.radiusd.8.gz
