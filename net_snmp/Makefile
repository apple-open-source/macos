##
# Makefile for net-snmp
##

# General project info
Project		= net-snmp
ProjectName	= net_snmp
UserType	= Administration
ToolType	= Commands
Submission	= 167


#
# Settings for the net-snmp project.
#
########################
#
# NOTE: when shipping library updates instead of the full package, you must also ship the updated
#       /usr/include/net-snmp/net-snmp-config.h
#
########################
#
# Justification for #defines:
# CONFORMANCE is to avoid confusing configure
# UNSTABLE is required to pick up some other required structs
# PrivateHeaders is so IPv6 can define struct ifnet.
###
# Disabled while trying to get build stuff working
###

###
# Enable text-based stub generation per default.
##
GENERATE_TEXT_BASED_STUBS ?= YES
GENERATE_TEXT_BASED_STUBS_UPPER := $(shell echo $(GENERATE_TEXT_BASED_STUBS) | tr a-z A-Z)
 
STUBIFY = $(if $(filter 1 TRUE ON YES,$(GENERATE_TEXT_BASED_STUBS_UPPER)),YES,NO)
$(info GENERATE_TEXT_BASED_STUBS=$(STUBIFY))

########################
# Allow selecting the compiler to be used
########################
SDKROOT ?=/
CC := $(shell xcrun -sdk $(SDKROOT) -f cc)
CXX := $(shell xcrun -sdk $(SDKROOT) -f c++)
TAPI := $(shell xcrun -sdk $(SDKROOT) -f tapi)
CLFAGS += -isysroot $(SDKROOT)
CXXFLAGS += -isysroot $(SDKROOT)
########################
# for bug 8862468
CFLAGS += -DNETSNMP_NO_INLINE

DEFINES			= -DBUILD=$(Submission) \
			-DMACOSX_DEPLOYMENT_TARGET=$(MACOSX_DEPLOYMENT_TARGET)
INCLUDES		= -F$(SDKROOT)/System/Library/PrivateFrameworks/ -F$(SDKROOT)/System/Library/Frameworks/ -I$(SDKROOT)/usr/local/libressl-2.2/include

# For Perl to build correctly, both CFLAGS (CC_Flags) and CCFLAGS (Cxx_Flags)
# must be properly defined.
Extra_CC_Flags		= $(DEFINES) $(INCLUDES)
Extra_Cxx_Flags		= $(Extra_CC_Flags)
# also need ARCHFLAGS
Default_ARCHFLAGS = -arch i386  -arch x86_64
ifneq "$(RC_CFLAGS)" ""
	ARCHFLAGS=$(RC_CFLAGS)
else
	ARCHFLAGS=$(Default_ARCHFLAGS)
endif

Extra_Configure_Flags	= --sysconfdir=/etc \
			--with-install-prefix=$(DSTROOT) \
			--with-default-snmp-version=2 \
			--with-persistent-directory=/var/db/net-snmp \
			--with-defaults \
			--without-rpm \
			--with-sys-contact="postmaster@example.com" \
			--with-mib-modules="host ucd-snmp/diskio ucd-snmp/loadave " \
			--disable-static \
			--enable-ipv6 \
			--with-perl-modules \
			--disable-perl-cc-checks \
			--disable-embedded-perl  \
			--datarootdir=/usr/share \
			--with-openssl=/usr/libressl/ \
			--without-kmem-usage

# ucd-snmp/lmSensorsTables

# Old / unused configure flags
#			--enable-mini-agent \
#			--with-mib-modules="hardware/memory hardware/cpu host ucd-snmp/diskio ucd-snmp/loadave ucd-snmp/memory" \
#			--with-out-mib-modules="mibII/icmp host/hr_swrun" \
#			--enable-ipv6 \
#			--enable-developer
#			--with-libwrap=/usr/lib/libwrap.dylib
#			--disable-embedded-perl  \
#			--datadir=/etc \



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
Extra_Environment	= AR="$(SRCROOT)/ar.sh" INC="-I../../include -I../../../include"
GnuAfterInstall		= install-macosx install-mibs install-compat
ifeq ($(STUBIFY),YES)
GnuAfterInstall	+= stubify
endif
#GnuAfterInstall         = install-macosx install-mibs	# do not include the compatibility libs

# Temporarily set for development
GnuNoInstallHeaders	= YES

# Binaries to strip
STRIPPED_BINS	= encode_keychange snmpbulkget snmpbulkwalk snmpdelta snmpdf \
			snmpget snmpgetnext snmpinform snmpnetstat \
			snmpset snmpstatus snmptable snmptest snmptranslate \
			snmptrap snmpusm snmpvacm snmpwalk agentxtrap
STRIPPED_SBINS	= snmpd snmptrapd
#STRIPPED_SNMPTRAPD	= snmptrapd
STRIPPED_LIBS	= libnetsnmp libnetsnmpagent libnetsnmphelpers libnetsnmpmibs libnetsnmptrapd

# Binaries to Lipo
LIPO_BINS = $(STRIPPED_BINS)
LIPO_SBINS = $(STRIPPED_SBINS)

# Binary to patch
CONFIGTOOL	= $(USRBINDIR)/net-snmp-config

# fixup tool for the above
FIXUP	= $(SRCROOT)/fixupConfig

# MIB files to install
MIBFILES	:= $(wildcard mibs/*.txt)
MIBDIR		= $(SHAREDIR)/snmp/mibs

# full patch list 
#AEP_Patches    = diskio.patch IPv6.patch universal_builds.patch \
#                        cache.patch container.patch darwin-header.patch \
#                        dir_utils.patch disk.patch host.patch 6581429.patch \
#                        lmsensors.patch darwin-sensors.patch swinst.patch swrun.patch \
#                        system.patch table.patch darwin64.patch perl-cc.patch

# Automatic Extract & Patch
AEP		= YES
AEP_Version	= 5.6.2.1
AEP_Patches    = diskio.patch IPv6.patch universal_builds.patch \
			container.patch darwin-header.patch 10268440.patch \
			host.patch CVE-2012-6151.patch CVE-2014-3565.patch \
			lmsensors.patch darwin-sensors.patch sanitize.patch 34748495.patch 34782045.patch 34782700.patch \
			34806284.patch warnings1.patch crTest.patch \
			darwin64.patch disk_label.patch 22291336.patch perl-cc.patch namespace.patch 
AEP_LaunchdConfigs	= org.net-snmp.snmpd.plist
AEP_ConfigDir	= $(ETCDIR)/snmp
AEP_ConfigFiles	= snmpd.conf


# Local targets that must be defined before including the following
# files to get the dependency order correct
.PHONY: $(GnuAfterInstall)

# added for rdar://problem/40039320
# if AEP_Version above is changed, update the tarfile names in the code below as well
installsrc: 
	# open up the archive
	$(_v) $(TAR) xzf  $(SRCROOT)/net-snmp-5.6.2.1.tar.gz -C $(SRCROOT)
	$(_v) $(MV) $(SRCROOT)/net-snmp-5.6.2.1.tar.gz $(SRCROOT)/net-snmp-5.6.2.1.tar.gz.old
	# trim the archive to remove the forbidden files
	$(_v) $(RM) $(SRCROOT)/net-snmp-5.6.2.1/perl/TrapReceiver/README
	$(_v) $(RM) $(SRCROOT)/net-snmp-5.6.2.1/snmplib/MSG00001.bin
	# close it back up	
	$(_v) $(TAR) czf  $(SRCROOT)/net-snmp-5.6.2.1.tar.gz -C $(SRCROOT) ./net-snmp-5.6.2.1 
	# cleanup
	$(_v) $(RM) -rf $(SRCROOT)/net-snmp-5.6.2.1
	$(_v) $(RM) $(SRCROOT)/net-snmp-5.6.2.1.tar.gz.old

install::

# Include common makefile targets for B&I
include $(MAKEFILEPATH)/CoreOS/ReleaseControl/GNUSource.make
include ./AEP.make

# Override settings from above include
ifndef MACOSX_DEPLOYMENT_TARGET
	MACOSX_DEPLOYMENT_TARGET = $(shell sw_vers -productVersion | cut -d. -f1-2)
endif

###
# Enable ASAN for debugging
#     must then run the perl executables with a special dylib using DYLD_INSERT_LIBRARIES with the path from
#     echo $(xcrun -sdk iphoneos clang -print-file-name=lib)/darwin/libclang_rt.asan_osx_dynamic.dylib
#

# build using buildit with the  -useAddressSanitization  flag
#
ifdef RC_ENABLE_ADDRESS_SANITIZATION
LD_FLAGS += -fsanitize=address
CLFLAGS += -fsanitize=address
CFLAGS += -fsanitize=address
LTCFLAGS = -fsanitize=address
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
	$(_v) for file in $(STRIPPED_LIBS); \
	do \
		$(CP) $(DSTROOT)$(USRLIBDIR)/$${file}*.dylib $(SYMROOT); \
		$(STRIP) -x $(DSTROOT)$(USRLIBDIR)/$${file}.dylib; \
	done
	$(_v)- $(FIND) $(DSTROOT)$(NSLIBRARYSUBDIR)/Perl -type f -name '*.bundle' -print -exec strip -S {} \;
ifeq "$(RC_i386)" "YES"
	@echo "Removing 32-bit executable code from binaries..."
	$(_v) for file in $(LIPO_BINS); \
	do \
		$(LIPO) -thin x86_64 $(DSTROOT)$(USRBINDIR)/$${file} -output $(DSTROOT)$(USRBINDIR)/$${file}.64; \
		$(RM) $(DSTROOT)$(USRBINDIR)/$${file};\
		$(MV) $(DSTROOT)$(USRBINDIR)/$${file}.64 $(DSTROOT)$(USRBINDIR)/$${file}; \
	done
	$(_v) for file in $(LIPO_SBINS); \
	do \
		$(LIPO) -thin x86_64 $(DSTROOT)$(USRSBINDIR)/$${file} -output $(DSTROOT)$(USRSBINDIR)/$${file}.64; \
		$(RM) $(DSTROOT)$(USRSBINDIR)/$${file};\
		$(MV) $(DSTROOT)$(USRSBINDIR)/$${file}.64 $(DSTROOT)$(USRSBINDIR)/$${file}; \
	done
endif
	@echo "Copying sensor data"
	$(_v) $(MKDIR) -p $(DSTROOT)$(SHAREDIR)/snmp
	$(_v) $(INSTALL_FILE) $(SRCROOT)/SensorDat.xml $(DSTROOT)$(SHAREDIR)/snmp
	@echo "Fixing permissions..."
	$(_v) $(FIND) $(DSTROOT)$(USRINCLUDEDIR)/net-snmp -type f -exec chmod 644 {} \;
	$(_v) $(FIND) $(DSTROOT)$(SHAREDIR)/snmp -type f -exec chmod 644 {} \;
	$(_v) $(RM) $(DSTROOT)$(USRLIBDIR)/*.a $(DSTROOT)$(USRLIBDIR)/*.la
	$(_v) $(FIND) $(DSTROOT)$(MANDIR) -type f -exec chmod 644 {} \;
	@echo "setting up snmpd proxy"
	$(_v) $(MKDIR) -p $(DSTROOT)/usr/libexec/
	$(_v) $(MV) $(DSTROOT)$(USRSBINDIR)/snmpd $(DSTROOT)/usr/libexec/snmpd
	$(_v) $(CP) $(SRCROOT)/snmpd-proxy $(DSTROOT)$(USRSBINDIR)/snmpd
	$(_v) $(CHOWN) root:wheel $(DSTROOT)$(USRSBINDIR)/snmpd
	@echo "Removing perllocal.pod..."
	$(_v) $(RM) -rf "$(DSTROOT)/System/Library/Perl"
	@echo "Eliminating architecture flags from $(CONFIGTOOL)..."
	$(MV) $(DSTROOT)$(CONFIGTOOL) $(DSTROOT)$(CONFIGTOOL).1
	$(SED) -Ee 's/-arch [-_a-z0-9]{3,10}//g' \
		-e '/^NSC_INCLUDEDIR=/s/=.*/=\/usr\/local\/include/' \
		-e '/^NSC_LIBDIR=/s/=.*/=" "/' $(DSTROOT)$(CONFIGTOOL).1 > $(DSTROOT)$(CONFIGTOOL).2
	$(FIXUP) $(DSTROOT)$(CONFIGTOOL).2 > $(DSTROOT)$(CONFIGTOOL)
	$(CHMOD) 755 $(DSTROOT)$(CONFIGTOOL)
	$(RM) $(DSTROOT)$(CONFIGTOOL).1
	$(RM) $(DSTROOT)$(CONFIGTOOL).2

install-mibs:
	$(_v) for file in $(MIBFILES); \
	do \
		$(INSTALL_FILE) $${file} $(DSTROOT)$(MIBDIR); \
	done


stubify: install-macosx
	@echo "Generating TBD files ..."
	$(_v) $(TAPI) stubify $(DSTROOT)


