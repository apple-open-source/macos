##
# Makefile for tcp_wrappers
##
MAJOR=7
MINOR=6

# Project info

Project             = tcp_wrappers
UserType            = Developer
ToolType            = Commands
Extra_CC_Flags      = -fno-common
Extra_Environment   = REAL_DAEMON_DIR=/usr/libexec	\
		      STYLE=-DPROCESS_OPTIONS		\
		      FACILITY=LOG_REMOTEAUTH		\
		      ALLOW_SEVERITY=LOG_INFO		\
		      DENY_SEVERITY=LOG_WARNING		\
		      PARANOID=				\
		      HOSTNAME=				\
		      BUGS=				\
		      MAJOR=$(MAJOR)			\
		      MINOR=$(MINOR)

include $(MAKEFILEPATH)/CoreOS/ReleaseControl/Common.make

install::
	$(INSTALL_DIRECTORY) $(DSTROOT)$(LIBEXECDIR)
	$(INSTALL_DIRECTORY) $(DSTROOT)$(USRSBINDIR)
	$(INSTALL_DIRECTORY) $(DSTROOT)/usr/lib
	$(INSTALL_DIRECTORY) $(DSTROOT)/usr/include
	$(INSTALL_DIRECTORY) $(DSTROOT)/usr/share/man/man3
	$(INSTALL_DIRECTORY) $(DSTROOT)$(MANDIR)/man5
	$(INSTALL_DIRECTORY) $(DSTROOT)$(MANDIR)/man8
	$(INSTALL_PROGRAM)  $(BuildDirectory)/$(Project)/tcpd        $(DSTROOT)$(LIBEXECDIR)
	$(INSTALL_PROGRAM)  $(BuildDirectory)/$(Project)/tcpdchk     $(DSTROOT)$(USRSBINDIR)
	$(INSTALL_PROGRAM)  $(BuildDirectory)/$(Project)/tcpdmatch   $(DSTROOT)$(USRSBINDIR)
	#$(INSTALL_PROGRAM) $(BuildDirectory)/$(Project)/try-from    $(DSTROOT)$(LIBEXECDIR)
	#$(INSTALL_PROGRAM) $(BuildDirectory)/$(Project)/safe_finger $(DSTROOT)$(LIBEXECDIR)
	$(INSTALL_FILE)     $(BuildDirectory)/$(Project)/libwrap.a   $(DSTROOT)/usr/lib
	ranlib $(DSTROOT)/usr/lib/libwrap.a
	strip -S $(DSTROOT)/usr/lib/libwrap.a
	$(INSTALL_FILE)     $(BuildDirectory)/$(Project)/libwrap.$(MAJOR).dylib   $(DSTROOT)/usr/lib
	strip -x $(DSTROOT)/usr/lib/libwrap.$(MAJOR).dylib
	ln -fs libwrap.$(MAJOR).dylib $(DSTROOT)/usr/lib/libwrap.$(MAJOR).$(MINOR).dylib
	ln -fs libwrap.$(MAJOR).dylib $(DSTROOT)/usr/lib/libwrap.dylib
	$(INSTALL_FILE) -c $(Sources)/$(Project)/tcpd.h          $(DSTROOT)/usr/include
	$(INSTALL_FILE) -c $(Sources)/$(Project)/hosts_access.3  $(DSTROOT)/usr/share/man/man3
	ln -fs hosts_access.3 $(DSTROOT)/usr/share/man/man3/hosts_ctl.3
	ln -fs hosts_access.3 $(DSTROOT)/usr/share/man/man3/request_init.3
	ln -fs hosts_access.3 $(DSTROOT)/usr/share/man/man3/request_set.3
	$(INSTALL_FILE) -c $(Sources)/$(Project)/hosts_access.5  $(DSTROOT)$(MANDIR)/man5
	$(INSTALL_FILE) -c $(Sources)/$(Project)/hosts_options.5  $(DSTROOT)$(MANDIR)/man5
	$(INSTALL_FILE) -c $(Sources)/$(Project)/tcpd.8          $(DSTROOT)$(MANDIR)/man8
	$(INSTALL_FILE) -c $(Sources)/$(Project)/tcpdchk.8       $(DSTROOT)$(MANDIR)/man8
	$(INSTALL_FILE) -c $(Sources)/$(Project)/tcpdmatch.8     $(DSTROOT)$(MANDIR)/man8
	@for binary in libexec/tcpd sbin/tcpdchk sbin/tcpdmatch; do \
		for arch in ppc64 x86_64; do \
			file=$(DSTROOT)/usr/$$binary; \
			echo lipo -remove $$arch -output $$file $$file; \
			lipo -remove $$arch -output $$file $$file || true; \
		done \
	done

build:: shadow_source
	$(_v) $(MAKE) -C $(BuildDirectory)/$(Project) $(Environment) macos
