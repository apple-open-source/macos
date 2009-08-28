/*
 * Copyright (c) 2000 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */
/*
 * Copyright (c) 1984, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Sun Microsystems, Inc.
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

/*
 * Modification History:
 *
 * 25 Feb 1998	Dieter Siegmund (dieter@apple.com)
 * - significantly restructured to make it useful for inclusion
 *   in a library
 *   - define MAIN to generate the arp command
 * - removed use of most global variables, abstracted
 *   out the ability to call arp_set() as a library
 *   routine, moved error messages to error codes
 *
 * 22 Sep 2000	Dieter Siegmund (dieter@apple.com)
 * - added arp_flush()
 */
#include <sys/param.h>
#include <sys/file.h>
#include <sys/socket.h>
#include <sys/sysctl.h>

#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_types.h>

#include <netinet/in.h>
#include <netinet/if_ether.h>

#include <arpa/inet.h>

#include <err.h>
#include <errno.h>
#include <netdb.h>
#include <nlist.h>
#include <paths.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "arp.h"

typedef struct {
    int				expire_time;
    int				flags;
    int				export_only;
    int				doing_proxy;
    int				if_index;
    struct sockaddr_inarp 	sin_m;
    struct sockaddr_dl 		sdl_m;
} route_options;

#ifdef MAIN
static int	delete __P((int, int, char * *));
static int	dump __P((u_long));
static int	ether_aton2 __P((char *, u_char *));
static void	ether_print __P((u_char *));
/*
static int	file __P((int, char *));
*/
static int	get __P((char *));
static int	getsocket __P((void));
static int	set __P((int, int, char **));
static void	usage __P((void));

static int nflag;
#endif MAIN

static int	rtmsg __P((int, int, route_msg *, route_options *));

static const struct sockaddr_inarp blank_sin = {sizeof(blank_sin), AF_INET };
static const struct sockaddr_dl blank_sdl = {sizeof(blank_sdl), AF_LINK };


static const char * arperrors[] = {
    "success",
    "cannot intuit interface index and type",
    "can only proxy",
    "proxy entry exists for non 802 device",
    "internal error",
    "write to routing socket failed",
    "read to routing socket failed",
    "can't locate",
    0
};

const char *
arp_strerror(int err)
{
    if (err < ARP_RETURN_LAST && err >= ARP_RETURN_SUCCESS)
	return (arperrors[err]);
    return ("unknown error");
}

#ifdef MAIN

typedef enum {
    command_none_e = 0,
    command_dump_e,
    command_delete_e,
    command_set_e,
    command_flush_e,
} command_t;

int
main(argc, argv)
	int argc;
	char **argv;
{
	int ch;
	int s;
	int aflag = 0;
	command_t cmd = command_none_e;
	int ret = 0;

	while ((ch = getopt(argc, argv, "andsF")) != EOF) {
		switch((char)ch) {
		case 'a':
			aflag = 1;
			break;
		case 'd':
			if (nflag ||aflag 
			    || cmd != command_none_e) {
				usage();
			}
			cmd = command_delete_e;
			break;
		case 'n':
			nflag = 1;
			break;
		case 's':
			if (cmd != command_none_e || argc < 4 || argc > 7)
				usage();
			cmd = command_set_e;
			break;
		case 'F':
			if (cmd != command_none_e)
				usage();
			cmd = command_flush_e;
			break;
		case '?':
		default:
			usage();
		}
	}
	if (cmd == command_none_e) {
		cmd = command_dump_e;
	}
	switch (cmd) {
	case command_dump_e:
		if (aflag)
			dump(0);
		else {
			if ((argc - optind) != 1)
				usage();
			ret = get(argv[optind]) ? 1 : 0;
		}
		break;
	case command_delete_e:
		if ((argc - optind) < 1 || (argc - optind) > 2)
			usage();
		s = getsocket();
		ret = delete(s, argc - optind, &argv[optind]) ? 1 : 0;
		break;
	case command_set_e:
		if ((argc - optind) < 2)
			usage();
		s = getsocket();
		ret = set(s, argc - optind, &argv[optind]) ? 1 : 0;
		break;
	case command_flush_e:
		s = getsocket();
		arp_flush(s, aflag);
		break;
	default:
		break;
	}
	exit(ret);
	return (0);
}

/*
 * Process a file to set standard arp entries
 */
/*
int
file(int s, char * name)
{
	FILE *fp;
	int i, retval;
	char line[100], arg[5][50], *args[5];

	if ((fp = fopen(name, "r")) == NULL) {
		fprintf(stderr, "arp: cannot open %s\n", name);
		exit(1);
	}
	args[0] = &arg[0][0];
	args[1] = &arg[1][0];
	args[2] = &arg[2][0];
	args[3] = &arg[3][0];
	args[4] = &arg[4][0];
	retval = 0;
	while(fgets(line, 100, fp) != NULL) {
		i = sscanf(line, "%s %s %s %s %s", arg[0], arg[1], arg[2],
		    arg[3], arg[4]);
		if (i < 2) {
			fprintf(stderr, "arp: bad line: %s\n", line);
			retval = 1;
			continue;
		}
		if (set(s, i, args))
			retval = 1;
	}
	fclose(fp);
	return (retval);
}
*/

int
getsocket() {
	int s;
	s = socket(AF_ROUTE, SOCK_RAW, 0);
	if (s < 0) {
	    perror("arp: socket");
	    exit(1);
	}
	return (s);
}



/*
 * Set an individual arp entry 
 */
int
set(int s, int argc, char *argv[])
{
	struct hostent *hp;
	char *host = argv[0], *eaddr = argv[1];
	struct in_addr iaddr;
	struct ether_addr ether;
	int 	temp = 0;
	int		pub = 0;
	
	argc -= 2;
	argv += 2;
	iaddr.s_addr = inet_addr(host);
	if (iaddr.s_addr == -1) {
		if (!(hp = gethostbyname(host))) {
			fprintf(stderr, "arp: %s: ", host);
			herror((char *)NULL);
			return (1);
		}
		iaddr = *((struct in_addr *)hp->h_addr);
	}
	if (ether_aton2(eaddr, (u_char *)&ether))
		return(1);
	
	while (argc-- > 0) {
		if (strncmp(argv[0], "temp", 4) == 0)
			temp = 1;
		else if (strncmp(argv[0], "pub", 3) == 0)
			pub = 1;
		else if (strncmp(argv[0], "trail", 5) == 0) {
			printf("%s: Sending trailers is no longer supported\n",
		               host);
	    	}
		argv++;
	}
	{
	    int ret;

	    errno = 0;
	    ret = arp_set(s, &iaddr, &ether, 6, temp, pub);
	    if (ret == ARP_RETURN_SUCCESS)
		return (0);
	    printf("set: %s, %s", arp_strerror(ret), host);
	    if (errno)
		printf(": %s\n", strerror(errno));
	    else
		printf("\n");
	}
	return (1);
}

/*
 * Display an individual arp entry
 */
int
get(host)
	char *host;
{
	struct hostent *hp;
	struct	sockaddr_inarp sin_m;
	struct sockaddr_inarp *sin = &sin_m;

	sin_m = blank_sin;
	sin->sin_addr.s_addr = inet_addr(host);
	if (sin->sin_addr.s_addr == -1) {
		if (!(hp = gethostbyname(host))) {
			fprintf(stderr, "arp: %s: ", host);
			herror((char *)NULL);
			return (1);
		}
		bcopy((char *)hp->h_addr, (char *)&sin->sin_addr,
		    sizeof sin->sin_addr);
	}
	if (dump(sin->sin_addr.s_addr) == 0) {
		printf("%s (%s) -- no entry\n",
		    host, inet_ntoa(sin->sin_addr));
		return (1);
	}
	return (0);
}

/*
 * Delete an arp entry 
 */
int
delete(int s, int argc, char * * argv)
{
	char * host;
	struct hostent *hp;
	int export = 0;
	struct in_addr iaddr;

	host = argv[0];
	if (argc > 1 && strncmp(argv[1], "pro", 3) )
		export = 1;

	iaddr.s_addr = inet_addr(host);
	if (iaddr.s_addr == -1) {
		if (!(hp = gethostbyname(host))) {
			fprintf(stderr, "arp: %s: ", host);
			herror((char *)NULL);
			return (1);
		}
		bcopy((char *)hp->h_addr, (char *)&iaddr,
		      sizeof (iaddr));
	}
	{
	    int ret;

	    errno = 0;
	    ret = arp_delete(s, iaddr, export);
	    if (ret == ARP_RETURN_SUCCESS) {
		printf("%s (%s) deleted\n", host, inet_ntoa(iaddr));
		return (0);
	    }
	    printf("delete: %s %s", arp_strerror(ret), host);
	    if (errno)
		printf(": %s\n", strerror(errno));
	    else
		printf("\n");
	}
	return (1);
}

/*
 * Dump the entire arp table
 */
int
dump(addr)
	u_long addr;
{
	int mib[6];
	size_t needed;
	char *host, *lim, *buf, *next;
	struct rt_msghdr *rtm;
	struct sockaddr_inarp *sin;
	struct sockaddr_dl *sdl;
	extern int h_errno;
	struct hostent *hp;
	int found_entry = 0;

	mib[0] = CTL_NET;
	mib[1] = PF_ROUTE;
	mib[2] = 0;
	mib[3] = AF_INET;
	mib[4] = NET_RT_FLAGS;
	mib[5] = RTF_LLINFO;
	if (sysctl(mib, 6, NULL, &needed, NULL, 0) < 0)
		err(1, "route-sysctl-estimate");
	if ((buf = malloc(needed)) == NULL)
		err(1, "malloc");
	if (sysctl(mib, 6, buf, &needed, NULL, 0) < 0)
		err(1, "actual retrieval of routing table");
	lim = buf + needed;
	for (next = buf; next < lim; next += rtm->rtm_msglen) {
		rtm = (struct rt_msghdr *)next;
		sin = (struct sockaddr_inarp *)(rtm + 1);
		sdl = (struct sockaddr_dl *)(sin + 1);
		if (addr) {
			if (addr != sin->sin_addr.s_addr)
				continue;
			found_entry = 1;
		}
		if (nflag == 0)
			hp = gethostbyaddr((caddr_t)&(sin->sin_addr),
			    sizeof sin->sin_addr, AF_INET);
		else
			hp = 0;
		if (hp)
			host = hp->h_name;
		else {
			host = "?";
			if (h_errno == TRY_AGAIN)
				nflag = 1;
		}
		printf("%s (%s) at ", host, inet_ntoa(sin->sin_addr));
		if (sdl->sdl_alen)
			ether_print((u_char *)LLADDR(sdl));
		else
			printf("(incomplete)");
		if (rtm->rtm_rmx.rmx_expire == 0)
			printf(" permanent");
		if (sin->sin_other & SIN_PROXY)
			printf(" published (proxy only)");
		if (rtm->rtm_addrs & RTA_NETMASK) {
			sin = (struct sockaddr_inarp *)
				(sdl->sdl_len + (char *)sdl);
			if (sin->sin_addr.s_addr == 0xffffffff)
				printf(" published");
			if (sin->sin_len != 8)
				printf("(weird)");
		}
		printf("\n");
	}
	return (found_entry);
}

void
ether_print(cp)
	u_char *cp;
{
	printf("%x:%x:%x:%x:%x:%x", cp[0], cp[1], cp[2], cp[3], cp[4], cp[5]);
}

int
ether_aton2(a, n)
	char *a;
	u_char *n;
{
	int i, o[6];

	i = sscanf(a, "%x:%x:%x:%x:%x:%x", &o[0], &o[1], &o[2],
					   &o[3], &o[4], &o[5]);
	if (i != 6) {
		fprintf(stderr, "arp: invalid Ethernet address '%s'\n", a);
		return (1);
	}
	for (i=0; i<6; i++)
		n[i] = o[i];
	return (0);
}

void
usage()
{
	printf("usage: arp hostname\n");
	printf("       arp -a [-n]\n");
	printf("       arp -d hostname\n");
	printf("       arp -s hostname ether_addr [temp] [pub]\n");
	printf("       arp -f filename\n");
	printf("       arp -F [-a]\n");
	exit(1);
}

#else MAIN
#ifdef TESTING

#define TRUE		1
#define FALSE		0

int
main()
{
    int s;
    char ea[6] = { 0, 1, 2, 3, 4, 5 };
    char buf[100];
    struct in_addr ia;

    s = socket(PF_ROUTE, SOCK_RAW, AF_INET);
    if (s < 0) {
	printf("couldn't get routing socket: %s\n", strerror(errno));
	exit(2);
    }

    while (TRUE) {
	int i;
	int arp_ret;

	for (i = 20; i < 45; i++) {
	    ea[5] = i;
	    sprintf(buf, "14.3.3.%d", i);
	    ia.s_addr = inet_addr(buf);
	    arp_delete(s, ia, FALSE);
//	    usleep(100 * 1000);
	    arp_ret = arp_set(s, &ia, (void *)ea, 6, TRUE, FALSE);
	    if (arp_ret)
		printf("arp_set failed: %s\n", arp_strerror(arp_ret));
	}
    }
    exit (0);
}

#endif TESTING
#endif MAIN

static int
rtmsg(int s, int cmd, route_msg * msg_p, route_options * opt)
{
	static int seq;
	register struct rt_msghdr *rtm = &(msg_p->m_rtm);
	register char *cp = msg_p->m_space;
	register int l;
	struct	sockaddr_in so_mask = {8, 0, 0, { 0xffffffff}};
	int pid = getpid();
	int rtmsg_tries = 0;

	if (cmd == RTM_DELETE)
		goto doit;
	bzero((char *)msg_p, sizeof(*msg_p));
	rtm->rtm_flags = opt->flags;
	if (opt->if_index != 0) {
	    rtm->rtm_index = opt->if_index;
	    rtm->rtm_flags |= RTF_IFSCOPE;
	}
	rtm->rtm_version = RTM_VERSION;

	switch (cmd) {
	default:
		return (ARP_RETURN_INTERNAL_ERROR);
		break;
	case RTM_CHANGE:
	case RTM_ADD:
		rtm->rtm_addrs |= RTA_GATEWAY;
		rtm->rtm_rmx.rmx_expire = opt->expire_time;
		rtm->rtm_inits = RTV_EXPIRE;
		rtm->rtm_flags |= (RTF_HOST | RTF_STATIC);
		opt->sin_m.sin_other = 0;
		if (opt->doing_proxy) {
			if (opt->export_only)
				opt->sin_m.sin_other = SIN_PROXY;
			else {
				rtm->rtm_addrs |= RTA_NETMASK;
				rtm->rtm_flags &= ~RTF_HOST;
			}
		}
		/* FALLTHROUGH */
	case RTM_GET:
		rtm->rtm_addrs |= RTA_DST;
	}
#define NEXTADDR(w, s, n) \
	if (rtm->rtm_addrs & (w)) { \
		bcopy((char *)s, cp, n); cp += n;}

	NEXTADDR(RTA_DST, &opt->sin_m, sizeof(opt->sin_m));
	NEXTADDR(RTA_GATEWAY, &opt->sdl_m, sizeof(opt->sdl_m));
	NEXTADDR(RTA_NETMASK, &so_mask, sizeof(so_mask));

	rtm->rtm_msglen = cp - (char *)msg_p;
      doit:
	l = rtm->rtm_msglen;
	rtm->rtm_seq = ++seq;
	rtm->rtm_type = cmd;
	if (write(s, (char *)msg_p, l) != l) {
#if 0
	    if (rtm->rtm_errno)
		printf("write rtm->rtm_errno %d, %s\n", rtm->rtm_errno,
		       strerror(rtm->rtm_errno));
#endif 0
	    if (rtm->rtm_errno != ESRCH || cmd != RTM_DELETE)
		return (ARP_RETURN_WRITE_FAILED);
	}
	errno = 0;
	while ((l = read(s, (char *)msg_p, sizeof(*msg_p))) > 0) {
#if 0
	    if (errno)
		perror("read");
	    if (rtm->rtm_errno)
		printf("read rtm->rtm_errno %d, %s\n", rtm->rtm_errno,
		       strerror(rtm->rtm_errno));
#endif 0
	    if (rtm->rtm_seq == seq && rtm->rtm_pid == pid) {
		return (ARP_RETURN_SUCCESS);
	    }
#define MAX_RETRIES		20
	    if (rtm->rtm_seq == 0) {
		rtmsg_tries++;
		if (rtmsg_tries > MAX_RETRIES) {
#if 0
		    printf("rtmsg_tries exceeded %d\n", MAX_RETRIES);
#endif 0
		    return (ARP_RETURN_READ_FAILED);
		}
#if 0
		printf("rtmsg_tries %d\n", rtmsg_tries);
#endif 0
	    }
#if 0
	    else
		printf("seq %d rtm_pid %d\n", rtm->rtm_seq, rtm->rtm_pid);
#endif 0
	}
	if (l < 0)
		return (ARP_RETURN_READ_FAILED);
	return (ARP_RETURN_SUCCESS);
}

int
arp_get(int s, route_msg * msg_p, struct in_addr * iaddr_p, int if_index)
{
	route_options 			opt;
   	register struct sockaddr_inarp *sin;
    	register struct sockaddr_dl *	sdl;
	int				ret;
    	register struct rt_msghdr *	rtm = &(msg_p->m_rtm);

    	bzero(&opt, sizeof(opt));
    	opt.sdl_m = blank_sdl;
    	opt.sin_m = blank_sin;
		opt.if_index = if_index;
    	sin = &opt.sin_m;
    	sin->sin_addr = *iaddr_p;
    
	ret = rtmsg(s, RTM_GET, msg_p, &opt);
	if (ret)
	    return (ret);
	sin = (struct sockaddr_inarp *)msg_p->m_space;
	sdl = (struct sockaddr_dl *)(sin->sin_len + (char *)sin);
	if (sin->sin_addr.s_addr == opt.sin_m.sin_addr.s_addr) {
		if (sdl->sdl_family == AF_LINK &&
		    (rtm->rtm_flags & RTF_LLINFO) &&
		    !(rtm->rtm_flags & RTF_GATEWAY)) switch (sdl->sdl_type) {
		case IFT_ETHER: case IFT_FDDI: case IFT_ISO88023:
		case IFT_ISO88024: case IFT_ISO88025: case IFT_IEEE1394:
			goto found_it;
		}
		return (ARP_RETURN_PROXY_ONLY);
	}
      found_it:
	return (0);
}

/*
 * Function: arp_set
 *
 * Purpose:
 *   Create an arp entry for the given host. If the entry already
 *   exists, delete it first.
 * Assumes:
 *   s is an open routing socket
 */
int
arp_set(int s, struct in_addr * iaddr_p, void * hwaddr_p, int hwaddr_len,
	int temp, int public)
{
	route_options opt;
   	register struct sockaddr_inarp *sin;
    	register struct sockaddr_dl *sdl;
    	route_msg			msg;
    	register struct rt_msghdr *rtm = &(msg.m_rtm);
	int ret;
    
    	bzero(&opt, sizeof(opt));
    	opt.sdl_m = blank_sdl;
    	opt.sin_m = blank_sin;
    	sin = &opt.sin_m;
    	sin->sin_addr = *iaddr_p;
    
    	bcopy(hwaddr_p, (u_char *)LLADDR(&opt.sdl_m), hwaddr_len);
    	opt.sdl_m.sdl_alen = hwaddr_len;
    	if (temp) {
		struct timeval time;
		gettimeofday(&time, 0);
		opt.expire_time = time.tv_sec + 20 * 60;
    	}
    	if (public) {
		opt.flags |= RTF_ANNOUNCE;
		opt.doing_proxy = SIN_PROXY;
   	}
      tryagain:
	ret = rtmsg(s, RTM_GET, &msg, &opt);
	if (ret)
	    return (ret);

	sin = (struct sockaddr_inarp *)msg.m_space;
	sdl = (struct sockaddr_dl *)(sin->sin_len + (char *)sin);
	if (sin->sin_addr.s_addr == opt.sin_m.sin_addr.s_addr) {
		if (sdl->sdl_family == AF_LINK &&
		    (rtm->rtm_flags & RTF_LLINFO) &&
		    !(rtm->rtm_flags & RTF_GATEWAY)) switch (sdl->sdl_type) {
		case IFT_ETHER: case IFT_FDDI: case IFT_ISO88023:
		case IFT_ISO88024: case IFT_ISO88025: case IFT_IEEE1394:
			goto overwrite;
		}
		if (opt.doing_proxy == 0)
			return (ARP_RETURN_PROXY_ONLY);
		if (opt.sin_m.sin_other & SIN_PROXY)
			return (ARP_RETURN_PROXY_ON_NON_802);
		opt.sin_m.sin_other = SIN_PROXY;
		opt.export_only = 1;
		goto tryagain;
	}
      overwrite:
	if (sdl->sdl_family != AF_LINK)
		return (ARP_RETURN_INTERFACE_NOT_FOUND);
	opt.sdl_m.sdl_type = sdl->sdl_type;
	opt.sdl_m.sdl_index = sdl->sdl_index;
	return (rtmsg(s, RTM_ADD, &msg, &opt));
}

/*
 * Function: arp_delete
 *
 * Purpose:
 *   Delete the arp entry for the given host.
 * Assumes:
 *   s is an open routing socket
 */
int 
arp_delete(int s, struct in_addr iaddr, int export)
{
    	route_options 			opt;
	register struct sockaddr_inarp *sin;
	route_msg			msg;
	register struct rt_msghdr *rtm = &(msg.m_rtm);
	struct sockaddr_dl *sdl;
	int ret;

	bzero(&opt, sizeof(opt));

	if (export)
		opt.export_only = 1;
	opt.sin_m = blank_sin;
	sin = &opt.sin_m;
	sin->sin_addr = iaddr;
      tryagain:
	ret = rtmsg(s, RTM_GET, &msg, &opt);
	if (ret)
		return (ret);
	sin = (struct sockaddr_inarp *)(rtm + 1);
	sdl = (struct sockaddr_dl *)(sin->sin_len + (char *)sin);
	if (sin->sin_addr.s_addr == opt.sin_m.sin_addr.s_addr) {
		if (sdl->sdl_family == AF_LINK &&
		    (rtm->rtm_flags & RTF_LLINFO) &&
		    !(rtm->rtm_flags & RTF_GATEWAY)) switch (sdl->sdl_type) {
		case IFT_ETHER: case IFT_FDDI: case IFT_ISO88023:
		case IFT_ISO88024: case IFT_ISO88025:
			goto delete;
		}
	}
	if (opt.sin_m.sin_other & SIN_PROXY) {
		return (ARP_RETURN_HOST_NOT_FOUND);
	} else {
		opt.sin_m.sin_other = SIN_PROXY;
		goto tryagain;
	}
      delete:
	if (sdl->sdl_family != AF_LINK) {
		return (ARP_RETURN_HOST_NOT_FOUND);
	}
	ret = rtmsg(s, RTM_DELETE, &msg, &opt);
	if (ret)
		return (ret);
	return (ARP_RETURN_SUCCESS);
}

int
arp_flush(int s, int all)
{
	int mib[6];
	size_t needed;
	char *lim, *buf, *next;
	struct rt_msghdr *rtm;
	struct sockaddr_inarp *sin;
	struct sockaddr_dl *sdl;

	mib[0] = CTL_NET;
	mib[1] = PF_ROUTE;
	mib[2] = 0;
	mib[3] = AF_INET;
	mib[4] = NET_RT_FLAGS;
	mib[5] = RTF_LLINFO;
#ifdef MAIN
	if (sysctl(mib, 6, NULL, &needed, NULL, 0) < 0)
		err(1, "route-sysctl-estimate");
	if ((buf = malloc(needed)) == NULL)
		err(1, "malloc");
	if (sysctl(mib, 6, buf, &needed, NULL, 0) < 0)
		err(1, "actual retrieval of routing table");
#else MAIN
	if (sysctl(mib, 6, NULL, &needed, NULL, 0) < 0)
	    	return (-1);
	if ((buf = malloc(needed)) == NULL)
		return (-1);
	if (sysctl(mib, 6, buf, &needed, NULL, 0) < 0) {
		free(buf);
	    	return (-1);
	}
#endif MAIN
	lim = buf + needed;
	for (next = buf; next < lim; next += rtm->rtm_msglen) {
		rtm = (struct rt_msghdr *)next;
		sin = (struct sockaddr_inarp *)(rtm + 1);
		sdl = (struct sockaddr_dl *)(sin + 1);
		if (rtm->rtm_rmx.rmx_expire == 0 && all == 0) {
		    /* permanent entry */
		    continue;
		}
		(void)arp_delete(s, sin->sin_addr, FALSE);
	}
	free(buf);
	return (0);
}
