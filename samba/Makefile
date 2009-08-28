# Project info
Project         := samba/source
UserType        := Administration
ToolType        := Services

GnuNoPatch      ?= NO
GnuNoChown      ?= YES
GnoNoStrip      ?= NO
GnuAfterInstall := install-directories \
		   install-startup-items \
		   install-tools \
		   install-config \
		   install-application-bundle \
		   install-testtools \
		   install-plugins \
		   install-strip

# Neutralise the installsrc target in Common.make. We do our own
# source installation using rsync, which is faster.
CommonNoInstallSource := YES

ifeq ($(GnuNoStrip),YES)
STRIP_X	:= true
else
STRIP_X	:= strip -x
endif

# Note that we use single quotes to stop the shell trying to expand
# anything. We want the Samba makefile to be the one expanding this.
Environment += \
	CONFIG_SITE=$(SRCROOT)/config.site.leopard \
	EXTRA_BIN_PROGS='bin/smbget$$(EXEEXT)' \
	EXTRA_ALL_TARGETS='bin/smbtorture$$(EXEEXT) \
		bin/msgtest$$(EXEEXT) bin/masktest$$(EXEEXT) \
		bin/locktest$$(EXEEXT) bin/locktest2$$(EXEEXT) \
		bin/vfstest$$(EXEEXT)'

build::
	@echo building from `pwd`
	$(_v) $(MKDIR) $(OBJROOT)/tools/prefsync
	( cd tools/prefsync && $(MAKE) \
		SRCROOT=$(SRCROOT)/tools/prefsync \
		OBJROOT=$(OBJROOT)/tools/prefsync \
		SYMROOT=$(SYMROOT) build )
	$(_v) $(MKDIR) $(OBJROOT)/tools/domain-auth
	( cd tools/domain-auth && $(MAKE) \
		SRCROOT=$(SRCROOT)/tools/domain-auth \
		OBJROOT=$(OBJROOT)/tools/domain-auth \
		SYMROOT=$(SYMROOT) build )

# This is a little subtle. We want to use parallel make to speed up the build,
# so we update MAKEFLAGS for the build target. Unfortunately, not all of the
# Samba build has proper dependencies, so we need to prepend a build phase to
# make proto.h and friends.
build:: MAKEFLAGS += -j $(NPROCS)

build:: configure
ifneq ($(GnuNoBuild),YES)
	$(_v) for arch in $(RC_ARCHS) ; do \
		echo "Building $(Project) headers for $$arch..." ;\
		$(MKDIR) $(BuildDirectory)/$$arch/bin && \
		cd $(BuildDirectory)/$$arch && \
			$(MAKE) $(Environment) proto && \
			$(MAKE) $(Environment) pch && \
			$(MAKE) $(Environment) SHOWFLAGS || exit 1; \
	done
endif


quick:
	./scripts/run-samba.sh QUICKLOOKS 2>&1 | tee quick.log

# When the default makefiles support building multiple architectures in
# separate directories with multiple invokations of configure, we
# can go back to using them.
# include $(MAKEFILEPATH)/CoreOS/ReleaseControl/GNUSource.make

include GNUSource.make
include make.common

Extra_CC_Flags += $(Extra_Samba_Flags)

Install_Target := install

# Hook the pre-configure stage in the release-control makefiles.
install_source:: rsync_source patch autogen

rsync_source:
	@echo Installing source for $(Project) using rsync ...
	$(_v) $(MKDIR) $(SRCROOT)
	$(_v) rsync --archive  --cvs-exclude . --exclude=.git $(SRCROOT)

ifeq ($(GnuNoPatch), YES)
patch:
	@echo skipping patch application phase
else
patch:
	@./scripts/apply-patches.sh
endif

autogen:
	cd $(Sources) && ./autogen.sh

norejects:
	@echo removing patch rejects
	@find . -name \*.rej | xargs rm -f

LAUNCH_DAEMONS := $(DSTROOT)/System/Library/LaunchDaemons
SYSTEM_CONFIGURATION := $(DSTROOT)/Library/Preferences/SystemConfiguration
OPENSOURCE_VERSIONS := $(DSTROOT)/usr/local/OpenSourceVersions
DIRECTORY_PREFERENCES := $(DSTROOT)/Library/Preferences/DirectoryService

install-directories:
	$(INSTALL) -d -m 755 $(LAUNCH_DAEMONS)
	$(INSTALL) -d -m 755 $(OPENSOURCE_VERSIONS)
	$(INSTALL) -d -m 755 $(SYSTEM_CONFIGURATION)
	$(INSTALL) -d -m 755 $(DSTROOT)$(SMB_CONFDIR)
	$(INSTALL) -d -m 755 $(DSTROOT)$(ETCDIR)/pam.d
	$(INSTALL) -d -m 755 $(DSTROOT)$(SMB_LOGDIR)
	$(INSTALL) -d -m 755 $(DSTROOT)$(SMB_LIBEXEC)
	$(INSTALL) -d -m 755 $(DSTROOT)$(CUPS_BACKEND)
	$(INSTALL) -d -m 755 -o root -g admin \
		$(DIRECTORY_PREFERENCES)
	$(INSTALL) -d -m 700 -o root -g wheel \
		$(DSTROOT)$(SMB_LOCKDIR)/winbindd_privileged
	$(INSTALL) -d -m 755 -o root -g wheel \
		$(DSTROOT)$(SMB_LOCKDIR)/winbindd_public

# Install all the launchd control files. The smbd and nmbd files
# must not have the org.samba prefix because these were not uses on
# 10.4. It's not worth the effort to track down and change everything
# that manipulates these just for the cosmetic benefit of consistent
# naming :(
# XXX Once all the UI tools start using the SMB preferences, we can do
# this rename.
install-startup-items:
	$(INSTALL) -c -m 644 -o root -g wheel \
		$(SRCROOT)/org.samba.smbd.plist \
		$(LAUNCH_DAEMONS)/smbd.plist
	$(INSTALL) -c -m 644 -o root -g wheel \
		$(SRCROOT)/org.samba.nmbd.plist \
		$(LAUNCH_DAEMONS)/nmbd.plist
	$(INSTALL) -c -m 644 -o root -g wheel \
		$(SRCROOT)/org.samba.winbindd.plist \
		$(LAUNCH_DAEMONS)
	$(INSTALL) -c -m 444 -o root -g wheel \
		$(SRCROOT)/tools/com.apple.smb.server.preferences.plist \
		$(LAUNCH_DAEMONS)
	$(INSTALL) -c -m 444 -o root -g wheel \
		$(SRCROOT)/tools/com.apple.smb.sharepoints.plist \
		$(LAUNCH_DAEMONS)

CORE_SERVICES := $(DSTROOT)$(NSLIBRARYDIR)/CoreServices

# Create an application bundle in CoreServices. This should give UI preference
# clients a convenient place to find all out public interfaces.
SMB_BUNDLE := SmbFileServer.bundle
install-application-bundle:
	$(INSTALL) -d -m 755 $(CORE_SERVICES)
	INSTALL="$(INSTALL)" $(SRCROOT)/scripts/create-bundle.sh \
			$(SMB_BUNDLE) $(CORE_SERVICES)
	$(INSTALL) -c -m 644 $(SRCROOT)/config/DesktopDefaults.plist \
		$(CORE_SERVICES)/$(SMB_BUNDLE)/Resources
	$(INSTALL) -c -m 644 $(SRCROOT)/config/ServerDefaults.plist \
		$(CORE_SERVICES)/$(SMB_BUNDLE)/Resources

# Install the config files. Use -b to preserve an existing config as
# smb.conf.old.
install-config:
	$(INSTALL) -c -m 644 -o root -g wheel \
		$(SRCROOT)/config/smb.conf.template $(DSTROOT)$(SMB_CONFDIR)
	$(INSTALL) -b -c -m 644 -o root -g wheel \
		$(SRCROOT)/config/smb.conf.template \
		$(DSTROOT)$(SMB_CONFDIR)/smb.conf
	$(INSTALL) -b -c -m 644 -o root -g wheel \
		$(SRCROOT)/config/samba.pam \
		$(DSTROOT)/$(ETCDIR)/pam.d/samba
	$(INSTALL) -c -m 644 -o root -g wheel \
		$(SRCROOT)/samba.plist $(OPENSOURCE_VERSIONS)

install-tools:
	( cd tools/prefsync && $(MAKE) \
	  	NoSymRootCopy=YES \
		SRCROOT=$(SRCROOT)/tools/prefsync \
		OBJROOT=$(OBJROOT)/tools/prefsync \
		SYMROOT=$(SYMROOT) \
		DSTROOT=$(DSTROOT) \
		install )
	( cd tools/domain-auth && $(MAKE) \
	  	NoSymRootCopy=YES \
		SRCROOT=$(SRCROOT)/tools/domain-auth \
		OBJROOT=$(OBJROOT)/tools/domain-auth \
		SYMROOT=$(SYMROOT) \
		DSTROOT=$(DSTROOT) \
		install )
	$(INSTALL) -c -m 600 -o root -g wheel \
		$(SRCROOT)/tools/mutex \
		$(DSTROOT)$(SMB_LOCKDIR)/shares.mutex
	$(INSTALL) -c -m 600 -o root -g wheel \
		$(SRCROOT)/tools/mutex \
		$(DSTROOT)$(SMB_LOCKDIR)/config.mutex
	$(INSTALL) -c -m 755 -o root -g wheel \
		$(SRCROOT)/tools/migrate-preferences \
		$(DSTROOT)$(SMB_LIBEXEC)
	$(INSTALL) -c -m 755 -o root -g wheel \
		$(SRCROOT)/tools/smb-conf-upgrade \
		$(DSTROOT)$(SMB_LIBEXEC)
	$(INSTALL) -c -m 500 -o root -g wheel \
		$(DSTROOT)/usr/bin/smbspool \
		$(DSTROOT)/$(CUPS_BACKEND)/smb
	$(INSTALL) -c -m 755 -o root -g wheel \
		$(SRCROOT)/tools/smb-sharepoints \
		$(DSTROOT)$(SMB_LIBEXEC)

NOSHIP_MANPAGES := \
	man8/smbmnt.8 \
	man8/smbmount.8 \
	man8/smbumount.8 \
	man8/mount.cifs.8 \
	man8/umount.cifs.8 \
	man8/swat.8 \
	man1/log2pcap.1 \
	man1/vfstest.1 \
	man7/libsmbclient.7

install-strip:
	( \
	  cd $(DSTROOT); \
	  find . -type f | while read f ; do \
		$(MKDIR) $(SYMROOT)/`dirname "$$f"` ; \
		cp "$$f" "$(SYMROOT)/$$f" ; \
		if [ -x "$$f" ] ; then \
			dsymutil "$(SYMROOT)/$$f" || true ; \
			$(STRIP_X) "$$f" || true ; \
		fi ; \
		case "$$f" in *.dylib) \
			dsymutil "$(SYMROOT)/$$f" || true ; \
			$(STRIP_X) "$$f"  || true ; \
			;; \
		esac \
	  done ; \
	)
	rmdir $(DSTROOT)$(SMB_PIDDIR) || true
	# Blow away man pages for things that won't exist for us.
	rm -f $(patsubst %,$(DSTROOT)$(SMB_MANDIR)/%,$(NOSHIP_MANPAGES))
	# Rename the winbind PAM module to match the other PAM modules
	( cd $(DSTROOT)$(SMB_PAMDIR) && mv pam_winbind.dylib pam_winbind.so)
	# Blow away anything that was renamed by the Samba install rules
	find $(DSTROOT) -name \*.old | xargs rm
	# The Samba build system bogusly creates this
	rmdir $(DSTROOT)/usr/var
	# We don't want to support any developer APIs, so blow away
	# all the installed headers.
	rm -rf $(DSTROOT)/usr/include

RC_OBJROOTS := $(addprefix $(OBJROOT)/, $(RC_ARCHS))

TESTTOOLS := smbtorture masktest vfstest msgtest locktest locktest2

install-testtools:
ifneq ($(GnuNoInstall),YES)
	$(_v) $(INSTALL) -d -m 755 $(DSTROOT)/usr/local/bin ; \
	$(_v) for tool in $(TESTTOOLS) ; do \
		$(_v) lipo -create -output $(DSTROOT)/usr/local/bin/$$tool \
			$(addsuffix /bin/$$tool, $(RC_OBJROOTS)) && \
		$(CHOWN) root:wheel $(DSTROOT)/usr/local/bin/$$tool && \
		$(CHMOD) 555 $(DSTROOT)/usr/local/bin/$$tool || exit 1; \
	done
endif

install-plugins: auth-opendirectory pdb-opendirectory

# Install compatibility symlinks for Open Directory AUTH module.
auth-opendirectory:
	@echo "installing $@";
	(cd $(DSTROOT)$(SMB_LIBDIR)/auth && \
	 	ln -s odsam.dylib opendirectory.dylib)

# Install compatibility symlinks for Open Directory SAM module.
pdb-opendirectory:
	@echo "installing $@";
	(cd $(DSTROOT)$(SMB_LIBDIR)/pdb && \
	 	ln -s odsam.dylib opendirectorysam.dylib)

# vim: set sw=8 ts=8 noet tw=0 :
