##
# Makefile for ntp
##

# Project info
Project           = ntp
UserType          = Administration
ToolType          = Services
Extra_Environment = AUTOCONF="$(Sources)/missing autoconf"	\
                    AUTOHEADER="$(Sources)/missing autoheader"	\
                    ac_cv_decl_syscall=no			\
                    ac_cv_header_netinfo_ni_h=no		\
                    LIBMATH=""
GnuAfterInstall   = install-man-pages rm-tickadj

# It's a GNU Source project
include $(MAKEFILEPATH)/CoreOS/ReleaseControl/GNUSource.make

MANPAGES = man/ntp-genkeys.8 \
	man/ntpd.8 \
	man/ntpdate.8 \
	man/ntpdc.8 \
	man/ntpq.8 \
	man/ntptime.8 \
	man/ntptrace.8

install-man-pages:
	install -d $(DSTROOT)/usr/share/man/man8
	install -c -m 444 $(SRCROOT)/$(MANPAGES) $(DSTROOT)/usr/share/man/man8/

# since the configure script is totally *broken*
# w/ regards to enabling/disabling tickadj, we remove it
# after-the-fact, by hand.
rm-tickadj:
	rm $(DSTROOT)/usr/sbin/tickadj
