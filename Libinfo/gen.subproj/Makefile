Project = gen
ProductType = staticlib
Install_Dir = /scratch
BuildDebug = YES
BuildProfile = YES

CFILES = ether_addr.c getifaddrs.c \
	if_indextoname.c if_nameindex.c if_nametoindex.c \
	inet_ntop.c inet_pton.c map_v4v6.c ip6opt.c rthdr.c vars.c \
	$(OBJROOT)/_version.c

MANPAGES = getifaddrs.3 if_indextoname.3 inet6_rthdr_space.3 \
	gethostbyname.3 inet6_option_space.3 \
	getprotoent.3 gethostbyname.3 getipnodebyname.3 \
	getnetent.3 getservent.3 inet6_option_space.3

Install_Headers = ifaddrs.h

Extra_CC_Flags = -Wall -fno-common -I. \
	-I../dns.subproj -I../lookup.subproj \
	-D__DARWIN_NON_CANCELABLE=1 -DINET6=1
	

include $(MAKEFILEPATH)/CoreOS/ReleaseControl/BSDCommon.make

$(OBJROOT)/_version.c:
	/Developer/Makefiles/bin/version.pl Libinfo > $@

after_install:
	@for LINK in endhostent.3 gethostbyaddr.3 gethostbyname2.3 \
		gethostent.3 herror.3 hstrerror.3 sethostent.3 ; do \
		$(LN) -f "$(DSTROOT)/usr/share/man/man3/gethostbyname.3" \
			"$(DSTROOT)/usr/share/man/man3/$${LINK}" ; \
	done
	$(LN) -f "$(DSTROOT)/usr/share/man/man3/getifaddrs.3" \
		"$(DSTROOT)/usr/share/man/man3/freeifaddrs.3"
	@for LINK in freehostent.3 getipnodebyaddr.3 ; do \
		$(LN) -f "$(DSTROOT)/usr/share/man/man3/getipnodebyname.3" \
			"$(DSTROOT)/usr/share/man/man3/$${LINK}" ; \
	done
	@for LINK in endnetent.3 getnetbyaddr.3 getnetbyname.3 \
		setnetent.3 ; do \
		$(LN) -f "$(DSTROOT)/usr/share/man/man3/getnetent.3" \
			"$(DSTROOT)/usr/share/man/man3/$${LINK}" ; \
	done
	@for LINK in endprotoent.3 getprotobyname.3 getprotobynumber.3 \
		setprotoent.3 ; do \
		$(LN) -f "$(DSTROOT)/usr/share/man/man3/getprotoent.3" \
			"$(DSTROOT)/usr/share/man/man3/$${LINK}" ; \
	done
	@for LINK in endservent.3 getservbyname.3 getservbyport.3 \
		setservent.3  ; do \
		$(LN) -f "$(DSTROOT)/usr/share/man/man3/getservent.3" \
			"$(DSTROOT)/usr/share/man/man3/$${LINK}" ; \
	done
	@for LINK in if_freenameindex.3 if_nameindex.3 if_nametoindex.3 ; do \
		$(LN) -f "$(DSTROOT)/usr/share/man/man3/if_indextoname.3" \
			"$(DSTROOT)/usr/share/man/man3/$${LINK}" ; \
	done
	@for LINK in inet6_option_alloc.3 inet6_option_append.3 \
		inet6_option_find.3 inet6_option_init.3 inet6_option_next.3 ; do \
		$(LN) -f "$(DSTROOT)/usr/share/man/man3/inet6_option_space.3" \
			"$(DSTROOT)/usr/share/man/man3/$${LINK}" ; \
	done
	@for LINK in inet6_rthdr_add.3 inet6_rthdr_getaddr.3 \
		inet6_rthdr_getflags.3 inet6_rthdr_init.3 \
		inet6_rthdr_lasthop.3 inet6_rthdr_reverse.3 \
		inet6_rthdr_segments.3 ; do \
		$(LN) -f "$(DSTROOT)/usr/share/man/man3/inet6_rthdr_space.3" \
			"$(DSTROOT)/usr/share/man/man3/$${LINK}" ; \
	done
