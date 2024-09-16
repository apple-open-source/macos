/*
 * Copyright (c) 2002-2023 Apple Inc. All rights reserved.
 *
 * @APPLE_OSREFERENCE_LICENSE_HEADER_START@
 *
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. The rights granted to you under the License
 * may not be used to create, or enable the creation or redistribution of,
 * unlawful or unlicensed copies of an Apple operating system, or to
 * circumvent, violate, or enable the circumvention or violation of, any
 * terms of an Apple operating system software license agreement.
 *
 * Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this file.
 *
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 *
 * @APPLE_OSREFERENCE_LICENSE_HEADER_END@
 */

/*
 * Copyright (C) 1995, 1996, 1997, and 1998 WIDE Project.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the project nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE PROJECT AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE PROJECT OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * Copyright (c) 1989, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Mike Muuss.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>

#ifndef lint
__unused static const char copyright[] =
"@(#) Copyright (c) 1989, 1993\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif /* not lint */

/*
 * Using the InterNet Control Message Protocol (ICMP) "ECHO" facility,
 * measure round-trip-delays and packet loss across network paths.
 *
 * Author -
 *	Mike Muuss
 *	U. S. Army Ballistic Research Laboratory
 *	December, 1983
 *
 * Status -
 *	Public Domain.  Distribution Unlimited.
 * Bugs -
 *	More statistics could always be gathered.
 *	This program has to run SUID to ROOT to access the ICMP socket.
 */
/*
 * NOTE:
 * USE_SIN6_SCOPE_ID assumes that sin6_scope_id has the same semantics
 * as IPV6_PKTINFO.  Some people object it (sin6_scope_id specifies *link*
 * while IPV6_PKTINFO specifies *interface*.  Link is defined as collection of
 * network attached to 1 or more interfaces)
 */

#include <sys/param.h>
#include <sys/uio.h>
#include <sys/socket.h>
#include <sys/time.h>

#include <net/if.h>
#include <net/route.h>

#include <netinet/in.h>
#include <netinet/ip6.h>
#include <netinet/icmp6.h>
#include <arpa/inet.h>
#include <arpa/nameser.h>
#include <netdb.h>

#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <math.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sysexits.h>
#include <getopt.h>

#ifdef IPSEC
#include <netinet6/ah.h>
#include <netinet6/ipsec.h>
#endif

#include "md5.h"

struct tv32 {
	u_int32_t tv32_sec;
	u_int32_t tv32_usec;
};

#define MAXPACKETLEN	131072
#define	IP6LEN		40
#define ICMP6ECHOLEN	8	/* icmp echo header len excluding time */
#define ICMP6ECHOTMLEN sizeof(struct tv32)
#define ICMP6_NIQLEN	(ICMP6ECHOLEN + 8)
# define CONTROLLEN	10240	/* ancillary data buffer size RFC3542 20.1 */
/* FQDN case, 64 bits of nonce + 32 bits ttl */
#define ICMP6_NIRLEN	(ICMP6ECHOLEN + 12)
#define	EXTRA		256	/* for AH and various other headers. weird. */
#define	DEFDATALEN	ICMP6ECHOTMLEN
#define MAXDATALEN	MAXPACKETLEN - IP6LEN - ICMP6ECHOLEN
#define	NROUTES		9		/* number of record route slots */
#define	MAXWAIT		10000		/* max ms to wait for response */
#define	MAXALARM	(60 * 60)	/* max seconds for alarm timeout */

#define	A(bit)		rcvd_tbl[(bit)>>3]	/* identify byte in array */
#define	B(bit)		(1 << ((bit) & 0x07))	/* identify bit in byte */
#define	SET(bit)	(A(bit) |= B(bit))
#define	CLR(bit)	(A(bit) &= (~B(bit)))
#define	TST(bit)	(A(bit) & B(bit))

#define	F_FLOOD		0x0001
#define	F_INTERVAL	0x0002
#define	F_PINGFILLED	0x0008
#define	F_QUIET		0x0010
#define	F_RROUTE	0x0020
#define	F_SO_DEBUG	0x0040
#define	F_PRTIME	0x0080
#define	F_VERBOSE	0x0100
#ifdef IPSEC
#ifdef IPSEC_POLICY_IPSEC
#define	F_POLICY	0x0400
#else
#define F_AUTHHDR	0x0200
#define F_ENCRYPT	0x0400
#endif /*IPSEC_POLICY_IPSEC*/
#endif /*IPSEC*/
#define F_NODEADDR	0x0800
#define F_FQDN		0x1000
#define F_INTERFACE	0x2000
#define F_SRCADDR	0x4000
#define F_HOSTNAME	0x10000
#define F_FQDNOLD	0x20000
#define F_NIGROUP	0x40000
#define F_SUPTYPES	0x80000
#define F_NOMINMTU	0x100000
#define F_ONCE		0x200000
#define F_AUDIBLE	0x400000
#define F_MISSED	0x800000
#define F_DONTFRAG	0x1000000
#define	F_SWEEP		0x2000000
#define F_CONNECT	0x4000000
#define F_NOUSERDATA	(F_NODEADDR | F_FQDN | F_FQDNOLD | F_SUPTYPES)
#define	F_WAITTIME	0x8000000
u_int options;

static int longopt_flag = 0;

#define	LOF_CONNECT	0x01
#define LOF_PRTIME	0x02

static const struct option longopts[] = {
	{ "apple-connect", no_argument, &longopt_flag, LOF_CONNECT },
	{ "apple-time", no_argument, &longopt_flag, LOF_PRTIME },
	{ NULL, 0, NULL, 0 }
};

#define IN6LEN		sizeof(struct in6_addr)
#define SA6LEN		sizeof(struct sockaddr_in6)
#define DUMMY_PORT	10101

#define SIN6(s)	((struct sockaddr_in6 *)(s))

#define MAXTOS 255
/*
 * MAX_DUP_CHK is the number of bits in received table, i.e. the maximum
 * number of received sequence numbers we can keep track of.  Change 128
 * to 8192 for complete accuracy...
 */
#define	MAX_DUP_CHK	(8 * 8192)
int mx_dup_ck = MAX_DUP_CHK;
char rcvd_tbl[MAX_DUP_CHK / 8];

struct addrinfo *res;
struct sockaddr_in6 dst;	/* who to ping6 */
struct sockaddr_in6 src;	/* src addr of this packet */
socklen_t srclen;
int datalen = DEFDATALEN;
int s;				/* socket file descriptor */
u_char outpack[MAXPACKETLEN];
char BSPACE = '\b';		/* characters written for flood */
char BBELL = '\a';		/* characters written for AUDIBLE */
char DOT = '.';
char *hostname;
int ident;			/* process id to identify our packets */
u_int8_t nonce[8];		/* nonce field for node information */
int hoplimit = -1;		/* hoplimit */
int pathmtu = 0;		/* path MTU for the destination.  0 = unspec. */
u_char *packet = NULL;
struct cmsghdr *cm = NULL;
char *boundif;
unsigned int ifscope;
int nocell;

/* counters */
long nmissedmax;		/* max value of ntransmitted - nreceived - 1 */
long npackets;			/* max packets to transmit */
long nreceived;			/* # of packets we got back */
long nrepeats;			/* number of duplicates */
long ntransmitted;		/* sequence # for outbound packets = #sent */
static int interval = 1000;	/* interval between packets in ms */
static int waittime = MAXWAIT;	/* timeout for each packet */
static long nrcvtimeout = 0;	/* # of packets we got back after waittime */

long snpackets = 0;		/* max packets to transmit in one sweep */
long sntransmitted = 0;		/* # of packets we sent in this sweep */
int sweepmax = 0;		/* max value of payload in sweep */
int sweepmin = 0;		/* start value of payload in sweep */
int sweepincr = 1;		/* payload increment in sweep */

/* timing */
int timing;			/* flag to do timing */
double tmin = 999999999.0;	/* minimum round trip time */
double tmax = 0.0;		/* maximum round trip time */
double tsum = 0.0;		/* sum of all times, for doing average */
double tsumsq = 0.0;		/* sum of all times squared, for std. dev. */

/* for node addresses */
u_short naflags;

/* for ancillary data(advanced API) */
struct msghdr smsghdr;
struct iovec smsgiov[2];
char *scmsg = NULL;

volatile sig_atomic_t seenalrm;
volatile sig_atomic_t seenint;
#ifdef SIGINFO
volatile sig_atomic_t seeninfo;
#endif

int rcvtclass = 0;

int use_sendmsg = 0;
int use_recvmsg = 0;
int so_traffic_class = SO_TC_CTL;	/* use control class, by default */
int net_service_type = -1;

int32_t thiszone;		/* seconds offset from gmt to local time */
extern int32_t gmt2local(time_t);
static void pr_currenttime(void);

int	 main(int, char *[]);
void	 fill(char *, char *);
int	 get_hoplim(struct msghdr *);
int	 get_pathmtu(struct msghdr *);
int	 get_tclass(struct msghdr *);
int	 get_so_traffic_class(struct msghdr *);
struct in6_pktinfo *get_rcvpktinfo(struct msghdr *);
void	 onsignal(int);
void	 onint(int);
size_t	 pingerlen(void);
int	 pinger(void);
const char *pr_addr(struct sockaddr *, int);
void	 pr_icmph(struct icmp6_hdr *, u_char *);
void	 pr_iph(struct ip6_hdr *);
void	 pr_suptypes(struct icmp6_nodeinfo *, size_t);
void	 pr_nodeaddr(struct icmp6_nodeinfo *, int);
int	 myechoreply(const struct icmp6_hdr *);
int	 mynireply(const struct icmp6_nodeinfo *);
char *dnsdecode(const u_char **, const u_char *, const u_char *,
	char *, size_t);
void	 pr_pack(u_char *, int, struct msghdr *);
void	 pr_exthdrs(struct msghdr *);
void	 pr_ip6opt(void *, size_t);
void	 pr_rthdr(void *, size_t);
int	 pr_bitrange(u_int32_t, int, int);
void	 pr_retip(struct ip6_hdr *, u_char *);
void	 summary(void);
void	 tvsub(struct timeval *, struct timeval *);
int	 setpolicy(int, char *);
char	*nigroup(char *, int);
static int str2sotc(const char *, bool *);
static int str2netservicetype(const char *, bool *);
static u_int8_t str2tclass(const char *, bool *);
void	 usage(void);

int
main(int argc, char *argv[])
{
	struct timeval last, intvl;
	struct sockaddr_in6 from;
	struct addrinfo hints;
	int cc, i;
	int almost_done, ch, hold, packlen, preload, optval, ret_ga;
	int nig_oldmcprefix = -1;
	u_char *datap;
	char *e, *target, *ifname = NULL, *gateway = NULL;
	int ip6optlen = 0;
	struct cmsghdr *scmsgp = NULL;
#if defined(SO_SNDBUF) && defined(SO_RCVBUF)
	u_long lsockbufsize;
	int sockbufsize = 0;
#endif
	int usepktinfo = 0;
	struct in6_pktinfo *pktinfo = NULL;
#ifdef USE_RFC2292BIS
	struct ip6_rthdr *rthdr = NULL;
#endif
#ifdef IPSEC_POLICY_IPSEC
	char *policy_in = NULL;
	char *policy_out = NULL;
#endif
	double t;
	u_long alarmtimeout;
	size_t rthlen;
#ifdef IPV6_USE_MIN_MTU
	int mflag = 0;
#endif
	/* T_CLASS value -1 means default, so -2 means do not bother */
	int tclass = -2;
	bool valid;

	/* just to be sure */
	memset(&smsghdr, 0, sizeof(smsghdr));
	memset(&smsgiov, 0, sizeof(smsgiov));

	preload = 0;
	datap = &outpack[ICMP6ECHOLEN + ICMP6ECHOTMLEN];
#ifndef IPSEC
#define ADDOPTS
#else
#ifdef IPSEC_POLICY_IPSEC
#define ADDOPTS	"P:"
#else
#define ADDOPTS	"AE"
#endif /*IPSEC_POLICY_IPSEC*/
#endif
	while ((ch = getopt_long(argc, argv,
	    "a:b:B:Cc:DdfHG:g:h:I:i:k:K:l:mnNop:qrRS:s:tvwWz:" ADDOPTS,
	    longopts, NULL)) != -1) {
#undef ADDOPTS
		switch (ch) {
		case 'a':
		{
			char *cp;

			options &= ~F_NOUSERDATA;
			options |= F_NODEADDR;
			for (cp = optarg; *cp != '\0'; cp++) {
				switch (*cp) {
				case 'a':
					naflags |= NI_NODEADDR_FLAG_ALL;
					break;
				case 'c':
				case 'C':
					naflags |= NI_NODEADDR_FLAG_COMPAT;
					break;
				case 'l':
				case 'L':
					naflags |= NI_NODEADDR_FLAG_LINKLOCAL;
					break;
				case 's':
				case 'S':
					naflags |= NI_NODEADDR_FLAG_SITELOCAL;
					break;
				case 'g':
				case 'G':
					naflags |= NI_NODEADDR_FLAG_GLOBAL;
					break;
				case 'A': /* experimental. not in the spec */
#ifdef NI_NODEADDR_FLAG_ANYCAST
					naflags |= NI_NODEADDR_FLAG_ANYCAST;
					break;
#else
					errx(1, "-a A is not supported on "
					    "the platform");
					/*NOTREACHED*/
#endif
				default:
					usage();
					/*NOTREACHED*/
				}
			}
			break;
		}
		case 'b':
#if defined(SO_SNDBUF) && defined(SO_RCVBUF)
			errno = 0;
			e = NULL;
			lsockbufsize = strtoul(optarg, &e, 10);
			sockbufsize = lsockbufsize;
			if (errno || !*optarg || *e ||
			    sockbufsize != lsockbufsize)
				errx(1, "invalid socket buffer size");
#else
			errx(1, "-b option ignored: SO_SNDBUF/SO_RCVBUF "
			    "socket options not supported");
#endif
			break;
		case 'B':
			boundif = optarg;
			break;
		case 'C':
			nocell++;
			break;
		case 'c':
			npackets = strtol(optarg, &e, 10);
			if (npackets <= 0 || *optarg == '\0' || *e != '\0')
				errx(1,
				    "illegal number of packets -- %s", optarg);
			break;
		case 'D':
			options |= F_DONTFRAG;
			break;
		case 'd':
			options |= F_SO_DEBUG;
			break;
		case 'f':
			if (getuid()) {
				errno = EPERM;
				errx(1, "Must be superuser to flood ping");
			}
			options |= F_FLOOD;
			setbuf(stdout, (char *)NULL);
			break;
		case 'g':
			gateway = optarg;
			break;
		case 'G': {
			char *ptr ;
			char *tofree;
			unsigned long ultmp;

			tofree = strdup(optarg);
			if (tofree == NULL)
				errx(1, "### strdup() failed");
			ptr = tofree;
			do {
				char *str;
				char *ep;

				if ((str = strsep(&ptr, ",")) == NULL)
					errx(1, "-G requires maximum packet size");
				ultmp = strtoul(str, &ep, 0);
				if (*ep || ep == optarg)
					errx(EX_USAGE, "option -G invalid maximum packet size: `%s'",
					     str);
				options |= F_SWEEP;
				sweepmax = ultmp;
				if (sweepmax < 1 || sweepmax > MAXDATALEN) {
					errx(1,
					     "-G invalid maximum packet size, needs to be between 1 and %d",
					     MAXDATALEN);
				}

				if ((str = strsep(&ptr, ",")) == NULL)
					break;
				if (*str != 0) {
					ultmp = strtoul(str, &ep, 0);
					if (*ep || ep == optarg)
						errx(EX_USAGE, "option -G invalid minimum packet size: `%s'",
						     str);
					sweepmin = ultmp;
					if (sweepmin < 0 || sweepmin > MAXDATALEN) {
						errx(1,
						     "-G invalid minimum packet size, needs to be between 0 and %d",
						     MAXDATALEN);
					}
				}

				if ((str = strsep(&ptr, ",")) == NULL)
					break;
				if (*str == 0)
					break;
				ultmp = strtoul(str, &ep, 0);
				if (*ep || ep == optarg)
					errx(EX_USAGE, "option -G invalid sweep increment size: `%s'",
					     str);
				sweepincr = ultmp;
				if (sweepincr < 1 || sweepincr > MAXDATALEN) {
					errx(1,
					     "-G invalid sweep increment size, needs to be between 1 and %d",
					     MAXDATALEN);
				}
			} while (0);
			free(tofree);
			break;
		}
		case 'H':
			options |= F_HOSTNAME;
			break;
		case 'h':		/* hoplimit */
			hoplimit = strtol(optarg, &e, 10);
			if (*optarg == '\0' || *e != '\0')
				errx(1, "illegal hoplimit %s", optarg);
			if (255 < hoplimit || hoplimit < -1)
				errx(1,
				    "illegal hoplimit -- %s", optarg);
			break;
		case 'I':
			ifname = optarg;
			options |= F_INTERFACE;
#ifndef USE_SIN6_SCOPE_ID
			usepktinfo++;
#endif
			break;
		case 'i':		/* wait between sending packets */
			t = strtod(optarg, &e);
			if (*optarg == '\0' || *e != '\0')
				errx(1, "illegal timing interval %s", optarg);
			if (t < 0.002 && getuid()) {
				errx(1, "%s: only root may use interval < 0.002s",
				    strerror(EPERM));
			}
			intvl.tv_sec = (long)t;
			intvl.tv_usec =
			    (long)((t - intvl.tv_sec) * 1000000);
			if (intvl.tv_sec < 0)
				errx(1, "illegal timing interval %s", optarg);
			/* less than 1/hz does not make sense */
			if (intvl.tv_sec == 0 && intvl.tv_usec < 1) {
				warnx("too small interval, raised to .000001");
				intvl.tv_usec = 1;
			}
			options |= F_INTERVAL;
			break;
		case 'k':
			if (strcasecmp(optarg, "sendmsg") == 0) {
				use_sendmsg++;
				break;
			}
			if (strcasecmp(optarg, "recvmsg") == 0) {
				use_recvmsg++;
				break;
			}
			so_traffic_class = str2sotc(optarg, &valid);
			if (valid == false)
				errx(EX_USAGE, "bad traffic class: `%s'",
				     optarg);
			break;
		case 'K':
			if (strcasecmp(optarg, "sendmsg") == 0) {
				use_sendmsg++;
				break;
			}
			net_service_type = str2netservicetype(optarg, &valid);
			if (valid == false)
				errx(EX_USAGE, "bad network service type: `%s'",
				     optarg);
			/* suppress default traffic class (-k can still be specified after -K) */
			so_traffic_class = -1;
			break;
		case 'l':
			if (getuid()) {
				errno = EPERM;
				errx(1, "Must be superuser to preload");
			}
			preload = strtol(optarg, &e, 10);
			if (preload < 0 || *optarg == '\0' || *e != '\0')
				errx(1, "illegal preload value -- %s", optarg);
			break;
		case 'm':
#ifdef IPV6_USE_MIN_MTU
			mflag++;
			break;
#else
			errx(1, "-%c is not supported on this platform", ch);
			/*NOTREACHED*/
#endif
		case 'n':
			options &= ~F_HOSTNAME;
			break;
		case 'N':
			options |= F_NIGROUP;
			nig_oldmcprefix++;
			break;
		case 'o':
			options |= F_ONCE;
			break;
		case 'p':		/* fill buffer with user pattern */
			options |= F_PINGFILLED;
			fill((char *)datap, optarg);
			break;
		case 'q':
			options |= F_QUIET;
			break;
		case 'r':
			options |= F_AUDIBLE;
			break;
		case 'R':
			options |= F_MISSED;
			break;
		case 'S':
			memset(&hints, 0, sizeof(struct addrinfo));
			hints.ai_flags = AI_NUMERICHOST; /* allow hostname? */
			hints.ai_family = AF_INET6;
			hints.ai_socktype = SOCK_RAW;
			hints.ai_protocol = IPPROTO_ICMPV6;

			ret_ga = getaddrinfo(optarg, NULL, &hints, &res);
			if (ret_ga) {
				errx(1, "invalid source address: %s",
				     gai_strerror(ret_ga));
			}
			/*
			 * res->ai_family must be AF_INET6 and res->ai_addrlen
			 * must be sizeof(src).
			 */
			memcpy(&src, res->ai_addr, res->ai_addrlen);
			srclen = res->ai_addrlen;
			freeaddrinfo(res);
			res = NULL;
			options |= F_SRCADDR;
			break;
		case 's':		/* size of packet to send */
			datalen = strtol(optarg, &e, 10);
			if (datalen <= 0 || *optarg == '\0' || *e != '\0')
				errx(1, "illegal datalen value -- %s", optarg);
			if (datalen > MAXDATALEN) {
				errx(1,
				    "datalen value too large, maximum is %d",
				    MAXDATALEN);
			}
			break;
		case 't':
			options &= ~F_NOUSERDATA;
			options |= F_SUPTYPES;
			break;
		case 'v':
			options |= F_VERBOSE;
			break;
		case 'w':
			options &= ~F_NOUSERDATA;
			options |= F_FQDN;
			break;
		case 'W':
			options &= ~F_NOUSERDATA;
			options |= F_FQDNOLD;
			break;
		case 'z':
			tclass = str2tclass(optarg, &valid);
			if (valid == false)
				errx(1, "illegal TOS value -- %s", optarg);
			rcvtclass = 1;
			break;
		case 'X':
			alarmtimeout = strtoul(optarg, &e, 0);
			if (alarmtimeout < 1 || alarmtimeout == ULONG_MAX)
				errx(EX_USAGE, "invalid timeout: `%s'",
				    optarg);
			if (alarmtimeout > MAXALARM)
				errx(EX_USAGE, "invalid timeout: `%s' > %d",
				    optarg, MAXALARM);
			alarm((int)alarmtimeout);
			break;
#ifdef IPSEC
#ifdef IPSEC_POLICY_IPSEC
		case 'P':
			options |= F_POLICY;
			if (!strncmp("in", optarg, 2)) {
				if ((policy_in = strdup(optarg)) == NULL)
					errx(1, "strdup");
			} else if (!strncmp("out", optarg, 3)) {
				if ((policy_out = strdup(optarg)) == NULL)
					errx(1, "strdup");
			} else
				errx(1, "invalid security policy");
			break;
#else
		case 'A':
			options |= F_AUTHHDR;
			break;
		case 'E':
			options |= F_ENCRYPT;
			break;
#endif /*IPSEC_POLICY_IPSEC*/
#endif /*IPSEC*/
		case 0:
			switch (longopt_flag) {
				case LOF_CONNECT:
					options |= F_CONNECT;
					break;
				case LOF_PRTIME:
					options |= F_PRTIME;
					thiszone = gmt2local(0);
					break;
				default:
					break;
			}
			longopt_flag = 0;
			break;
		default:
			usage();
			/*NOTREACHED*/
		}
	}

	if (boundif != NULL && (ifscope = if_nametoindex(boundif)) == 0)
		errx(1, "bad interface name");

	if ((options & F_SWEEP) && !sweepmax)
		errx(EX_USAGE, "Maximum sweep size must be specified");

	if ((options & F_SWEEP) && (options & F_NOUSERDATA))
		errx(EX_USAGE, "Option -G incompatible with -t, -w and -W");

	argc -= optind;
	argv += optind;

	if (argc < 1) {
		usage();
		/*NOTREACHED*/
	}

	if (argc > 1) {
#ifdef IPV6_RECVRTHDR	/* 2292bis */
		rthlen = CMSG_SPACE(inet6_rth_space(IPV6_RTHDR_TYPE_0,
		    argc - 1));
#else  /* RFC2292 */
		rthlen = inet6_rthdr_space(IPV6_RTHDR_TYPE_0, argc - 1);
#endif
		if (rthlen == 0) {
			errx(1, "too many intermediate hops");
			/*NOTREACHED*/
		}
		ip6optlen += rthlen;
	}

	if (options & F_NIGROUP) {
		target = nigroup(argv[argc - 1], nig_oldmcprefix);
		if (target == NULL) {
			usage();
			/*NOTREACHED*/
		}
	} else
		target = argv[argc - 1];

	/* getaddrinfo */
	memset(&hints, 0, sizeof(struct addrinfo));
	hints.ai_flags = AI_CANONNAME;
	hints.ai_family = AF_INET6;
	hints.ai_socktype = SOCK_RAW;
	hints.ai_protocol = IPPROTO_ICMPV6;

	ret_ga = getaddrinfo(target, NULL, &hints, &res);
	if (ret_ga)
		errx(1, "getaddrinfo -- %s", gai_strerror(ret_ga));
	if (res->ai_canonname)
		hostname = strdup(res->ai_canonname);
	else
		hostname = target;

	if (!res->ai_addr)
		errx(1, "getaddrinfo failed");

	(void)memcpy(&dst, res->ai_addr, res->ai_addrlen);

	res->ai_socktype = getuid() ? SOCK_DGRAM : SOCK_RAW;
	res->ai_protocol = IPPROTO_ICMPV6;

	if ((s = socket(res->ai_family, res->ai_socktype,
	    res->ai_protocol)) < 0)
		err(1, "socket");

	if (ifscope != 0) {
		if (setsockopt(s, IPPROTO_IPV6, IPV6_BOUND_IF,
		    (char *)&ifscope, sizeof (ifscope)) != 0)
			err(1, "setsockopt(IPV6_BOUND_IF)");
	}

	if (nocell != 0) {
		if (setsockopt(s, IPPROTO_IPV6, IPV6_NO_IFT_CELLULAR,
		    (char *)&nocell, sizeof (nocell)) != 0)
			err(1, "setsockopt(IPV6_NO_IFT_CELLULAR)");
	}

	/* set the source address if specified. */
	if ((options & F_SRCADDR) &&
	    bind(s, (struct sockaddr *)&src, srclen) != 0) {
		err(1, "bind");
	}

	/* set the gateway (next hop) if specified */
	if (gateway) {
		struct addrinfo ghints, *gres;
		int error;

		memset(&ghints, 0, sizeof(ghints));
		ghints.ai_family = AF_INET6;
		ghints.ai_socktype = SOCK_RAW;
		ghints.ai_protocol = IPPROTO_ICMPV6;

		error = getaddrinfo(gateway, NULL, &hints, &gres);
		if (error) {
			errx(1, "getaddrinfo for the gateway %s: %s",
			     gateway, gai_strerror(error));
		}
		if (gres->ai_next && (options & F_VERBOSE))
			warnx("gateway resolves to multiple addresses");

		if (setsockopt(s, IPPROTO_IPV6, IPV6_NEXTHOP,
			       gres->ai_addr, gres->ai_addrlen)) {
			err(1, "setsockopt(IPV6_NEXTHOP)");
		}

		freeaddrinfo(gres);
	}

	/*
	 * let the kerel pass extension headers of incoming packets,
	 * for privileged socket options
	 */
	if ((options & F_VERBOSE) != 0) {
		int opton = 1;

#ifdef IPV6_RECVHOPOPTS
		if (setsockopt(s, IPPROTO_IPV6, IPV6_RECVHOPOPTS, &opton,
		    sizeof(opton)))
			err(1, "setsockopt(IPV6_RECVHOPOPTS)");
#else  /* old adv. API */
		if (setsockopt(s, IPPROTO_IPV6, IPV6_HOPOPTS, &opton,
		    sizeof(opton)))
			err(1, "setsockopt(IPV6_HOPOPTS)");
#endif
#ifdef IPV6_RECVDSTOPTS
		if (setsockopt(s, IPPROTO_IPV6, IPV6_RECVDSTOPTS, &opton,
		    sizeof(opton)))
			err(1, "setsockopt(IPV6_RECVDSTOPTS)");
#else  /* old adv. API */
		if (setsockopt(s, IPPROTO_IPV6, IPV6_DSTOPTS, &opton,
		    sizeof(opton)))
			err(1, "setsockopt(IPV6_DSTOPTS)");
#endif
#ifdef IPV6_RECVRTHDRDSTOPTS
		if (setsockopt(s, IPPROTO_IPV6, IPV6_RECVRTHDRDSTOPTS, &opton,
		    sizeof(opton)))
			err(1, "setsockopt(IPV6_RECVRTHDRDSTOPTS)");
#endif
	}

	/* revoke root privilege */
	if (seteuid(getuid()) != 0)
		err(1, "seteuid() failed");
	if (setuid(getuid()) != 0)
		err(1, "setuid() failed");

	if ((options & F_FLOOD) && (options & F_INTERVAL))
		errx(1, "-f and -i incompatible options");

	if ((options & F_CONNECT)) {
		if (connect(s, (struct sockaddr *)&dst, sizeof(dst)) == -1)
			err(EX_OSERR, "connect");
	}

	if (sweepmax) {
		if (sweepmin >= sweepmax)
			errx(EX_USAGE, "Maximum packet size must be greater than the minimum packet size");

		if (datalen != DEFDATALEN)
			errx(EX_USAGE, "Packet size and ping sweep are mutually exclusive");

		if (npackets > 0) {
			snpackets = npackets;
			npackets = 0;
		} else
			snpackets = 1;
		datalen = sweepmin;
	}

	if ((options & F_NOUSERDATA) == 0) {
		if (datalen >= sizeof(struct tv32)) {
			/* we can time transfer */
			timing = 1;
		} else
			timing = 0;
		/* in F_VERBOSE case, we may get non-echoreply packets*/
		if (options & F_VERBOSE)
			packlen = MAX(2048, sweepmax) + IP6LEN + ICMP6ECHOLEN + EXTRA;
		else
			packlen = MAX(datalen, sweepmax) + IP6LEN + ICMP6ECHOLEN + EXTRA;
	} else {
		/* suppress timing for node information query */
		timing = 0;
		datalen = 2048;
		packlen = 2048 + IP6LEN + ICMP6ECHOLEN + EXTRA;
	}

	if (!(packet = (u_char *)malloc(packlen)))
		err(1, "Unable to allocate packet");
	if (!(options & F_PINGFILLED))
		for (i = ICMP6ECHOLEN; i < MAX(datalen, sweepmax); ++i)
			*datap++ = i;

	ident = getpid() & 0xFFFF;
	arc4random_buf(nonce, sizeof(nonce));
	optval = 1;
	if (options & F_DONTFRAG)
		if (setsockopt(s, IPPROTO_IPV6, IPV6_DONTFRAG,
		    &optval, sizeof(optval)) == -1)
			err(1, "IPV6_DONTFRAG");
	hold = 1;

	if (options & F_SO_DEBUG)
		(void)setsockopt(s, SOL_SOCKET, SO_DEBUG, (char *)&hold,
		    sizeof(hold));

	hold = 1;
	(void) setsockopt(s, SOL_SOCKET, SO_RECV_ANYIF, (char *)&hold,
	    sizeof(hold));

	optval = IPV6_DEFHLIM;
	if (IN6_IS_ADDR_MULTICAST(&dst.sin6_addr))
		if (setsockopt(s, IPPROTO_IPV6, IPV6_MULTICAST_HOPS,
		    &optval, sizeof(optval)) == -1)
			err(1, "IPV6_MULTICAST_HOPS");
#ifdef IPV6_USE_MIN_MTU
	if (mflag != 1) {
		optval = mflag > 1 ? 0 : 1;

		if (setsockopt(s, IPPROTO_IPV6, IPV6_USE_MIN_MTU,
		    &optval, sizeof(optval)) == -1)
			err(1, "setsockopt(IPV6_USE_MIN_MTU)");
	}
#ifdef IPV6_RECVPATHMTU
	else {
		optval = 1;
		if (setsockopt(s, IPPROTO_IPV6, IPV6_RECVPATHMTU,
		    &optval, sizeof(optval)) == -1)
			err(1, "setsockopt(IPV6_RECVPATHMTU)");
	}
#endif /* IPV6_RECVPATHMTU */
#endif /* IPV6_USE_MIN_MTU */

#ifdef IPSEC
#ifdef IPSEC_POLICY_IPSEC
	if (options & F_POLICY) {
		if (setpolicy(s, policy_in) < 0)
			errx(1, "%s", ipsec_strerror());
		if (setpolicy(s, policy_out) < 0)
			errx(1, "%s", ipsec_strerror());
	}
#else
	if (options & F_AUTHHDR) {
		optval = IPSEC_LEVEL_REQUIRE;
#ifdef IPV6_AUTH_TRANS_LEVEL
		if (setsockopt(s, IPPROTO_IPV6, IPV6_AUTH_TRANS_LEVEL,
		    &optval, sizeof(optval)) == -1)
			err(1, "setsockopt(IPV6_AUTH_TRANS_LEVEL)");
#else /* old def */
		if (setsockopt(s, IPPROTO_IPV6, IPV6_AUTH_LEVEL,
		    &optval, sizeof(optval)) == -1)
			err(1, "setsockopt(IPV6_AUTH_LEVEL)");
#endif
	}
	if (options & F_ENCRYPT) {
		optval = IPSEC_LEVEL_REQUIRE;
		if (setsockopt(s, IPPROTO_IPV6, IPV6_ESP_TRANS_LEVEL,
		    &optval, sizeof(optval)) == -1)
			err(1, "setsockopt(IPV6_ESP_TRANS_LEVEL)");
	}
#endif /*IPSEC_POLICY_IPSEC*/
#endif

#ifdef ICMP6_FILTER
    {
	struct icmp6_filter filt;
	if (!(options & F_VERBOSE)) {
		ICMP6_FILTER_SETBLOCKALL(&filt);
		if ((options & F_FQDN) || (options & F_FQDNOLD) ||
		    (options & F_NODEADDR) || (options & F_SUPTYPES))
			ICMP6_FILTER_SETPASS(ICMP6_NI_REPLY, &filt);
		else
			ICMP6_FILTER_SETPASS(ICMP6_ECHO_REPLY, &filt);
	} else {
		ICMP6_FILTER_SETPASSALL(&filt);
	}
	if (setsockopt(s, IPPROTO_ICMPV6, ICMP6_FILTER, &filt,
	    sizeof(filt)) < 0)
		err(1, "setsockopt(ICMP6_FILTER)");
    }
#endif /*ICMP6_FILTER*/

	/* let the kerel pass extension headers of incoming packets */
	if ((options & F_VERBOSE) != 0) {
		int opton = 1;

#ifdef IPV6_RECVRTHDR
		if (setsockopt(s, IPPROTO_IPV6, IPV6_RECVRTHDR, &opton,
		    sizeof(opton)))
			err(1, "setsockopt(IPV6_RECVRTHDR)");
#else  /* old adv. API */
		if (setsockopt(s, IPPROTO_IPV6, IPV6_RTHDR, &opton,
		    sizeof(opton)))
			err(1, "setsockopt(IPV6_RTHDR)");
#endif
	}

	if (tclass != -2) {
		int on = 1;

		if (setsockopt(s, IPPROTO_IPV6, IPV6_RECVTCLASS, &on,
		    sizeof(on)))
			err(1, "setsockopt(IPV6_RECVTCLASS)");

		if (setsockopt(s, IPPROTO_IPV6, IPV6_TCLASS, &tclass,
		    sizeof(tclass)))
			err(1, "setsockopt(IPV6_TCLASS)");
	}

/*
	optval = 1;
	if (IN6_IS_ADDR_MULTICAST(&dst.sin6_addr))
		if (setsockopt(s, IPPROTO_IPV6, IPV6_MULTICAST_LOOP,
		    &optval, sizeof(optval)) == -1)
			err(1, "IPV6_MULTICAST_LOOP");
*/

	/* Specify the outgoing interface and/or the source address */
	if (usepktinfo)
		ip6optlen += CMSG_SPACE(sizeof(struct in6_pktinfo));

	if (hoplimit != -1)
		ip6optlen += CMSG_SPACE(sizeof(int));

#ifdef IPV6_USE_MIN_MTU
	if (mflag != 1)
		ip6optlen += CMSG_SPACE(sizeof(int));
#endif /* IPV6_USE_MIN_MTU */

	if (tclass != -2)
		ip6optlen += CMSG_SPACE(sizeof(int));

	if (use_sendmsg == 0) {
		if (net_service_type != -1)
			if (setsockopt(s, SOL_SOCKET, SO_NET_SERVICE_TYPE,
				       (void *)&net_service_type, sizeof (net_service_type)) != 0)
				warn("setsockopt(SO_NET_SERVICE_TYPE");
		if (so_traffic_class != -1) {
			if (setsockopt(s, SOL_SOCKET, SO_TRAFFIC_CLASS,
			    (void *)&so_traffic_class, sizeof (so_traffic_class)) != 0)
				warn("setsockopt(SO_TRAFFIC_CLASS");

		}
	} else {
		if (net_service_type != -1)
			ip6optlen += CMSG_SPACE(sizeof(int));
		if (so_traffic_class != -1)
			ip6optlen += CMSG_SPACE(sizeof(int));
	}
	if (use_recvmsg > 0) {
		int on = 1;
		if (setsockopt(s, SOL_SOCKET, SO_RECV_TRAFFIC_CLASS,
		    (void *)&on, sizeof (on)) != 0)
			warn("setsockopt(SO_RECV_TRAFFIC_CLASS");
	}

	/* set IP6 packet options */
	if (ip6optlen) {
		if ((scmsg = (char *)malloc(ip6optlen)) == 0)
			errx(1, "can't allocate enough memory");
		smsghdr.msg_control = (caddr_t)scmsg;
		smsghdr.msg_controllen = ip6optlen;
		scmsgp = (struct cmsghdr *)scmsg;
	}
	if (usepktinfo) {
		pktinfo = (struct in6_pktinfo *)(CMSG_DATA(scmsgp));
		memset(pktinfo, 0, sizeof(*pktinfo));
		scmsgp->cmsg_len = CMSG_LEN(sizeof(struct in6_pktinfo));
		scmsgp->cmsg_level = IPPROTO_IPV6;
		scmsgp->cmsg_type = IPV6_PKTINFO;
		scmsgp = CMSG_NXTHDR(&smsghdr, scmsgp);
	}

	/* set the outgoing interface */
	if (ifname) {
#ifndef USE_SIN6_SCOPE_ID
		/* pktinfo must have already been allocated */
		if ((pktinfo->ipi6_ifindex = if_nametoindex(ifname)) == 0)
			errx(1, "%s: invalid interface name", ifname);
#else
		if ((dst.sin6_scope_id = if_nametoindex(ifname)) == 0)
			errx(1, "%s: invalid interface name", ifname);
#endif
	}
	if (hoplimit != -1) {
		scmsgp->cmsg_len = CMSG_LEN(sizeof(int));
		scmsgp->cmsg_level = IPPROTO_IPV6;
		scmsgp->cmsg_type = IPV6_HOPLIMIT;
		*(int *)(CMSG_DATA(scmsgp)) = hoplimit;

		scmsgp = CMSG_NXTHDR(&smsghdr, scmsgp);
	}

#ifdef IPV6_USE_MIN_MTU
	if (mflag != 1) {
		optval = mflag > 1 ? 0 : 1;

		scmsgp->cmsg_len = CMSG_LEN(sizeof(int));
		scmsgp->cmsg_level = IPPROTO_IPV6;
		scmsgp->cmsg_type = IPV6_USE_MIN_MTU;
		*(int *)(CMSG_DATA(scmsgp)) = optval;

		scmsgp = CMSG_NXTHDR(&smsghdr, scmsgp);
	}
#endif /* IPV6_USE_MIN_MTU */

	if (argc > 1) {	/* some intermediate addrs are specified */
		int hops, error;
#ifdef USE_RFC2292BIS
		int rthdrlen;
#endif

#ifdef USE_RFC2292BIS
		rthdrlen = inet6_rth_space(IPV6_RTHDR_TYPE_0, argc - 1);
		scmsgp->cmsg_len = CMSG_LEN(rthdrlen);
		scmsgp->cmsg_level = IPPROTO_IPV6;
		scmsgp->cmsg_type = IPV6_RTHDR;
		rthdr = (struct ip6_rthdr *)CMSG_DATA(scmsgp);
		rthdr = inet6_rth_init((void *)rthdr, rthdrlen,
		    IPV6_RTHDR_TYPE_0, argc - 1);
		if (rthdr == NULL)
			errx(1, "can't initialize rthdr");
#else  /* old advanced API */
		if ((scmsgp = (struct cmsghdr *)inet6_rthdr_init(scmsgp,
		    IPV6_RTHDR_TYPE_0)) == 0)
			errx(1, "can't initialize rthdr");
#endif /* USE_RFC2292BIS */

		for (hops = 0; hops < argc - 1; hops++) {
			struct addrinfo *iaip;

			if ((error = getaddrinfo(argv[hops], NULL, &hints,
			    &iaip)))
				errx(1, "%s", gai_strerror(error));
			if (SIN6(iaip->ai_addr)->sin6_family != AF_INET6)
				errx(1,
				    "bad addr family of an intermediate addr");

#ifdef USE_RFC2292BIS
			if (inet6_rth_add(rthdr,
			    &(SIN6(iaip->ai_addr))->sin6_addr))
				errx(1, "can't add an intermediate node");
#else  /* old advanced API */
			if (inet6_rthdr_add(scmsgp,
			    &(SIN6(iaip->ai_addr))->sin6_addr,
			    IPV6_RTHDR_LOOSE))
				errx(1, "can't add an intermediate node");
#endif /* USE_RFC2292BIS */
			freeaddrinfo(iaip);
		}

#ifndef USE_RFC2292BIS
		if (inet6_rthdr_lasthop(scmsgp, IPV6_RTHDR_LOOSE))
			errx(1, "can't set the last flag");
#endif

		scmsgp = CMSG_NXTHDR(&smsghdr, scmsgp);
	}

	if (tclass != -2) {
		scmsgp->cmsg_len = CMSG_LEN(sizeof(int));
		scmsgp->cmsg_level = IPPROTO_IPV6;
		scmsgp->cmsg_type = IPV6_TCLASS;
		*(int *)(CMSG_DATA(scmsgp)) = tclass;

		scmsgp = CMSG_NXTHDR(&smsghdr, scmsgp);
	}
	if (use_sendmsg != 0) {
		if (so_traffic_class != -1) {
			scmsgp->cmsg_len = CMSG_LEN(sizeof(int));
			scmsgp->cmsg_level = SOL_SOCKET;
			scmsgp->cmsg_type = SO_TRAFFIC_CLASS;
			*(int *)(CMSG_DATA(scmsgp)) = so_traffic_class;

			scmsgp = CMSG_NXTHDR(&smsghdr, scmsgp);
		}
		if (net_service_type != -1) {
			scmsgp->cmsg_len = CMSG_LEN(sizeof(int));
			scmsgp->cmsg_level = SOL_SOCKET;
			scmsgp->cmsg_type = SO_NET_SERVICE_TYPE;
			*(int *)(CMSG_DATA(scmsgp)) = net_service_type;
		}
	}
	if (!(options & F_SRCADDR)) {
		/*
		 * get the source address. XXX since we revoked the root
		 * privilege, we cannot use a raw socket for this.
		 */
		int dummy;
		socklen_t len = sizeof(src);

		if ((dummy = socket(AF_INET6, SOCK_DGRAM, 0)) < 0)
			err(1, "UDP socket");

		if (ifscope != 0) {
			if (setsockopt(dummy, IPPROTO_IPV6, IPV6_BOUND_IF,
			    (char *)&ifscope, sizeof (ifscope)) != 0)
				err(1, "setsockopt(IPV6_BOUND_IF)");
		}

		if (nocell != 0) {
			if (setsockopt(dummy, IPPROTO_IPV6, IPV6_NO_IFT_CELLULAR,
			    (char *)&nocell, sizeof (nocell)) != 0)
				err(1, "setsockopt(IPV6_NO_IFT_CELLULAR)");
		}

		src.sin6_family = AF_INET6;
		src.sin6_addr = dst.sin6_addr;
		src.sin6_port = ntohs(DUMMY_PORT);
		src.sin6_scope_id = dst.sin6_scope_id;

#ifdef USE_RFC2292BIS
		if (pktinfo &&
		    setsockopt(dummy, IPPROTO_IPV6, IPV6_PKTINFO,
		    (void *)pktinfo, sizeof(*pktinfo)))
			err(1, "UDP setsockopt(IPV6_PKTINFO)");

		if (hoplimit != -1 &&
		    setsockopt(dummy, IPPROTO_IPV6, IPV6_UNICAST_HOPS,
		    (void *)&hoplimit, sizeof(hoplimit)))
			err(1, "UDP setsockopt(IPV6_UNICAST_HOPS)");

		if (hoplimit != -1 &&
		    setsockopt(dummy, IPPROTO_IPV6, IPV6_MULTICAST_HOPS,
		    (void *)&hoplimit, sizeof(hoplimit)))
			err(1, "UDP setsockopt(IPV6_MULTICAST_HOPS)");

		if (rthdr &&
		    setsockopt(dummy, IPPROTO_IPV6, IPV6_RTHDR,
		    (void *)rthdr, (rthdr->ip6r_len + 1) << 3))
			err(1, "UDP setsockopt(IPV6_RTHDR)");
#else  /* old advanced API */
		if (smsghdr.msg_control &&
		    setsockopt(dummy, IPPROTO_IPV6, IPV6_PKTOPTIONS,
		    (void *)smsghdr.msg_control, smsghdr.msg_controllen))
			err(1, "UDP setsockopt(IPV6_PKTOPTIONS)");
#endif

		if (connect(dummy, (struct sockaddr *)&src, len) < 0)
			err(1, "UDP connect");

		if (getsockname(dummy, (struct sockaddr *)&src, &len) < 0)
			err(1, "getsockname");

		close(dummy);
	}

#if defined(SO_SNDBUF) && defined(SO_RCVBUF)
	if (sockbufsize) {
		if (MAX(datalen, sweepmax) > sockbufsize)
			warnx("you need -b to increase socket buffer size");
		if (setsockopt(s, SOL_SOCKET, SO_SNDBUF, &sockbufsize,
		    sizeof(sockbufsize)) < 0)
			err(1, "setsockopt(SO_SNDBUF)");
		if (setsockopt(s, SOL_SOCKET, SO_RCVBUF, &sockbufsize,
		    sizeof(sockbufsize)) < 0)
			err(1, "setsockopt(SO_RCVBUF)");
	}
	else {
		if (MAX(datalen, sweepmax) > 8 * 1024)	/*XXX*/
			warnx("you need -b to increase socket buffer size");
		/*
		 * When pinging the broadcast address, you can get a lot of
		 * answers. Doing something so evil is useful if you are trying
		 * to stress the ethernet, or just want to fill the arp cache
		 * to get some stuff for /etc/ethers.
		 */
		hold = 48 * 1024;
		setsockopt(s, SOL_SOCKET, SO_RCVBUF, (char *)&hold,
		    sizeof(hold));
	}
#endif

	optval = 1;
#ifndef USE_SIN6_SCOPE_ID
#ifdef IPV6_RECVPKTINFO
	if (setsockopt(s, IPPROTO_IPV6, IPV6_RECVPKTINFO, &optval,
	    sizeof(optval)) < 0)
		warn("setsockopt(IPV6_RECVPKTINFO)"); /* XXX err? */
#else  /* old adv. API */
	if (setsockopt(s, IPPROTO_IPV6, IPV6_PKTINFO, &optval,
	    sizeof(optval)) < 0)
		warn("setsockopt(IPV6_PKTINFO)"); /* XXX err? */
#endif
#endif /* USE_SIN6_SCOPE_ID */
#ifdef IPV6_RECVHOPLIMIT
	if (setsockopt(s, IPPROTO_IPV6, IPV6_RECVHOPLIMIT, &optval,
	    sizeof(optval)) < 0)
		warn("setsockopt(IPV6_RECVHOPLIMIT)"); /* XXX err? */
#else  /* old adv. API */
	if (setsockopt(s, IPPROTO_IPV6, IPV6_HOPLIMIT, &optval,
	    sizeof(optval)) < 0)
		warn("setsockopt(IPV6_HOPLIMIT, %d, %lu)",
		    optval, sizeof(optval)); /* XXX err? */
#endif

	if (sweepmax)
		printf("PING6(40+8+[%lu...%lu] bytes) ",
		       (unsigned long)(sweepmin),
		       (unsigned long)(sweepmax));
	else
		printf("PING6(%lu=40+8+%lu bytes) ", (unsigned long)(40 + pingerlen()),
	    		(unsigned long)(pingerlen() - 8));
	printf("%s --> ", pr_addr((struct sockaddr *)&src, sizeof(src)));
	printf("%s\n", pr_addr((struct sockaddr *)&dst, sizeof(dst)));

	if (preload == 0)
		pinger();
	else {
		if (npackets != 0 && preload > npackets)
			preload = npackets;
		while (preload--)
			pinger();
	}
	gettimeofday(&last, NULL);

	/*
	 * rdar://25829310
	 *
	 * Clear blocked signals inherited from the parent
	 */
	sigset_t newset;
	sigemptyset(&newset);
	if (sigprocmask(SIG_SETMASK, &newset, NULL) != 0)
		err(EX_OSERR, "sigprocmask(newset)");

	seenalrm = seenint = 0;
#ifdef SIGINFO
	seeninfo = 0;
#endif

	(void)signal(SIGINT, onsignal);
#ifdef SIGINFO
	(void)signal(SIGINFO, onsignal);
#endif
	if (alarmtimeout > 0) {
		(void)signal(SIGALRM, onsignal);
	}

	if (options & F_FLOOD) {
		intvl.tv_sec = 0;
		intvl.tv_usec = 10000;
	} else if ((options & F_INTERVAL) == 0) {
		intvl.tv_sec = interval / 1000;
		intvl.tv_usec = interval % 1000 * 1000;
	}

	/* For control (ancillary) data received from recvmsg() */
	cm = (struct cmsghdr *)malloc(CONTROLLEN);
	if (cm == NULL)
		err(1, "malloc");

	almost_done = 0;
	while (seenint == 0) {
		struct timeval now, timeout;
		struct msghdr m;
		struct iovec iov[2];
		fd_set rfds;
		int n;

		/* signal handling */
		if (seenint)
			onint(SIGINT);
#ifdef SIGINFO
		if (seeninfo) {
			summary();
			seeninfo = 0;
			continue;
		}
#endif
		FD_ZERO(&rfds);
		FD_SET(s, &rfds);
		gettimeofday(&now, NULL);
		timeout.tv_sec = last.tv_sec + intvl.tv_sec - now.tv_sec;
		timeout.tv_usec = last.tv_usec + intvl.tv_usec - now.tv_usec;
		while (timeout.tv_usec < 0) {
			timeout.tv_usec += 1000000;
			timeout.tv_sec--;
			}
		while (timeout.tv_usec > 1000000) {
			timeout.tv_usec -= 1000000;
			timeout.tv_sec++;
		}
		if (timeout.tv_sec < 0)
			timeout.tv_sec = timeout.tv_usec = 0;

		n = select(s + 1, &rfds, NULL, NULL, &timeout);
		if (n < 0)
			continue;	/* EINTR */
		if (n == 1) {
			m.msg_name = (caddr_t)&from;
			m.msg_namelen = sizeof(from);
			memset(&iov, 0, sizeof(iov));
			iov[0].iov_base = (caddr_t)packet;
			iov[0].iov_len = packlen;
			m.msg_iov = iov;
			m.msg_iovlen = 1;
			memset(cm, 0, CONTROLLEN);
			m.msg_control = (void *)cm;
			m.msg_controllen = CONTROLLEN;
			m.msg_flags = 0;

			cc = recvmsg(s, &m, 0);
			if (cc < 0) {
				if (errno != EINTR) {
					warn("recvmsg");
					sleep(1);
				}
				continue;
			} else if (cc == 0) {
				int mtu;

				/*
				 * receive control messages only. Process the
				 * exceptions (currently the only possibility is
				 * a path MTU notification.)
				 */
				if ((mtu = get_pathmtu(&m)) > 0) {
					if ((options & F_VERBOSE) != 0) {
						printf("new path MTU (%d) is "
						       "notified\n", mtu);
					}
				}
				continue;
			} else {
				/*
				 * an ICMPv6 message (probably an echoreply)
				 * arrived.
				 */
				pr_pack(packet, cc, &m);
			}
			if (((options & F_ONCE) != 0 && nreceived > 0) ||
			    (npackets > 0 && nreceived >= npackets) ||
			     (sweepmax && datalen > sweepmax))
				break;
		}
		if (n == 0 || (options & F_FLOOD)) {
			if (npackets == 0 || ntransmitted < npackets)
				pinger();
			else {
				if (almost_done)
					break;
				almost_done = 1;
				/*
				 * If we're not transmitting any more packets,
				 * change the timer to wait two round-trip times
				 * if we've received any packets or (waittime)
				 * milliseconds if we haven't.
				 */
				intvl.tv_usec = 0;
				if (nreceived) {
					intvl.tv_sec = 2 * tmax / 1000;
					if (intvl.tv_sec == 0)
						intvl.tv_sec = 1;
				} else {
					intvl.tv_sec = waittime / 1000;
					intvl.tv_usec = waittime % 1000 * 1000;
				}
			}
			gettimeofday(&last, NULL);
			if (ntransmitted - nreceived - 1 > nmissedmax) {
				nmissedmax = ntransmitted - nreceived - 1;
				if (options & F_MISSED)
					(void)write(STDOUT_FILENO, &BBELL, 1);
				if (options & F_PRTIME) {
					pr_currenttime();
					printf("Request timeout for seq %u\n",
                                               (uint16_t)(ntransmitted - 2));
				}
			}
		}
	}
	sigemptyset(&newset);
	if (sigprocmask(SIG_SETMASK, &newset, NULL) != 0)
		err(EX_OSERR, "sigprocmask(newset)");
	summary();

        if (packet != NULL)
                free(packet);

	exit(nreceived == 0 ? 2 : 0);
}

void
onsignal(int sig)
{
	fflush(stdout);

	switch (sig) {
	case SIGINT:
	case SIGALRM:
		seenint++;
		break;
#ifdef SIGINFO
	case SIGINFO:
		seeninfo++;
		break;
#endif
	}
}

/*
 * pinger --
 *	Compose and transmit an ICMP ECHO REQUEST packet.  The IP packet
 * will be added on by the kernel.  The ID field is our UNIX process ID,
 * and the sequence number is an ascending integer.  The first 8 bytes
 * of the data portion are used to hold a UNIX "timeval" struct in VAX
 * byte-order, to compute the round-trip time.
 */
size_t
pingerlen(void)
{
	size_t l;

	if (options & F_FQDN)
		l = ICMP6_NIQLEN + sizeof(dst.sin6_addr);
	else if (options & F_FQDNOLD)
		l = ICMP6_NIQLEN;
	else if (options & F_NODEADDR)
		l = ICMP6_NIQLEN + sizeof(dst.sin6_addr);
	else if (options & F_SUPTYPES)
		l = ICMP6_NIQLEN;
	else
		l = ICMP6ECHOLEN + datalen;

	return l;
}

int
pinger(void)
{
	struct icmp6_hdr *icp;
	int i, cc;
	struct icmp6_nodeinfo *nip;
	int seq;

	if (npackets && ntransmitted >= npackets)
		return(-1);	/* no more transmission */

	if (sweepmax && sntransmitted == snpackets) {
		datalen += sweepincr;
		if (datalen > sweepmax)
			return(-1);	/* no more transmission */
		sntransmitted = 0;
	}

	icp = (struct icmp6_hdr *)outpack;
	nip = (struct icmp6_nodeinfo *)outpack;
	memset(icp, 0, sizeof(*icp));
	icp->icmp6_cksum = 0;
	seq = ntransmitted++;
	CLR(seq % mx_dup_ck);

	if (options & F_FQDN) {
		icp->icmp6_type = ICMP6_NI_QUERY;
		icp->icmp6_code = ICMP6_NI_SUBJ_IPV6;
		nip->ni_qtype = htons(NI_QTYPE_FQDN);
		nip->ni_flags = htons(0);

		memcpy(nip->icmp6_ni_nonce, nonce,
		    sizeof(nip->icmp6_ni_nonce));
		*(u_int16_t *)nip->icmp6_ni_nonce = ntohs(seq);

		memcpy(&outpack[ICMP6_NIQLEN], &dst.sin6_addr,
		    sizeof(dst.sin6_addr));
		cc = ICMP6_NIQLEN + sizeof(dst.sin6_addr);
		datalen = 0;
	} else if (options & F_FQDNOLD) {
		/* packet format in 03 draft - no Subject data on queries */
		icp->icmp6_type = ICMP6_NI_QUERY;
		icp->icmp6_code = 0;	/* code field is always 0 */
		nip->ni_qtype = htons(NI_QTYPE_FQDN);
		nip->ni_flags = htons(0);

		memcpy(nip->icmp6_ni_nonce, nonce,
		    sizeof(nip->icmp6_ni_nonce));
		*(u_int16_t *)nip->icmp6_ni_nonce = ntohs(seq);

		cc = ICMP6_NIQLEN;
		datalen = 0;
	} else if (options & F_NODEADDR) {
		icp->icmp6_type = ICMP6_NI_QUERY;
		icp->icmp6_code = ICMP6_NI_SUBJ_IPV6;
		nip->ni_qtype = htons(NI_QTYPE_NODEADDR);
		nip->ni_flags = naflags;

		memcpy(nip->icmp6_ni_nonce, nonce,
		    sizeof(nip->icmp6_ni_nonce));
		*(u_int16_t *)nip->icmp6_ni_nonce = ntohs(seq);

		memcpy(&outpack[ICMP6_NIQLEN], &dst.sin6_addr,
		    sizeof(dst.sin6_addr));
		cc = ICMP6_NIQLEN + sizeof(dst.sin6_addr);
		datalen = 0;
	} else if (options & F_SUPTYPES) {
		icp->icmp6_type = ICMP6_NI_QUERY;
		icp->icmp6_code = ICMP6_NI_SUBJ_FQDN;	/*empty*/
		nip->ni_qtype = htons(NI_QTYPE_SUPTYPES);
		/* we support compressed bitmap */
		nip->ni_flags = NI_SUPTYPE_FLAG_COMPRESS;

		memcpy(nip->icmp6_ni_nonce, nonce,
		    sizeof(nip->icmp6_ni_nonce));
		*(u_int16_t *)nip->icmp6_ni_nonce = ntohs(seq);
		cc = ICMP6_NIQLEN;
		datalen = 0;
	} else {
		icp->icmp6_type = ICMP6_ECHO_REQUEST;
		icp->icmp6_code = 0;
		icp->icmp6_id = htons(ident);
		icp->icmp6_seq = ntohs(seq);
		if (timing) {
			struct timeval tv;
			struct tv32 *tv32;
			(void)gettimeofday(&tv, NULL);
			tv32 = (struct tv32 *)&outpack[ICMP6ECHOLEN];
			tv32->tv32_sec = htonl(tv.tv_sec);
			tv32->tv32_usec = htonl(tv.tv_usec);
		}
		cc = ICMP6ECHOLEN + datalen;
	}

#ifdef DIAGNOSTIC
	if (pingerlen() != cc)
		errx(1, "internal error; length mismatch");
#endif

	if ((options & F_CONNECT)) {
		smsghdr.msg_name = NULL;
		smsghdr.msg_namelen = 0;
	} else {
	smsghdr.msg_name = (caddr_t)&dst;
	smsghdr.msg_namelen = sizeof(dst);
	}
	memset(&smsgiov, 0, sizeof(smsgiov));
	smsgiov[0].iov_base = (caddr_t)outpack;
	smsgiov[0].iov_len = cc;
	smsghdr.msg_iov = smsgiov;
	smsghdr.msg_iovlen = 1;

	i = sendmsg(s, &smsghdr, 0);

	if (i < 0 || i != cc)  {
		if (i < 0)
			warn("sendmsg");
		(void)printf("ping6: wrote %s %d chars, ret=%d\n",
		    hostname, cc, i);
	}
	sntransmitted++;
	if (!(options & F_QUIET) && options & F_FLOOD)
		(void)write(STDOUT_FILENO, &DOT, 1);

	return(0);
}

int
myechoreply(const struct icmp6_hdr *icp)
{
	if (ntohs(icp->icmp6_id) == ident)
		return 1;
	else
		return 0;
}

int
mynireply(const struct icmp6_nodeinfo *nip)
{
	if (memcmp(nip->icmp6_ni_nonce + sizeof(u_int16_t),
	    nonce + sizeof(u_int16_t),
	    sizeof(nonce) - sizeof(u_int16_t)) == 0)
		return 1;
	else
		return 0;
}

char *
dnsdecode(const u_char **sp, const u_char *ep, const u_char *base, char *buf,
    size_t bufsiz)
	/*base for compressed name*/
{
	int i = 0;
	const u_char *cp;
	char cresult[NS_MAXDNAME + 1];
	const u_char *comp;
	int l;

	cp = *sp;
	*buf = '\0';

	if (cp >= ep)
		return NULL;
	while (cp < ep) {
		i = *cp;
		if (i == 0 || cp != *sp) {
			if (strlcat((char *)buf, ".", bufsiz) >= bufsiz)
				return NULL;	/*result overrun*/
		}
		if (i == 0)
			break;
		cp++;

		if ((i & 0xc0) == 0xc0 && cp - base > (i & 0x3f)) {
			/* DNS compression */
			if (!base)
				return NULL;

			comp = base + (i & 0x3f);
			if (dnsdecode(&comp, cp, base, cresult,
			    sizeof(cresult)) == NULL)
				return NULL;
			if (strlcat(buf, cresult, bufsiz) >= bufsiz)
				return NULL;	/*result overrun*/
			break;
		} else if ((i & 0x3f) == i) {
			if (i > ep - cp)
				return NULL;	/*source overrun*/
			while (i-- > 0 && cp < ep) {
				l = snprintf(cresult, sizeof(cresult),
				    isprint(*cp) ? "%c" : "\\%03o", *cp & 0xff);
				if (l >= sizeof(cresult) || l < 0)
					return NULL;
				if (strlcat(buf, cresult, bufsiz) >= bufsiz)
					return NULL;	/*result overrun*/
				cp++;
			}
		} else
			return NULL;	/*invalid label*/
	}
	if (i != 0)
		return NULL;	/*not terminated*/
	cp++;
	*sp = cp;
	return buf;
}

/*
 * pr_pack --
 *	Print out the packet, if it came from us.  This logic is necessary
 * because ALL readers of the ICMP socket get a copy of ALL ICMP packets
 * which arrive ('tis only fair).  This permits multiple copies of this
 * program to be run without having intermingled output (or statistics!).
 */
void
pr_pack(u_char *buf, int cc, struct msghdr *mhdr)
{
#define safeputc(c)	printf((isprint((c)) ? "%c" : "\\%03o"), c)
	struct icmp6_hdr *icp;
	struct icmp6_nodeinfo *ni;
	int i;
	int hoplim;
	struct sockaddr *from;
	int fromlen;
	u_char *cp = NULL, *dp, *end = buf + cc;
	struct in6_pktinfo *pktinfo = NULL;
	struct timeval tv, tp;
	struct tv32 *tpp;
	double triptime = 0;
	int dupflag;
	size_t off;
	int oldfqdn;
	u_int16_t seq;
	char dnsname[NS_MAXDNAME + 1];
	int tclass = 0;
	int sotc = -1;

	(void)gettimeofday(&tv, NULL);

	if (!mhdr || !mhdr->msg_name ||
	    mhdr->msg_namelen != sizeof(struct sockaddr_in6) ||
	    ((struct sockaddr *)mhdr->msg_name)->sa_family != AF_INET6) {
		if (options & F_VERBOSE)
			warnx("invalid peername");
		return;
	}
	from = (struct sockaddr *)mhdr->msg_name;
	fromlen = mhdr->msg_namelen;
	if (cc < sizeof(struct icmp6_hdr)) {
		if (options & F_VERBOSE)
			warnx("packet too short (%d bytes) from %s", cc,
			    pr_addr(from, fromlen));
		return;
	}
	if (((mhdr->msg_flags & MSG_CTRUNC) != 0) &&
	    (options & F_VERBOSE) != 0)
		warnx("some control data discarded, insufficient buffer size");
	icp = (struct icmp6_hdr *)buf;
	ni = (struct icmp6_nodeinfo *)buf;
	off = 0;

	if ((hoplim = get_hoplim(mhdr)) == -1) {
		warnx("failed to get receiving hop limit");
		return;
	}
	if ((pktinfo = get_rcvpktinfo(mhdr)) == NULL) {
		warnx("failed to get receiving packet information");
		return;
	}
	if (rcvtclass && (tclass = get_tclass(mhdr)) == -1) {
		warnx("failed to get receiving traffic class");
		return;
	}

	if (use_recvmsg > 0)
		sotc = get_so_traffic_class(mhdr);

	if (icp->icmp6_type == ICMP6_ECHO_REPLY && myechoreply(icp)) {
		seq = ntohs(icp->icmp6_seq);
		++nreceived;
		if (timing) {
			tpp = (struct tv32 *)(icp + 1);
			tp.tv_sec = ntohl(tpp->tv32_sec);
			tp.tv_usec = ntohl(tpp->tv32_usec);
			tvsub(&tv, &tp);
			triptime = ((double)tv.tv_sec) * 1000.0 +
			    ((double)tv.tv_usec) / 1000.0;
			tsum += triptime;
			tsumsq += triptime * triptime;
			if (triptime < tmin)
				tmin = triptime;
			if (triptime > tmax)
				tmax = triptime;
		}

		if (TST(seq % mx_dup_ck)) {
			++nrepeats;
			--nreceived;
			dupflag = 1;
		} else {
			SET(seq % mx_dup_ck);
			dupflag = 0;
		}

		if (options & F_QUIET)
			return;

		if (options & F_WAITTIME && triptime > waittime) {
			++nrcvtimeout;
			return;
		}

		if (options & F_FLOOD)
			(void)write(STDOUT_FILENO, &BSPACE, 1);
		else {
			if (options & F_AUDIBLE)
				(void)write(STDOUT_FILENO, &BBELL, 1);
			if (options & F_PRTIME)
				pr_currenttime();
			(void)printf("%d bytes from %s, icmp_seq=%u", cc,
			    pr_addr(from, fromlen), seq);
			(void)printf(" hlim=%d", hoplim);
			if ((options & F_VERBOSE) != 0) {
				struct sockaddr_in6 dstsa;

				memset(&dstsa, 0, sizeof(dstsa));
				dstsa.sin6_family = AF_INET6;
				dstsa.sin6_len = sizeof(dstsa);
				dstsa.sin6_scope_id = pktinfo->ipi6_ifindex;
				dstsa.sin6_addr = pktinfo->ipi6_addr;
				(void)printf(" dst=%s",
				    pr_addr((struct sockaddr *)&dstsa,
				    sizeof(dstsa)));
			}
			if (timing)
				(void)printf(" time=%.3f ms", triptime);
			if (dupflag) {
				if (!IN6_IS_ADDR_MULTICAST(&dst.sin6_addr))
					(void)printf("(DUP!)");
			}
			if (rcvtclass)
				(void)printf(" tclass=%d", tclass);
			if (sotc != -1)
				(void)printf(" sotc=%d", sotc);
			/* check the data */
			cp = buf + off + ICMP6ECHOLEN + ICMP6ECHOTMLEN;
			dp = outpack + ICMP6ECHOLEN + ICMP6ECHOTMLEN;
			for (i = 8; cp < end; ++i, ++cp, ++dp) {
				if (*cp != *dp) {
					(void)printf("\nwrong data byte #%d should be 0x%x but was 0x%x", i, *dp, *cp);
					break;
				}
			}
		}
	} else if (icp->icmp6_type == ICMP6_NI_REPLY && mynireply(ni)) {
		seq = ntohs(*(u_int16_t *)ni->icmp6_ni_nonce);
		++nreceived;
		if (TST(seq % mx_dup_ck)) {
			++nrepeats;
			--nreceived;
			dupflag = 1;
		} else {
			SET(seq % mx_dup_ck);
			dupflag = 0;
		}

		if (options & F_QUIET)
			return;

		if (options & F_PRTIME)
			pr_currenttime();
		(void)printf("%d bytes from %s: ", cc, pr_addr(from, fromlen));

		switch (ntohs(ni->ni_code)) {
		case ICMP6_NI_SUCCESS:
			break;
		case ICMP6_NI_REFUSED:
			printf("refused, type 0x%x", ntohs(ni->ni_type));
			goto fqdnend;
		case ICMP6_NI_UNKNOWN:
			printf("unknown, type 0x%x", ntohs(ni->ni_type));
			goto fqdnend;
		default:
			printf("unknown code 0x%x, type 0x%x",
			    ntohs(ni->ni_code), ntohs(ni->ni_type));
			goto fqdnend;
		}

		switch (ntohs(ni->ni_qtype)) {
		case NI_QTYPE_NOOP:
			printf("NodeInfo NOOP");
			break;
		case NI_QTYPE_SUPTYPES:
			pr_suptypes(ni, end - (u_char *)ni);
			break;
		case NI_QTYPE_NODEADDR:
			pr_nodeaddr(ni, end - (u_char *)ni);
			break;
		case NI_QTYPE_FQDN:
		default:	/* XXX: for backward compatibility */
			cp = (u_char *)ni + ICMP6_NIRLEN;
			if (buf[off + ICMP6_NIRLEN] ==
			    cc - off - ICMP6_NIRLEN - 1)
				oldfqdn = 1;
			else
				oldfqdn = 0;
			if (oldfqdn) {
				cp++;	/* skip length */
				while (cp < end) {
					safeputc(*cp & 0xff);
					cp++;
				}
			} else {
				i = 0;
				while (cp < end) {
					if (dnsdecode((const u_char **)&cp, end,
					    (const u_char *)(ni + 1), dnsname,
					    sizeof(dnsname)) == NULL) {
						printf("???");
						break;
					}
					/*
					 * name-lookup special handling for
					 * truncated name
					 */
					if (cp + 1 <= end && !*cp &&
					    strlen(dnsname) > 0) {
						dnsname[strlen(dnsname) - 1] = '\0';
						cp++;
					}
					printf("%s%s", i > 0 ? "," : "",
					    dnsname);
				}
			}
			if (options & F_VERBOSE) {
				int32_t ttl;
				int comma = 0;

				(void)printf(" (");	/*)*/

				switch (ni->ni_code) {
				case ICMP6_NI_REFUSED:
					(void)printf("refused");
					comma++;
					break;
				case ICMP6_NI_UNKNOWN:
					(void)printf("unknown qtype");
					comma++;
					break;
				}

				if ((end - (u_char *)ni) < ICMP6_NIRLEN) {
					/* case of refusion, unknown */
					/*(*/
					putchar(')');
					goto fqdnend;
				}
				ttl = (int32_t)ntohl(*(u_long *)&buf[off+ICMP6ECHOLEN+8]);
				if (comma)
					printf(",");
				if (!(ni->ni_flags & NI_FQDN_FLAG_VALIDTTL)) {
					(void)printf("TTL=%d:meaningless",
					    (int)ttl);
				} else {
					if (ttl < 0) {
						(void)printf("TTL=%d:invalid",
						   ttl);
					} else
						(void)printf("TTL=%d", ttl);
				}
				comma++;

				if (oldfqdn) {
					if (comma)
						printf(",");
					printf("03 draft");
					comma++;
				} else {
					cp = (u_char *)ni + ICMP6_NIRLEN;
					if (cp == end) {
						if (comma)
							printf(",");
						printf("no name");
						comma++;
					}
				}

				if (buf[off + ICMP6_NIRLEN] !=
				    cc - off - ICMP6_NIRLEN - 1 && oldfqdn) {
					if (comma)
						printf(",");
					(void)printf("invalid namelen:%d/%lu",
					    buf[off + ICMP6_NIRLEN],
					    (u_long)cc - off - ICMP6_NIRLEN - 1);
					comma++;
				}
				/*(*/
				putchar(')');
			}
		fqdnend:
			;
		}
	} else {
		/* We've got something other than an ECHOREPLY */
		if (!(options & F_VERBOSE))
			return;
		if (options & F_PRTIME)
			pr_currenttime();
		(void)printf("%d bytes from %s: ", cc, pr_addr(from, fromlen));
		pr_icmph(icp, end);
	}

	if (!(options & F_FLOOD)) {
		(void)putchar('\n');
		if (options & F_VERBOSE)
			pr_exthdrs(mhdr);
		(void)fflush(stdout);
	}
#undef safeputc
}

void
pr_exthdrs(struct msghdr *mhdr)
{
	ssize_t	bufsize;
	void	*bufp;
	struct cmsghdr *cm;

	bufsize = 0;
	bufp = mhdr->msg_control;
	for (cm = (struct cmsghdr *)CMSG_FIRSTHDR(mhdr); cm;
	     cm = (struct cmsghdr *)CMSG_NXTHDR(mhdr, cm)) {
		if (cm->cmsg_level != IPPROTO_IPV6)
			continue;

		bufsize = CONTROLLEN - ((caddr_t)CMSG_DATA(cm) - (caddr_t)bufp);
		if (bufsize <= 0)
			continue;
		switch (cm->cmsg_type) {
		case IPV6_HOPOPTS:
			printf("  HbH Options: ");
			pr_ip6opt(CMSG_DATA(cm), (size_t)bufsize);
			break;
		case IPV6_DSTOPTS:
#ifdef IPV6_RTHDRDSTOPTS
		case IPV6_RTHDRDSTOPTS:
#endif
			printf("  Dst Options: ");
			pr_ip6opt(CMSG_DATA(cm), (size_t)bufsize);
			break;
		case IPV6_RTHDR:
			printf("  Routing: ");
			pr_rthdr(CMSG_DATA(cm), (size_t)bufsize);
			break;
		}
	}
}

#ifdef USE_RFC2292BIS
void
pr_ip6opt(void *extbuf, size_t bufsize)
{
	struct ip6_hbh *ext;
	int currentlen;
	u_int8_t type;
	socklen_t extlen, len;
	void *databuf;
	size_t offset;
	u_int16_t value2;
	u_int32_t value4;

	ext = (struct ip6_hbh *)extbuf;
	extlen = (ext->ip6h_len + 1) * 8;
	printf("nxt %u, len %u (%lu bytes)\n", ext->ip6h_nxt,
	    (unsigned int)ext->ip6h_len, (unsigned long)extlen);

	/*
	 * Bounds checking on the ancillary data buffer:
	 *     subtract the size of a cmsg structure from the buffer size.
	 */
	if (bufsize < (extlen  + CMSG_SPACE(0))) {
		extlen = bufsize - CMSG_SPACE(0);
		warnx("options truncated, showing only %u (total=%u)",
		    (unsigned int)(extlen / 8 - 1),
		    (unsigned int)(ext->ip6h_len));
	}

	currentlen = 0;
	while (1) {
		currentlen = inet6_opt_next(extbuf, extlen, currentlen,
		    &type, &len, &databuf);
		if (currentlen == -1)
			break;
		switch (type) {
		/*
		 * Note that inet6_opt_next automatically skips any padding
		 * optins.
		 */
		case IP6OPT_JUMBO:
			offset = 0;
			(void) inet6_opt_get_val(databuf, offset,
			    &value4, sizeof(value4));
			printf("    Jumbo Payload Opt: Length %u\n",
			    (u_int32_t)ntohl(value4));
			break;
		case IP6OPT_ROUTER_ALERT:
			offset = 0;
			(void)inet6_opt_get_val(databuf, offset,
						   &value2, sizeof(value2));
			printf("    Router Alert Opt: Type %u\n",
			    ntohs(value2));
			break;
		default:
			printf("    Received Opt %u len %lu\n",
			    type, (unsigned long)len);
			break;
		}
	}
	return;
}
#else  /* !USE_RFC2292BIS */
/* ARGSUSED */
void
pr_ip6opt(void *extbuf, size_t bufsize __unused)
{
	putchar('\n');
	return;
}
#endif /* USE_RFC2292BIS */

#ifdef USE_RFC2292BIS
void
pr_rthdr(void *extbuf, size_t bufsize)
{
	struct in6_addr *in6;
	char ntopbuf[INET6_ADDRSTRLEN];
	struct ip6_rthdr *rh = (struct ip6_rthdr *)extbuf;
	int i, segments, origsegs, rthsize, size0, size1;

	/* print fixed part of the header */
	printf("nxt %u, len %u (%d bytes), type %u, ", rh->ip6r_nxt,
	    rh->ip6r_len, (rh->ip6r_len + 1) << 3, rh->ip6r_type);
	if ((segments = inet6_rth_segments(extbuf)) >= 0) {
		printf("%d segments, ", segments);
		printf("%d left\n", rh->ip6r_segleft);
	} else {
		printf("segments unknown, ");
		printf("%d left\n", rh->ip6r_segleft);
		return;
	}

	/*
	 * Bounds checking on the ancillary data buffer. When calculating
	 * the number of items to show keep in mind:
	 *	- The size of the cmsg structure
	 *	- The size of one segment (the size of a Type 0 routing header)
	 *	- When dividing add a fudge factor of one in case the
	 *	  dividend is not evenly divisible by the divisor
	 */
	rthsize = (rh->ip6r_len + 1) * 8;
	if (bufsize < (rthsize + CMSG_SPACE(0))) {
		origsegs = segments;
		size0 = inet6_rth_space(IPV6_RTHDR_TYPE_0, 0);
		size1 = inet6_rth_space(IPV6_RTHDR_TYPE_0, 1);
		segments -= (rthsize - (bufsize - CMSG_SPACE(0))) /
		    (size1 - size0) + 1;
		warnx("segments truncated, showing only %d (total=%d)",
		    segments, origsegs);
	}

	for (i = 0; i < segments; i++) {
		in6 = inet6_rth_getaddr(extbuf, i);
		if (in6 == NULL)
			printf("   [%d]<NULL>\n", i);
		else {
			if (!inet_ntop(AF_INET6, in6, ntopbuf,
			    sizeof(ntopbuf)))
				strlcpy(ntopbuf, "?", sizeof(ntopbuf));
			printf("   [%d]%s\n", i, ntopbuf);
		}
	}

	return;

}

#else  /* !USE_RFC2292BIS */
/* ARGSUSED */
void
pr_rthdr(void *extbuf, size_t bufsize __unused)
{
	putchar('\n');
	return;
}
#endif /* USE_RFC2292BIS */

int
pr_bitrange(u_int32_t v, int soff, int ii)
{
	int off;
	int i;

	off = 0;
	while (off < 32) {
		/* shift till we have 0x01 */
		if ((v & 0x01) == 0) {
			if (ii > 1)
				printf("-%u", soff + off - 1);
			ii = 0;
			switch (v & 0x0f) {
			case 0x00:
				v >>= 4;
				off += 4;
				continue;
			case 0x08:
				v >>= 3;
				off += 3;
				continue;
			case 0x04: case 0x0c:
				v >>= 2;
				off += 2;
				continue;
			default:
				v >>= 1;
				off += 1;
				continue;
			}
		}

		/* we have 0x01 with us */
		for (i = 0; i < 32 - off; i++) {
			if ((v & (0x01 << i)) == 0)
				break;
		}
		if (!ii)
			printf(" %u", soff + off);
		ii += i;
		v >>= i; off += i;
	}
	return ii;
}

void
pr_suptypes(struct icmp6_nodeinfo *ni, size_t nilen)
	/* ni->qtype must be SUPTYPES */
{
	size_t clen;
	u_int32_t v;
	const u_char *cp, *end;
	u_int16_t cur;
	struct cbit {
		u_int16_t words;	/*32bit count*/
		u_int16_t skip;
	} cbit = { 0, 0 };
#define MAXQTYPES	(1 << 16)
	size_t off;
	int b;

	cp = (u_char *)(ni + 1);
	end = ((u_char *)ni) + nilen;
	cur = 0;
	b = 0;

	printf("NodeInfo Supported Qtypes");
	if (options & F_VERBOSE) {
		if (ni->ni_flags & NI_SUPTYPE_FLAG_COMPRESS)
			printf(", compressed bitmap");
		else
			printf(", raw bitmap");
	}

	while (cp < end) {
		clen = (size_t)(end - cp);
		if ((ni->ni_flags & NI_SUPTYPE_FLAG_COMPRESS) == 0) {
			if (clen == 0 || clen > MAXQTYPES / 8 ||
			    clen % sizeof(v)) {
				printf("???");
				return;
			}
		} else {
			if (clen < sizeof(cbit) || clen % sizeof(v))
				return;
			memcpy(&cbit, cp, sizeof(cbit));
			if (sizeof(cbit) + ntohs(cbit.words) * sizeof(v) >
			    clen)
				return;
			cp += sizeof(cbit);
			clen = ntohs(cbit.words) * sizeof(v);
			if (cur + clen * 8 + (u_long)ntohs(cbit.skip) * 32 >
			    MAXQTYPES)
				return;
		}

		for (off = 0; off < clen; off += sizeof(v)) {
			memcpy(&v, cp + off, sizeof(v));
			v = (u_int32_t)ntohl(v);
			b = pr_bitrange(v, (int)(cur + off * 8), b);
		}
		/* flush the remaining bits */
		b = pr_bitrange(0, (int)(cur + off * 8), b);

		cp += clen;
		cur += clen * 8;
		if ((ni->ni_flags & NI_SUPTYPE_FLAG_COMPRESS) != 0)
			cur += ntohs(cbit.skip) * 32;
	}
}

void
pr_nodeaddr(struct icmp6_nodeinfo *ni, int nilen)
	/* ni->qtype must be NODEADDR */
{
	u_char *cp = (u_char *)(ni + 1);
	char ntop_buf[INET6_ADDRSTRLEN];
	int withttl = 0;

	nilen -= sizeof(struct icmp6_nodeinfo);

	if (options & F_VERBOSE) {
		switch (ni->ni_code) {
		case ICMP6_NI_REFUSED:
			(void)printf("refused");
			break;
		case ICMP6_NI_UNKNOWN:
			(void)printf("unknown qtype");
			break;
		}
		if (ni->ni_flags & NI_NODEADDR_FLAG_TRUNCATE)
			(void)printf(" truncated");
	}
	putchar('\n');
	if (nilen <= 0)
		printf("  no address\n");

	/*
	 * In icmp-name-lookups 05 and later, TTL of each returned address
	 * is contained in the resposne. We try to detect the version
	 * by the length of the data, but note that the detection algorithm
	 * is incomplete. We assume the latest draft by default.
	 */
	if (nilen % (sizeof(u_int32_t) + sizeof(struct in6_addr)) == 0)
		withttl = 1;
	while (nilen > 0) {
		u_int32_t ttl = 0;

		if (withttl) {
			/* XXX: alignment? */
			ttl = (u_int32_t)ntohl(*(u_int32_t *)cp);
			cp += sizeof(u_int32_t);
			nilen -= sizeof(u_int32_t);
		}

		if (inet_ntop(AF_INET6, cp, ntop_buf, sizeof(ntop_buf)) ==
		    NULL)
			strlcpy(ntop_buf, "?", sizeof(ntop_buf));
		printf("  %s", ntop_buf);
		if (withttl) {
			if (ttl == 0xffffffff) {
				/*
				 * XXX: can this convention be applied to all
				 * type of TTL (i.e. non-ND TTL)?
				 */
				printf("(TTL=infty)");
			}
			else
				printf("(TTL=%u)", ttl);
		}
		putchar('\n');

		nilen -= sizeof(struct in6_addr);
		cp += sizeof(struct in6_addr);
	}
}

int
get_hoplim(struct msghdr *mhdr)
{
	struct cmsghdr *cm;

	for (cm = (struct cmsghdr *)CMSG_FIRSTHDR(mhdr); cm;
	     cm = (struct cmsghdr *)CMSG_NXTHDR(mhdr, cm)) {
		if (cm->cmsg_len == 0)
			return(-1);

		if (cm->cmsg_level == IPPROTO_IPV6 &&
		    cm->cmsg_type == IPV6_HOPLIMIT &&
		    cm->cmsg_len == CMSG_LEN(sizeof(int)))
			return(*(int *)CMSG_DATA(cm));
	}

	return(-1);
}

struct in6_pktinfo *
get_rcvpktinfo(struct msghdr *mhdr)
{
	struct cmsghdr *cm;

	for (cm = (struct cmsghdr *)CMSG_FIRSTHDR(mhdr); cm;
	     cm = (struct cmsghdr *)CMSG_NXTHDR(mhdr, cm)) {
		if (cm->cmsg_len == 0)
			return(NULL);

		if (cm->cmsg_level == IPPROTO_IPV6 &&
		    cm->cmsg_type == IPV6_PKTINFO &&
		    cm->cmsg_len == CMSG_LEN(sizeof(struct in6_pktinfo)))
			return((struct in6_pktinfo *)CMSG_DATA(cm));
	}

	return(NULL);
}

int
get_pathmtu(struct msghdr *mhdr)
{
#ifdef IPV6_RECVPATHMTU
	struct cmsghdr *cm;
	struct ip6_mtuinfo *mtuctl = NULL;

	for (cm = (struct cmsghdr *)CMSG_FIRSTHDR(mhdr); cm;
	     cm = (struct cmsghdr *)CMSG_NXTHDR(mhdr, cm)) {
		if (cm->cmsg_len == 0)
			return(0);

		if (cm->cmsg_level == IPPROTO_IPV6 &&
		    cm->cmsg_type == IPV6_PATHMTU &&
		    cm->cmsg_len == CMSG_LEN(sizeof(struct ip6_mtuinfo))) {
			mtuctl = (struct ip6_mtuinfo *)CMSG_DATA(cm);

			/*
			 * If the notified destination is different from
			 * the one we are pinging, just ignore the info.
			 * We check the scope ID only when both notified value
			 * and our own value have non-0 values, because we may
			 * have used the default scope zone ID for sending,
			 * in which case the scope ID value is 0.
			 */
			if (!IN6_ARE_ADDR_EQUAL(&mtuctl->ip6m_addr.sin6_addr,
						&dst.sin6_addr) ||
			    (mtuctl->ip6m_addr.sin6_scope_id &&
			     dst.sin6_scope_id &&
			     mtuctl->ip6m_addr.sin6_scope_id !=
			     dst.sin6_scope_id)) {
				if ((options & F_VERBOSE) != 0) {
					printf("path MTU for %s is notified. "
					       "(ignored)\n",
					   pr_addr((struct sockaddr *)&mtuctl->ip6m_addr,
					   sizeof(mtuctl->ip6m_addr)));
				}
				return(0);
			}

			/*
			 * Ignore an invalid MTU. XXX: can we just believe
			 * the kernel check?
			 */
			if (mtuctl->ip6m_mtu < IPV6_MMTU)
				return(0);

			/* notification for our destination. return the MTU. */
			return((int)mtuctl->ip6m_mtu);
		}
	}
#endif
	return(0);
}

int
get_tclass(mhdr)
	struct msghdr *mhdr;
{
	struct cmsghdr *cm;

	for (cm = (struct cmsghdr *)CMSG_FIRSTHDR(mhdr); cm;
	     cm = (struct cmsghdr *)CMSG_NXTHDR(mhdr, cm)) {
		if (cm->cmsg_len == 0)
			return(-1);

		if (cm->cmsg_level == IPPROTO_IPV6 &&
		    cm->cmsg_type == IPV6_TCLASS &&
		    cm->cmsg_len == CMSG_LEN(sizeof(int)))
			return(*(int *)CMSG_DATA(cm));
	}

	return(-1);
}

int
get_so_traffic_class(struct msghdr *mhdr)
{
	struct cmsghdr *cm;

	for (cm = (struct cmsghdr *)CMSG_FIRSTHDR(mhdr); cm;
	     cm = (struct cmsghdr *)CMSG_NXTHDR(mhdr, cm)) {
		if (cm->cmsg_len == 0)
			return(-1);

		if (cm->cmsg_level == SOL_SOCKET &&
		    cm->cmsg_type == SO_TRAFFIC_CLASS &&
		    cm->cmsg_len == CMSG_LEN(sizeof(int)))
			return(*(int *)CMSG_DATA(cm));
	}

	return(-1);
}

/*
 * tvsub --
 *	Subtract 2 timeval structs:  out = out - in.  Out is assumed to
 * be >= in.
 */
void
tvsub(struct timeval *out, struct timeval *in)
{
	if ((out->tv_usec -= in->tv_usec) < 0) {
		--out->tv_sec;
		out->tv_usec += 1000000;
	}
	out->tv_sec -= in->tv_sec;
}

/*
 * onint --
 *	SIGINT handler.
 */
/* ARGSUSED */
void
onint(int notused __unused)
{
	/*
	 * When doing reverse DNS lookups, the seenint flag might not
	 * be noticed for a while.  Just exit if we get a second SIGINT.
	 */
	if ((options & F_HOSTNAME) && seenint != 0)
		_exit(nreceived ? 0 : 2);
}

/*
 * summary --
 *	Print out statistics.
 */
void
summary(void)
{
	(void)printf("\n--- %s ping6 statistics ---\n", hostname);
	(void)printf("%ld packets transmitted, ", ntransmitted);
	(void)printf("%ld packets received, ", nreceived);
	if (nrepeats)
		(void)printf("+%ld duplicates, ", nrepeats);
	if (ntransmitted) {
		if (nreceived > ntransmitted)
			(void)printf("-- somebody's duplicating packets!");
		else
			(void)printf("%.1f%% packet loss",
			    ((((double)ntransmitted - nreceived) * 100.0) /
			    ntransmitted));
	}
	if (nrcvtimeout)
		printf(", %ld packets out of wait time", nrcvtimeout);
	(void)putchar('\n');
	if (nreceived && timing) {
		/* Only display average to microseconds */
		double num = nreceived + nrepeats;
		double avg = tsum / num;
		double dev = sqrt(tsumsq / num - avg * avg);
		(void)printf(
		    "round-trip min/avg/max/std-dev = %.3f/%.3f/%.3f/%.3f ms\n",
		    tmin, avg, tmax, dev);
		(void)fflush(stdout);
	}
	(void)fflush(stdout);
}

/*subject type*/
static const char *niqcode[] = {
	"IPv6 address",
	"DNS label",	/*or empty*/
	"IPv4 address",
};

/*result code*/
static const char *nircode[] = {
	"Success", "Refused", "Unknown",
};

/*
 * pr_icmph --
 *	Print a descriptive string about an ICMP header.
 */
void
pr_icmph(struct icmp6_hdr *icp, u_char *end)
{
	char ntop_buf[INET6_ADDRSTRLEN];
	struct nd_redirect *red;
	struct icmp6_nodeinfo *ni;
	char dnsname[NS_MAXDNAME + 1];
	const u_char *cp;
	size_t l;

	switch (icp->icmp6_type) {
	case ICMP6_DST_UNREACH:
		switch (icp->icmp6_code) {
		case ICMP6_DST_UNREACH_NOROUTE:
			(void)printf("No Route to Destination\n");
			break;
		case ICMP6_DST_UNREACH_ADMIN:
			(void)printf("Destination Administratively "
			    "Unreachable\n");
			break;
		case ICMP6_DST_UNREACH_BEYONDSCOPE:
			(void)printf("Destination Unreachable Beyond Scope\n");
			break;
		case ICMP6_DST_UNREACH_ADDR:
			(void)printf("Destination Host Unreachable\n");
			break;
		case ICMP6_DST_UNREACH_NOPORT:
			(void)printf("Destination Port Unreachable\n");
			break;
		default:
			(void)printf("Destination Unreachable, Bad Code: %d\n",
			    icp->icmp6_code);
			break;
		}
		/* Print returned IP header information */
		pr_retip((struct ip6_hdr *)(icp + 1), end);
		break;
	case ICMP6_PACKET_TOO_BIG:
		(void)printf("Packet too big mtu = %d\n",
		    (int)ntohl(icp->icmp6_mtu));
		pr_retip((struct ip6_hdr *)(icp + 1), end);
		break;
	case ICMP6_TIME_EXCEEDED:
		switch (icp->icmp6_code) {
		case ICMP6_TIME_EXCEED_TRANSIT:
			(void)printf("Time to live exceeded\n");
			break;
		case ICMP6_TIME_EXCEED_REASSEMBLY:
			(void)printf("Frag reassembly time exceeded\n");
			break;
		default:
			(void)printf("Time exceeded, Bad Code: %d\n",
			    icp->icmp6_code);
			break;
		}
		pr_retip((struct ip6_hdr *)(icp + 1), end);
		break;
	case ICMP6_PARAM_PROB:
		(void)printf("Parameter problem: ");
		switch (icp->icmp6_code) {
		case ICMP6_PARAMPROB_HEADER:
			(void)printf("Erroneous Header ");
			break;
		case ICMP6_PARAMPROB_NEXTHEADER:
			(void)printf("Unknown Nextheader ");
			break;
		case ICMP6_PARAMPROB_OPTION:
			(void)printf("Unrecognized Option ");
			break;
		default:
			(void)printf("Bad code(%d) ", icp->icmp6_code);
			break;
		}
		(void)printf("pointer = 0x%02x\n",
		    (u_int32_t)ntohl(icp->icmp6_pptr));
		pr_retip((struct ip6_hdr *)(icp + 1), end);
		break;
	case ICMP6_ECHO_REQUEST:
		(void)printf("Echo Request");
		/* XXX ID + Seq + Data */
		break;
	case ICMP6_ECHO_REPLY:
		(void)printf("Echo Reply");
		/* XXX ID + Seq + Data */
		break;
	case ICMP6_MEMBERSHIP_QUERY:
		(void)printf("Listener Query");
		break;
	case ICMP6_MEMBERSHIP_REPORT:
		(void)printf("Listener Report");
		break;
	case ICMP6_MEMBERSHIP_REDUCTION:
		(void)printf("Listener Done");
		break;
	case ND_ROUTER_SOLICIT:
		(void)printf("Router Solicitation");
		break;
	case ND_ROUTER_ADVERT:
		(void)printf("Router Advertisement");
		break;
	case ND_NEIGHBOR_SOLICIT:
		(void)printf("Neighbor Solicitation");
		break;
	case ND_NEIGHBOR_ADVERT:
		(void)printf("Neighbor Advertisement");
		break;
	case ND_REDIRECT:
		red = (struct nd_redirect *)icp;
		(void)printf("Redirect\n");
		if (!inet_ntop(AF_INET6, &red->nd_rd_dst, ntop_buf,
		    sizeof(ntop_buf)))
			strlcpy(ntop_buf, "?", sizeof(ntop_buf));
		(void)printf("Destination: %s", ntop_buf);
		if (!inet_ntop(AF_INET6, &red->nd_rd_target, ntop_buf,
		    sizeof(ntop_buf)))
			strlcpy(ntop_buf, "?", sizeof(ntop_buf));
		(void)printf(" New Target: %s", ntop_buf);
		break;
	case ICMP6_NI_QUERY:
		(void)printf("Node Information Query");
		/* XXX ID + Seq + Data */
		ni = (struct icmp6_nodeinfo *)icp;
		l = end - (u_char *)(ni + 1);
		printf(", ");
		switch (ntohs(ni->ni_qtype)) {
		case NI_QTYPE_NOOP:
			(void)printf("NOOP");
			break;
		case NI_QTYPE_SUPTYPES:
			(void)printf("Supported qtypes");
			break;
		case NI_QTYPE_FQDN:
			(void)printf("DNS name");
			break;
		case NI_QTYPE_NODEADDR:
			(void)printf("nodeaddr");
			break;
		case NI_QTYPE_IPV4ADDR:
			(void)printf("IPv4 nodeaddr");
			break;
		default:
			(void)printf("unknown qtype");
			break;
		}
		if (options & F_VERBOSE) {
			switch (ni->ni_code) {
			case ICMP6_NI_SUBJ_IPV6:
				if (l == sizeof(struct in6_addr) &&
				    inet_ntop(AF_INET6, ni + 1, ntop_buf,
				    sizeof(ntop_buf)) != NULL) {
					(void)printf(", subject=%s(%s)",
					    niqcode[ni->ni_code], ntop_buf);
				} else {
#if 1
					/* backward compat to -W */
					(void)printf(", oldfqdn");
#else
					(void)printf(", invalid");
#endif
				}
				break;
			case ICMP6_NI_SUBJ_FQDN:
				if (end == (u_char *)(ni + 1)) {
					(void)printf(", no subject");
					break;
				}
				printf(", subject=%s", niqcode[ni->ni_code]);
				cp = (const u_char *)(ni + 1);
				if (dnsdecode(&cp, end, NULL, dnsname,
				    sizeof(dnsname)) != NULL)
					printf("(%s)", dnsname);
				else
					printf("(invalid)");
				break;
			case ICMP6_NI_SUBJ_IPV4:
				if (l == sizeof(struct in_addr) &&
				    inet_ntop(AF_INET, ni + 1, ntop_buf,
				    sizeof(ntop_buf)) != NULL) {
					(void)printf(", subject=%s(%s)",
					    niqcode[ni->ni_code], ntop_buf);
				} else
					(void)printf(", invalid");
				break;
			default:
				(void)printf(", invalid");
				break;
			}
		}
		break;
	case ICMP6_NI_REPLY:
		(void)printf("Node Information Reply");
		/* XXX ID + Seq + Data */
		ni = (struct icmp6_nodeinfo *)icp;
		printf(", ");
		switch (ntohs(ni->ni_qtype)) {
		case NI_QTYPE_NOOP:
			(void)printf("NOOP");
			break;
		case NI_QTYPE_SUPTYPES:
			(void)printf("Supported qtypes");
			break;
		case NI_QTYPE_FQDN:
			(void)printf("DNS name");
			break;
		case NI_QTYPE_NODEADDR:
			(void)printf("nodeaddr");
			break;
		case NI_QTYPE_IPV4ADDR:
			(void)printf("IPv4 nodeaddr");
			break;
		default:
			(void)printf("unknown qtype");
			break;
		}
		if (options & F_VERBOSE) {
			if (ni->ni_code > sizeof(nircode) / sizeof(nircode[0]))
				printf(", invalid");
			else
				printf(", %s", nircode[ni->ni_code]);
		}
		break;
	default:
		(void)printf("Bad ICMP type: %d", icp->icmp6_type);
	}
}

/*
 * pr_iph --
 *	Print an IP6 header.
 */
void
pr_iph(struct ip6_hdr *ip6)
{
	u_int32_t flow = ip6->ip6_flow & IPV6_FLOWLABEL_MASK;
	u_int8_t tc;
	char ntop_buf[INET6_ADDRSTRLEN];

	tc = *(&ip6->ip6_vfc + 1); /* XXX */
	tc = (tc >> 4) & 0x0f;
	tc |= (ip6->ip6_vfc << 4);

	printf("Vr TC  Flow Plen Nxt Hlim\n");
	printf(" %1x %02x %05x %04x  %02x   %02x\n",
	    (ip6->ip6_vfc & IPV6_VERSION_MASK) >> 4, tc, (u_int32_t)ntohl(flow),
	    ntohs(ip6->ip6_plen), ip6->ip6_nxt, ip6->ip6_hlim);
	if (!inet_ntop(AF_INET6, &ip6->ip6_src, ntop_buf, sizeof(ntop_buf)))
		strlcpy(ntop_buf, "?", sizeof(ntop_buf));
	printf("%s->", ntop_buf);
	if (!inet_ntop(AF_INET6, &ip6->ip6_dst, ntop_buf, sizeof(ntop_buf)))
		strlcpy(ntop_buf, "?", sizeof(ntop_buf));
	printf("%s\n", ntop_buf);
}

/*
 * pr_addr --
 *	Return an ascii host address as a dotted quad and optionally with
 * a hostname.
 */
const char *
pr_addr(struct sockaddr *addr, int addrlen)
{
	static char buf[NI_MAXHOST];
	int flag = 0;

	if ((options & F_HOSTNAME) == 0)
		flag |= NI_NUMERICHOST;

	if (getnameinfo(addr, addrlen, buf, sizeof(buf), NULL, 0, flag) == 0)
		return (buf);
	else
		return "?";
}

/*
 * pr_retip --
 *	Dump some info on a returned (via ICMPv6) IPv6 packet.
 */
void
pr_retip(struct ip6_hdr *ip6, u_char *end)
{
	u_char *cp = (u_char *)ip6, nh;
	int hlen;

	if (end - (u_char *)ip6 < sizeof(*ip6)) {
		printf("IP6");
		goto trunc;
	}
	pr_iph(ip6);
	hlen = sizeof(*ip6);

	nh = ip6->ip6_nxt;
	cp += hlen;
	while (end - cp >= 8) {
		switch (nh) {
		case IPPROTO_HOPOPTS:
			printf("HBH ");
			hlen = (((struct ip6_hbh *)cp)->ip6h_len+1) << 3;
			nh = ((struct ip6_hbh *)cp)->ip6h_nxt;
			break;
		case IPPROTO_DSTOPTS:
			printf("DSTOPT ");
			hlen = (((struct ip6_dest *)cp)->ip6d_len+1) << 3;
			nh = ((struct ip6_dest *)cp)->ip6d_nxt;
			break;
		case IPPROTO_FRAGMENT:
			printf("FRAG ");
			hlen = sizeof(struct ip6_frag);
			nh = ((struct ip6_frag *)cp)->ip6f_nxt;
			break;
		case IPPROTO_ROUTING:
			printf("RTHDR ");
			hlen = (((struct ip6_rthdr *)cp)->ip6r_len+1) << 3;
			nh = ((struct ip6_rthdr *)cp)->ip6r_nxt;
			break;
#ifdef IPSEC
		case IPPROTO_AH:
			printf("AH ");
			hlen = (((struct ah *)cp)->ah_len+2) << 2;
			nh = ((struct ah *)cp)->ah_nxt;
			break;
#endif
		case IPPROTO_ICMPV6:
			printf("ICMP6: type = %d, code = %d\n",
			    *cp, *(cp + 1));
			return;
		case IPPROTO_ESP:
			printf("ESP\n");
			return;
		case IPPROTO_TCP:
			printf("TCP: from port %u, to port %u (decimal)\n",
			    (*cp * 256 + *(cp + 1)),
			    (*(cp + 2) * 256 + *(cp + 3)));
			return;
		case IPPROTO_UDP:
			printf("UDP: from port %u, to port %u (decimal)\n",
			    (*cp * 256 + *(cp + 1)),
			    (*(cp + 2) * 256 + *(cp + 3)));
			return;
		default:
			printf("Unknown Header(%d)\n", nh);
			return;
		}

		if ((cp += hlen) >= end)
			goto trunc;
	}
	if (end - cp < 8)
		goto trunc;

	putchar('\n');
	return;

  trunc:
	printf("...\n");
	return;
}

void
fill(char *bp, char *patp)
{
	int ii, jj, kk;
	int pat[16];
	char *cp;

	for (cp = patp; *cp; cp++)
		if (!isxdigit(*cp))
			errx(1, "patterns must be specified as hex digits");
	ii = sscanf(patp,
	    "%2x%2x%2x%2x%2x%2x%2x%2x%2x%2x%2x%2x%2x%2x%2x%2x",
	    &pat[0], &pat[1], &pat[2], &pat[3], &pat[4], &pat[5], &pat[6],
	    &pat[7], &pat[8], &pat[9], &pat[10], &pat[11], &pat[12],
	    &pat[13], &pat[14], &pat[15]);

/* xxx */
	if (ii > 0)
		for (kk = 0;
		    kk <= MAXDATALEN - (8 + sizeof(struct tv32) + ii);
		    kk += ii)
			for (jj = 0; jj < ii; ++jj)
				bp[jj + kk] = pat[jj];
	if (!(options & F_QUIET)) {
		(void)printf("PATTERN: 0x");
		for (jj = 0; jj < ii; ++jj)
			(void)printf("%02x", bp[jj] & 0xFF);
		(void)printf("\n");
	}
}

#ifdef IPSEC
#ifdef IPSEC_POLICY_IPSEC
int
setpolicy(int so __unused, char *policy)
{
	char *buf;

	if (policy == NULL)
		return 0;	/* ignore */

	buf = ipsec_set_policy(policy, strlen(policy));
	if (buf == NULL)
		errx(1, "%s", ipsec_strerror());
	if (setsockopt(s, IPPROTO_IPV6, IPV6_IPSEC_POLICY, buf,
	    ipsec_get_policylen(buf)) < 0)
		warnx("Unable to set IPsec policy");
	free(buf);

	return 0;
}
#endif
#endif

char *
nigroup(char *name, int nig_oldmcprefix)
{
	char *p;
	char *q;
	MD5_CTX ctxt;
	u_int8_t digest[16];
	u_int8_t c;
	size_t l;
	char hbuf[NI_MAXHOST];
	struct in6_addr in6;
	int valid;

	p = strchr(name, '.');
	if (!p)
		p = name + strlen(name);
	l = p - name;
	if (l > 63 || l > sizeof(hbuf) - 1)
		return NULL;	/*label too long*/
	strlcpy(hbuf, name, l);
	hbuf[(int)l] = '\0';

	for (q = name; *q; q++) {
		if (isupper(*(unsigned char *)q))
			*q = tolower(*(unsigned char *)q);
	}

	/* generate 16 bytes of pseudo-random value. */
	memset(&ctxt, 0, sizeof(ctxt));
	MD5Init(&ctxt);
	c = l & 0xff;
	MD5Update(&ctxt, &c, sizeof(c));
	MD5Update(&ctxt, (unsigned char *)name, l);
	MD5Final(digest, &ctxt);

	if (nig_oldmcprefix) {
		/* draft-ietf-ipngwg-icmp-name-lookup */
		valid = inet_pton(AF_INET6, "ff02::2:0000:0000", &in6);
	} else {
		/* RFC 4620 */
		valid = inet_pton(AF_INET6, "ff02::2:ff00:0000", &in6);
	}
	if (valid != 1)
		return NULL;	/*XXX*/

	if (nig_oldmcprefix) {
		/* draft-ietf-ipngwg-icmp-name-lookup */
	bcopy(digest, &in6.s6_addr[12], 4);
	} else {
		/* RFC 4620 */
		bcopy(digest, &in6.s6_addr[13], 3);
	}

	if (inet_ntop(AF_INET6, &in6, hbuf, sizeof(hbuf)) == NULL)
		return NULL;

	return strdup(hbuf);
}

int
str2sotc(const char *str, bool *valid)
{
	int sotc = -1;
	char *endptr;

	*valid = true;

	if (str == NULL || *str == '\0')
		*valid = false;
	else if (strcasecmp(str, "BK_SYS") == 0)
		return SO_TC_BK_SYS;
	else if (strcasecmp(str, "BK") == 0)
		return SO_TC_BK;
	else if (strcasecmp(str, "BE") == 0)
		return SO_TC_BE;
	else if (strcasecmp(str, "RD") == 0)
		return SO_TC_RD;
	else if (strcasecmp(str, "OAM") == 0)
		return SO_TC_OAM;
	else if (strcasecmp(str, "AV") == 0)
		return SO_TC_AV;
	else if (strcasecmp(str, "RV") == 0)
		return SO_TC_RV;
	else if (strcasecmp(str, "VI") == 0)
		return SO_TC_VI;
	else if (strcasecmp(str, "VO") == 0)
		return SO_TC_VO;
	else if (strcasecmp(str, "CTL") == 0)
		return SO_TC_CTL;
	else {
		sotc = (int)strtol(str, &endptr, 0);
		if (*endptr != '\0')
			*valid = false;
	}
	return (sotc);
}

int
str2netservicetype(const char *str, bool *valid)
{
	int svc = -1;
	char *endptr;

	*valid = true;

	if (str == NULL || *str == '\0')
		*valid = false;
	else if (strcasecmp(str, "BK") == 0)
		return NET_SERVICE_TYPE_BK;
	else if (strcasecmp(str, "BE") == 0)
		return NET_SERVICE_TYPE_BE;
	else if (strcasecmp(str, "VI") == 0)
		return NET_SERVICE_TYPE_VI;
	else if (strcasecmp(str, "SIG") == 0)
		return NET_SERVICE_TYPE_SIG;
	else if (strcasecmp(str, "VO") == 0)
		return NET_SERVICE_TYPE_VO;
	else if (strcasecmp(str, "RV") == 0)
		return NET_SERVICE_TYPE_RV;
	else if (strcasecmp(str, "AV") == 0)
		return NET_SERVICE_TYPE_AV;
	else if (strcasecmp(str, "OAM") == 0)
		return NET_SERVICE_TYPE_OAM;
	else if (strcasecmp(str, "RD") == 0)
		return NET_SERVICE_TYPE_RD;
	else {
		svc = (int)strtol(str, &endptr, 0);
		if (*endptr != '\0')
			*valid = false;
	}
	return (svc);
}

u_int8_t
str2tclass(const char *str, bool *valid)
{
	u_int8_t dscp = -1;
	char *endptr;

	*valid = true;

	if (str == NULL || *str == '\0')
		*valid = false;
	else if (strcasecmp(str, "DF") == 0)
		dscp = _DSCP_DF;
	else if (strcasecmp(str, "EF") == 0)
		dscp = _DSCP_EF;
	else if (strcasecmp(str, "VA") == 0)
		dscp = _DSCP_VA;

	else if (strcasecmp(str, "CS0") == 0)
		dscp = _DSCP_CS0;
	else if (strcasecmp(str, "CS1") == 0)
		dscp = _DSCP_CS1;
	else if (strcasecmp(str, "CS2") == 0)
		dscp = _DSCP_CS2;
	else if (strcasecmp(str, "CS3") == 0)
		dscp = _DSCP_CS3;
	else if (strcasecmp(str, "CS4") == 0)
		dscp = _DSCP_CS4;
	else if (strcasecmp(str, "CS5") == 0)
		dscp = _DSCP_CS5;
	else if (strcasecmp(str, "CS6") == 0)
		dscp = _DSCP_CS6;
	else if (strcasecmp(str, "CS7") == 0)
		dscp = _DSCP_CS7;

	else if (strcasecmp(str, "AF11") == 0)
		dscp = _DSCP_AF11;
	else if (strcasecmp(str, "AF12") == 0)
		dscp = _DSCP_AF12;
	else if (strcasecmp(str, "AF13") == 0)
		dscp = _DSCP_AF13;
	else if (strcasecmp(str, "AF21") == 0)
		dscp = _DSCP_AF21;
	else if (strcasecmp(str, "AF22") == 0)
		dscp = _DSCP_AF22;
	else if (strcasecmp(str, "AF23") == 0)
		dscp = _DSCP_AF23;
	else if (strcasecmp(str, "AF31") == 0)
		dscp = _DSCP_AF31;
	else if (strcasecmp(str, "AF32") == 0)
		dscp = _DSCP_AF32;
	else if (strcasecmp(str, "AF33") == 0)
		dscp = _DSCP_AF33;
	else if (strcasecmp(str, "AF41") == 0)
		dscp = _DSCP_AF41;
	else if (strcasecmp(str, "AF42") == 0)
		dscp = _DSCP_AF42;
	else if (strcasecmp(str, "AF43") == 0)
		dscp = _DSCP_AF43;

	else {
		unsigned long val = strtoul(str, &endptr, 0);
		if (*endptr != '\0' || val > 255)
			*valid = false;
		else
			return ((u_int8_t)val);
	}
	/* DSCP occupies the 6 upper bits of the traffic class field */
	return (dscp << 2);
}

void
pr_currenttime(void)
{
	int s;
	struct timeval tv;

	gettimeofday(&tv, NULL);

	s = (tv.tv_sec + thiszone) % 86400;
	printf("%02d:%02d:%02d.%06u ", s / 3600, (s % 3600) / 60, s % 60,
	       (u_int32_t)tv.tv_usec);
}

void
usage(void)
{
	(void)fprintf(stderr,
#if defined(IPSEC) && !defined(IPSEC_POLICY_IPSEC)
	    "A"
#endif
	    "usage: ping6 [-"
	    "Dd"
#if defined(IPSEC) && !defined(IPSEC_POLICY_IPSEC)
	    "E"
#endif
	    "fH"
#ifdef IPV6_USE_MIN_MTU
	    "m"
#endif
	    "nNoqrRtvwW] "
	    "[-a addrtype] [-b bufsiz] [-c count]\n"
	    "             [-g gateway] [-h hoplimit] [-I interface] [-i wait] [-l preload]"
#if defined(IPSEC) && defined(IPSEC_POLICY_IPSEC)
	    " [-P policy]"
#endif
	    "\n"
	    "             [-p pattern] [-S sourceaddr] [-s packetsize] [-z tclass] "
	    "[-k traffic_class] [-K net_service_type] "
	    "[hops ...] host\n");
	(void)fprintf(stderr, "Apple specific options (to be specified before hops or host like all options)\n");
	(void)fprintf(stderr, "            -b boundif           # bind the socket to the interface\n");
	(void)fprintf(stderr, "            -k traffic_class     # set traffic class socket option\n");
	(void)fprintf(stderr, "            -K net_service_type  # set traffic class socket options\n");
	(void)fprintf(stderr, "            --apple-connect      # call connect(2) in the socket\n");
	(void)fprintf(stderr, "            --apple-time         # display current time\n");
	exit(1);
}
