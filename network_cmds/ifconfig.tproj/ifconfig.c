/*
 * Copyright (c) 2009-2024 Apple Inc. All rights reserved.
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
"@(#) Copyright (c) 1983, 1993\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif /* not lint */

#include <sys/param.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/sysctl.h>
#include <sys/time.h>
#ifndef __APPLE__
#include <sys/module.h>
#include <sys/linker.h>
#endif

#include <net/ethernet.h>
#include <net/if.h>
#include <net/if_var.h>
#include <net/if_dl.h>
#include <net/if_types.h>
#include <net/if_mib.h>
#include <net/route.h>
#include <net/pktsched/pktsched.h>
#include <net/network_agent.h>

/* IP */
#include <netinet/in.h>
#include <netinet/in_var.h>
#include <arpa/inet.h>
#include <netdb.h>

#include <ifaddrs.h>
#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <sysexits.h>

#include <stdbool.h>
#include <regex.h>

#include "ifconfig.h"

#ifdef __APPLE__
#include <TargetConditionals.h>
#endif

/*
 * Since "struct ifreq" is composed of various union members, callers
 * should pay special attention to interprete the value.
 * (.e.g. little/big endian difference in the structure.)
 */
struct	ifreq ifr;

char	name[IFNAMSIZ];
int	setaddr;
int	setmask;
int	doalias;
int	clearaddr;
int	newaddr = 1;
int	noload;
int all;

int bond_details = 0;
int	supmedia = 0;
#if (TARGET_OS_IPHONE && !TARGET_OS_SIMULATOR)
int	verbose = 1;
int	showrtref = 1;
#else /* (TARGET_OS_IPHONE && !TARGET_OS_SIMULATOR) */
int	verbose = 0;
int	showrtref = 0;
#endif /* (TARGET_OS_IPHONE && !TARGET_OS_SIMULATOR) */
int	printkeys = 0;		/* Print keying material for interfaces. */

static	int ifconfig(int argc, char *const *argv, int iscreate,
		const struct afswtch *afp);
static	void status(const struct afswtch *afp, const struct sockaddr_dl *sdl,
		struct ifaddrs *ifa);
static char *bytes_to_str(unsigned long long bytes);
static char *bps_to_str(unsigned long long rate);
static char *ns_to_str(unsigned long long nsec);
static	void tunnel_status(int s);
static void clat46_addr(int s, char *name);
static void nat64_status(int s, char *name);
static	void usage(void);
static char *sched2str(unsigned int s);
static char *tl2str(unsigned int s);
static char *ift2str(unsigned int t, unsigned int f, unsigned int sf);
static char *iffunct2str(u_int32_t functional_type);

static struct afswtch *af_getbyname(const char *name);
static struct afswtch *af_getbyfamily(int af);
static void af_other_status(int);

/* Formatter Strings */
char	*f_inet, *f_inet6, *f_ether, *f_addr;

static void freeformat(void);
static void setformat(char *input);

static struct option *opts = NULL;

void
opt_register(struct option *p)
{
	p->next = opts;
	opts = p;
}
static void
usage(void)
{
	char options[1024];
	struct option *p;

	/* XXX not right but close enough for now */
	options[0] = '\0';
	for (p = opts; p != NULL; p = p->next) {
		strlcat(options, p->opt_usage, sizeof(options));
		strlcat(options, " ", sizeof(options));
	}

	fprintf(stderr,
	"usage: ifconfig %sinterface address_family [address [dest_address]]\n"
	"                [parameters]\n"
	"       ifconfig interface create\n"
	"       ifconfig -a %s[-d] [-m] [-u] [-v] [address_family]\n"
	"       ifconfig -l [-d] [-u] [address_family]\n"
	"       ifconfig %s[-d] [-m] [-u] [-v]\n"
	"       ifconfig -X pattern %s[-a] [-d] [-d] [-m] [-u] [-v] [address_family]\n",
		options, options, options, options);
	exit(1);
}

int
main(int argc, char *argv[])
{
	int c, namesonly, downonly, uponly;
	const struct afswtch *afp = NULL;
	int ifindex;
	struct ifaddrs *ifap, *ifa;
	struct ifreq paifr;
	const struct sockaddr_dl *sdl;
	char options[1024], *cp;
	const char *ifname = NULL;
	struct option *p;
	size_t iflen;
	bool is_regex = false;
	regex_t if_reg = {};

	all = downonly = uponly = namesonly = noload = 0;

	/* Parse leading line options */
	strlcpy(options, "X:abdf:lmruv", sizeof(options));
	for (p = opts; p != NULL; p = p->next)
		strlcat(options, p->opt, sizeof(options));
	while ((c = getopt(argc, argv, options)) != -1) {
		switch (c) {
#ifdef __APPLE__
		case 'X':
			is_regex = true;
			ifname = optarg;
			break;
#endif /* __APPLE__ */
		case 'a':	/* scan all interfaces */
			all++;
			break;
		case 'b':	/* bond detailed output */
			bond_details++;
			break;				
		case 'd':	/* restrict scan to "down" interfaces */
			downonly++;
			break;
		case 'f':
			if (optarg == NULL)
				usage();
			setformat(optarg);
			break;
#ifndef __APPLE__
		case 'k':
			printkeys++;
			break;
#endif
		case 'l':	/* scan interface names only */
			namesonly++;
			break;
		case 'm':	/* show media choices in status */
			supmedia = 1;
			break;
#ifndef __APPLE__
		case 'n':	/* suppress module loading */
			noload++;
			break;
#endif
		case 'r':
			showrtref++;
			break;
		case 'u':	/* restrict scan to "up" interfaces */
			uponly++;
			break;
		case 'v':
			verbose++;
			break;
		default:
			for (p = opts; p != NULL; p = p->next)
				if (p->opt[0] == c) {
					p->cb(optarg);
					break;
				}
			if (p == NULL)
				usage();
			break;
		}
	}
	argc -= optind;
	argv += optind;

	/* -l cannot be used with -a or -q or -m or -b */
	if (namesonly &&
	    (all || supmedia || bond_details))
		usage();

	/* nonsense.. */
	if (uponly && downonly)
		usage();

	/* no arguments is equivalent to '-a' */
	if (!namesonly && argc < 1)
		all = 1;

	if (is_regex) {
		if (regcomp(&if_reg, ifname, REG_EXTENDED | REG_NOSUB) != 0) {
			errx(1, "bad interface pattern '%s'", ifname);
		}
	}

	/* -a and -l allow an address family arg to limit the output */
	if (all || namesonly || is_regex) {
		if (argc > 1)
			usage();

		if (argc == 1) {
			afp = af_getbyname(*argv);
			if (afp == NULL)
				usage();
			if (afp->af_name != NULL)
				argc--, argv++;
			/* leave with afp non-zero */
		}
	} else {
		/* not listing, need an argument */
		if (argc < 1)
			usage();

		ifname = *argv;
		argc--, argv++;

		ifindex = if_nametoindex(ifname);
		if (ifindex == 0) {
			/*
			 * NOTE:  We must special-case the `create' command
			 * right here as we would otherwise fail when trying
			 * to find the interface.
			 */
			if (argc > 0 && (strcmp(argv[0], "create") == 0 ||
							 strcmp(argv[0], "plumb") == 0)) {
				iflen = strlcpy(name, ifname, sizeof(name));
				if (iflen >= sizeof(name))
					errx(1, "%s: cloning name too long",
						 ifname);
				ifconfig(argc, argv, 1, NULL);
				exit(0);
			}
			errx(1, "interface %s does not exist", ifname);
		}
	}

	/* Check for address family */
	if (argc > 0) {
		afp = af_getbyname(*argv);
		if (afp != NULL)
			argc--, argv++;
	}

	if (getifaddrs(&ifap) != 0)
		err(EXIT_FAILURE, "getifaddrs");
	cp = NULL;
	ifindex = 0;
	for (ifa = ifap; ifa; ifa = ifa->ifa_next) {
		memset(&paifr, 0, sizeof(paifr));
		strlcpy(paifr.ifr_name, ifa->ifa_name, sizeof(paifr.ifr_name));
		if (sizeof(paifr.ifr_addr) >= ifa->ifa_addr->sa_len) {
			memcpy(&paifr.ifr_addr, ifa->ifa_addr,
			    ifa->ifa_addr->sa_len);
		}

		if (is_regex) {
			if (regexec(&if_reg, ifa->ifa_name, 0, NULL, 0) != 0) {
				continue;
			}
		} else {
			if (ifname != NULL && strcmp(ifname, ifa->ifa_name) != 0)
				continue;
		}
		if (ifa->ifa_addr->sa_family == AF_LINK)
			sdl = (const struct sockaddr_dl *) ifa->ifa_addr;
		else
			sdl = NULL;
		if (cp != NULL && strcmp(cp, ifa->ifa_name) == 0)
			continue;
		iflen = strlcpy(name, ifa->ifa_name, sizeof(name));
		if (iflen >= sizeof(name)) {
			warnx("%s: interface name too long, skipping",
			    ifa->ifa_name);
			continue;
		}
		cp = ifa->ifa_name;

		if (downonly && (ifa->ifa_flags & IFF_UP) != 0)
			continue;
		if (uponly && (ifa->ifa_flags & IFF_UP) == 0)
			continue;
		ifindex++;
		/*
		 * Are we just listing the interfaces?
		 */
		if (namesonly) {
			if (ifindex > 1)
				printf(" ");
			fputs(name, stdout);
			continue;
		}

		if (argc > 0)
			ifconfig(argc, argv, 0, afp);
		else
			status(afp, sdl, ifa);
	}
	if (namesonly)
		printf("\n");
	freeifaddrs(ifap);
	if (is_regex) {
		regfree(&if_reg);
	}
	freeformat();

	exit(0);
}

static struct afswtch *afs = NULL;

void
af_register(struct afswtch *p)
{
	p->af_next = afs;
	afs = p;
}

static struct afswtch *
af_getbyname(const char *name)
{
	struct afswtch *afp;

	for (afp = afs; afp !=  NULL; afp = afp->af_next)
		if (strcmp(afp->af_name, name) == 0)
			return afp;
	return NULL;
}

static struct afswtch *
af_getbyfamily(int af)
{
	struct afswtch *afp;

	for (afp = afs; afp != NULL; afp = afp->af_next)
		if (afp->af_af == af)
			return afp;
	return NULL;
}

static void
call_af_other_status(const struct afswtch *afp, int s)
{
	if (afp->af_clone_name != NULL &&
	    strncmp(name, afp->af_clone_name, afp->af_clone_name_length) != 0) {
		/* clone specific status */
		return;
	}
	(*afp->af_other_status)(s);
}

static void
af_other_status(int s)
{
	struct afswtch *afp;
	uint8_t afmask[howmany(AF_MAX, NBBY)];

	memset(afmask, 0, sizeof(afmask));
	for (afp = afs; afp != NULL; afp = afp->af_next) {
		if (afp->af_other_status == NULL)
			continue;
		if (afp->af_af != AF_UNSPEC && isset(afmask, afp->af_af))
			continue;
		call_af_other_status(afp, s);
		setbit(afmask, afp->af_af);
	}
}

static void
af_all_tunnel_status(int s)
{
	struct afswtch *afp;
	uint8_t afmask[howmany(AF_MAX, NBBY)];

	memset(afmask, 0, sizeof(afmask));
	for (afp = afs; afp != NULL; afp = afp->af_next) {
		if (afp->af_status_tunnel == NULL)
			continue;
		if (afp->af_af != AF_UNSPEC && isset(afmask, afp->af_af))
			continue;
		afp->af_status_tunnel(s);
		setbit(afmask, afp->af_af);
	}
}

static struct cmd *cmds = NULL;

void
cmd_register(struct cmd *p)
{
	p->c_next = cmds;
	cmds = p;
}

static const struct cmd *
cmd_lookup(const char *name)
{
#define	N(a)	(sizeof(a)/sizeof(a[0]))
	const struct cmd *p;

	for (p = cmds; p != NULL; p = p->c_next)
		if (strcmp(name, p->c_name) == 0)
			return p;
	return NULL;
#undef N
}

struct callback {
	callback_func *cb_func;
	void	*cb_arg;
	struct callback *cb_next;
};
static struct callback *callbacks = NULL;

void
callback_register(callback_func *func, void *arg)
{
	struct callback *cb;

	cb = malloc(sizeof(struct callback));
	if (cb == NULL)
		errx(1, "unable to allocate memory for callback");
	cb->cb_func = func;
	cb->cb_arg = arg;
	cb->cb_next = callbacks;
	callbacks = cb;
}

/* specially-handled commands */
static void setifaddr(const char *, int, int, const struct afswtch *);
static const struct cmd setifaddr_cmd = DEF_CMD("ifaddr", 0, setifaddr);

static void setifdstaddr(const char *, int, int, const struct afswtch *);
static const struct cmd setifdstaddr_cmd =
	DEF_CMD("ifdstaddr", 0, setifdstaddr);

static int
ifconfig(int argc, char *const *argv, int iscreate, const struct afswtch *afp)
{
	const struct afswtch *nafp;
	struct callback *cb;
	int ret, s;

	strlcpy(ifr.ifr_name, name, sizeof ifr.ifr_name);
top:
	if (afp == NULL)
		afp = af_getbyname("inet");
	ifr.ifr_addr.sa_family =
		afp->af_af == AF_LINK || afp->af_af == AF_UNSPEC ?
		AF_INET : afp->af_af;

	if ((s = socket(ifr.ifr_addr.sa_family, SOCK_DGRAM, 0)) < 0)
		err(1, "socket(family %u,SOCK_DGRAM", ifr.ifr_addr.sa_family);

	while (argc > 0) {
		const struct cmd *p;

		p = cmd_lookup(*argv);
		if (p == NULL) {
			/*
			 * Not a recognized command, choose between setting
			 * the interface address and the dst address.
			 */
			p = (setaddr ? &setifdstaddr_cmd : &setifaddr_cmd);
		}
		if (p->c_u.c_func || p->c_u.c_func2) {
			if (iscreate && !p->c_iscloneop) { 
				/*
				 * Push the clone create callback so the new
				 * device is created and can be used for any
				 * remaining arguments.
				 */
				cb = callbacks;
				if (cb == NULL)
					errx(1, "internal error, no callback");
				callbacks = cb->cb_next;
				cb->cb_func(s, cb->cb_arg);
				iscreate = 0;
				/*
				 * Handle any address family spec that
				 * immediately follows and potentially
				 * recreate the socket.
				 */
				nafp = af_getbyname(*argv);
				if (nafp != NULL) {
					argc--, argv++;
					if (nafp != afp) {
						close(s);
						afp = nafp;
						goto top;
					}
				}
			}
			if (p->c_parameter == NEXTARG) {
				if (argv[1] == NULL)
					errx(1, "'%s' requires argument",
					    p->c_name);
				p->c_u.c_func(argv[1], 0, s, afp);
				argc--, argv++;
			} else if (p->c_parameter == OPTARG) {
				p->c_u.c_func(argv[1], 0, s, afp);
				if (argv[1] != NULL)
					argc--, argv++;
			} else if (p->c_parameter == NEXTARG2) {
				if (argc < 3)
					errx(1, "'%s' requires 2 arguments",
					    p->c_name);
				p->c_u.c_func2(argv[1], argv[2], s, afp);
				argc -= 2, argv += 2;
			} else if (p->c_parameter == VAARGS) {
				ret = p->c_u.c_funcv(argc - 1, argv + 1, s, afp);
				if (ret < 0)
					errx(1, "'%s' command error",
					    p->c_name);
				argc -= ret, argv += ret;
			} else {
				p->c_u.c_func(*argv, p->c_parameter, s, afp);
			}
		}
		argc--, argv++;
	}

	/*
	 * Do any post argument processing required by the address family.
	 */
	if (afp->af_postproc != NULL)
		afp->af_postproc(s, afp);
	/*
	 * Do deferred callbacks registered while processing
	 * command-line arguments.
	 */
	for (cb = callbacks; cb != NULL; cb = cb->cb_next)
		cb->cb_func(s, cb->cb_arg);
	/*
	 * Do deferred operations.
	 */
	if (clearaddr) {
		if (afp->af_ridreq == NULL || afp->af_difaddr == 0) {
			warnx("interface %s cannot change %s addresses!",
			      name, afp->af_name);
			clearaddr = 0;
		}
	}
	if (clearaddr) {
		strlcpy(afp->af_ridreq, name, sizeof ifr.ifr_name);
		ret = ioctl(s, afp->af_difaddr, afp->af_ridreq);
		if (ret < 0) {
			if (errno == EADDRNOTAVAIL && (doalias >= 0)) {
				/* means no previous address for interface */
			} else
				Perror("ioctl (SIOCDIFADDR)");
		}
	}
	if (newaddr) {
		if (afp->af_addreq == NULL || afp->af_aifaddr == 0) {
			warnx("interface %s cannot change %s addresses!",
			      name, afp->af_name);
			newaddr = 0;
		}
	}
	if (newaddr && (setaddr || setmask)) {
		strlcpy(afp->af_addreq, name, sizeof ifr.ifr_name);
		if (ioctl(s, afp->af_aifaddr, afp->af_addreq) < 0)
			Perror("ioctl (SIOCAIFADDR)");
	}

	close(s);
	return(0);
}

/*ARGSUSED*/
static void
setifaddr(const char *addr, int param, int s, const struct afswtch *afp)
{
	if (afp->af_getaddr == NULL)
		return;
	/*
	 * Delay the ioctl to set the interface addr until flags are all set.
	 * The address interpretation may depend on the flags,
	 * and the flags may change when the address is set.
	 */
	setaddr++;
	if (doalias == 0 && afp->af_af != AF_LINK)
		clearaddr = 1;
	afp->af_getaddr(addr, (doalias >= 0 ? ADDR : RIDADDR));
}

static void
settunnel(const char *src, const char *dst, int s, const struct afswtch *afp)
{
	struct addrinfo *srcres, *dstres;
	int ecode;

	if (afp->af_settunnel == NULL) {
		warn("address family %s does not support tunnel setup",
			afp->af_name);
		return;
	}

	if ((ecode = getaddrinfo(src, NULL, NULL, &srcres)) != 0)
		errx(1, "error in parsing address string: %s",
		    gai_strerror(ecode));

	if ((ecode = getaddrinfo(dst, NULL, NULL, &dstres)) != 0)  
		errx(1, "error in parsing address string: %s",
		    gai_strerror(ecode));

	if (srcres->ai_addr->sa_family != dstres->ai_addr->sa_family)
		errx(1,
		    "source and destination address families do not match");

	afp->af_settunnel(s, srcres, dstres);

	freeaddrinfo(srcres);
	freeaddrinfo(dstres);
}

/* ARGSUSED */
static void
deletetunnel(const char *vname, int param, int s, const struct afswtch *afp)
{

	if (ioctl(s, SIOCDIFPHYADDR, &ifr) < 0)
		err(1, "SIOCDIFPHYADDR");
}

static void
setifnetmask(const char *addr, int dummy __unused, int s,
    const struct afswtch *afp)
{
	if (afp->af_getaddr != NULL) {
		setmask++;
		afp->af_getaddr(addr, MASK);
	}
}

static void
setifbroadaddr(const char *addr, int dummy __unused, int s,
    const struct afswtch *afp)
{
	if (afp->af_getaddr != NULL)
		afp->af_getaddr(addr, DSTADDR);
}

static void
setifipdst(const char *addr, int dummy __unused, int s,
    const struct afswtch *afp)
{
	const struct afswtch *inet;

	inet = af_getbyname("inet");
	if (inet == NULL)
		return;
	inet->af_getaddr(addr, DSTADDR);
	clearaddr = 0;
	newaddr = 0;
}

static void
notealias(const char *addr, int param, int s, const struct afswtch *afp)
{
#define rqtosa(x) (&(((struct ifreq *)(afp->x))->ifr_addr))
	if (setaddr && doalias == 0 && param < 0)
		if (afp->af_addreq != NULL && afp->af_ridreq != NULL)
			bcopy((caddr_t)rqtosa(af_addreq),
			      (caddr_t)rqtosa(af_ridreq),
			      rqtosa(af_addreq)->sa_len);
	doalias = param;
	if (param < 0) {
		clearaddr = 1;
		newaddr = 0;
	} else
		clearaddr = 0;
#undef rqtosa
}

/*ARGSUSED*/
static void
setifdstaddr(const char *addr, int param __unused, int s, 
    const struct afswtch *afp)
{
	if (afp->af_getaddr != NULL)
		afp->af_getaddr(addr, DSTADDR);
}

/*
 * Note: doing an SIOCIGIFFLAGS scribbles on the union portion
 * of the ifreq structure, which may confuse other parts of ifconfig.
 * Make a private copy so we can avoid that.
 */
static void
setifflags(const char *vname, int value, int s, const struct afswtch *afp)
{
	struct ifreq		my_ifr;
	int flags;

	bcopy((char *)&ifr, (char *)&my_ifr, sizeof(struct ifreq));

 	if (ioctl(s, SIOCGIFFLAGS, (caddr_t)&my_ifr) < 0) {
 		Perror("ioctl (SIOCGIFFLAGS)");
 		exit(1);
 	}
	strlcpy(my_ifr.ifr_name, name, sizeof (my_ifr.ifr_name));
	flags = my_ifr.ifr_flags;
	
	if (value < 0) {
		value = -value;
		flags &= ~value;
	} else
		flags |= value;
	my_ifr.ifr_flags = flags & 0xffff;
	if (ioctl(s, SIOCSIFFLAGS, (caddr_t)&my_ifr) < 0)
		Perror(vname);
}

void
setifcap(const char *vname, int value, int s, const struct afswtch *afp)
{
	int flags;

 	if (ioctl(s, SIOCGIFCAP, (caddr_t)&ifr) < 0) {
 		Perror("ioctl (SIOCGIFCAP)");
 		exit(1);
 	}
	flags = ifr.ifr_curcap;
	if (value < 0) {
		value = -value;
		flags &= ~value;
	} else {
		flags |= value;
	}
	/* SIOCGIFCAP returns the supported capabilities in ifr_reqcap */
	if ((value & ifr.ifr_reqcap) != value) {
		errx(1, "%s does not support %s", ifr.ifr_name, vname);
	}
	flags &= ifr.ifr_reqcap;
	ifr.ifr_reqcap = flags;
	if (ioctl(s, SIOCSIFCAP, (caddr_t)&ifr) < 0)
		Perror(vname);
}

static void
setifmetric(const char *val, int dummy __unused, int s, 
    const struct afswtch *afp)
{
	strlcpy(ifr.ifr_name, name, sizeof (ifr.ifr_name));
	ifr.ifr_metric = atoi(val);
	if (ioctl(s, SIOCSIFMETRIC, (caddr_t)&ifr) < 0)
		warn("ioctl (set metric)");
}

static void
setifmtu(const char *val, int dummy __unused, int s, 
    const struct afswtch *afp)
{
	strlcpy(ifr.ifr_name, name, sizeof (ifr.ifr_name));
	ifr.ifr_mtu = atoi(val);
	if (ioctl(s, SIOCSIFMTU, (caddr_t)&ifr) < 0)
		warn("ioctl (set mtu)");
}

#ifndef __APPLE__
static void
setifname(const char *val, int dummy __unused, int s, 
    const struct afswtch *afp)
{
	char *newname;

	newname = strdup(val);
	if (newname == NULL) {
		warn("no memory to set ifname");
		return;
	}
	ifr.ifr_data = newname;
	if (ioctl(s, SIOCSIFNAME, (caddr_t)&ifr) < 0) {
		warn("ioctl (set name)");
		free(newname);
		return;
	}
	strlcpy(name, newname, sizeof(name));
	free(newname);
}
#endif

static void
setrouter(const char *vname, int value, int s, const struct afswtch *afp)
{
	if (afp->af_setrouter == NULL) {
		warn("address family %s does not support router mode",
		    afp->af_name);
		return;
	}

	afp->af_setrouter(s, value);
}

static int
routermode(int argc, char *const *argv, int s, const struct afswtch *afp)
{
	return (*afp->af_routermode)(s, argc, argv);
}


static void
setifdesc(const char *val, int dummy __unused, int s, const struct afswtch *afp)
{
	struct if_descreq ifdr;

	bzero(&ifdr, sizeof (ifdr));
	strlcpy(ifdr.ifdr_name, name, sizeof (ifdr.ifdr_name));
	ifdr.ifdr_len = strlen(val);
	strlcpy((char *)ifdr.ifdr_desc, val, sizeof (ifdr.ifdr_desc));

	if (ioctl(s, SIOCSIFDESC, (caddr_t)&ifdr) < 0) {
		warn("ioctl (set desc)");
	}
}

static void
settbr(const char *val, int dummy __unused, int s, const struct afswtch *afp)
{
	struct if_linkparamsreq iflpr;
	long double bps;
	u_int64_t rate;
	u_int32_t percent = 0;
	char *cp;

	errno = 0;
	bzero(&iflpr, sizeof (iflpr));
	strlcpy(iflpr.iflpr_name, name, sizeof (iflpr.iflpr_name));

	bps = strtold(val, &cp);
	if (val == cp || errno != 0) {
		warn("Invalid value '%s'", val);
		return;
	}
	rate = (u_int64_t)bps;
	if (cp != NULL) {
		if (!strcmp(cp, "b") || !strcmp(cp, "bps")) {
			; /* nothing */
		} else if (!strcmp(cp, "Kb") || !strcmp(cp, "Kbps")) {
			rate *= 1000;
		} else if (!strcmp(cp, "Mb") || !strcmp(cp, "Mbps")) {
			rate *= 1000 * 1000;
		} else if (!strcmp(cp, "Gb") || !strcmp(cp, "Gbps")) {
			rate *= 1000 * 1000 * 1000;
		} else if (!strcmp(cp, "%")) {
			percent = rate;
			if (percent == 0 || percent > 100) {
				printf("Value out of range '%s'", val);
				return;
			}
		} else if (*cp != '\0') {
			printf("Unknown unit '%s'", cp);
			return;
		}
	}
	iflpr.iflpr_output_tbr_rate = rate;
	iflpr.iflpr_output_tbr_percent = percent;
	if (ioctl(s, SIOCSIFLINKPARAMS, &iflpr) < 0 &&
	    errno != ENOENT && errno != ENXIO && errno != ENODEV) {
		warn("ioctl (set link params)");
	} else if (errno == ENXIO) {
		printf("TBR cannot be set on %s\n", name);
	} else if (errno == 0 && rate == 0) {
		printf("%s: TBR is now disabled\n", name);
	} else if (errno == ENODEV) {
		printf("%s: requires absolute TBR rate\n", name);
	} else if (percent != 0) {
		printf("%s: TBR rate set to %u%% of effective link rate\n",
		    name, percent);
	} else {
		printf("%s: TBR rate set to %s\n", name, bps_to_str(rate));
	}
}

static bool
get_longlong(long long *value, char const *s)
{
    long long result;
	char *cp;
	result = strtoll(s, &cp, 10);
    if (cp == s) {
        return false; // no digits at all
    }
    if (result == 0) {
        if (errno == EINVAL) {
            return false;
        }
    } else if (result == LLONG_MIN || result == LLONG_MAX) {
        if (errno == ERANGE) {
            fprintf(stderr, "The value provided was out of range\n");
            return false;
        }
    }

    *value = result;
	return true;
}

static bool
get_uint32(uint32_t *value, char const *s)
{
    long long result;
    if (!get_longlong(&result, s)) {
        return false;
    }

    if (result > UINT32_MAX) {
        fprintf(stderr, "The value provided was out of range\n");
    }

    *value = (uint32_t)result;
    return true;
}

static bool
get_uint64(uint64_t *value, char const *s)
{
    long long result;
    if (!get_longlong(&result, s)) {
        return false;
    }

    if (result > UINT64_MAX) {
        fprintf(stderr, "The value provided was out of range\n");
    }

    *value = (uint64_t)result;
    return true;
}

static bool
get_percent(double *d, const char *s)
{
	char *cp;
	*d = strtod(s, &cp) / (double)100;
	if (*d == HUGE_VALF || *d == HUGE_VALL) {
		return false;
	}
	if (*d == 0.0 || (*cp != '\0' && strcmp(cp, "%") != 0)) {
		return false;
	}
	return true;
}

static bool
get_percent_fixed_point(uint32_t *i, const char *s)
{
	double p;

	if (!get_percent(&p, s)){
		return false;
	}

	*i = p * IF_NETEM_PARAMS_PSCALE;
	return true;
}

static int
netem_parse_args(struct if_netem_params *p, int argc, char *const *argv)
{
	int argc_saved = argc;
	uint64_t bandwitdh = UINT64_MAX;
	uint32_t latency = 0, jitter = 0;
	uint32_t corruption = 0;
	uint32_t duplication = 0;
	uint32_t loss_p_gr_gl = 0, loss_p_gr_bl = 0, loss_p_bl_br = 0,
	    loss_p_bl_gr = 0, loss_p_br_bl = 0;
	uint32_t loss_recovery_ms = 0;
	uint32_t reordering = 0;
	uint32_t output_ival_ms = 0;

	bzero(p, sizeof (*p));
	p->ifnetem_model = IF_NETEM_MODEL_NLC; /* default NLC model */

	/* take out "input"/"output" */
	argc--, argv++;

	for ( ; argc > 0; ) {
		if (strcmp(*argv, "model") == 0) {
			argc--, argv++;
			if (strcmp(*argv, "nlc") == 0) {
				p->ifnetem_model = IF_NETEM_MODEL_NLC;
			} else if (strcmp(*argv, "iod") == 0) {
				p->ifnetem_model = IF_NETEM_MODEL_IOD;
			} else if (strcmp(*argv, "fpd") == 0) {
				p->ifnetem_model = IF_NETEM_MODEL_FPD;
			} else {
				err(1, "Invalid model '%s'", *argv);
			}
			argc--, argv++;
		} else if (strcmp(*argv, "bandwidth") == 0) {
			argc--, argv++;
			if (argc <= 0 || !get_uint64(&bandwitdh, *argv)) {
				err(1, "Invalid value '%s' for bandwidth", *argv);
			}
			argc--, argv++;
		} else if (strcmp(*argv, "corruption") == 0) {
			argc--, argv++;
			if (argc <= 0 || !get_percent_fixed_point(&corruption, *argv)) {
				err(1, "Invalid value '%s' for corruption", *argv);
			}
			argc--, argv++;
		} else if (strcmp(*argv, "delay") == 0) {
			argc--, argv++;
			if (argc <= 0 || !get_uint32(&latency, *argv)) {
				err(1, "Invalid value '%s' for delay", *argv);
			}
			argc--, argv++;
			if (argc > 0 && get_uint32(&jitter, *argv)) {
				argc--, argv++;
			}
		} else if (strcmp(*argv, "duplication") == 0) {
			argc--, argv++;
			if (argc <= 0 || !get_percent_fixed_point(&duplication, *argv)) {
				err(1, "Invalid value '%s' for duplication", *argv);
				return (-1);
			}
			argc--, argv++;
		} else if (strcmp(*argv, "loss") == 0) {
			argc--, argv++;
			if (argc <= 0 || !get_percent_fixed_point(&loss_p_gr_gl, *argv)) {
				err(1, "Invalid value '%s' for loss", *argv);
			}
			/* we may have all 5 probs, use naive model if not */
			argc--, argv++;
			if (argc <= 0 || !get_percent_fixed_point(&loss_p_gr_bl, *argv)) {
				continue;
			}
			/* if more than p_gr_gl, then should have all probs */
			argc--, argv++;
			if (argc <= 0 || !get_percent_fixed_point(&loss_p_bl_br, *argv)) {
				err(1, "Invalid value '%s' for p_bl_br", *argv);
			}
			argc--, argv++;
			if (argc <= 0 || !get_percent_fixed_point(&loss_p_bl_gr, *argv)) {
				err(1, "Invalid value '%s' for p_bl_gr", *argv);
			}
			argc--, argv++;
			if (argc <= 0 || !get_percent_fixed_point(&loss_p_br_bl, *argv)) {
				err(1, "Invalid value '%s' for p_br_bl", *argv);
			}
			argc--, argv++;
		} else if (strcmp(*argv, "recovery") == 0) {
			argc--, argv++;
			if (argc <= 0 || !get_uint32(&loss_recovery_ms, *argv)) {
				err(1, "Invalid value '%s' for recovery", *argv);
			}
			argc--, argv++;
		} else if (strcmp(*argv, "reordering") == 0) {
			argc--, argv++;
			if (argc <= 0 || !get_percent_fixed_point(&reordering, *argv)) {
				err(1, "Invalid value '%s' for reordering", *argv);
			}
			argc--, argv++;
		} else if (strcmp(*argv, "ival") == 0) {
			argc--, argv++;
			if (argc <= 0 || !get_uint32(&output_ival_ms, *argv)) {
				err(1, "Invalid value '%s' for ival", *argv);
			}
			argc--, argv++;
		} else {
			return (-1);
		}
	}

	if (corruption > IF_NETEM_PARAMS_PSCALE) {
		err(1, "corruption percentage > 100%%");
	}

	if (duplication > IF_NETEM_PARAMS_PSCALE) {
		err(1, "duplication percentage > 100%%");
	}

	if (duplication > 0 && latency == 0) {
		/* we need to insert dup'ed packet with latency */
		err(1, "duplication needs latency param");
	}

	if (latency > 1000) {
		err(1, "latency %dms too big (> 1 sec)", latency);
	}

	if (jitter * 3 > latency) {
		err(1, "jitter %dms too big (latency %dms)", jitter, latency);
	}

	/* if gr_gl == 0 (no loss), other prob should all be zero */
	if (loss_p_gr_gl == 0 &&
	    (loss_p_gr_bl != 0 || loss_p_bl_br != 0 || loss_p_bl_gr != 0 ||
	    loss_p_br_bl != 0)) {
		err(1, "loss params not all zero when gr_gl is zero");
	}

	/* check state machine transition prob integrity */
	if (loss_p_gr_gl > IF_NETEM_PARAMS_PSCALE ||
	    /* gr_gl = IF_NETEM_PARAMS_PSCALE for total loss */
	    loss_p_gr_bl > IF_NETEM_PARAMS_PSCALE ||
	    loss_p_bl_br > IF_NETEM_PARAMS_PSCALE ||
	    loss_p_bl_gr > IF_NETEM_PARAMS_PSCALE ||
	    loss_p_br_bl > IF_NETEM_PARAMS_PSCALE ||
	    loss_p_gr_gl + loss_p_gr_bl > IF_NETEM_PARAMS_PSCALE ||
	    loss_p_bl_br + loss_p_bl_gr > IF_NETEM_PARAMS_PSCALE) {
		err(1, "loss params too big");
	}

	if (reordering > IF_NETEM_PARAMS_PSCALE) {
	        err(1, "reordering percentage > 100%%");
	}

	p->ifnetem_bandwidth_bps = bandwitdh;
	p->ifnetem_latency_ms = latency;
	p->ifnetem_jitter_ms = jitter;
	p->ifnetem_corruption_p = corruption;
	p->ifnetem_duplication_p = duplication;
	p->ifnetem_loss_p_gr_gl = loss_p_gr_gl;
	p->ifnetem_loss_p_gr_bl = loss_p_gr_bl;
	p->ifnetem_loss_p_bl_br = loss_p_bl_br;
	p->ifnetem_loss_p_bl_gr = loss_p_bl_gr;
	p->ifnetem_loss_p_br_bl = loss_p_br_bl;
	p->ifnetem_loss_recovery_ms = loss_recovery_ms;
	p->ifnetem_reordering_p = reordering;
	p->ifnetem_output_ival_ms = output_ival_ms;

	return (argc_saved - argc);
}

static char *
netem_model_str(if_netem_model_t model)
{
	switch (model) {
		case IF_NETEM_MODEL_NLC:
			return ("Network link conditioner");
			break;
		case IF_NETEM_MODEL_IOD:
			return ("In-order delivery");
			break;
		case IF_NETEM_MODEL_FPD:
			return ("Fast packet delivery");
			break;
		default:
			return ("unknown");
			break;
	}
}

static void
print_netem_params(struct if_netem_params *p, const char *desc)
{
	struct if_netem_params zero_params;
	double pscale = IF_NETEM_PARAMS_PSCALE / 100;
	bzero(&zero_params, sizeof (zero_params));

	if (memcmp(p, &zero_params, sizeof (zero_params)) == 0) {
		printf("%s NetEm Disabled\n\n", desc);
	} else {
		printf(
		    "%s NetEm Parameters\n"
		    "\tmodel                          %s\n",
		    desc, netem_model_str(p->ifnetem_model));

		if (p->ifnetem_bandwidth_bps == UINT64_MAX) {
			printf("\tbandwidth rate                 unlimited\n");
		} else if (p->ifnetem_bandwidth_bps == 0) {
			printf("\tbandwidth rate                 0, blocking all\n");
		} else {
			printf("\tbandwidth rate                 %llubps\n",
			    p->ifnetem_bandwidth_bps);
		}

		printf(
		    "\tdelay latency                  %dms\n"
		    "\t      jitter                   %dms\n"
		    "\tcorruption                     %.3f%%\n"
		    "\treordering                     %.3f%%\n\n"
		    "\trecovery                       %dms\n",
		    p->ifnetem_latency_ms,
		    p->ifnetem_jitter_ms,
		    (double) p->ifnetem_corruption_p / pscale,
		    (double) p->ifnetem_reordering_p / pscale,
		    p->ifnetem_loss_recovery_ms);

		if (p->ifnetem_loss_p_gr_bl == 0 &&
		    p->ifnetem_loss_p_bl_br == 0 &&
		    p->ifnetem_loss_p_bl_gr == 0 &&
		    p->ifnetem_loss_p_br_bl == 0) {
			printf(
		    "\tloss                           %.3f%%\n",
		    (double) p->ifnetem_loss_p_gr_gl / pscale);
		} else {
			printf(
		    "\tloss GAP_RECV   -> GAP_LOSS    %.3f%%\n"
		    "\t     GAP_RECV   -> BURST_LOSS  %.3f%%\n"
		    "\t     BURST_LOSS -> BURST_RECV  %.3f%%\n"
		    "\t     BURST_LOSS -> GAP_RECV    %.3f%%\n"
		    "\t     BURST_RECV -> BURST_LOSS  %.3f%%\n",
		    (double) p->ifnetem_loss_p_gr_gl / pscale,
		    (double) p->ifnetem_loss_p_gr_bl / pscale,
		    (double) p->ifnetem_loss_p_bl_br / pscale,
		    (double) p->ifnetem_loss_p_bl_gr / pscale,
		    (double) p->ifnetem_loss_p_br_bl / pscale);
		}
	}
}

static int
setnetem(int argc, char *const *argv, int s, const struct afswtch *afp)
{
	struct if_linkparamsreq iflpr;
	struct if_netem_params input_params, output_params;
	int ret = 0, error = 0;

	bzero(&iflpr, sizeof (iflpr));
	bzero(&input_params, sizeof (input_params));
	bzero(&output_params, sizeof (output_params));

	if (argc > 1) {
		if (strcmp(argv[0], "input") == 0) {
			ret = netem_parse_args(&input_params, argc, argv);
		} else if (strcmp(argv[0], "output") == 0) {
			ret = netem_parse_args(&output_params, argc, argv);
		} else if (strcmp(argv[0], "-h") == 0 || strcmp(argv[0], "--help") == 0) {
			goto bad_args;
		} else {
			fprintf(stderr, "uknown option %s\n", argv[0]);
			goto bad_args;
		}
		if (ret < 0) {
			goto bad_args;
		}
	}

	errno = 0;
	strlcpy(iflpr.iflpr_name, name, sizeof (iflpr.iflpr_name));
	error = ioctl(s, SIOCGIFLINKPARAMS, &iflpr);
	if (error < 0) {
		warn("ioctl (get link params)");
	}

	if (argc == 0) {
		print_netem_params(&iflpr.iflpr_input_netem, "Input");
		print_netem_params(&iflpr.iflpr_output_netem, "Output");
		return (0);
	} else if (argc == 1) {
		if (strcmp(argv[0], "input") == 0) {
			bzero(&iflpr.iflpr_input_netem,
			    sizeof (iflpr.iflpr_input_netem));
		} else if (strcmp(argv[0], "output") == 0) {
			bzero(&iflpr.iflpr_output_netem,
			    sizeof (iflpr.iflpr_output_netem));
		} else {
			fprintf(stderr, "uknown option %s\n", argv[0]);
			goto bad_args;
		}
		printf("%s: netem is now disabled for %s\n", name, argv[0]);
		ret = 1;
	} else {
		if (strcmp(argv[0], "input") == 0) {
			iflpr.iflpr_input_netem = input_params;
		} else if (strcmp(argv[0], "output") == 0) {
			iflpr.iflpr_output_netem = output_params;
		}
	}

	error = ioctl(s, SIOCSIFLINKPARAMS, &iflpr);
	if (error < 0 && errno != ENOENT && errno != ENXIO && errno != ENODEV) {
		warn("ioctl (set link params)");
	} else if (errno == ENXIO) {
		printf("netem cannot be set on %s\n", name);
	} else {
		printf("%s: netem configured\n", name);
	}

	return (ret);
bad_args:
	fprintf(stderr, "Usage:\n"
			"\tTo enable/set netem params\n"
			"\t\tnetem <input|output>\n"
			"\t\t      [ bandwidth BIT_PER_SEC ]\n"
			"\t\t      [ delay DELAY_MSEC [ JITTER_MSEC ] ]\n"
			"\t\t      [ loss PERCENTAGE ]\n"
			"\t\t      [ duplication PERCENTAGE ]\n"
			"\t\t      [ reordering PERCENTAGE ]\n\n"
			"\tTo disable <input|output> netem\n"
			"\t\tnetem <input|output>\n\n"
			"\tTo show current settings\n"
			"\t\tnetem\n\n");
	return (-1);
}

static void
setthrottle(const char *val, int dummy __unused, int s,
    const struct afswtch *afp)
{
	struct if_throttlereq iftr;
	char *cp;

	errno = 0;
	bzero(&iftr, sizeof (iftr));
	strlcpy(iftr.ifthr_name, name, sizeof (iftr.ifthr_name));

	iftr.ifthr_level = strtold(val, &cp);
	if (val == cp || errno != 0) {
		warn("Invalid value '%s'", val);
		return;
	}

	if (ioctl(s, SIOCSIFTHROTTLE, &iftr) < 0 && errno != ENXIO) {
		warn("ioctl (set throttling level)");
	} else if (errno == ENXIO) {
		printf("throttling level cannot be set on %s\n", name);
	} else {
		printf("%s: throttling level set to %d\n", name,
		    iftr.ifthr_level);
	}
}

static void
setdisableoutput(const char *val, int dummy __unused, int s,
    const struct afswtch *afp)
{
	struct ifreq ifr;
	char *cp;
	errno = 0;
	bzero(&ifr, sizeof (ifr));
	strlcpy(ifr.ifr_name, name, sizeof (ifr.ifr_name));

	ifr.ifr_ifru.ifru_disable_output = strtold(val, &cp);
	if (val == cp || errno != 0) {
		warn("Invalid value '%s'", val);
		return;
	}

	if (ioctl(s, SIOCSIFDISABLEOUTPUT, &ifr) < 0 && errno != ENXIO) {
		warn("ioctl set disable output");
	} else if (errno == ENXIO) {
		printf("output thread can not be disabled on %s\n", name);
	} else {
		printf("output %s on %s\n",
		    ((ifr.ifr_ifru.ifru_disable_output == 0) ? "enabled" : "disabled"),
		    name);
	}
}

static void
setlog(const char *val, int dummy __unused, int s,
    const struct afswtch *afp)
{
	char *cp;

	errno = 0;
	strlcpy(ifr.ifr_name, name, sizeof(ifr.ifr_name));

	ifr.ifr_log.ifl_level = strtold(val, &cp);
	if (val == cp || errno != 0) {
		warn("Invalid value '%s'", val);
		return;
	}
	ifr.ifr_log.ifl_flags = (IFRLOGF_DLIL|IFRLOGF_FAMILY|IFRLOGF_DRIVER|
	    IFRLOGF_FIRMWARE);

	if (ioctl(s, SIOCSIFLOG, &ifr) < 0)
		warn("ioctl (set logging parameters)");
}

void
setcl2k(const char *vname, int value, int s, const struct afswtch *afp)
{
	strlcpy(ifr.ifr_name, name, sizeof(ifr.ifr_name));
	ifr.ifr_ifru.ifru_2kcl = value;
	
	if (ioctl(s, SIOCSIF2KCL, (caddr_t)&ifr) < 0)
		Perror(vname);
}

void
setexpensive(const char *vname, int value, int s, const struct afswtch *afp)
{
	strlcpy(ifr.ifr_name, name, sizeof(ifr.ifr_name));
	ifr.ifr_ifru.ifru_expensive = value;
	
	if (ioctl(s, SIOCSIFEXPENSIVE, (caddr_t)&ifr) < 0)
		Perror(vname);
}

void
setconstrained(const char *vname, int value, int s, const struct afswtch *afp)
{
    strlcpy(ifr.ifr_name, name, sizeof(ifr.ifr_name));
    ifr.ifr_ifru.ifru_constrained = value;

    if (ioctl(s, SIOCSIFCONSTRAINED, (caddr_t)&ifr) < 0)
        Perror(vname);
}

static void
setifmpklog(const char *vname, int value, int s, const struct afswtch *afp)
{
	strlcpy(ifr.ifr_name, name, sizeof(ifr.ifr_name));
	ifr.ifr_ifru.ifru_mpk_log = value;

	if (ioctl(s, SIOCSIFMPKLOG, (caddr_t)&ifr) < 0)
		Perror(vname);
}

void
settimestamp(const char *vname, int value, int s, const struct afswtch *afp)
{
	strlcpy(ifr.ifr_name, name, sizeof(ifr.ifr_name));
	
	if (value == 0) {
		if (ioctl(s, SIOCSIFTIMESTAMPDISABLE, (caddr_t)&ifr) < 0)
			Perror(vname);
	} else {
		if (ioctl(s, SIOCSIFTIMESTAMPENABLE, (caddr_t)&ifr) < 0)
			Perror(vname);
	}
}

void
setecnmode(const char *val, int dummy __unused, int s,
    const struct afswtch *afp)
{
	char *cp;

	if (strcmp(val, "default") == 0)
		ifr.ifr_ifru.ifru_ecn_mode = IFRTYPE_ECN_DEFAULT;
	else if (strcmp(val, "enable") == 0)
		ifr.ifr_ifru.ifru_ecn_mode = IFRTYPE_ECN_ENABLE;
	else if (strcmp(val, "disable") == 0)
		ifr.ifr_ifru.ifru_ecn_mode = IFRTYPE_ECN_DISABLE;
	else {
		ifr.ifr_ifru.ifru_ecn_mode = strtold(val, &cp);
		if (val == cp || errno != 0) {
			warn("Invalid ECN mode value '%s'", val);
			return;
		}
	}
	
	strlcpy(ifr.ifr_name, name, sizeof(ifr.ifr_name));
	
	if (ioctl(s, SIOCSECNMODE, (caddr_t)&ifr) < 0)
		Perror("ioctl(SIOCSECNMODE)");
}

void
setprobeconnectivity(const char *vname, int value, int s, const struct afswtch *afp)
{
	strlcpy(ifr.ifr_name, name, sizeof(ifr.ifr_name));
	ifr.ifr_ifru.ifru_probe_connectivity = value;

	if (ioctl(s, SIOCSIFPROBECONNECTIVITY, (caddr_t)&ifr) < 0)
		Perror(vname);
}

void
setqosmarking(const char *cmd, const char *arg, int s, const struct afswtch *afp)
{
	u_long ioc;

	strlcpy(ifr.ifr_name, name, sizeof(ifr.ifr_name));
	
	if (strcmp(cmd, "mode") == 0) {
		ioc = SIOCSQOSMARKINGMODE;
		
		if (strcmp(arg, "fastlane") == 0)
			ifr.ifr_qosmarking_mode = IFRTYPE_QOSMARKING_FASTLANE;
        else if (strcmp(arg, "rfc4594") == 0)
            ifr.ifr_qosmarking_mode = IFRTYPE_QOSMARKING_RFC4594;
        else if (strcmp(arg, "custom") == 0)
            ifr.ifr_qosmarking_mode = IFRTYPE_QOSMARKING_CUSTOM;
		else if (strcasecmp(arg, "none") == 0 || strcasecmp(arg, "off") == 0)
			ifr.ifr_qosmarking_mode = IFRTYPE_QOSMARKING_MODE_NONE;
		else
			err(EX_USAGE, "bad value for qosmarking mode: %s", arg);
	} else if (strcmp(cmd, "enabled") == 0) {
		ioc = SIOCSQOSMARKINGENABLED;
		if (strcmp(arg, "1") == 0 || strcasecmp(arg, "on") == 0||
		    strcasecmp(arg, "yes") == 0 || strcasecmp(arg, "true") == 0)
			ifr.ifr_qosmarking_enabled = 1;
		else if (strcmp(arg, "0") == 0 || strcasecmp(arg, "off") == 0||
			 strcasecmp(arg, "no") == 0 || strcasecmp(arg, "false") == 0)
			ifr.ifr_qosmarking_enabled = 0;
		else
			err(EX_USAGE, "bad value for qosmarking enabled: %s", arg);
	} else {
		err(EX_USAGE, "qosmarking takes mode or enabled");
	}
	
	if (ioctl(s, ioc, (caddr_t)&ifr) < 0)
		err(EX_OSERR, "ioctl(%s, %s)", cmd, arg);
}

void
setfastlane(const char *cmd, const char *arg, int s, const struct afswtch *afp)
{
	strlcpy(ifr.ifr_name, name, sizeof(ifr.ifr_name));
	
	warnx("### fastlane is obsolete, use qosmarking ###");
	
	if (strcmp(cmd, "capable") == 0) {
		if (strcmp(arg, "1") == 0 || strcasecmp(arg, "on") == 0||
		    strcasecmp(arg, "yes") == 0 || strcasecmp(arg, "true") == 0)
			setqosmarking("mode", "fastlane", s, afp);
		else if (strcmp(arg, "0") == 0 || strcasecmp(arg, "off") == 0||
			 strcasecmp(arg, "no") == 0 || strcasecmp(arg, "false") == 0)
			setqosmarking("mode", "off", s, afp);
		else
			err(EX_USAGE, "bad value for fastlane %s", cmd);
	} else if (strcmp(cmd, "enable") == 0) {
		if (strcmp(arg, "1") == 0 || strcasecmp(arg, "on") == 0||
		    strcasecmp(arg, "yes") == 0 || strcasecmp(arg, "true") == 0)
			setqosmarking("enabled", "1", s, afp);
		else if (strcmp(arg, "0") == 0 || strcasecmp(arg, "off") == 0||
			 strcasecmp(arg, "no") == 0 || strcasecmp(arg, "false") == 0)
			setqosmarking("enabled", "0", s, afp);
		else
			err(EX_USAGE, "bad value for fastlane %s", cmd);
	} else {
		err(EX_USAGE, "fastlane takes capable or enable");
	}
}

void
setlowpowermode(const char *vname, int value, int s, const struct afswtch *afp)
{
	strlcpy(ifr.ifr_name, name, sizeof(ifr.ifr_name));
	ifr.ifr_low_power_mode = !!value;

	if (ioctl(s, SIOCSIFLOWPOWER, (caddr_t)&ifr) < 0)
		Perror(vname);
}

void
setifmarkwakepkt(const char *vname, int value, int s, const struct afswtch *afp)
{
	strlcpy(ifr.ifr_name, name, sizeof(ifr.ifr_name));
	ifr.ifr_intval = value;

	if (ioctl(s, SIOCSIFMARKWAKEPKT, (caddr_t)&ifr) < 0)
		Perror(vname);
}

void
setnoackpri(const char *vname, int value, int s, const struct afswtch *afp)
{
	strlcpy(ifr.ifr_name, name, sizeof(ifr.ifr_name));
	ifr.ifr_noack_prio = value;

	if (ioctl(s, SIOCSIFNOACKPRIO, (caddr_t)&ifr) < 0)
		Perror(vname);
}

void
setnoshaping(const char *vname, int value, int s, const struct afswtch *afp)
{
	strlcpy(ifr.ifr_name, name, sizeof(ifr.ifr_name));
	ifr.ifr_intval = value;
#ifdef SIOCSIFNOTRAFFICSHAPING
	if (ioctl(s, SIOCSIFNOTRAFFICSHAPING, (caddr_t)&ifr) < 0)
		Perror(vname);
#endif /* SIOCSIFNOTRAFFICSHAPING */
}

void
setmanagement(const char *vname, int value, int s, const struct afswtch *afp)
{
	strlcpy(ifr.ifr_name, name, sizeof(ifr.ifr_name));
	ifr.ifr_intval = value;
#ifdef SIOCSIFMANAGEMENT
	if (ioctl(s, SIOCSIFMANAGEMENT, (caddr_t)&ifr) < 0)
		Perror(vname);
#endif /* SIOCSIFMANAGEMENT */
}

void
setdisableinput(const char *vname, int value, int s, const struct afswtch *afp)
{
	strlcpy(ifr.ifr_name, name, sizeof(ifr.ifr_name));
	ifr.ifr_intval = value;
#ifdef SIOCSIFDISABLEINPUT
	if (ioctl(s, SIOCSIFDISABLEINPUT, (caddr_t)&ifr) < 0)
		Perror(vname);
#endif /* SIOCSIFDISABLEINPUT */
}

struct str2num {
	const char *str;
	uint32_t num;
};

static struct str2num subfamily_str2num[] = {
	{ .str = "any", .num = IFRTYPE_SUBFAMILY_ANY },
	{ .str = "USB", .num = IFRTYPE_SUBFAMILY_USB },
	{ .str = "Bluetooth", .num = IFRTYPE_SUBFAMILY_BLUETOOTH },
	{ .str = "Wi-Fi", .num = IFRTYPE_SUBFAMILY_WIFI },
	{ .str = "wifi", .num = IFRTYPE_SUBFAMILY_WIFI },
	{ .str = "Thunderbolt", .num = IFRTYPE_SUBFAMILY_THUNDERBOLT },
	{ .str = "reserverd", .num = IFRTYPE_SUBFAMILY_RESERVED },
	{ .str = "intcoproc", .num = IFRTYPE_SUBFAMILY_INTCOPROC },
	{ .str = "QuickRelay", .num = IFRTYPE_SUBFAMILY_QUICKRELAY },
	{ .str = "Default", .num = IFRTYPE_SUBFAMILY_DEFAULT },
	{ .str = NULL, .num = 0 },
};

static uint32_t
get_num_from_str(struct str2num* str2nums, const char *str)
{
	struct str2num *str2num = str2nums;

	while (str2num != NULL && str2num->str != NULL) {
		if (strcasecmp(str2num->str, str) == 0) {
			return str2num->num;
		}
		str2num++;
	}
	return 0;
}

static void
setifsubfamily(const char *val, int dummy __unused, int s,
	 const struct afswtch *afp)
{
	strlcpy(ifr.ifr_name, name, sizeof (ifr.ifr_name));

	char *endptr;
	uint32_t subfamily = strtoul(val, &endptr, 0);
	if (*endptr != 0) {
		subfamily = get_num_from_str(subfamily_str2num, val);
		if (subfamily == 0) {
			return;
		}
	}

	ifr.ifr_type.ift_subfamily = subfamily;
	if (ioctl(s, SIOCSIFSUBFAMILY, (caddr_t)&ifr) < 0)
		warn("ioctl(SIOCSIFSUBFAMILY)");
}

void
setifavailability(const char *vname, int value, int s, const struct afswtch *afp)
{
	strlcpy(ifr.ifr_name, name, sizeof(ifr.ifr_name));
	ifr.ifr_interface_state.valid_bitmask = IF_INTERFACE_STATE_INTERFACE_AVAILABILITY_VALID;
	if (value == 0) {
		ifr.ifr_interface_state.interface_availability = IF_INTERFACE_STATE_INTERFACE_UNAVAILABLE;
	} else {
		ifr.ifr_interface_state.interface_availability = IF_INTERFACE_STATE_INTERFACE_AVAILABLE;
	}
	if (ioctl(s, SIOCSIFINTERFACESTATE, (caddr_t)&ifr) < 0)
		warn("ioctl(SIOCSIFINTERFACESTATE)");
}

static void
show_routermode(int s)
{
	struct afswtch *afp;

	afp = af_getbyname("inet");
	if (afp != NULL) {
		(*afp->af_routermode)(s, 0, NULL);
	}
}

static void
show_routermode6(void)
{
	struct afswtch *afp;
	static int 	s = -1;

	afp = af_getbyname("inet6");
	if (afp != NULL) {
		if (s < 0) {
			s = socket(AF_INET6, SOCK_DGRAM, 0);
			if (s < 0) {
				perror("socket");
				return;
			}
		}
		(*afp->af_routermode)(s, 0, NULL);
	}
}

#define	IFHWASSISTBITS \
"\020\1CSUM_IP\2CSUM_TCP\3CSUM_UDP\4CSUM_IP_FRAGS\5CSUM_FRAGMENT\6CSUM_TCPIPV6\7CSUM_UDPIPV6" \
"\10CSUM_FRAGMENT_IPV6\15CSUM_PARTIAL\16CSUM_ZERO_INVERT" \
"\21VLAN_TAGGING\22VLAN_MTU\25MULTIPAGES\26TSO_V4\27TSO_V6" \
"\30TXSTATUS\31HW_TIMESTAMP\32SW_TIMESTAMP\35LRO\36RX_CSUM "

static void
show_hwassist(void)
{
	int mib[6];
	char *buf = NULL;
	size_t buf_len = 0;
	struct if_msghdr *ifm;

	mib[0] = CTL_NET;
	mib[1] = PF_ROUTE;
	mib[2] = 0;
	mib[3] = AF_LINK;
	mib[4] = NET_RT_IFLIST;
	mib[5] = if_nametoindex(name);

	if (sysctl(mib, 6, NULL, &buf_len, NULL, 0) == -1) {
		perror("sysctl");
		goto done;
	}
	buf = calloc(buf_len, 1);
	if (buf == NULL) {
		perror("calloc");
		goto done;
	}
	if (sysctl(mib, 6, buf, &buf_len, NULL, 0) == -1) {
		perror("sysctl");
		goto done;
	}
	ifm = (struct if_msghdr *)(void *)buf;
	if (ifm->ifm_data.ifi_hwassist != 0) {
		printb("\thwassist", ifm->ifm_data.ifi_hwassist, IFHWASSISTBITS);
		printf("\n");
	}

done:
	if (buf != NULL) {
		free(buf);
	}
}


#define	IFFBITS \
"\020\1UP\2BROADCAST\3DEBUG\4LOOPBACK\5POINTOPOINT\6SMART\7RUNNING" \
"\10NOARP\11PROMISC\12ALLMULTI\13OACTIVE\14SIMPLEX\15LINK0\16LINK1\17LINK2" \
"\20MULTICAST"

#define	IFEFBITS \
"\020\1AUTOCONFIGURING\4PROBE_CONNECTIVITY\5ADV_REPORT\6IPV6_DISABLED\7ACCEPT_RTADV\10TXSTART\11RXPOLL" \
"\12VLAN\13BOND\14ARPLL\15CLAT46\16NOAUTOIPV6LL\17EXPENSIVE\20ROUTER4\21CLONE" \
"\22LOCALNET_PRIVATE\23ND6ALT\24RESTRICTED_RECV\25AWDL\26NOACKPRI" \
"\27AWDL_RESTRICTED\30CL2K\31ECN_ENABLE\32ECN_DISABLE\33CHANNEL_DRV\34CA" \
"\35SENDLIST\36DIRECTLINK\37FASTLN_ON\40UPDOWNCHANGE"

#define	IFXFBITS \
"\020\1WOL\2TIMESTAMP\3NOAUTONX\4LEGACY\5TXLOWINET\6RXLOWINET\7ALLOCKPI" \
"\10LOWPOWER\11MPKLOG\12CONSTRAINED\13LOWLAT\14MARKWKPKT\15FPD\16NOSHAPING" \
"\17MANAGEMENT\20ULTRA_CONSTRAINED\21IS_VPN\22DELAYWAKEPKTEVENT\23DISABLE_INPUT"

#define	IFCAPBITS \
"\020\1RXCSUM\2TXCSUM\3VLAN_MTU\4VLAN_HWTAGGING\5JUMBO_MTU" \
"\6TSO4\7TSO6\10LRO\11AV\12TXSTATUS\13CHANNEL_IO\14HW_TIMESTAMP\15SW_TIMESTAMP" \
"\16PARTIAL_CSUM\17ZEROINVERT_CSUM\20LRO_NUM_SEG"

#define	IFRLOGF_BITS \
"\020\1DLIL\21FAMILY\31DRIVER\35FIRMWARE"

/*
 * Print the status of the interface.  If an address family was
 * specified, show only it; otherwise, show them all.
 */
static void
status(const struct afswtch *afp, const struct sockaddr_dl *sdl,
	struct ifaddrs *ifa)
{
	struct ifaddrs *ift;
	int allfamilies, s;
	struct ifstat ifs;
	struct if_descreq ifdr;
	struct if_linkparamsreq iflpr;
	int mib[6];
	struct ifmibdata_supplemental ifmsupp;
	size_t miblen = sizeof(struct ifmibdata_supplemental);
	u_int64_t eflags = 0;
	u_int64_t xflags = 0;
	int curcap = 0;
	
	if (afp == NULL) {
		allfamilies = 1;
		afp = af_getbyname("inet");
	} else
		allfamilies = 0;

	ifr.ifr_addr.sa_family = afp->af_af == AF_LINK ? AF_INET : afp->af_af;
	strlcpy(ifr.ifr_name, name, sizeof(ifr.ifr_name));

	s = socket(ifr.ifr_addr.sa_family, SOCK_DGRAM, 0);
	if (s < 0)
		err(1, "socket(family %u,SOCK_DGRAM)", ifr.ifr_addr.sa_family);

	printf("%s: ", name);
	printb("flags", ifa->ifa_flags, IFFBITS);
	if (ioctl(s, SIOCGIFMETRIC, &ifr) != -1)
		if (ifr.ifr_metric)
			printf(" metric %d", ifr.ifr_metric);
	if (ioctl(s, SIOCGIFMTU, &ifr) != -1)
		printf(" mtu %d", ifr.ifr_mtu);
	if (showrtref && ioctl(s, SIOCGIFGETRTREFCNT, &ifr) != -1)
		printf(" rtref %d", ifr.ifr_route_refcnt);
	if (verbose) {
		unsigned int ifindex = if_nametoindex(ifa->ifa_name);
		if (ifindex != 0)
			printf(" index %u", ifindex);
	}
#ifdef SIOCGIFCONSTRAINED
    // Constrained is stored in if_xflags which isn't exposed directly
    if (ioctl(s, SIOCGIFCONSTRAINED, (caddr_t)&ifr) == 0 &&
        ifr.ifr_constrained != 0) {
        printf(" constrained");
    }
#endif
	putchar('\n');

	if (verbose && ioctl(s, SIOCGIFEFLAGS, (caddr_t)&ifr) != -1 &&
	    (eflags = ifr.ifr_eflags) != 0) {
		printb("\teflags", eflags, IFEFBITS);
		putchar('\n');
	}

	if (verbose && ioctl(s, SIOCGIFXFLAGS, (caddr_t)&ifr) != -1 &&
	    (xflags = ifr.ifr_xflags) != 0) {
		printb("\txflags", xflags, IFXFBITS);
		putchar('\n');
	}

	if (ioctl(s, SIOCGIFCAP, (caddr_t)&ifr) == 0) {
		if (ifr.ifr_curcap != 0) {
			curcap = ifr.ifr_curcap;
			printb("\toptions", ifr.ifr_curcap, IFCAPBITS);
			putchar('\n');
		}
		if (supmedia && ifr.ifr_reqcap != 0) {
			printb("\tcapabilities", ifr.ifr_reqcap, IFCAPBITS);
			putchar('\n');
		}
	}

	if (verbose) {
		show_hwassist();
	}

	tunnel_status(s);

	for (ift = ifa; ift != NULL; ift = ift->ifa_next) {
		if (ift->ifa_addr == NULL)
			continue;
		if (strcmp(ifa->ifa_name, ift->ifa_name) != 0)
			continue;
		if (allfamilies) {
			const struct afswtch *p;
			p = af_getbyfamily(ift->ifa_addr->sa_family);
			if (p != NULL && p->af_status != NULL)
				p->af_status(s, ift);
		} else if (afp->af_af == ift->ifa_addr->sa_family)
			afp->af_status(s, ift);
	}

/* Print CLAT46 address */
	clat46_addr(s, name);

/* Print NAT64 prefix */
	nat64_status(s, name);

#if 0
	if (allfamilies || afp->af_af == AF_LINK) {
		const struct afswtch *lafp;

		/*
		 * Hack; the link level address is received separately
		 * from the routing information so any address is not
		 * handled above.  Cobble together an entry and invoke
		 * the status method specially.
		 */
		lafp = af_getbyname("lladdr");
		if (lafp != NULL) {
			info.rti_info[RTAX_IFA] = (struct sockaddr *)sdl;
			lafp->af_status(s, &info);
		}
	}
#endif
	if (allfamilies)
		af_other_status(s);
	else if (afp->af_other_status != NULL) {
		call_af_other_status(afp, s);
	}

	strlcpy(ifs.ifs_name, name, sizeof ifs.ifs_name);
	if (ioctl(s, SIOCGIFSTATUS, &ifs) == 0) 
		printf("%s", ifs.ascii);

	/* The rest is for when verbose is set; if not set, we're done */
	if (!verbose)
		goto done;

#ifdef SIOCGIFGENERATIONID
	if (ioctl(s, SIOCGIFGENERATIONID, &ifr) != -1) {
		printf("\tgeneration id: %llu\n", ifr.ifr_creation_generation_id);
	}
#endif /* SIOCGIFGENERATIONID */

	if (ioctl(s, SIOCGIFTYPE, &ifr) != -1) {
		char *c = ift2str(ifr.ifr_type.ift_type,
		    ifr.ifr_type.ift_family, ifr.ifr_type.ift_subfamily);
		if (c != NULL)
			printf("\ttype: %s\n", c);
	}

	if (verbose > 1) {
		if (ioctl(s, SIOCGIFFUNCTIONALTYPE, &ifr) != -1) {
			char *c = iffunct2str(ifr.ifr_functional_type);
			if (c != NULL)
				printf("\tfunctional type: %s\n", c);
		}
	}
	{
		struct if_agentidsreq ifar;
		memset(&ifar, 0, sizeof(ifar));

		strlcpy(ifar.ifar_name, name, sizeof(ifar.ifar_name));

		if (ioctl(s, SIOCGIFAGENTIDS, &ifar) != -1) {
			if (ifar.ifar_count != 0) {
				ifar.ifar_uuids = calloc(ifar.ifar_count, sizeof(uuid_t));
				if (ifar.ifar_uuids != NULL) {
					if (ioctl(s, SIOCGIFAGENTIDS, &ifar) != 1) {
						for (int agent_i = 0; agent_i < ifar.ifar_count; agent_i++) {
							struct netagent_req nar;
							memset(&nar, 0, sizeof(nar));

							uuid_copy(nar.netagent_uuid, ifar.ifar_uuids[agent_i]);

							if (ioctl(s, SIOCGIFAGENTDATA, &nar) != 1) {
								printf("\tagent domain:%s type:%s flags:0x%x desc:\"%s\"\n",
									   nar.netagent_domain, nar.netagent_type,
									   nar.netagent_flags, nar.netagent_desc);
							}
						}
					}
					free(ifar.ifar_uuids);
				}
			}
		}
	}

	if (ioctl(s, SIOCGIFLINKQUALITYMETRIC, &ifr) != -1) {
		int lqm = ifr.ifr_link_quality_metric;
		if (verbose > 1) {
			printf("\tlink quality: %d ", lqm);
			if (lqm == IFNET_LQM_THRESH_OFF)
				printf("(off)");
			else if (lqm == IFNET_LQM_THRESH_UNKNOWN)
				printf("(unknown)");
			else if (lqm > IFNET_LQM_THRESH_UNKNOWN &&
				 lqm <= IFNET_LQM_THRESH_BAD)
				printf("(bad)");
			else if (lqm > IFNET_LQM_THRESH_UNKNOWN &&
				 lqm <= IFNET_LQM_THRESH_POOR)
				printf("(poor)");
			else if (lqm > IFNET_LQM_THRESH_POOR &&
			    lqm <= IFNET_LQM_THRESH_GOOD)
				printf("(good)");
			else
				printf("(?)");
			printf("\n");
		} else if (lqm > IFNET_LQM_THRESH_UNKNOWN) {
			printf("\tlink quality: %d ", lqm);
			if (lqm <= IFNET_LQM_THRESH_BAD)
				printf("(bad)");
			else if (lqm <= IFNET_LQM_THRESH_POOR)
				printf("(poor)");
			else if (lqm <= IFNET_LQM_THRESH_GOOD)
				printf("(good)");
			else
				printf("(?)");
			printf("\n");
		}
	}

	{
		if (ioctl(s, SIOCGIFINTERFACESTATE, &ifr) != -1) {
			printf("\tstate");
			if (ifr.ifr_interface_state.valid_bitmask &
			    IF_INTERFACE_STATE_RRC_STATE_VALID) {
				uint8_t rrc_state = ifr.ifr_interface_state.rrc_state;
				
				printf(" rrc: %u ", rrc_state);
				if (rrc_state == IF_INTERFACE_STATE_RRC_STATE_CONNECTED)
					printf("(connected)");
				else if (rrc_state == IF_INTERFACE_STATE_RRC_STATE_IDLE)
					printf("(idle)");
				else
					printf("(?)");
			}
			if (ifr.ifr_interface_state.valid_bitmask &
			    IF_INTERFACE_STATE_INTERFACE_AVAILABILITY_VALID) {
				uint8_t ifavail = ifr.ifr_interface_state.interface_availability;
				
				printf(" availability: %u ", ifavail);
				if (ifavail == IF_INTERFACE_STATE_INTERFACE_AVAILABLE)
					printf("(true)");
				else if (ifavail == IF_INTERFACE_STATE_INTERFACE_UNAVAILABLE)
					printf("(false)");
				else
					printf("(?)");
			} else {
				printf(" availability: (not valid)");
			}
			if (verbose > 1 &&
			    ifr.ifr_interface_state.valid_bitmask &
			    IF_INTERFACE_STATE_LQM_STATE_VALID) {
				int8_t lqm = ifr.ifr_interface_state.lqm_state;
				
				printf(" lqm: %d", lqm);
				
				if (lqm == IFNET_LQM_THRESH_OFF)
					printf("(off)");
				else if (lqm == IFNET_LQM_THRESH_UNKNOWN)
					printf("(unknown)");
				else if (lqm == IFNET_LQM_THRESH_BAD)
					printf("(bad)");
				else if (lqm == IFNET_LQM_THRESH_POOR)
					printf("(poor)");
				else if (lqm == IFNET_LQM_THRESH_GOOD)
					printf("(good)");
				else
					printf("(?)");
			}
		}
		printf("\n");
	}
	
	bzero(&iflpr, sizeof (iflpr));
	strlcpy(iflpr.iflpr_name, name, sizeof (iflpr.iflpr_name));
	if (ioctl(s, SIOCGIFLINKPARAMS, &iflpr) != -1) {
		u_int64_t ibw_max = iflpr.iflpr_input_bw.max_bw;
		u_int64_t ibw_eff = iflpr.iflpr_input_bw.eff_bw;
		u_int64_t obw_max = iflpr.iflpr_output_bw.max_bw;
		u_int64_t obw_eff = iflpr.iflpr_output_bw.eff_bw;
		u_int64_t obw_tbr = iflpr.iflpr_output_tbr_rate;
		u_int32_t obw_pct = iflpr.iflpr_output_tbr_percent;
		u_int64_t ilt_max = iflpr.iflpr_input_lt.max_lt;
		u_int64_t ilt_eff = iflpr.iflpr_input_lt.eff_lt;
		u_int64_t olt_max = iflpr.iflpr_output_lt.max_lt;
		u_int64_t olt_eff = iflpr.iflpr_output_lt.eff_lt;


		if (eflags & IFEF_TXSTART) {
			u_int32_t flags = iflpr.iflpr_flags;
			u_int32_t sched = iflpr.iflpr_output_sched;
			struct if_throttlereq iftr;

			printf("\tscheduler: %s%s ",
			    (flags & IFLPRF_ALTQ) ? "ALTQ_" : "",
			    sched2str(sched));
			if (flags & IFLPRF_DRVMANAGED)
				printf("(driver managed)");
			printf("\n");

			bzero(&iftr, sizeof (iftr));
			strlcpy(iftr.ifthr_name, name,
			    sizeof (iftr.ifthr_name));
			if (ioctl(s, SIOCGIFTHROTTLE, &iftr) != -1 &&
			    iftr.ifthr_level != IFNET_THROTTLE_OFF)
				printf("\tthrottling: level %d (%s)\n",
				    iftr.ifthr_level, tl2str(iftr.ifthr_level));
		}

		if (obw_tbr != 0 && obw_eff > obw_tbr)
			obw_eff = obw_tbr;

		if (ibw_max != 0 || obw_max != 0) {
			if (ibw_max == obw_max && ibw_eff == obw_eff &&
			    ibw_max == ibw_eff && obw_tbr == 0) {
				printf("\tlink rate: %s\n",
				    bps_to_str(ibw_max));
			} else {
				printf("\tuplink rate: %s [eff] / ",
				    bps_to_str(obw_eff));
				if (obw_tbr != 0) {
					if (obw_pct == 0)
						printf("%s [tbr] / ",
						    bps_to_str(obw_tbr));
					else
						printf("%s [tbr %u%%] / ",
						    bps_to_str(obw_tbr),
						    obw_pct);
				}
				printf("%s", bps_to_str(obw_max));
				if (obw_tbr != 0)
					printf(" [max]");
				printf("\n");
				if (ibw_eff == ibw_max) {
					printf("\tdownlink rate: %s\n",
					    bps_to_str(ibw_max));
				} else {
					printf("\tdownlink rate: "
					    "%s [eff] / ", bps_to_str(ibw_eff));
					printf("%s [max]\n",
					    bps_to_str(ibw_max));
				}
			}
		} else if (obw_tbr != 0) {
			printf("\tuplink rate: %s [tbr]\n",
			    bps_to_str(obw_tbr));
		}

		if (ilt_max != 0 || olt_max != 0) {
			if (ilt_max == olt_max && ilt_eff == olt_eff &&
			    ilt_max == ilt_eff) {
				printf("\tlink latency: %s\n",
				    ns_to_str(ilt_max));
			} else {
				if (olt_max != 0 && olt_eff == olt_max) {
					printf("\tuplink latency: %s\n",
					    ns_to_str(olt_max));
				} else if (olt_max != 0) {
					printf("\tuplink latency: "
					    "%s [eff] / ", ns_to_str(olt_eff));
					printf("%s [max]\n",
					    ns_to_str(olt_max));
				}
				if (ilt_max != 0 && ilt_eff == ilt_max) {
					printf("\tdownlink latency: %s\n",
					    ns_to_str(ilt_max));
				} else if (ilt_max != 0) {
					printf("\tdownlink latency: "
					    "%s [eff] / ", ns_to_str(ilt_eff));
					printf("%s [max]\n",
					    ns_to_str(ilt_max));
				}
			}
		}
	}

	/* Common OID prefix */
	mib[0] = CTL_NET;
	mib[1] = PF_LINK;
	mib[2] = NETLINK_GENERIC;
	mib[3] = IFMIB_IFDATA;
	mib[4] = if_nametoindex(name);
	mib[5] = IFDATA_SUPPLEMENTAL;
	if (sysctl(mib, 6, &ifmsupp, &miblen, (void *)0, 0) == -1)
		err(1, "sysctl IFDATA_SUPPLEMENTAL");

	if (ifmsupp.ifmd_data_extended.ifi_alignerrs != 0) {
		printf("\tunaligned pkts: %llu\n",
		    ifmsupp.ifmd_data_extended.ifi_alignerrs);
	}
	if (ifmsupp.ifmd_data_extended.ifi_dt_bytes != 0) {
		printf("\tdata milestone interval: %s\n",
		    bytes_to_str(ifmsupp.ifmd_data_extended.ifi_dt_bytes));
	}

	bzero(&ifdr, sizeof (ifdr));
	strlcpy(ifdr.ifdr_name, name, sizeof (ifdr.ifdr_name));
	if (ioctl(s, SIOCGIFDESC, &ifdr) != -1 && ifdr.ifdr_len) {
		printf("\tdesc: %s\n", ifdr.ifdr_desc);
	}

	if (ioctl(s, SIOCGIFLOG, &ifr) != -1 && ifr.ifr_log.ifl_level) {
		printf("\tlogging: level %d ", ifr.ifr_log.ifl_level);
		printb("facilities", ifr.ifr_log.ifl_flags, IFRLOGF_BITS);
		putchar('\n');
	}

	if (ioctl(s, SIOCGIFDELEGATE, &ifr) != -1 && ifr.ifr_delegated) {
		char delegatedif[IFNAMSIZ+1];
		if (if_indextoname(ifr.ifr_delegated, delegatedif) != NULL)
			printf("\teffective interface: %s\n", delegatedif);
	}

	if (ioctl(s, SIOCGSTARTDELAY, &ifr) != -1) {
		if (ifr.ifr_start_delay_qlen > 0 &&
		    ifr.ifr_start_delay_timeout > 0) {
			printf("\ttxstart qlen: %u packets "
			    "timeout: %u microseconds\n",
			    ifr.ifr_start_delay_qlen,
			    ifr.ifr_start_delay_timeout/1000);
		}
	}

    if ((curcap & (IFCAP_HW_TIMESTAMP | IFCAP_SW_TIMESTAMP)) &&
	    ioctl(s, SIOCGIFTIMESTAMPENABLED, &ifr) != -1) {
		printf("\ttimestamp: %s\n",
		       (ifr.ifr_intval != 0) ? "enabled" : "disabled");
	}

    if (ioctl(s, SIOCGQOSMARKINGENABLED, &ifr) != -1) {
		printf("\tqosmarking enabled: %s mode: ",
		       ifr.ifr_qosmarking_enabled ? "yes" : "no");
		if (ioctl(s, SIOCGQOSMARKINGMODE, &ifr) != -1) {
			switch (ifr.ifr_qosmarking_mode) {
				case IFRTYPE_QOSMARKING_FASTLANE:
					printf("fastlane\n");
					break;
                case IFRTYPE_QOSMARKING_RFC4594:
                    printf("RFC4594\n");
                    break;
                case IFRTYPE_QOSMARKING_CUSTOM:
                    printf("custom\n");
                    break;
				case IFRTYPE_QOSMARKING_MODE_NONE:
					printf("none\n");
					break;
				default:
					printf("unknown (%u)\n", ifr.ifr_qosmarking_mode);
					break;
			}
		}
	}

	if (ioctl(s, SIOCGIFLOWPOWER, &ifr) != -1) {
		printf("\tlow power mode: %s\n",
		       (ifr.ifr_low_power_mode != 0) ? "enabled" : "disabled");
	}
	if (ioctl(s, SIOCGIFMPKLOG, &ifr) != -1) {
		printf("\tmulti layer packet logging (mpklog): %s\n",
		       (ifr.ifr_mpk_log != 0) ? "enabled" : "disabled");
	}
	show_routermode(s);
	show_routermode6();
done:
	close(s);
	return;
}

#define	KILOBYTES	1024
#define	MEGABYTES	(KILOBYTES * KILOBYTES)
#define	GIGABYTES	(KILOBYTES * KILOBYTES * KILOBYTES)

static char *
bytes_to_str(unsigned long long bytes)
{
        static char buf[32];
        const char *u;
        long double n = bytes, t;

        if (bytes >= GIGABYTES) {
                t = n / GIGABYTES;
                u = "GB";
        } else if (n >= MEGABYTES) {
                t = n / MEGABYTES;
                u = "MB";
        } else if (n >= KILOBYTES) {
                t = n / KILOBYTES;
                u = "KB";
        } else {
                t = n;
                u = "bytes";
        }

        snprintf(buf, sizeof (buf), "%-4.2Lf %s", t, u);
        return (buf);
}

#define	GIGABIT_PER_SEC	1000000000	/* gigabit per second */
#define MEGABIT_PER_SEC	1000000		/* megabit per second */
#define	KILOBIT_PER_SEC	1000		/* kilobit per second */

static char *
bps_to_str(unsigned long long rate)
{
        static char buf[32];
        const char *u;
        long double n = rate, t;

        if (rate >= GIGABIT_PER_SEC) {
                t = n / GIGABIT_PER_SEC;
                u = "Gbps";
        } else if (n >= MEGABIT_PER_SEC) {
                t = n / MEGABIT_PER_SEC;
                u = "Mbps";
        } else if (n >= KILOBIT_PER_SEC) {
                t = n / KILOBIT_PER_SEC;
                u = "Kbps";
        } else {
                t = n;
                u = "bps ";
        }

        snprintf(buf, sizeof (buf), "%-4.2Lf %4s", t, u);
        return (buf);
}

#define	NSEC_PER_SEC	1000000000	/* nanosecond per second */
#define	USEC_PER_SEC	1000000		/* microsecond per second */
#define	MSEC_PER_SEC	1000		/* millisecond per second */

static char *
ns_to_str(unsigned long long nsec)
{
        static char buf[32];
        const char *u;
        long double n = nsec, t;

        if (nsec >= NSEC_PER_SEC) {
                t = n / NSEC_PER_SEC;
                u = "sec ";
        } else if (n >= USEC_PER_SEC) {
                t = n / USEC_PER_SEC;
                u = "msec";
        } else if (n >= MSEC_PER_SEC) {
                t = n / MSEC_PER_SEC;
                u = "usec";
        } else {
                t = n;
                u = "nsec";
        }

        snprintf(buf, sizeof (buf), "%-4.2Lf %4s", t, u);
        return (buf);
}

static void
tunnel_status(int s)
{
	af_all_tunnel_status(s);
}

static void
clat46_addr(int s, char * if_name)
{
	struct if_clat46req ifr;
	char buf[MAXHOSTNAMELEN];

	bzero(&ifr, sizeof (ifr));
	strlcpy(ifr.ifclat46_name, if_name, sizeof(ifr.ifclat46_name));

	if (ioctl(s, SIOCGIFCLAT46ADDR, &ifr) < 0) {
		if (errno != ENOENT && errno != ENOMEM && errno != EPERM)
			warn("ioctl (SIOCGIFCLAT46ADDR)");
		return;
	}

	if (inet_ntop(AF_INET6, &ifr.ifclat46_addr.v6_address, buf, sizeof(buf)) != NULL)
		printf("\tinet6 %s prefixlen %d clat46\n",
			buf, ifr.ifclat46_addr.v6_prefixlen);
}

static void
nat64_status(int s, char * if_name)
{
	int i;
	struct if_nat64req ifr;
	char buf[MAXHOSTNAMELEN];

	bzero(&ifr, sizeof(ifr));
	strlcpy(ifr.ifnat64_name, if_name, sizeof(ifr.ifnat64_name));

	if (ioctl(s, SIOCGIFNAT64PREFIX, &ifr) < 0) {
		if (errno != ENOENT && errno != ENOMEM && errno != EPERM)
			warn("ioctl(SIOCGIFNAT64PREFIX)");
		return;
	}

	for (i = 0; i < NAT64_MAX_NUM_PREFIXES; i++) {
		if (ifr.ifnat64_prefixes[i].prefix_len > 0) {
			inet_ntop(AF_INET6, &ifr.ifnat64_prefixes[i].ipv6_prefix, buf, sizeof(buf));
			printf("\tnat64 prefix %s prefixlen %d\n",
			    buf, ifr.ifnat64_prefixes[i].prefix_len << 3);
		}
	}
}

void
Perror(const char *cmd)
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

/*
 * Print a value a la the %b format of the kernel's printf
 */
void
printb(const char *s, unsigned v, const char *bits)
{
	int i, any = 0;
	char c;

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

static struct cmd basic_cmds[] = {
	DEF_CMD("up",		IFF_UP,		setifflags),
	DEF_CMD("down",		-IFF_UP,	setifflags),
	DEF_CMD("arp",		-IFF_NOARP,	setifflags),
	DEF_CMD("-arp",		IFF_NOARP,	setifflags),
	DEF_CMD("debug",	IFF_DEBUG,	setifflags),
	DEF_CMD("-debug",	-IFF_DEBUG,	setifflags),
#ifdef IFF_PPROMISC
	DEF_CMD("promisc",	IFF_PPROMISC,	setifflags),
	DEF_CMD("-promisc",	-IFF_PPROMISC,	setifflags),
#endif /* IFF_PPROMISC */
	DEF_CMD("add",		IFF_UP,		notealias),
	DEF_CMD("alias",	IFF_UP,		notealias),
	DEF_CMD("-alias",	-IFF_UP,	notealias),
	DEF_CMD("delete",	-IFF_UP,	notealias),
	DEF_CMD("remove",	-IFF_UP,	notealias),
#ifdef notdef
#define	EN_SWABIPS	0x1000
	DEF_CMD("swabips",	EN_SWABIPS,	setifflags),
	DEF_CMD("-swabips",	-EN_SWABIPS,	setifflags),
#endif
	DEF_CMD_ARG("netmask",			setifnetmask),
	DEF_CMD_ARG("metric",			setifmetric),
	DEF_CMD_ARG("broadcast",		setifbroadaddr),
	DEF_CMD_ARG("ipdst",			setifipdst),
	DEF_CMD_ARG2("tunnel",			settunnel),
	DEF_CMD("-tunnel", 0,			deletetunnel),
	DEF_CMD("deletetunnel", 0,		deletetunnel),
	DEF_CMD("link0",	IFF_LINK0,	setifflags),
	DEF_CMD("-link0",	-IFF_LINK0,	setifflags),
	DEF_CMD("link1",	IFF_LINK1,	setifflags),
	DEF_CMD("-link1",	-IFF_LINK1,	setifflags),
	DEF_CMD("link2",	IFF_LINK2,	setifflags),
	DEF_CMD("-link2",	-IFF_LINK2,	setifflags),
#ifdef IFF_MONITOR
	DEF_CMD("monitor",	IFF_MONITOR:,	setifflags),
	DEF_CMD("-monitor",	-IFF_MONITOR,	setifflags),
#endif /* IFF_MONITOR */
	DEF_CMD("mpklog",	1,		setifmpklog),
	DEF_CMD("-mpklog",	0,		setifmpklog),
#ifdef IFF_STATICARP
	DEF_CMD("staticarp",	IFF_STATICARP,	setifflags),
	DEF_CMD("-staticarp",	-IFF_STATICARP,	setifflags),
#endif /* IFF_STATICARP */
#ifdef IFCAP_RXCSUM
	DEF_CMD("rxcsum",	IFCAP_RXCSUM,	setifcap),
	DEF_CMD("-rxcsum",	-IFCAP_RXCSUM,	setifcap),
#endif /* IFCAP_RXCSUM */
#ifdef IFCAP_TXCSUM
	DEF_CMD("txcsum",	IFCAP_TXCSUM,	setifcap),
	DEF_CMD("-txcsum",	-IFCAP_TXCSUM,	setifcap),
#endif /* IFCAP_TXCSUM */
#ifdef IFCAP_NETCONS
	DEF_CMD("netcons",	IFCAP_NETCONS,	setifcap),
	DEF_CMD("-netcons",	-IFCAP_NETCONS,	setifcap),
#endif /* IFCAP_NETCONS */
#ifdef IFCAP_POLLING
	DEF_CMD("polling",	IFCAP_POLLING,	setifcap),
	DEF_CMD("-polling",	-IFCAP_POLLING,	setifcap),
#endif /* IFCAP_POLLING */
#ifdef IFCAP_TSO
	DEF_CMD("tso",		IFCAP_TSO,	setifcap),
	DEF_CMD("-tso",		-IFCAP_TSO,	setifcap),
#endif /* IFCAP_TSO */
#ifdef IFCAP_LRO
	DEF_CMD("lro",		IFCAP_LRO,	setifcap),
	DEF_CMD("-lro",		-IFCAP_LRO,	setifcap),
#endif /* IFCAP_LRO */
#ifdef IFCAP_WOL
	DEF_CMD("wol",		IFCAP_WOL,	setifcap),
	DEF_CMD("-wol",		-IFCAP_WOL,	setifcap),
#endif /* IFCAP_WOL */
#ifdef IFCAP_WOL_UCAST
	DEF_CMD("wol_ucast",	IFCAP_WOL_UCAST,	setifcap),
	DEF_CMD("-wol_ucast",	-IFCAP_WOL_UCAST,	setifcap),
#endif /* IFCAP_WOL_UCAST */
#ifdef IFCAP_WOL_MCAST
	DEF_CMD("wol_mcast",	IFCAP_WOL_MCAST,	setifcap),
	DEF_CMD("-wol_mcast",	-IFCAP_WOL_MCAST,	setifcap),
#endif /* IFCAP_WOL_MCAST */
#ifdef IFCAP_WOL_MAGIC
	DEF_CMD("wol_magic",	IFCAP_WOL_MAGIC,	setifcap),
	DEF_CMD("-wol_magic",	-IFCAP_WOL_MAGIC,	setifcap),
#endif /* IFCAP_WOL_MAGIC */
	DEF_CMD("normal",	-IFF_LINK0,	setifflags),
	DEF_CMD("compress",	IFF_LINK0,	setifflags),
	DEF_CMD("noicmp",	IFF_LINK1,	setifflags),
	DEF_CMD_ARG("mtu",			setifmtu),
#ifdef notdef
	DEF_CMD_ARG("name",			setifname),
#endif /* notdef */
#ifdef IFCAP_AV
	DEF_CMD("av", IFCAP_AV, setifcap),
	DEF_CMD("-av", -IFCAP_AV, setifcap),
#endif /* IFCAP_AV */
	DEF_CMD("router",	1,		setrouter),
	DEF_CMD("-router",	0,		setrouter),
	DEF_CMD_VA("routermode", 		routermode),
	DEF_CMD_ARG("desc",			setifdesc),
	DEF_CMD_ARG("tbr",			settbr),
	DEF_CMD_VA("netem",			setnetem),
	DEF_CMD_ARG("throttle",			setthrottle),
	DEF_CMD_ARG("log",			setlog),
	DEF_CMD("cl2k",	1,			setcl2k),
	DEF_CMD("-cl2k",	0,		setcl2k),
	DEF_CMD("expensive",	1,		setexpensive),
	DEF_CMD("-expensive",	0,		setexpensive),
#ifdef SIOCSIFCONSTRAINED
    DEF_CMD("constrained",  1,      setconstrained),
    DEF_CMD("-constrained", 0,      setconstrained),
#endif
	DEF_CMD("timestamp",	1,		settimestamp),
	DEF_CMD("-timestamp",	0,		settimestamp),
	DEF_CMD_ARG("ecn",			setecnmode),
	DEF_CMD_ARG2("fastlane",		setfastlane),
	DEF_CMD_ARG2("qosmarking",		setqosmarking),
	DEF_CMD_ARG("disable_output",		setdisableoutput),
	DEF_CMD("probe_connectivity",	1,		setprobeconnectivity),
	DEF_CMD("-probe_connectivity",	0,		setprobeconnectivity),
	DEF_CMD("lowpowermode",	1,		setlowpowermode),
	DEF_CMD("-lowpowermode",	0,	setlowpowermode),
	DEF_CMD_ARG("subfamily",		setifsubfamily),
	DEF_CMD("available",	1,	setifavailability),
	DEF_CMD("-available",	0,	setifavailability),
	DEF_CMD("unavailable",	0,	setifavailability),
	DEF_CMD("markwakepkt",	1,	setifmarkwakepkt),
	DEF_CMD("-markwakepkt",	0,	setifmarkwakepkt),
	DEF_CMD("noackpri",  1,      setnoackpri),
	DEF_CMD("-noackpri", 0,      setnoackpri),
	DEF_CMD("noshaping",  1,      setnoshaping),
	DEF_CMD("-noshaping", 0,      setnoshaping),
	DEF_CMD("management", 1,      setmanagement),
	DEF_CMD("-management", 0,      setmanagement),
	DEF_CMD("disableinput", 1,      setdisableinput),
	DEF_CMD("-disableinput", 0,      setdisableinput),
};

static __constructor void
ifconfig_ctor(void)
{
#define	N(a)	(sizeof(a) / sizeof(a[0]))
	int i;

	for (i = 0; i < N(basic_cmds);  i++)
		cmd_register(&basic_cmds[i]);
#undef N
}

static char *
sched2str(unsigned int s)
{
	char *c;

	switch (s) {
	case PKTSCHEDT_NONE:
		c = "NONE";
		break;
	case PKTSCHEDT_FQ_CODEL:
		c = "FQ_CODEL";
		break;
	default:
		c = "UNKNOWN";
		break;
	}

	return (c);
}

static char *
tl2str(unsigned int s)
{
	char *c;

	switch (s) {
	case IFNET_THROTTLE_OFF:
		c = "off";
		break;
	case IFNET_THROTTLE_OPPORTUNISTIC:
		c = "opportunistic";
		break;
	default:
		c = "unknown";
		break;
	}

	return (c);
}

static char *
ift2str(unsigned int t, unsigned int f, unsigned int sf)
{
	static char buf[256];
	char *c = NULL;

	switch (t) {
	case IFT_ETHER:
		switch (sf) {
		case IFRTYPE_SUBFAMILY_USB:
			c = "USB Ethernet";
			break;
		case IFRTYPE_SUBFAMILY_BLUETOOTH:
			c = "Bluetooth PAN";
			break;
		case IFRTYPE_SUBFAMILY_WIFI:
			c = "Wi-Fi";
			break;
		case IFRTYPE_SUBFAMILY_THUNDERBOLT:
			c = "IP over Thunderbolt";
			break;
		case IFRTYPE_SUBFAMILY_ANY:
		default:
			c = "Ethernet";
			break;
		}
		break;

	case IFT_IEEE1394:
		c = "IP over FireWire";
		break;

	case IFT_PKTAP:
		c = "Packet capture";
		break;

	case IFT_CELLULAR:
		c = "Cellular";
		break;

	case IFT_OTHER:
		if (ifr.ifr_type.ift_family == APPLE_IF_FAM_IPSEC) {
			if (ifr.ifr_type.ift_subfamily == IFRTYPE_SUBFAMILY_BLUETOOTH) {
				c = "Companion Link Bluetooth";
			} else if (ifr.ifr_type.ift_subfamily == IFRTYPE_SUBFAMILY_QUICKRELAY) {
				c = "Companion Link QuickRelay";
			} else if (ifr.ifr_type.ift_subfamily == IFRTYPE_SUBFAMILY_WIFI) {
				c = "Companion Link Wi-Fi";
			} else if (ifr.ifr_type.ift_subfamily == IFRTYPE_SUBFAMILY_DEFAULT) {
				c = "Companion Link Default";
			}
		}
		break;

	case IFT_BRIDGE:
	case IFT_PFLOG:
	case IFT_PFSYNC:
	case IFT_PPP:
	case IFT_LOOP:
	case IFT_GIF:
	case IFT_STF:
	case IFT_L2VLAN:
	case IFT_IEEE8023ADLAG:
	default:
		break;
	}

	if (verbose > 1) {
		if (c == NULL) {
			(void) snprintf(buf, sizeof (buf),
			    "0x%x family: %u subfamily: %u",
			    ifr.ifr_type.ift_type, ifr.ifr_type.ift_family,
			    ifr.ifr_type.ift_subfamily);
		} else {
			(void) snprintf(buf, sizeof (buf),
			    "%s (0x%x) family: %u subfamily: %u", c,
			    ifr.ifr_type.ift_type, ifr.ifr_type.ift_family,
			    ifr.ifr_type.ift_subfamily);
		}
		c = buf;
	}

	return (c);
}

static char *
iffunct2str(u_int32_t functional_type)
{
	char *str = NULL;

	switch (functional_type) {
		case IFRTYPE_FUNCTIONAL_UNKNOWN:
			break;

		case IFRTYPE_FUNCTIONAL_LOOPBACK:
			str = "loopback";
			break;

		case IFRTYPE_FUNCTIONAL_WIRED:
			str = "wired";
			break;

		case IFRTYPE_FUNCTIONAL_WIFI_INFRA:
			str = "wifi";
			break;

		case IFRTYPE_FUNCTIONAL_WIFI_AWDL:
			str = "awdl";
			break;

		case IFRTYPE_FUNCTIONAL_CELLULAR:
			str = "cellular";
			break;

		case IFRTYPE_FUNCTIONAL_INTCOPROC:
			break;

		case IFRTYPE_FUNCTIONAL_COMPANIONLINK:
			str = "companionlink";
			break;

		default:
			break;
	}
	return str;
}

static void freeformat(void)
{

	if (f_inet != NULL)
		free(f_inet);
	if (f_inet6 != NULL)
		free(f_inet6);
	if (f_ether != NULL)
		free(f_ether);
	if (f_addr != NULL)
		free(f_addr);
}

static void setformat(char *input)
{
	char	*formatstr, *category, *modifier;

	formatstr = strdup(input);
	while ((category = strsep(&formatstr, ",")) != NULL) {
		modifier = strchr(category, ':');
		if (modifier == NULL || modifier[1] == '\0') {
			warnx("Skipping invalid format specification: %s\n",
				category);
			continue;
		}

		/* Split the string on the separator, then seek past it */
		modifier[0] = '\0';
		modifier++;

		if (strcmp(category, "addr") == 0)
			f_addr = strdup(modifier);
		else if (strcmp(category, "ether") == 0)
			f_ether = strdup(modifier);
		else if (strcmp(category, "inet") == 0)
			f_inet = strdup(modifier);
		else if (strcmp(category, "inet6") == 0)
			f_inet6 = strdup(modifier);
	}
	free(formatstr);
}
