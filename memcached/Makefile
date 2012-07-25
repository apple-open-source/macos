##
# Makefile for memcached
##

# Project info
Project	    = memcached
ProjectName = memcached
UserType    = Developer
ToolType    = Library

Configure = $(BuildDirectory)/$(Project)-src/configure

LIBEVENT = $(USRDIR)/local/libevent2

# libevent is installed in a private location
Extra_Configure_Flags = --with-libevent="$(LIBEVENT)"
  #--enable-sasl   # Doesn't build properly; probably just needs a -lsasl or some such
  #--enable-dtrace # Doesn't build properly due to objroot vs. srcdir

# Link libevent in statically because the shared library won't be available
Extra_Environment = LIBS="$(LIBEVENT)/lib/libevent.a"
# Otherwise, we would do this instead
#Extra_LD_Flags = -L"$(USRDIR)/local/libevent/lib" 

# Include common makefile targets for B&I
include $(MAKEFILEPATH)/CoreOS/ReleaseControl/GNUSource.make

#
# Automatic Extract & Patch
#

AEP	       = YES
AEP_ProjVers   = $(Project)-1.4.13
AEP_Filename   = $(AEP_ProjVers).tar.gz
AEP_ExtractDir = $(AEP_ProjVers)
AEP_Patches    = 

lazy_install_source::
	$(_v) if [ ! -f "$(SRCROOT)/$(AEP_ProjVers)" ]; then $(MAKE) extract_source; fi

extract_source::
ifeq ($(AEP),YES)
	@echo "Extracting source for $(Project)...";
	$(_v) $(MKDIR) -p "$(BuildDirectory)";
	$(_v) $(TAR) -C "$(BuildDirectory)" -xzf "$(SRCROOT)/$(AEP_Filename)";
	$(_v) $(RMDIR) "$(BuildDirectory)/$(Project)";
	$(_v) $(MV) "$(BuildDirectory)/$(AEP_ExtractDir)" "$(BuildDirectory)/$(Project)-src";
	$(_v) for patchfile in $(AEP_Patches); do \
	   cd "$(BuildDirectory)/$(Project)-src" && patch -lp0 < "$(SRCROOT)/patches/$${patchfile}"; \
	done;
endif

#
# Open Source Hooey
#

OSV = /usr/local/OpenSourceVersions
OSL = /usr/local/OpenSourceLicenses

install:: install-ossfiles install-man install-launchd

install-ossfiles::
	$(_v) $(INSTALL_DIRECTORY) "$(DSTROOT)/$(OSV)";
	$(_v) $(INSTALL_FILE) "$(SRCROOT)/OpenSource.plist" "$(DSTROOT)/$(OSV)/$(ProjectName).plist";
	$(_v) $(INSTALL_DIRECTORY) "$(DSTROOT)/$(OSL)";
	$(_v) $(INSTALL_FILE) "$(BuildDirectory)/$(Project)-src/COPYING" "$(DSTROOT)/$(OSL)/$(ProjectName).txt";

install-man::
	$(_v) $(INSTALL_FILE) "$(BuildDirectory)/$(Project)-src/doc/memcached.1" "$(DSTROOT)$(MANDIR)/man1/memcached.1";
	$(_v) $(LN) "$(DSTROOT)$(MANDIR)/man1/memcached.1" "$(DSTROOT)$(MANDIR)/man1/memcached-debug.1"

install-launchd::
	$(_v) $(INSTALL_DIRECTORY) "$(DSTROOT)$(NSLIBRARYDIR)/LaunchDaemons";
	$(_v) $(INSTALL_FILE) "$(SRCROOT)/launchd.plist" "$(DSTROOT)$(NSLIBRARYDIR)/LaunchDaemons/com.danga.memcached.plist";
