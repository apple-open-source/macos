#	from @(#)Makefile.inc	8.2 (Berkeley) 9/5/93
# $FreeBSD: src/lib/libc/net/Makefile.inc,v 1.43 2001/10/23 06:22:14 imp Exp $

# machine-independent net sources
.PATH: ${.CURDIR}/${MACHINE_ARCH}/net ${.CURDIR}/net

SRCS+=  inet_addr.c      inet_netof.c     \
	inet_lnaof.c     inet_network.c   linkaddr.c       nsap_addr.c \
	inet_makeaddr.c  inet_ntoa.c      recv.c


CFLAGS+=-DINET6 -I${.OBJDIR}

YFLAGS+=-p_nsyy
LFLAGS+=-P_nsyy

CLEANFILES+=nsparser.c nslexer.c nsparser.h

nsparser.h: nsparser.c
	mv y.tab.h ${.TARGET}

nslexer.c: nslexer.l nsparser.h
	${LEX} ${LFLAGS} -o/dev/stdout ${.IMPSRC} | \
		sed -e '/YY_BUF_SIZE/s/16384/1024/' >${.TARGET}

# machine-dependent net sources
.if exists(${.CURDIR}/${MACHINE_ARCH}/net/Makefile.inc)
.include "${.CURDIR}/${MACHINE_ARCH}/net/Makefile.inc"
.endif

.if ${LIB} == "c"
MAN3+=	addr2ascii.3 byteorder.3 ethers.3 hesiod.3 \
	inet.3 inet_net.3 \
	nsdispatch.3 rcmdsh.3 resolver.3
# not installed: iso_addr.3 ns.3

MLINKS+=addr2ascii.3 ascii2addr.3
MLINKS+=byteorder.3 htonl.3 byteorder.3 htons.3 byteorder.3 ntohl.3 \
	byteorder.3 ntohs.3
MLINKS+=ethers.3 ether_aton.3 ethers.3 ether_hostton.3 ethers.3 ether_line.3 \
	ethers.3 ether_ntoa.3 ethers.3 ether_ntohost.3
MLINKS+=inet.3 addr.3 inet.3 inet_addr.3 inet.3 inet_aton.3 \
	inet.3 inet_lnaof.3 inet.3 inet_makeaddr.3 inet.3 inet_netof.3 \
	inet.3 inet_network.3 inet.3 inet_ntoa.3 \
	inet.3 inet_ntop.3 inet.3 inet_pton.3 \
	inet.3 network.3 inet.3 ntoa.3 
MLINKS+=inet_net.3 inet_net_ntop.3 inet_net.3 inet_net_pton.3
#MLINKS+=ns.3 ns_addr.3 ns.3 ns_ntoa.3
MLINKS+=resolver.3 dn_comp.3 resolver.3 dn_expand.3 resolver.3 res_init.3 \
	resolver.3 res_mkquery.3 resolver.3 res_query.3 \
	resolver.3 res_search.3 resolver.3 res_send.3 resolver.3 dn_skipname.3 \
	resolver.3 ns_get16.3 resolver.3 ns_get32.3 \
	resolver.3 ns_put16.3 resolver.3 ns_put32.3
.endif
