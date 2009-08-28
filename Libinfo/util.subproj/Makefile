Project = util
ProductType = staticlib
Install_Dir = /scratch
BuildDebug = YES
BuildProfile = YES

CFILES = hton.c putpwpasswd.c rcmd.c rcmdsh.c
MANPAGES = rcmd.3 hosts.equiv.5

Extra_CC_Flags = -Wall -fno-common -I. \
	-I../dns.subproj -I../gen.subproj -I../lookup.subproj \
	-D__DARWIN_NON_CANCELABLE=1 -DINET6=1

include $(MAKEFILEPATH)/CoreOS/ReleaseControl/BSDCommon.make

after_install:
	@for LINK in iruserok.3 iruserok_sa.3 rcmd_af.3 rresvport.3 \
		rresvport_af.3 ruserok.3 ; do \
			$(LN) -f "$(DSTROOT)/usr/share/man/man3/rcmd.3" \
			"$(DSTROOT)/usr/share/man/man3/$${LINK}" ; \
	done
	$(LN) -f "$(DSTROOT)/usr/share/man/man5//hosts.equiv.5" \
		"$(DSTROOT)/usr/share/man/man5/.rhosts.5"
