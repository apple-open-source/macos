/*
 * Copyright (c) 1983, 1993
 *	The Regents of the University of California.  All rights reserved.
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

#ifndef lint
static const char copyright[] =
"@(#) Copyright (c) 1983, 1993\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif /* not lint */

#ifndef lint
#if 0
static char sccsid[] = "@(#)ifconfig.c	8.2 (Berkeley) 2/16/94";
#endif
static const char rcsid[] =
	"$Id: ifconfig.c,v 1.1.1.2 2000/01/11 01:48:49 wsanchez Exp $";
#endif /* not lint */

#include <sys/param.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/sysctl.h>
#include <sys/time.h>

#include <net/if.h>
#include <net/if_var.h>
#include <net/if_dl.h>
#include <net/if_types.h>
#include <net/route.h>

/* IP */
#include <netinet/in.h>
#include <netinet/in_var.h>
#include <arpa/inet.h>
#include <netdb.h>

/* OSI */

#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "ifconfig.h"

struct	ifreq		ifr, ridreq;
struct	ifaliasreq	addreq;
struct	sockaddr_in	netmask;


char	name[32];
int	flags;
int	metric;
int	mtu;
int	setaddr;
int	setipdst;
int	doalias;
int	clearaddr;
int	newaddr = 1;

struct	afswtch;

void	Perror __P((const char *cmd));

int	ifconfig __P((int argc, char *const *argv, const struct afswtch *afp));
void	notealias __P((const char *, int, int, const struct afswtch *afp));
void	printb __P((const char *s, unsigned value, const char *bits));
void	rt_xaddrs __P((caddr_t, caddr_t, struct rt_addrinfo *));
void	status __P((const struct afswtch *afp, int addrcount,
		    struct sockaddr_dl *sdl, struct if_msghdr *ifm,
		    struct ifa_msghdr *ifam));
void	usage __P((void));

typedef	void c_func __P((const char *cmd, int arg, int s, const struct afswtch *afp));

c_func	setifaddr, setifbroadaddr, setifdstaddr, setifnetmask;
c_func	setifipdst;
c_func	setifflags, setifmetric, setifmtu;


#define	NEXTARG		0xffffff

const
struct	cmd {
	const	char *c_name;
	int	c_parameter;		/* NEXTARG means next argv */
	void	(*c_func) __P((const char *, int, int, const struct afswtch *afp));
} cmds[] = {
	{ "up",		IFF_UP,		setifflags } ,
	{ "down",	-IFF_UP,	setifflags },
	{ "arp",	-IFF_NOARP,	setifflags },
	{ "-arp",	IFF_NOARP,	setifflags },
	{ "debug",	IFF_DEBUG,	setifflags },
	{ "-debug",	-IFF_DEBUG,	setifflags },
	{ "alias",	IFF_UP,		notealias },
	{ "-alias",	-IFF_UP,	notealias },
	{ "delete",	-IFF_UP,	notealias },
#ifdef notdef
#define	EN_SWABIPS	0x1000
	{ "swabips",	EN_SWABIPS,	setifflags },
	{ "-swabips",	-EN_SWABIPS,	setifflags },
#endif
	{ "netmask",	NEXTARG,	setifnetmask },
	{ "range",	NEXTARG,	0 },
	{ "phase",	NEXTARG,	0 },
	{ "metric",	NEXTARG,	setifmetric },
	{ "broadcast",	NEXTARG,	setifbroadaddr },
	{ "ipdst",	NEXTARG,	setifipdst },
	{ "link0",	IFF_LINK0,	setifflags },
	{ "-link0",	-IFF_LINK0,	setifflags },
	{ "link1",	IFF_LINK1,	setifflags },
	{ "-link1",	-IFF_LINK1,	setifflags },
	{ "link2",	IFF_LINK2,	setifflags },
	{ "-link2",	-IFF_LINK2,	setifflags },
#if USE_IF_MEDIA
	{ "media",	NEXTARG,	setmedia },
	{ "mediaopt",	NEXTARG,	setmediaopt },
	{ "-mediaopt",	NEXTARG,	unsetmediaopt },
#endif
	{ "normal",	-IFF_LINK0,	setifflags },
	{ "compress",	IFF_LINK0,	setifflags },
	{ "noicmp",	IFF_LINK1,	setifflags },
	{ "mtu",	NEXTARG,	setifmtu },
	{ 0,		0,		setifaddr },
	{ 0,		0,		setifdstaddr },
};

/*
 * XNS support liberally adapted from code written at the University of
 * Maryland principally by James O'Toole and Chris Torek.
 */
typedef	void af_status __P((int, struct rt_addrinfo *));
typedef	void af_getaddr __P((const char *, int));

af_status	in_status, ipx_status, at_status, ether_status;
af_getaddr	in_getaddr, ipx_getaddr, at_getaddr;

/* Known address families */
const
struct	afswtch {
	const char *af_name;
	short af_af;
	af_status *af_status;
	af_getaddr *af_getaddr;
	u_long af_difaddr;
	u_long af_aifaddr;
	caddr_t af_ridreq;
	caddr_t af_addreq;
} afs[] = {
#define C(x) ((caddr_t) &x)
	{ "inet", AF_INET, in_status, in_getaddr,
	     SIOCDIFADDR, SIOCAIFADDR, C(ridreq), C(addreq) },
	{ "ether", AF_INET, ether_status, NULL },	/* XXX not real!! */
#if 0	/* XXX conflicts with the media command */
#if USE_IF_MEDIA
	{ "media", AF_INET, media_status, NULL },	/* XXX not real!! */
#endif
#endif
	{ 0,	0,	    0,		0 }
};

/*
 * Expand the compacted form of addresses as returned via the
 * configuration read via sysctl().
 */

#define ROUNDUP(a) \
	((a) > 0 ? (1 + (((a) - 1) | (sizeof(long) - 1))) : sizeof(long))
#define ADVANCE(x, n) (x += ROUNDUP((n)->sa_len))

void
rt_xaddrs(cp, cplim, rtinfo)
	caddr_t cp, cplim;
	struct rt_addrinfo *rtinfo;
{
	struct sockaddr *sa;
	int i;

	memset(rtinfo->rti_info, 0, sizeof(rtinfo->rti_info));
	for (i = 0; (i < RTAX_MAX) && (cp < cplim); i++) {
		if ((rtinfo->rti_addrs & (1 << i)) == 0)
			continue;
		rtinfo->rti_info[i] = sa = (struct sockaddr *)cp;
		ADVANCE(cp, sa);
	}
}


void
usage()
{
	fprintf(stderr, "%s\n%s\n%s\n%s\n",
	"usage: ifconfig interface address_family [address [dest_address]]",
	"                [parameters]",
	"       ifconfig -a [-d] [-u] [address_family]",
	"       ifconfig -l [-d] [-u] [address_family]");
	exit(1);
}

int
main(argc, argv)
	int argc;
	char *const *argv;
{
	int c;
	int all, namesonly, downonly, uponly;
	int foundit = 0, need_nl = 0;
	const struct afswtch *afp = 0;
	int addrcount;
	struct	if_msghdr *ifm, *nextifm;
	struct	ifa_msghdr *ifam;
	struct	sockaddr_dl *sdl;
	char	*buf, *lim, *next;


	size_t needed;
	int mib[6];

	/* Parse leading line options */
	all = downonly = uponly = namesonly = 0;
	while ((c = getopt(argc, argv, "adlmu")) != -1) {
		switch (c) {
		case 'a':	/* scan all interfaces */
			all++;
			break;
		case 'l':	/* scan interface names only */
			namesonly++;
			break;
		case 'd':	/* restrict scan to "down" interfaces */
			downonly++;
			break;
		case 'u':	/* restrict scan to "up" interfaces */
			uponly++;
			break;
		case 'm':	/* show media choices in status */
			/* ignored for compatibility */
			break;
		default:
			usage();
			break;
		}
	}
	argc -= optind;
	argv += optind;

	/* -l cannot be used with -a or -m */
	if (namesonly && all)
		usage();

	/* nonsense.. */
	if (uponly && downonly)
		usage();

	/* -a and -l allow an address family arg to limit the output */
	if (all || namesonly) {
		if (argc > 1)
			usage();

		if (argc == 1) {
			for (afp = afs; afp->af_name; afp++)
				if (strcmp(afp->af_name, *argv) == 0) {
					argc--, argv++;
					break;
				}
			if (afp->af_name == NULL)
				usage();
			/* leave with afp non-zero */
		}
	} else {
		/* not listing, need an argument */
		if (argc < 1)
			usage();

		strncpy(name, *argv, sizeof(name));
		argc--, argv++;
	}

	/* Check for address family */
	if (argc > 0) {
		for (afp = afs; afp->af_name; afp++)
			if (strcmp(afp->af_name, *argv) == 0) {
				argc--, argv++;
				break;
			}
		if (afp->af_name == NULL)
			afp = NULL;	/* not a family, NULL */
	}

	mib[0] = CTL_NET;
	mib[1] = PF_ROUTE;
	mib[2] = 0;
	mib[3] = 0;	/* address family */
	mib[4] = NET_RT_IFLIST;
	mib[5] = 0;

	/* if particular family specified, only ask about it */
	if (afp)
		mib[3] = afp->af_af;

	if (sysctl(mib, 6, NULL, &needed, NULL, 0) < 0)
		errx(1, "iflist-sysctl-estimate");
	if ((buf = malloc(needed)) == NULL)
		errx(1, "malloc");
	if (sysctl(mib, 6, buf, &needed, NULL, 0) < 0)
		errx(1, "actual retrieval of interface table");
	lim = buf + needed;

	next = buf;
	while (next < lim) {

		ifm = (struct if_msghdr *)next;
		
		if (ifm->ifm_type == RTM_IFINFO) {
			sdl = (struct sockaddr_dl *)(ifm + 1);
			flags = ifm->ifm_flags;
		} else {
			fprintf(stderr, "out of sync parsing NET_RT_IFLIST\n");
			fprintf(stderr, "expected %d, got %d\n", RTM_IFINFO,
				ifm->ifm_type);
			fprintf(stderr, "msglen = %d\n", ifm->ifm_msglen);
			fprintf(stderr, "buf:%p, next:%p, lim:%p\n", buf, next,
				lim);
			exit (1);
		}

		next += ifm->ifm_msglen;
		ifam = NULL;
		addrcount = 0;
		while (next < lim) {

			nextifm = (struct if_msghdr *)next;

			if (nextifm->ifm_type != RTM_NEWADDR)
				break;

			if (ifam == NULL)
				ifam = (struct ifa_msghdr *)nextifm;

			addrcount++;
			next += nextifm->ifm_msglen;
		}

		if (all || namesonly) {
			if (uponly)
				if ((flags & IFF_UP) == 0)
					continue; /* not up */
			if (downonly)
				if (flags & IFF_UP)
					continue; /* not down */
			strncpy(name, sdl->sdl_data, sdl->sdl_nlen);
			name[sdl->sdl_nlen] = '\0';
			if (namesonly) {
				if (afp == NULL ||
					afp->af_status != ether_status ||
					sdl->sdl_type == IFT_ETHER) {
					if (need_nl)
						putchar(' ');
					fputs(name, stdout);
					need_nl++;
				}
				continue;
			}
		} else {
			if (strlen(name) != sdl->sdl_nlen)
				continue; /* not same len */
			if (strncmp(name, sdl->sdl_data, sdl->sdl_nlen) != 0)
				continue; /* not same name */
		}

		if (argc > 0)
			ifconfig(argc, argv, afp);
		else
			status(afp, addrcount, sdl, ifm, ifam);

		if (all == 0 && namesonly == 0) {
			foundit++; /* flag it as 'done' */
			break;
		}
	}
	free(buf);

	if (namesonly && need_nl > 0)
		putchar('\n');

	if (all == 0 && namesonly == 0 && foundit == 0)
		errx(1, "interface %s does not exist", name);


	exit (0);
}


int
ifconfig(argc, argv, afp)
	int argc;
	char *const *argv;
	const struct afswtch *afp;
{
	int s;

	if (afp == NULL)
		afp = &afs[0];
	ifr.ifr_addr.sa_family = afp->af_af;
	strncpy(ifr.ifr_name, name, sizeof ifr.ifr_name);

	if ((s = socket(ifr.ifr_addr.sa_family, SOCK_DGRAM, 0)) < 0)
		err(1, "socket");

	while (argc > 0) {
		register const struct cmd *p;

		for (p = cmds; p->c_name; p++)
			if (strcmp(*argv, p->c_name) == 0)
				break;
		if (p->c_name == 0 && setaddr)
			p++;	/* got src, do dst */
		if (p->c_func) {
			if (p->c_parameter == NEXTARG) {
				if (argv[1] == NULL)
					errx(1, "'%s' requires argument",
					    p->c_name);
				(*p->c_func)(argv[1], 0, s, afp);
				argc--, argv++;
			} else
				(*p->c_func)(*argv, p->c_parameter, s, afp);
		}
		argc--, argv++;
	}

	if (clearaddr) {
		if (afp->af_ridreq == NULL || afp->af_difaddr == 0) {
			warnx("interface %s cannot change %s addresses!",
			      name, afp->af_name);
			clearaddr = NULL;
		}
	}
	if (clearaddr) {
		int ret;
		strncpy(afp->af_ridreq, name, sizeof ifr.ifr_name);
		if ((ret = ioctl(s, afp->af_difaddr, afp->af_ridreq)) < 0) {
			if (errno == EADDRNOTAVAIL && (doalias >= 0)) {
				/* means no previous address for interface */
			} else
				Perror("ioctl (SIOCDIFADDR)");
		}
	}
	if (newaddr) {
		if (afp->af_ridreq == NULL || afp->af_difaddr == 0) {
			warnx("interface %s cannot change %s addresses!",
			      name, afp->af_name);
			newaddr = NULL;
		}
	}
	if (newaddr) {
		strncpy(afp->af_addreq, name, sizeof ifr.ifr_name);
		if (ioctl(s, afp->af_aifaddr, afp->af_addreq) < 0)
			Perror("ioctl (SIOCAIFADDR)");
	}
	close(s);
	return(0);
}
#define RIDADDR 0
#define ADDR	1
#define MASK	2
#define DSTADDR	3

/*ARGSUSED*/
void
setifaddr(addr, param, s, afp)
	const char *addr;
	int param;
	int s;
	const struct afswtch *afp;
{
	/*
	 * Delay the ioctl to set the interface addr until flags are all set.
	 * The address interpretation may depend on the flags,
	 * and the flags may change when the address is set.
	 */
	setaddr++;
	if (doalias == 0)
		clearaddr = 1;
	(*afp->af_getaddr)(addr, (doalias >= 0 ? ADDR : RIDADDR));
}

void
setifnetmask(addr, dummy, s, afp)
	const char *addr;
	int dummy ;
	int s;
	const struct afswtch *afp;
{
	(*afp->af_getaddr)(addr, MASK);
}

void
setifbroadaddr(addr, dummy, s, afp)
	const char *addr;
	int dummy ;
	int s;
	const struct afswtch *afp;
{
	(*afp->af_getaddr)(addr, DSTADDR);
}

void
setifipdst(addr, dummy, s, afp)
	const char *addr;
	int dummy ;
	int s;
	const struct afswtch *afp;
{
	in_getaddr(addr, DSTADDR);
	setipdst++;
	clearaddr = 0;
	newaddr = 0;
}
#define rqtosa(x) (&(((struct ifreq *)(afp->x))->ifr_addr))

void
notealias(addr, param, s, afp)
	const char *addr;
	int param;
	int s;
	const struct afswtch *afp;
{
	if (setaddr && doalias == 0 && param < 0)
		bcopy((caddr_t)rqtosa(af_addreq),
		      (caddr_t)rqtosa(af_ridreq),
		      rqtosa(af_addreq)->sa_len);
	doalias = param;
	if (param < 0) {
		clearaddr = 1;
		newaddr = 0;
	} else
		clearaddr = 0;
}

/*ARGSUSED*/
void
setifdstaddr(addr, param, s, afp)
	const char *addr;
	int param ;
	int s;
	const struct afswtch *afp;
{
	(*afp->af_getaddr)(addr, DSTADDR);
}

void
setifflags(vname, value, s, afp)
	const char *vname;
	int value;
	int s;
	const struct afswtch *afp;
{
 	if (ioctl(s, SIOCGIFFLAGS, (caddr_t)&ifr) < 0) {
 		Perror("ioctl (SIOCGIFFLAGS)");
 		exit(1);
 	}
	strncpy(ifr.ifr_name, name, sizeof (ifr.ifr_name));
 	flags = ifr.ifr_flags;

	if (value < 0) {
		value = -value;
		flags &= ~value;
	} else
		flags |= value;
	ifr.ifr_flags = flags;
	if (ioctl(s, SIOCSIFFLAGS, (caddr_t)&ifr) < 0)
		Perror(vname);
}

void
setifmetric(val, dummy, s, afp)
	const char *val;
	int dummy ;
	int s;
	const struct afswtch *afp;
{
	strncpy(ifr.ifr_name, name, sizeof (ifr.ifr_name));
	ifr.ifr_metric = atoi(val);
	if (ioctl(s, SIOCSIFMETRIC, (caddr_t)&ifr) < 0)
		warn("ioctl (set metric)");
}

void
setifmtu(val, dummy, s, afp)
	const char *val;
	int dummy ;
	int s;
	const struct afswtch *afp;
{
	strncpy(ifr.ifr_name, name, sizeof (ifr.ifr_name));
	ifr.ifr_mtu = atoi(val);
	if (ioctl(s, SIOCSIFMTU, (caddr_t)&ifr) < 0)
		warn("ioctl (set mtu)");
}


#define	IFFBITS \
"\020\1UP\2BROADCAST\3DEBUG\4LOOPBACK\5POINTOPOINT\6b6\7RUNNING" \
"\10NOARP\11PROMISC\12ALLMULTI\13OACTIVE\14SIMPLEX\15LINK0\16LINK1\17LINK2" \
"\20MULTICAST"

/*
 * Print the status of the interface.  If an address family was
 * specified, show it and it only; otherwise, show them all.
 */
void
status(afp, addrcount, sdl, ifm, ifam)
	const struct afswtch *afp;
	int addrcount;
	struct	sockaddr_dl *sdl;
	struct if_msghdr *ifm;
	struct ifa_msghdr *ifam;
{
	const struct afswtch *p = NULL;
	struct	rt_addrinfo info;
	int allfamilies, s;

	if (afp == NULL) {
		allfamilies = 1;
		afp = &afs[0];
	} else
		allfamilies = 0;

	ifr.ifr_addr.sa_family = afp->af_af;
	strncpy(ifr.ifr_name, name, sizeof ifr.ifr_name);

	if ((s = socket(ifr.ifr_addr.sa_family, SOCK_DGRAM, 0)) < 0)
		err(1, "socket");

	/*
	 * XXX is it we are doing a SIOCGIFMETRIC etc for one family.
	 * is it possible that the metric and mtu can be different for
	 * each family?  If so, we have a format problem, because the
	 * metric and mtu is printed on the global the flags line.
	 */
	if (ioctl(s, SIOCGIFMETRIC, (caddr_t)&ifr) < 0)
		warn("ioctl (SIOCGIFMETRIC)");
	else
		metric = ifr.ifr_metric;

	if (ioctl(s, SIOCGIFMTU, (caddr_t)&ifr) < 0)
		warn("ioctl (SIOCGIFMTU)");
	else
		mtu = ifr.ifr_mtu;

	printf("%s: ", name);
	printb("flags", flags, IFFBITS);
	if (metric)
		printf(" metric %d", metric);
	if (mtu)
		printf(" mtu %d", mtu);
	putchar('\n');

	while (addrcount > 0) {
		
		info.rti_addrs = ifam->ifam_addrs;

		/* Expand the compacted addresses */
		rt_xaddrs((char *)(ifam + 1), ifam->ifam_msglen + (char *)ifam,
			  &info);

		if (!allfamilies) {
			if (afp->af_af == info.rti_info[RTAX_IFA]->sa_family &&
#if USE_IF_MEDIA
			    afp->af_status != media_status &&
#endif
			    afp->af_status != ether_status) {
				p = afp;
				(*p->af_status)(s, &info);
			}
		} else for (p = afs; p->af_name; p++) {
			if (p->af_af == info.rti_info[RTAX_IFA]->sa_family &&
#if USE_IF_MEDIA
			    p->af_status != media_status &&
#endif
			    p->af_status != ether_status) 
				(*p->af_status)(s, &info);
		}
		addrcount--;
		ifam = (struct ifa_msghdr *)((char *)ifam + ifam->ifam_msglen);
	}
	if (allfamilies || afp->af_status == ether_status)
		ether_status(s, (struct rt_addrinfo *)sdl);
#if USE_IF_MEDIA
	if (allfamilies || afp->af_status == media_status)
		media_status(s, NULL);
#endif
	if (!allfamilies && !p && 
	    afp->af_status != ether_status)
		warnx("%s has no %s interface address!", name, afp->af_name);

	close(s);
	return;
}

void
in_status(s, info)
	int s ;
	struct rt_addrinfo * info;
{
	struct sockaddr_in *sin, null_sin;
	
	memset(&null_sin, 0, sizeof(null_sin));

	sin = (struct sockaddr_in *)info->rti_info[RTAX_IFA];
	printf("\tinet %s ", inet_ntoa(sin->sin_addr));

	if (flags & IFF_POINTOPOINT) {
		/* note RTAX_BRD overlap with IFF_BROADCAST */
		sin = (struct sockaddr_in *)info->rti_info[RTAX_BRD];
		if (!sin)
			sin = &null_sin;
		printf("--> %s ", inet_ntoa(sin->sin_addr));
	}

	sin = (struct sockaddr_in *)info->rti_info[RTAX_NETMASK];
	if (!sin)
		sin = &null_sin;
	printf("netmask 0x%lx ", (unsigned long)ntohl(sin->sin_addr.s_addr));

	if (flags & IFF_BROADCAST) {
		/* note RTAX_BRD overlap with IFF_POINTOPOINT */
		sin = (struct sockaddr_in *)info->rti_info[RTAX_BRD];
		if (sin && sin->sin_addr.s_addr != 0)
			printf("broadcast %s", inet_ntoa(sin->sin_addr));
	}
	putchar('\n');
}

void
ether_status(s, info)
	int s ;
	struct rt_addrinfo *info;
{
	char *cp;
	int n;
	struct sockaddr_dl *sdl = (struct sockaddr_dl *)info;

	cp = (char *)LLADDR(sdl);
	if ((n = sdl->sdl_alen) > 0) {
		if (sdl->sdl_type == IFT_ETHER)
			printf ("\tether ");
		else
			printf ("\tlladdr ");
             	while (--n >= 0)
			printf("%02x%c",*cp++ & 0xff, n>0? ':' : ' ');
		putchar('\n');
	}
}

void
Perror(cmd)
	const char *cmd;
{
	switch (errno) {

	case ENXIO:
		errx(1, "%s: no such interface", cmd);
		break;

	case EPERM:
		errx(1, "%s: permission denied", cmd);
		break;

	default:
		err(1, "%s", cmd);
	}
}

#define SIN(x) ((struct sockaddr_in *) &(x))
struct sockaddr_in *sintab[] = {
SIN(ridreq.ifr_addr), SIN(addreq.ifra_addr),
SIN(addreq.ifra_mask), SIN(addreq.ifra_broadaddr)};

void
in_getaddr(s, which)
	const char *s;
	int which;
{
	register struct sockaddr_in *sin = sintab[which];
	struct hostent *hp;
	struct netent *np;

	sin->sin_len = sizeof(*sin);
	if (which != MASK)
		sin->sin_family = AF_INET;

	if (inet_aton(s, &sin->sin_addr))
		return;
	if ((hp = gethostbyname(s)) != 0)
		bcopy(hp->h_addr, (char *)&sin->sin_addr, 
		    MIN(hp->h_length, sizeof(sin->sin_addr)));
	else if ((np = getnetbyname(s)) != 0)
		sin->sin_addr = inet_makeaddr(np->n_net, INADDR_ANY);
	else
		errx(1, "%s: bad value", s);
}

/*
 * Print a value a la the %b format of the kernel's printf
 */
void
printb(s, v, bits)
	const char *s;
	register unsigned v;
	register const char *bits;
{
	register int i, any = 0;
	register char c;

	if (bits && *bits == 8)
		printf("%s=%o", s, v);
	else
		printf("%s=%x", s, v);
	bits++;
	if (bits) {
		putchar('<');
		while ((i = *bits++) != '\0') {
			if (v & (1 << (i-1))) {
				if (any)
					putchar(',');
				any = 1;
				for (; (c = *bits) > 32; bits++)
					putchar(c);
			} else
				for (; *bits > 32; bits++)
					;
		}
		putchar('>');
	}
}


