##
# Makefile for net-snmp
##

# General project info
Project		= net-snmp
ProjectName	= net_snmp
UserType	= Administration
ToolType	= Commands
Submission	= 121

#
# Settings for the net-snmp project.
#
# Justification for #defines:
# CONFORMANCE is to avoid confusing configure
# UNSTABLE is required to pick up some other required structs
# PrivateHeaders is so IPv6 can define struct ifnet.
###
# Disabled while trying to get build stuff working
###
DEFINES			= -DBUILD=$(Submission) \
			-DMACOSX_DEPLOYMENT_TARGET=$(MACOSX_DEPLOYMENT_TARGET)
INCLUDES		= -F/System/Library/PrivateFrameworks/ -F/System/Library/Frameworks/

# For Perl to build correctly, both CFLAGS (CC_Flags) and CCFLAGS (Cxx_Flags)
# must be properly defined.
Extra_CC_Flags		= $(DEFINES) $(INCLUDES)
Extra_Cxx_Flags		= $(Extra_CC_Flags)
# also need ARCHFLAGS
Default_AARCHFLAGS = -arch ppc -arch i386  -arch x86_64
ifneq "$(RC_CFLAGS)" ""
	ARCHFLAGS=$(RC_CLFLAGS)
else
	ARCHFLAGS=$(Default_AARCHFLAGS)
endif

Extra_Configure_Flags	= --sysconfdir=/etc \
			--with-install-prefix=$(DSTROOT) \
			--with-default-snmp-version=2 \
			--with-persistent-directory=/var/db/net-snmp \
			--with-defaults \
			--without-rpm \
			--with-sys-contact="postmaster@example.com" \
			--with-mib-modules="host ucd-snmp/diskio ucd-snmp/loadave ucd-snmp/lmSensorsTables" \
			--disable-static \
			--enable-ipv6 \
			--with-perl-modules \
			--disable-embedded-perl  \
			--without-kmem-usage

# Old / unused configure flags
#			--enable-mini-agent \
#			--with-mib-modules="hardware/memory hardware/cpu host ucd-snmp/diskio ucd-snmp/loadave ucd-snmp/memory" \
#			--with-out-mib-modules="mibII/icmp host/hr_swrun" \
#			--enable-ipv6 \
#			--enable-developer
#			--with-libwrap=/usr/lib/libwrap.dylib
#			--disable-embedded-perl  \


# The following are sometimes necessary if DESTDIR or --with-install-prefix
# are not respected.
#Extra_Install_Flags	= exec_prefix=$(DSTROOT)$(Install_Prefix) \
#			bindir=$(DSTROOT)$(USRBINDIR) \
#			sbindir=$(DSTROOT)$(USRSBINDIR) \
#			sysconfdir=$(DSTROOT)/etc \
#			datadir=$(DSTROOT)$(SHAREDIR) \
#			includedir=$(DSTROOT)$(USRINCLUDEDIR)/net-snmp \
#			libdir=$(DSTROOT)$(USRLIBDIR) \
#			libexecdir=$(DSTROOT)$(LIBEXECDIR) \
#			localstatedir=$(DSTROOT)$(SHAREDIR)

# For some reason, the Perl modules Makefiles use the environment
# version of CCFLAGS instead of the one defined by the Makefile. The
# wacky "INC=" is to used but not defined by the Makefile, so it's
# used here to point to the project's headers instead of those already
# installed on the system (which are out of date).
Extra_Environment	= AR="$(SRCROOT)/ar.sh" INC="-I../../include"
GnuAfterInstall		= install-macosx install-mibs install-compat

# Temporarily set for development
GnuNoInstallHeaders	= YES

# Binaries to strip
STRIPPED_BINS	= encode_keychange snmpbulkget snmpbulkwalk snmpdelta snmpdf \
			snmpget snmpget snmpgetnext snmpinform snmpnetstat \
			snmpset snmpstatus snmptable snmptest snmptranslate \
			snmptrap snmpusm snmpvacm snmpwalk
STRIPPED_SBINS	= snmpd 
STRIPPED_SNMPTRAPD	= snmptrapd
STRIPPED_LIBS	= libnetsnmp libnetsnmpagent libnetsnmphelpers libnetsnmpmibs libnetsnmptrapd
# Binary to patch
CONFIGTOOL	= $(USRBINDIR)/net-snmp-config
# MIB files to install
MIBFILES	:= $(wildcard mibs/*.txt)
MIBDIR		= $(SHAREDIR)/snmp/mibs


# Automatic Extract & Patch
AEP		= YES
AEP_Version	= 5.4.2.1
AEP_Patches    = diskio.patch IPv6.patch universal_builds.patch \
			cache.patch container.patch darwin-header.patch \
			dir_utils.patch disk.patch host.patch \
			lmsensors.patch darwin-sensors.patch swinst.patch swrun.patch \
			system.patch table.patch darwin64.patch perl-cc.patch
AEP_LaunchdConfigs	= org.net-snmp.snmpd.plist
AEP_ConfigDir	= $(ETCDIR)/snmp
AEP_ConfigFiles	= snmpd.conf


# Local targets that must be defined before including the following
# files to get the dependency order correct
.PHONY: $(GnuAfterInstall)

install::

# Include common makefile targets for B&I
include $(MAKEFILEPATH)/CoreOS/ReleaseControl/GNUSource.make
include AEP.make

# Override settings from above include
ifndef MACOSX_DEPLOYMENT_TARGET
	MACOSX_DEPLOYMENT_TARGET = $(shell sw_vers -productVersion | cut -d. -f1-2)
endif
DESTDIR	= $(DSTROOT)

# This project must be built in the source directory because the real
# project (with configure) is there.
BuildDirectory	= $(Sources)

# This needs to be overridden because the project properly uses DESTDIR
# (and supports the configure flag "--with-install-prefix").
Install_Flags	= DESTDIR="$(DSTROOT)"
# This project does not support the default "install-strip" target.
Install_Target	= install 


#
# Post-extract target
#
#extract-source::
#	$(CP) $(SHAREDIR)/libtool/ltmain.sh $(Sources)

#
# Pre-build targets
#
ifneq ($(GnuNoInstallHeaders),YES)
install_headers:: $(GNUConfigStamp)
	@echo "Installing headers..."
	$(_v) umask $(Install_Mask) ; $(MAKE) -C $(BuildDirectory) $(Environment) installheaders
	$(_v) $(FIND) $(DSTROOT) $(Find_Cruft) | $(XARGS) $(RMDIR)
	$(_v) $(FIND) $(SYMROOT) $(Find_Cruft) | $(XARGS) $(RMDIR)
ifneq ($(GnuNoChown),YES)
	$(_v)- $(CHOWN) -R $(Install_User):$(Install_Group) $(DSTROOT) $(SYMROOT)
endif
endif


#
# Post-install targets
#
install-macosx:
	@echo "Reorganizing install for Mac OS X..."
	if [ -d $(DSTROOT)/etc ]; then 				\
		$(MKDIR) -m 755 $(DSTROOT)/private;		\
		$(MV) $(DSTROOT)/etc $(DSTROOT)/private;	\
	fi
	if [ -d $(DSTROOT)/var ]; then 				\
		$(MKDIR) -m 755 $(DSTROOT)/private;		\
		$(MV) $(DSTROOT)/var $(DSTROOT)/private;	\
	fi
	$(MKDIR) -m 700 $(DSTROOT)/private/var/agentx
	@echo "Unzipping man pages..."
	$(_v) if [ -d $(DSTROOT)$(Install_Man) ]; then		\
		$(FIND) $(DSTROOT)/$(Install_Man) -name '*.gz' -print -exec gunzip {} \; ;	\
	fi
	@echo "Removing pod files..."
	$(_v) $(RM) $(DSTROOT)/System/Library/Perl/5.8.8/darwin-thread-multi-2level/perllocal.pod
	@echo "Removing broken tools..."
	$(_v) $(RM) $(DSTROOT)$(USRBINDIR)/snmpcheck \
				$(DSTROOT)$(USRBINDIR)/ipf-mod.pl
	@echo "Stripping unstripped binaries..."
	if [ ! -d $(SYMROOT) ]; then \
		$(MKDIR) -m 755 $(SYMROOT); \
	fi
	$(_v) for file in $(STRIPPED_BINS); \
	do \
		$(CP) $(DSTROOT)$(USRBINDIR)/$${file} $(SYMROOT); \
		$(STRIP) $(DSTROOT)$(USRBINDIR)/$${file}; \
	done
	$(_v) for file in $(STRIPPED_SBINS); \
	do \
		$(CP) $(DSTROOT)$(USRSBINDIR)/$${file} $(SYMROOT); \
		$(STRIP) $(DSTROOT)$(USRSBINDIR)/$${file}; \
	done
	$(_v) for file in $(STRIPPED_SNMPTRAPD); \
	do \
		$(CP) $(DSTROOT)$(USRSBINDIR)/$${file} $(SYMROOT); \
		echo "_SyslogTrap" > $(DSTROOT)$(USRSBINDIR)/snmptrapd.exp; \
		echo "_dropauth" >> $(DSTROOT)$(USRSBINDIR)/snmptrapd.exp; \
		$(STRIP) -s $(DSTROOT)$(USRSBINDIR)/snmptrapd.exp $(DSTROOT)$(USRSBINDIR)/$${file}; \
		$(RM) $(DSTROOT)$(USRSBINDIR)/snmptrapd.exp; \
	done
	$(_v) for file in $(STRIPPED_LIBS); \
	do \
		$(CP) $(DSTROOT)$(USRLIBDIR)/$${file}*.dylib $(SYMROOT); \
		$(STRIP) -x $(DSTROOT)$(USRLIBDIR)/$${file}.dylib; \
	done
	$(_v)- $(FIND) $(DSTROOT)$(NSLIBRARYSUBDIR)/Perl -type f -name '*.bundle' -print -exec strip -S {} \;
	@echo "Copying sensor data"
	$(_v) $(INSTALL_FILE) $(SRCROOT)/SensorDat.xml $(DSTROOT)$(SHAREDIR)/snmp
	@echo "Fixing permissions..."
	$(_v) $(FIND) $(DSTROOT)$(USRINCLUDEDIR)/net-snmp -type f -exec chmod 644 {} \;
	$(_v) $(FIND) $(DSTROOT)$(SHAREDIR)/snmp -type f -exec chmod 644 {} \;
	$(_v) $(RM) $(DSTROOT)$(USRLIBDIR)/*.a $(DSTROOT)$(USRLIBDIR)/*.la
	$(_v) $(FIND) $(DSTROOT)$(MANDIR) -type f -exec chmod 644 {} \;
	@echo "Eliminating architecture flags from $(CONFIGTOOL)..."
	$(MV) $(DSTROOT)$(CONFIGTOOL) $(DSTROOT)$(CONFIGTOOL).old
	$(SED) -Ee 's/-arch [-_a-z0-9]{3,10}//g' \
		-e '/^NSC_INCLUDEDIR=/s/=.*/=\/usr\/local\/include/' \
		-e '/^NSC_LIBDIR=/s/=.*/=" "/' $(DSTROOT)$(CONFIGTOOL).old > $(DSTROOT)$(CONFIGTOOL)
	$(CHMOD) 755 $(DSTROOT)$(CONFIGTOOL)
	$(RM) $(DSTROOT)$(CONFIGTOOL).old

install-mibs:
	$(_v) for file in $(MIBFILES); \
	do \
		$(INSTALL_FILE) $${file} $(DSTROOT)$(MIBDIR); \
	done

install-compat:
	$(_v) $(TAR) -C $(DSTROOT)$(USRLIBDIR) -xzf $(SRCROOT)/libs-5.2.1.tar.gz
