# Set these variables as needed, then include this file, then:
#

# Project info
Project         = source
UserType        = Administration
ToolType        = Services

GnuNoChown		= YES
GnuAfterInstall = install-startup install-config install-logdir

Extra_Configure_Flags = --with-swatdir="$(SHAREDIR)/swat"			\
			--with-sambabook="$(SHAREDIR)/swat/using_samba"		\
			--with-privatedir="/var/db/samba"			\
			--libdir="/etc"						\
			--with-lockdir="$(SPOOLDIR)/lock" \
			--with-logfilebase="/var/log/samba" 
Extra_Environment     = CODEPAGEDIR="$(SHAREDIR)/codepages"
Extra_Install_Flags   = CODEPAGEDIR="$(DSTROOT)$(SHAREDIR)/codepages"		\
			SWATDIR="$(DSTROOT)$(SHAREDIR)/swat"			\
			SAMBABOOK="$(DSTROOT)$(SHAREDIR)/swat/using_samba"	\
			PRIVATEDIR="$(DSTROOT)/private/var/db/samba"		\
			VARDIR="$(DSTROOT)/private/var"				\
			LIBDIR="$(DSTROOT)/private/etc"				\
			LOCKDIR="$(DSTROOT)$(SPOOLDIR)/lock"			

include $(MAKEFILEPATH)/CoreOS/ReleaseControl/GNUSource.make

Install_Target = install
Extra_CC_Flags = -traditional-cpp \
		 -Wno-four-char-constants \
		 -F/System/Library/PrivateFrameworks \
		 -DDIRECTORY_SERVICE_X -DUSES_CFSTRING -DBOOL_DEFINED 
LDFLAGS += -framework DirectoryService -framework Security -framework CoreFoundation

install-startup:
	mkdir -p $(DSTROOT)/System/Library/StartupItems/Samba/Resources/English.lproj
	$(INSTALL) -c -m 555 $(SRCROOT)/Samba $(DSTROOT)/System/Library/StartupItems/Samba/Samba
	$(INSTALL) -c -m 444 $(SRCROOT)/StartupParameters.plist $(DSTROOT)/System/Library/StartupItems/Samba/
	$(INSTALL) -c -m 444 $(SRCROOT)/Localizable.strings $(DSTROOT)/System/Library/StartupItems/Samba/Resources/English.lproj

install-config:
	$(INSTALL) -c -m 444 $(SRCROOT)/examples/smb.conf.template $(DSTROOT)/private/etc

install-logdir:
	$(INSTALL) -d -m 755 $(DSTROOT)/private/var/log/samba
