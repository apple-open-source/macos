##
# Makefile for tcp_wrappers
##

# Project info
Project             = tcp_wrappers
UserType            = Developer
ToolType            = Commands
Extra_Environment   = REAL_DAEMON_DIR=/usr/libexec	\
		      STYLE=-DPROCESS_OPTIONS		\
		      FACILITY=LOG_REMOTEAUTH		\
		      ALLOW_SEVERITY=LOG_INFO		\
		      DENY_SEVERITY=LOG_WARNING		\
		      PARANOID=				\
		      HOSTNAME=				\
		      BUGS=

include $(MAKEFILEPATH)/CoreOS/ReleaseControl/Common.make

install::
	$(INSTALL_DIRECTORY) $(DSTROOT)$(LIBEXECDIR)
	$(INSTALL_DIRECTORY) $(DSTROOT)$(USRSBINDIR)
	$(INSTALL_DIRECTORY) $(DSTROOT)/usr/local/lib
	$(INSTALL_DIRECTORY) $(DSTROOT)/usr/local/include
	$(INSTALL_DIRECTORY) $(DSTROOT)/usr/local/share/man/man3
	$(INSTALL_DIRECTORY) $(DSTROOT)$(MANDIR)/man5
	$(INSTALL_DIRECTORY) $(DSTROOT)$(MANDIR)/man8
	$(INSTALL_PROGRAM)  $(BuildDirectory)/$(Project)/tcpd        $(DSTROOT)$(LIBEXECDIR)
	$(INSTALL_PROGRAM)  $(BuildDirectory)/$(Project)/tcpdchk     $(DSTROOT)$(USRSBINDIR)
	$(INSTALL_PROGRAM)  $(BuildDirectory)/$(Project)/tcpdmatch   $(DSTROOT)$(USRSBINDIR)
	#$(INSTALL_PROGRAM) $(BuildDirectory)/$(Project)/try-from    $(DSTROOT)$(LIBEXECDIR)
	#$(INSTALL_PROGRAM) $(BuildDirectory)/$(Project)/safe_finger $(DSTROOT)$(LIBEXECDIR)
	$(INSTALL_FILE)     $(BuildDirectory)/$(Project)/libwrap.a   $(DSTROOT)/usr/local/lib
	ranlib $(DSTROOT)/usr/local/lib/libwrap.a
	$(INSTALL_FILE) -c $(Sources)/$(Project)/tcpd.h          $(DSTROOT)/usr/local/include
	$(INSTALL_FILE) -c $(Sources)/$(Project)/hosts_access.3  $(DSTROOT)/usr/local/share/man/man3
	$(INSTALL_FILE) -c $(Sources)/$(Project)/hosts_access.5  $(DSTROOT)$(MANDIR)/man5
	$(INSTALL_FILE) -c $(Sources)/$(Project)/tcpd.8          $(DSTROOT)$(MANDIR)/man8
	$(INSTALL_FILE) -c $(Sources)/$(Project)/tcpdchk.8       $(DSTROOT)$(MANDIR)/man8
	$(INSTALL_FILE) -c $(Sources)/$(Project)/tcpdmatch.8     $(DSTROOT)$(MANDIR)/man8

build:: shadow_source
	$(_v) $(MAKE) -C $(BuildDirectory)/$(Project) $(Environment) macos
