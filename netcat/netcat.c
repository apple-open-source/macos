/* $OpenBSD: netcat.c,v 1.82 2005/07/24 09:33:56 marius Exp $ */
/*
 * Copyright (c) 2001 Eric Jackson <ericj@monkey.org>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *   notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *   notice, this list of conditions and the following disclaimer in the
 *   documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *   derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Re-written nc(1) for OpenBSD. Original implementation by
 * *Hobbit* <hobbit@avian.org>.
 */

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/un.h>
#include <sys/event.h>
#include <sys/ioctl.h>

#include <net/if.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <arpa/telnet.h>
#include <arpa/inet.h>

#include <err.h>
#include <errno.h>
#ifdef __APPLE__
#include <limits.h>
#endif /* __APPLE__ */
#include <netdb.h>
#include <poll.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <string.h>
#include <getopt.h>
#include <fcntl.h>
#include <pthread.h>
#include <sysexits.h>

#include "atomicio.h"

#include <network/conninfo.h>

static int str2sotc(const char *);
static int str2netservicetype(const char *);
static u_int8_t str2tos(const char *, bool *);

#ifndef SUN_LEN
#define SUN_LEN(su) \
	(sizeof(*(su)) - sizeof((su)->sun_path) + strlen((su)->sun_path))
#endif

#define PORT_MAX	65535
#define PORT_MAX_LEN	6

/* Command Line Options */
int	cflag;					/* CRLF line-ending */
int	dflag;					/* detached, no stdin */
int	iflag;					/* Interval Flag */
#ifndef __APPLE__
int	jflag;					/* use jumbo frames if we can */
#endif /* !__APPLE__ */
int	kflag;					/* More than one connect */
int	lflag;					/* Bind to local port */
int	nflag;					/* Don't do name look up */
char   *pflag;					/* Localport flag */
int	rflag;					/* Random ports flag */
char   *sflag;					/* Source Address */
int	tflag;					/* Telnet Emulation */
int	uflag;					/* UDP - Default to TCP */
int	vflag;					/* Verbosity */
int	xflag;					/* Socks proxy */
int	Oflag;					/* use connect vs. connectx */
int	zflag;					/* Port Scan Flag */
int	Dflag;					/* sodebug */
#ifndef __APPLE__
int	Sflag;					/* TCP MD5 signature option */
#endif /* !__APPLE__ */

#ifdef __APPLE__
int	Aflag;					/* Set SO_RECV_ANYIF on socket */
int	aflag;					/* Set SO_AWDL_UNRESTRICTED on socket */
int	mflag;					/* Set SO_INTCOPROC_ALLOW on socket */
char	*boundif;				/* interface to bind to */
int	ifscope;				/* idx of bound to interface */
int	Cflag;					/* cellular connection OFF option */
int	Eflag;					/* expensive connection OFF option */
int	tclass = SO_TC_BE;			/* traffic class value */
int	Kflag;					/* traffic class option */
int	Fflag;					/* disable flow advisory for UDP if set */
int	Gflag;					/* TCP connection timeout */
int	tcp_conn_timeout;			/* Value of TCP connection timeout */
int	Hflag;					/* TCP keep idle option */
int	tcp_conn_keepidle;			/* Value of TCP keep idle interval in seconds */
int	Iflag;					/* TCP keep intvl option */
int	tcp_conn_keepintvl;			/* Value of TCP keep interval in seconds */
int	Jflag;					/* TCP keep count option */
int	tcp_conn_keepcnt;			/* Value of TCP keep count */
int	Lflag;					/* TCP adaptive read timeout */
int	Mflag;					/* MULTIPATH domain */
int	Nflag;					/* TCP adaptive write timeout */
int	oflag;					/* set options after connect/bind */
int	tcp_conn_adaptive_rtimo;		/* Value of TCP adaptive timeout */
int	tcp_conn_adaptive_wtimo;		/* Value of TCP adaptive timeout */

int		pid_flag = 0;			/* delegated pid */
const char	*pid_optarg = NULL;		/* delegated pid option */
pid_t		pid = -1;			/* value of delegated pid */

int		uuid_flag = 0;			/* delegated uuid */
const char	*uuid_optarg = NULL;		/* delegated uuid option */
uuid_t	uuid;					/* value of delegated uuid */

int		extbkidle_flag = 0;		/* extended background idle mode */

int		nowakefromsleep_flag = 0;	/* extended background idle mode */

int		ecn_mode_flag = 0;		/* ECN mode option */
const char	*ecn_mode_optarg = NULL;	/* ECN mode option */
int		ecn_mode = -1;			/* TCP_ECN_MODE value  */

int		kao_flag;				/* Keep Alive Offload option */
const char	*kao_optarg;				/* Keep Alive Offload option */
int		kao = -1;				/* Keep Alive Offload value */

int		Tflag = -1;			/* IP Type of Service */

int		netsvctype_flag = 0;		/* Network service type  */
const char	*netsvctype_optarg = NULL;	/* Network service type option string */
int	netsvctype = -1;			/* SO_NET_SERVICE_TYPE value */
#endif /* __APPLE__ */

int srcroute = 0;				/* Source routing IPv4/IPv6 options */
char *srcroute_hosts = NULL;

/* Variables for receiving socket events */
int sockev = 0;
pthread_t sockev_thread;
void* sockev_receive(void *arg);

int notify_ack = 0;

int use_flowadv = 1;
int timeout = -1;
int family = AF_UNSPEC;
char *portlist[PORT_MAX+1];

void	atelnet(int, unsigned char *, unsigned int);
void	build_ports(char *);
void	help(void);
int	local_listen(char *, char *, struct addrinfo);
void	readwrite(int);
int	remote_connect(const char *, const char *, struct addrinfo);
int	remote_connectx(const char *, const char *, struct addrinfo);
int	socks_connect(const char *, const char *, struct addrinfo, const char *, const char *,
	struct addrinfo, int);
int	udptest(int);
int	unix_connect(char *);
int	unix_listen(char *);
void    set_common_sockopts(int, int);
void	usage(int);
int	showconninfo(int, sae_connid_t);
void	showmpinfo(int);

extern int sourceroute(struct addrinfo *, char *, char **, int *, int *, int *);

const struct option long_options[] =
{
	{ "apple-resvd-0",	no_argument,		NULL,	'4' },
	{ "apple-resvd-1",	no_argument,		NULL,	'6' },
#ifdef __APPLE__
	{ "apple-recv-anyif",	no_argument,		NULL,	'A' },
	{ "apple-awdl-unres",	no_argument,		NULL,	'a' },
	{ "apple-boundif",	required_argument,	NULL,	'b' },
	{ "apple-no-cellular",	no_argument,		NULL,	'C' },
#endif /* __APPLE__ */
	{ "apple-resvd-2",	no_argument,		NULL,	'c' },
	{ "apple-resvd-3",	no_argument,		NULL,	'D' },
	{ "apple-resvd-4",	no_argument,		NULL,	'd' },
#ifdef __APPLE__
	{ "apple-no-expensive",	no_argument,		NULL,	'E' },
	{ "apple-no-flowadv",	no_argument,		NULL,	'F' },
	{ "apple-tcp-timeout",	required_argument,	NULL,	'G' },
	{ "apple-tcp-keepalive",required_argument,	NULL,	'H' },
#endif /* __APPLE__ */
	{ "apple-resvd-5",	no_argument,		NULL,	'h' },
	{ "apple-resvd-6",	required_argument,	NULL,	'i' },
#ifdef __APPLE__
	{ "apple-tcp-keepintvl",required_argument,	NULL,	'I' },
	{ "apple-tcp-keepcnt",	required_argument,	NULL,	'J' },
	{ "apple-tclass",	required_argument,	NULL,	'K' },
#endif /* __APPLE__ */
	{ "apple-resvd-7",	no_argument,		NULL,	'k' },
	{ "apple-resvd-8",	no_argument,		NULL,	'l' },
#ifdef __APPLE__
	{ "apple-tcp-adp-rtimo",required_argument,	NULL,	'L' },
	{ "apple-multipath",	no_argument,		NULL,	'M' },
	{ "apple-tcp-adp-wtimo",required_argument,	NULL,	'N' },
#endif /* __APPLE__ */
	{ "apple-resvd-9",	no_argument,		NULL,	'n' },
#ifdef __APPLE__
	{ "apple-setsockopt-later",no_argument,		NULL,	'o' },
	{ "apple-no-connectx",	no_argument,		NULL,	'O' },
#endif /* __APPLE__ */
	{ "apple-resvd-10",	required_argument,	NULL,	'p' },
#ifdef __APPLE__
	{ "apple-delegate-pid",	required_argument,	&pid_flag, 1 },
	{ "apple-delegate-uuid",required_argument,	&uuid_flag, 1 },
#endif /* __APPLE__ */
	{ "apple-resvd-11",	no_argument,		NULL,	'r' },
	{ "apple-resvd-12",	required_argument,	NULL,	's' },
#ifndef __APPLE__
	{ "apple-resvd-13",	no_argument,		NULL,	'S' },
#endif /* !__APPLE__ */
	{ "apple-tos",		required_argument,	NULL,	'T' },
	{ "apple-resvd-14",	no_argument,		NULL,	't' },
	{ "apple-resvd-15",	no_argument,		NULL,	'U' },
	{ "apple-resvd-16",	no_argument,		NULL,	'u' },
	{ "apple-resvd-17",	no_argument,		NULL,	'v' },
	{ "apple-resvd-18",	required_argument,	NULL,	'w' },
	{ "apple-resvd-19",	required_argument,	NULL,	'X' },
	{ "apple-resvd-20",	required_argument,	NULL,	'x' },
	{ "apple-resvd-21",	no_argument,		NULL,	'z' },
	{ "apple-ext-bk-idle",	no_argument,		&extbkidle_flag, 1 },
	{ "apple-nowakefromsleep",	no_argument,	&nowakefromsleep_flag, 1 },
	{ "apple-ecn",		required_argument,	&ecn_mode_flag, 1 },
	{ "apple-kao",		required_argument,	&kao_flag, 1 },
	{ "apple-sockev",	no_argument,		&sockev, 1},
	{ "apple-notify-ack",	no_argument,		&notify_ack, 1},
	{ "apple-netsvctype",	required_argument,	&netsvctype_flag, 1},
	{ NULL,			0,			NULL,	0 }
};

int
main(int argc, char *argv[])
{
	int ch, s, ret, socksv;
	char *host, *uport, *endp;
	struct addrinfo hints;
	struct servent *sv;
	socklen_t len;
	struct sockaddr_storage cliaddr;
	char *proxy;
	const char *proxyhost = "", *proxyport = NULL;
	struct addrinfo proxyhints;

	ret = 1;
	s = 0;
	socksv = 5;
	host = NULL;
	uport = NULL;
	endp = NULL;
	sv = NULL;

	while ((ch = getopt_long(argc, argv,
	    "46AacDCb:dEhi:jFG:H:I:J:K:L:klMnN:Oop:rSs:T:tUuvw:X:x:z",
	    long_options, NULL)) != -1) {
		switch (ch) {
		case '4':
			family = AF_INET;
			break;
		case '6':
			family = AF_INET6;
			break;
		case 'A':
			Aflag = 1;
			break;
		case 'a':
			aflag = 1;
			break;
		case 'U':
			family = AF_UNIX;
			break;
		case 'm':
			mflag = 1;
			break;
		case 'M':
			Mflag = 1;
			break;
		case 'X':
			if (strcasecmp(optarg, "connect") == 0)
				socksv = -1; /* HTTP proxy CONNECT */
			else if (strcmp(optarg, "4") == 0)
				socksv = 4; /* SOCKS v.4 */
			else if (strcmp(optarg, "5") == 0)
				socksv = 5; /* SOCKS v.5 */
			else
				errx(1, "unsupported proxy protocol");
			break;
		case 'c':
			cflag = 1;
			break;
#ifdef __APPLE__
		case 'C':
			Cflag = 1;
			break;

		case 'E':
			Eflag = 1;
			break;
		case 'b':
			boundif = optarg;
			if ((ifscope = if_nametoindex(boundif)) == 0)
				errx(1, "bad interface name");
			break;
#endif /* __APPLE__ */
		case 'd':
			dflag = 1;
			break;
		case 'h':
			help();
			break;
		case 'i':
			iflag = (int)strtoul(optarg, &endp, 10);
			if (iflag < 0 || *endp != '\0')
				errx(1, "interval cannot be negative");
			break;
#ifndef __APPLE__
		case 'j':
			jflag = 1;
			break;
#endif /* !__APPLE__ */
		case 'k':
			kflag = 1;
			break;
#ifdef __APPLE__
		case 'K':
			Kflag = 1;
			tclass = str2sotc(optarg);
			if (tclass == -1)
				errx(1, "invalid traffic class");
			break;
		case 'F':
			Fflag = 1;
			use_flowadv = 0;
			break;
		case 'G':
			Gflag = 1;
			tcp_conn_timeout = (int)strtol(optarg, &endp, 10);
			if (tcp_conn_timeout < 0 || *endp != '\0')
				errx(1, "invalid tcp connection timeout");
			break;
		case 'H':
			Hflag = 1;
			tcp_conn_keepidle = (int)strtol(optarg, &endp, 10);
			if (tcp_conn_keepidle < 0 || *endp != '\0')
				errx(1, "invalid tcp keep idle interval");
			break;
		case 'I':
			Iflag = 1;
			tcp_conn_keepintvl = (int)strtol(optarg, &endp, 10);
			if (tcp_conn_keepintvl < 0 || *endp != '\0')
				errx(1, "invalid tcp keep interval");
			break;
		case 'J':
			Jflag = 1;
			tcp_conn_keepcnt = (int)strtol(optarg, &endp, 10);
			if (tcp_conn_keepcnt < 0 || *endp != '\0')
				errx(1, "invalid tcp keep count");
			break;
		case 'L':
			Lflag = 1;
			tcp_conn_adaptive_rtimo = (int)strtol(optarg, &endp, 10);
			if (tcp_conn_adaptive_rtimo < 0 || *endp != '\0')
				errx(1, "invalid tcp adaptive read timeout value");
			break;
		case 'N':
			Nflag = 1;
			tcp_conn_adaptive_wtimo = (int)strtol(optarg, &endp, 10);
			if (tcp_conn_adaptive_wtimo < 0 || *endp != '\0')
				errx(1, "invalid tcp adaptive write timeout value");
			break;
#endif /* __APPLE__ */
		case 'l':
			lflag = 1;
			break;
		case 'n':
			nflag = 1;
			break;
		case 'o':
			oflag = 1;
			break;
		case 'O':
			Oflag = 1;
			break;
		case 'p':
			pflag = optarg;
			break;
		case 'r':
			rflag = 1;
			break;
		case 's':
			sflag = optarg;
			break;
		case 't':
			tflag = 1;
			break;
		case 'T': {
			bool valid;

			Tflag = str2tos(optarg, &valid);
			if (valid == false)
				errx(EX_USAGE, "invalid TOS value: `%s'", optarg);
			break;
		}
		case 'u':
			uflag = 1;
			break;
		case 'v':
			vflag = 1;
			break;
		case 'w':
			timeout = (int)strtoul(optarg, &endp, 10);
			if (timeout < 0 || *endp != '\0')
				errx(1, "timeout cannot be negative");
			if (timeout >= (INT_MAX / 1000))
				errx(1, "timeout too large");
#ifndef USE_SELECT
			timeout *= 1000;
#endif
			break;
		case 'x':
			xflag = 1;
			if ((proxy = strdup(optarg)) == NULL)
				err(1, NULL);
			break;
		case 'z':
			zflag = 1;
			break;
		case 'D':
			Dflag = 1;
			break;
#ifndef __APPLE__
		case 'S':
			Sflag = 1;
			break;
#endif /* !__APPLE__ */
		case 0:
#ifdef __APPLE__
			if (pid_flag) {
				pid_optarg = optarg;
				pid_flag = 0;
			}

			if (uuid_flag) {
				uuid_optarg = optarg;
				uuid_flag = 0;
			}
			if (ecn_mode_flag) {
				ecn_mode_optarg = optarg;
				ecn_mode_flag = 0;
			}
			if (netsvctype_flag != 0) {
				netsvctype_optarg = optarg;
				netsvctype_flag = 0;
			}
			if (kao_flag != 0) {
				kao_optarg = optarg;
				kao_flag = 0;
			}
#endif /* __APPLE__ */
			break;
		default:
			usage(1);
		}
	}
	argc -= optind;
	argv += optind;

	if (pid_optarg != NULL && uuid_optarg != NULL)
				errx(1, "cannot use -apple-delegate-pid and --apple-delegate-uuid");

	if (pid_optarg != NULL) {
		pid = (pid_t)strtoul(pid_optarg, &endp, 0);
		if (pid == ULONG_MAX || *endp != '\0')
			errx(1, "invalid pid value");
	}

	if (uuid_optarg != NULL) {
		if (uuid_parse(pid_optarg, uuid))
			errx(1, "invalid uuid value");
	}
	if (ecn_mode_optarg != NULL) {
		if (strcmp(ecn_mode_optarg, "default") == 0)
			ecn_mode = ECN_MODE_DEFAULT;
		else if (strcmp(ecn_mode_optarg, "enable") == 0)
			ecn_mode = ECN_MODE_ENABLE;
		else if (strcmp(ecn_mode_optarg, "disable") == 0)
			ecn_mode = ECN_MODE_DISABLE;
		else {
			ecn_mode = (int)strtol(ecn_mode_optarg, &endp, 0);
			if (ecn_mode == (int)LONG_MAX || *endp != '\0')
				errx(1, "invalid ECN mode value");
		}
	}
	if (netsvctype_optarg != NULL) {
		netsvctype = str2netservicetype(netsvctype_optarg);
		if (netsvctype == -1)
			errx(1, "invalid network service type %s", netsvctype_optarg);
	}
	if (kao_optarg != NULL) {
		kao = strtol(kao_optarg, &endp, 0);
		if (kao < 0 || *endp != '\0')
			errx(1, "invalid kao value");
	}

	/* Cruft to make sure options are clean, and used properly. */
	if (argv[0] && !argv[1] && family == AF_UNIX) {
		if (uflag)
			errx(1, "cannot use -u and -U");
		if (Mflag)
			errx(1, "cannot use -M and -U");
		host = argv[0];
		uport = NULL;
	} else if (argv[0] && !argv[1]) {
		if  (!lflag) {
			warnx("missing hostname and port");
			usage(1);
		}
		uport = argv[0];
		host = NULL;
	} else if (argv[0] && argv[1]) {
		host = argv[0];
		uport = argv[1];
	} else {
		if (lflag)
			warnx("missing port with option -l");
		usage(1);
	}
	/* Detect if hostname has the telnet source-routing syntax (see sourceroute()) */
	srcroute_hosts = host;
	if (srcroute_hosts && (srcroute_hosts[0] == '@' || srcroute_hosts[0] == '!')) {
		if (
#ifdef INET6
			family == AF_INET6 ||
#endif
			(host = strrchr(srcroute_hosts, ':')) == NULL)
		{
			host = strrchr(srcroute_hosts, '@');
		}
		if (host == NULL) {
			host = srcroute_hosts;
		} else {
			host++;
			srcroute = 1;
		}
	}

	if (Mflag && Oflag)
		errx(1, "cannot use -M and -O");
	if (srcroute && Mflag)
		errx(1, "source routing isn't compatible with -M");
	if (srcroute && !Oflag)
		errx(1, "must use -O for source routing");
	if (lflag && sflag)
		errx(1, "cannot use -s and -l");
	if (lflag && pflag)
		errx(1, "cannot use -p and -l");
	if (lflag && zflag)
		errx(1, "cannot use -z and -l");
	if (!lflag && kflag)
		errx(1, "must use -l with -k");

	/* Initialize addrinfo structure. */
	if (family != AF_UNIX) {
		memset(&hints, 0, sizeof(struct addrinfo));
		hints.ai_family = family;
		hints.ai_socktype = uflag ? SOCK_DGRAM : SOCK_STREAM;
		hints.ai_protocol = uflag ? IPPROTO_UDP : IPPROTO_TCP;
		if (nflag)
			hints.ai_flags |= AI_NUMERICHOST;
	}

	if (xflag) {
		if (uflag)
			errx(1, "no proxy support for UDP mode");

		if (lflag)
			errx(1, "no proxy support for listen");

		if (family == AF_UNIX)
			errx(1, "no proxy support for unix sockets");

		/* XXX IPv6 transport to proxy would probably work */
		if (family == AF_INET6)
			errx(1, "no proxy support for IPv6");

		if (sflag)
			errx(1, "no proxy support for local source address");

		proxyhost = strsep(&proxy, ":");
		proxyport = proxy;

		memset(&proxyhints, 0, sizeof(struct addrinfo));
		proxyhints.ai_family = family;
		proxyhints.ai_socktype = SOCK_STREAM;
		proxyhints.ai_protocol = IPPROTO_TCP;
		if (nflag)
			proxyhints.ai_flags |= AI_NUMERICHOST;
	}

	if (lflag) {
		int connfd;
		ret = 0;

		if (family == AF_UNIX)
			s = unix_listen(host);

		/* Allow only one connection at a time, but stay alive. */
		for (;;) {
			if (family != AF_UNIX)
				s = local_listen(host, uport, hints);
			if (s < 0)
				err(1, NULL);
			/*
			 * For UDP, we will use recvfrom() initially
			 * to wait for a caller, then use the regular
			 * functions to talk to the caller.
			 */
			if (uflag) {
				int rv, plen;
				char buf[8192];
				struct sockaddr_storage z;

				len = sizeof(z);
#ifndef __APPLE__
				plen = jflag ? 8192 : 1024;
#else /* __APPLE__ */
				plen = 1024;
#endif /* !__APPLE__ */
				rv = recvfrom(s, buf, plen, MSG_PEEK,
				    (struct sockaddr *)&z, &len);
				if (rv < 0)
					err(1, "recvfrom");

				rv = connect(s, (struct sockaddr *)&z, len);
				if (rv < 0)
					err(1, "connect");

				connfd = s;
			} else {
				len = sizeof(cliaddr);
				connfd = accept(s, (struct sockaddr *)&cliaddr,
				    &len);
			}

			readwrite(connfd);
			close(connfd);
			if (family != AF_UNIX)
				close(s);

			if (!kflag)
				break;
		}
	} else if (family == AF_UNIX) {
		ret = 0;

		if ((s = unix_connect(host)) > 0 && !zflag) {
			readwrite(s);
			close(s);
		} else
			ret = 1;

		exit(ret);

	} else {
		int i = 0;

		/* Construct the portlist[] array. */
		build_ports(uport);

		/* Cycle through portlist, connecting to each port. */
		for (i = 0; portlist[i] != NULL; i++) {
			if (s)
				close(s);

			if (xflag)
				s = socks_connect(host, portlist[i], hints,
				    proxyhost, proxyport, proxyhints, socksv);
			else if (!Oflag)
				s = remote_connectx(host, portlist[i], hints);
			else
				s = remote_connect(host, portlist[i], hints);

			if (s < 0)
				continue;

			ret = 0;
			if (vflag || zflag) {
				/* For UDP, make sure we are connected. */
				if (uflag) {
					if (udptest(s) == -1) {
						ret = 1;
						continue;
					}
				}

				/* Don't look up port if -n. */
				if (nflag)
					sv = NULL;
				else {
					sv = getservbyport(
					    ntohs(atoi(portlist[i])),
					    uflag ? "udp" : "tcp");
				}

				fprintf(stderr, "Connection to %s port %s [%s/%s] succeeded!\n",
				    host, portlist[i], uflag ? "udp" : "tcp",
				    sv ? sv->s_name : "*");
			}
			if (!zflag)
				readwrite(s);
		}
	}

	if (s)
		close(s);

	exit(ret);
}

/*
 * unix_connect()
 * Returns a socket connected to a local unix socket. Returns -1 on failure.
 */
int
unix_connect(char *path)
{
	struct sockaddr_un sun;
	int s;

	if ((s = socket(AF_UNIX, SOCK_STREAM, 0)) < 0)
		return (-1);
	(void)fcntl(s, F_SETFD, 1);

	memset(&sun, 0, sizeof(struct sockaddr_un));
	sun.sun_family = AF_UNIX;

	if (strlcpy(sun.sun_path, path, sizeof(sun.sun_path)) >=
	    sizeof(sun.sun_path)) {
		close(s);
		errno = ENAMETOOLONG;
		return (-1);
	}
	if (connect(s, (struct sockaddr *)&sun, SUN_LEN(&sun)) < 0) {
		close(s);
		return (-1);
	}
	return (s);

}

/*
 * unix_listen()
 * Create a unix domain socket, and listen on it.
 */
int
unix_listen(char *path)
{
	struct sockaddr_un sun;
	int s;

	/* Create unix domain socket. */
	if ((s = socket(AF_UNIX, SOCK_STREAM, 0)) < 0)
		return (-1);

	memset(&sun, 0, sizeof(struct sockaddr_un));
	sun.sun_family = AF_UNIX;

	if (strlcpy(sun.sun_path, path, sizeof(sun.sun_path)) >=
	    sizeof(sun.sun_path)) {
		close(s);
		errno = ENAMETOOLONG;
		return (-1);
	}

	if (bind(s, (struct sockaddr *)&sun, SUN_LEN(&sun)) < 0) {
		close(s);
		return (-1);
	}

	if (listen(s, 5) < 0) {
		close(s);
		return (-1);
	}
	return (s);
}

/*
 * remote_connect()
 * Returns a socket connected to a remote host. Properly binds to a local
 * port or source address if needed. Returns -1 on failure.
 */
int
remote_connect(const char *host, const char *port, struct addrinfo hints)
{
	struct addrinfo *res, *res0;
	int s, error;

	if ((error = getaddrinfo(host, port, &hints, &res)))
		errx(1, "getaddrinfo: %s", gai_strerror(error));

	res0 = res;
	do {
		if ((s = socket(res0->ai_family, res0->ai_socktype,
		    res0->ai_protocol)) < 0)
			continue;

		/* Bind to a local port or source address if specified. */
		if (sflag || pflag) {
			struct addrinfo ahints, *ares;

			if (!(sflag && pflag)) {
				if (!sflag)
					sflag = NULL;
				else
					pflag = NULL;
			}

			memset(&ahints, 0, sizeof(struct addrinfo));
			ahints.ai_family = res0->ai_family;
			ahints.ai_socktype = uflag ? SOCK_DGRAM : SOCK_STREAM;
			ahints.ai_protocol = uflag ? IPPROTO_UDP : IPPROTO_TCP;
			ahints.ai_flags = AI_PASSIVE;
			if ((error = getaddrinfo(sflag, pflag, &ahints, &ares)))
				errx(1, "getaddrinfo: %s", gai_strerror(error));

			if (bind(s, (struct sockaddr *)ares->ai_addr,
			    ares->ai_addrlen) < 0)
				errx(1, "bind failed: %s", strerror(errno));
			freeaddrinfo(ares);
		}

		/* Set source-routing option */
		if (srcroute != 0) {
			int result, proto, opt, srlen = 0;
			char *srp = NULL;

			result = sourceroute(res, srcroute_hosts, &srp, &srlen, &proto, &opt);
			if (result == 1 && srp != NULL) {
				if (setsockopt(s, proto, opt, srp, srlen) < 0)
					perror("setsockopt (source route)");
			} else {
				warn("bad source route option: %s\n", srcroute_hosts);
			}
		}

		if (!oflag)
			set_common_sockopts(s, res0->ai_family);

		if (connect(s, res0->ai_addr, res0->ai_addrlen) == 0) {
			if (oflag)
				set_common_sockopts(s, res0->ai_family);
			break;
		} else if (vflag) {
			warn("connect to %s port %s (%s) failed", host, port,
			    uflag ? "udp" : "tcp");
		}
		printf("error = %d %d \n", error, errno);
		close(s);
		s = -1;
	} while ((res0 = res0->ai_next) != NULL);

	freeaddrinfo(res);

	return (s);
}

/*
 * remote_connectx()
 * Returns a socket connected to a remote host. Properly binds to a local
 * port or source address if needed, using connectx(2). Returns -1 on failure.
 */
int
remote_connectx(const char *host, const char *port, struct addrinfo hints)
{
	struct addrinfo *res, *res0, *ares = NULL;
	sae_connid_t cid;
	int s, error;

	if ((error = getaddrinfo(host, port, &hints, &res)))
		errx(1, "getaddrinfo: %s", gai_strerror(error));

	res0 = res;
	do {
		if ((s = socket(Mflag ? PF_MULTIPATH : res0->ai_family,
		     res0->ai_socktype, res0->ai_protocol)) < 0) {
			warn("socket(%d,%d,%d) failed",
			    (Mflag ? PF_MULTIPATH : res0->ai_family),
			    res0->ai_socktype, res0->ai_protocol);
			continue;
		}

		/* Bind to a local port or source address if specified. */
		if (sflag || pflag) {
			struct addrinfo ahints;

			if (!(sflag && pflag)) {
				if (!sflag)
					sflag = NULL;
				else
					pflag = NULL;
			}
			if (ares != NULL) {
				freeaddrinfo(ares);
				ares = NULL;
			}

			memset(&ahints, 0, sizeof(struct addrinfo));
			ahints.ai_family = res0->ai_family;
			ahints.ai_socktype = uflag ? SOCK_DGRAM : SOCK_STREAM;
			ahints.ai_protocol = uflag ? IPPROTO_UDP : IPPROTO_TCP;
			ahints.ai_flags = AI_PASSIVE;
			if ((error = getaddrinfo(sflag, pflag, &ahints, &ares)))
				errx(1, "getaddrinfo: %s", gai_strerror(error));
		}

		if (!oflag)
			set_common_sockopts(s, res0->ai_family);

		cid = SAE_CONNID_ANY;
		if (ares == NULL) {
			sa_endpoints_t sa;
			bzero(&sa, sizeof(sa));
			sa.sae_dstaddr = res0->ai_addr;
			sa.sae_dstaddrlen = res0->ai_addrlen;
			sa.sae_srcif = ifscope;
			error = connectx(s, &sa, SAE_ASSOCID_ANY, 0, NULL, 0, NULL, &cid);
		} else {
			sa_endpoints_t sa;
			bzero(&sa, sizeof(sa));
			sa.sae_srcaddr = ares->ai_addr;
			sa.sae_srcaddrlen = ares->ai_addrlen;
			sa.sae_dstaddr = res0->ai_addr;
			sa.sae_dstaddrlen = res0->ai_addrlen;
			sa.sae_srcif = ifscope;
			error = connectx(s, &sa, SAE_ASSOCID_ANY, 0, NULL, 0, NULL, &cid);
		}

		if (error == 0) {
			if (oflag)
				set_common_sockopts(s, res0->ai_family);
			if (vflag)
				showmpinfo(s);
			break;
		} else if (vflag) {
			warn("connectx to %s port %s (%s) failed", host, port,
			    (uflag ? "udp" : (Mflag ? "mptcp" : "tcp")));
		}

		close(s);
		s = -1;
	} while ((res0 = res0->ai_next) != NULL);

	freeaddrinfo(res);
	if (ares != NULL)
		freeaddrinfo(ares);

	return (s);
}

/*
 * local_listen()
 * Returns a socket listening on a local port, binds to specified source
 * address. Returns -1 on failure.
 */
int
local_listen(char *host, char *port, struct addrinfo hints)
{
	struct addrinfo *res, *res0;
	int s, ret, x = 1;
	int error;

	/* Allow nodename to be null. */
	hints.ai_flags |= AI_PASSIVE;

	/*
	 * In the case of binding to a wildcard address
	 * default to binding to an ipv4 address.
	 */
	if (host == NULL && hints.ai_family == AF_UNSPEC)
		hints.ai_family = AF_INET;

	if ((error = getaddrinfo(host, port, &hints, &res)))
		errx(1, "getaddrinfo: %s", gai_strerror(error));

	res0 = res;
	do {
		if ((s = socket(res0->ai_family, res0->ai_socktype,
		    res0->ai_protocol)) < 0)
			continue;

		ret = setsockopt(s, SOL_SOCKET, SO_REUSEPORT, &x, sizeof(x));
		if (ret == -1)
			err(1, NULL);

		if (!oflag)
			set_common_sockopts(s, res0->ai_family);

		if (bind(s, (struct sockaddr *)res0->ai_addr,
		    res0->ai_addrlen) == 0) {
			if (oflag)
				set_common_sockopts(s, res0->ai_family);
			break;
		}

		close(s);
		s = -1;
	} while ((res0 = res0->ai_next) != NULL);

	if (!uflag && s != -1) {
		if (listen(s, 1) < 0)
			err(1, "listen");
	}

	freeaddrinfo(res);

	return (s);
}

/*
 * readwrite()
 * Loop that polls on the network file descriptor and stdin.
 */
void
readwrite(int nfd)
{
#ifdef USE_SELECT
	fd_set readfds;
	struct timeval tv;
	int nfd_open = 1, wfd_open = 1;
#else
	struct pollfd pfd[2];
#endif
	unsigned char buf[8192];
	int n, wfd = fileno(stdin);
	int lfd = fileno(stdout);
	int plen, rc, marker_id = 0;

#ifndef __APPLE__
	plen = jflag ? 8192 : 1024;
#else /* __APPLE__ */
	plen = 1024;
#endif /* !__APPLE__ */

#ifndef USE_SELECT
	/* Setup Network FD */
	pfd[0].fd = nfd;
	pfd[0].events = POLLIN;

	/* Set up STDIN FD. */
	pfd[1].fd = wfd;
	pfd[1].events = POLLIN;
#endif
	if (sockev > 0) {
		bzero(&sockev_thread, sizeof(sockev_thread));
		/*
		 * create another thread to listen/print
		 * socket events
		 */
		rc = pthread_create(&sockev_thread, NULL,
		    &sockev_receive, &nfd);
		if (rc != 0)
			errx(1, "pthread_create failed: %d", rc);
	}


#ifdef USE_SELECT
	while (nfd_open) {
#else
	while (pfd[0].fd != -1) {
#endif
		if (iflag)
			sleep(iflag);

#ifdef USE_SELECT
		FD_ZERO(&readfds);
		if (nfd_open)
			FD_SET(nfd, &readfds);
		if (wfd_open)
			FD_SET(wfd, &readfds);

		tv.tv_sec = timeout;
		tv.tv_usec = 0;

		if ((n = select(nfd + 1, &readfds, NULL, NULL, (timeout == -1) ? NULL : &tv)) < 0) {
#else
		if ((n = poll(pfd, 2 - dflag, timeout)) < 0) {
#endif
			close(nfd);
			err(1, "Polling Error");
		}

		if (n == 0)
			return;

#ifdef USE_SELECT
		if (FD_ISSET(nfd, &readfds)) {
#else
		if (pfd[0].revents & POLLIN) {
#endif
			if ((n = read(nfd, buf, plen)) < 0)
				return;
			else if (n == 0) {
				shutdown(nfd, SHUT_RD);
#ifdef USE_SELECT
				nfd_open = 0;
#else
				pfd[0].fd = -1;
				pfd[0].events = 0;
#endif
			} else {
				if (tflag)
					atelnet(nfd, buf, n);
				if (atomicio(vwrite, lfd, buf, n) != n)
					return;
			}
		}

#ifdef USE_SELECT
		if (!dflag && FD_ISSET(wfd, &readfds)) {
#else
		if (!dflag && pfd[1].revents & POLLIN) {
#endif
			if ((n = read(wfd, buf, plen)) < 0)
				return;
			else if (n == 0) {
				shutdown(nfd, SHUT_WR);
#ifdef USE_SELECT
				wfd_open = 0;
#else
				pfd[1].fd = -1;
				pfd[1].events = 0;
#endif
			} else {
				if ((cflag) && (buf[n - 1] == '\n')) {
					if (atomicio(vwrite, nfd, buf, n - 1) != (n - 1))
						return;
					if (atomicio(vwrite, nfd, "\r\n", 2) != 2)
						return;
				} else {
					if (atomicio(vwrite, nfd, buf, n) != n)
						return;
				}
				if (notify_ack > 0) {
					++marker_id;
					/* set NOTIFY_ACK socket option */
					rc = setsockopt(nfd, IPPROTO_TCP,
					    TCP_NOTIFY_ACKNOWLEDGEMENT,
					    &marker_id, sizeof (marker_id));
					if (rc < 0) {
						perror("setsockopt TCP_NOTIFY_ACKNOWLEDGEMENT failed");
						notify_ack = 0;
					}
				}
			}
		}
	}

	if (sockev > 0) {
		rc = pthread_join(sockev_thread, NULL);
		if (rc > 0)
			errx(1, "pthread_join failed: %d", rc);
	}
}

/* Deal with RFC 854 WILL/WONT DO/DONT negotiation. */
void
atelnet(int nfd, unsigned char *buf, unsigned int size)
{
	unsigned char *p, *end;
	unsigned char obuf[4];

	end = buf + size;
	obuf[0] = '\0';

	for (p = buf; p < end; p++) {
		if (*p != IAC)
			break;

		obuf[0] = IAC;
		p++;
		if ((*p == WILL) || (*p == WONT))
			obuf[1] = DONT;
		if ((*p == DO) || (*p == DONT))
			obuf[1] = WONT;

		p++;
		obuf[2] = *p;
		obuf[3] = '\0';
		if (atomicio(vwrite, nfd, obuf, 3) != 3)
			warn("Write Error!");
		obuf[0] = '\0';
	}
}

/*
 * build_ports()
 * Build an array or ports in portlist[], listing each port
 * that we should try to connect to.
 */
void
build_ports(char *p)
{
	char *n, *endp;
	int hi, lo, cp;
	int x = 0;

	if ((n = strchr(p, '-')) != NULL) {
		if (lflag)
			errx(1, "Cannot use -l with multiple ports!");

		*n = '\0';
		n++;

		/* Make sure the ports are in order: lowest->highest. */
		hi = (int)strtoul(n, &endp, 10);
		if (hi <= 0 || hi > PORT_MAX || *endp != '\0')
			errx(1, "port range not valid");
		lo = (int)strtoul(p, &endp, 10);
		if (lo <= 0 || lo > PORT_MAX || *endp != '\0')
			errx(1, "port range not valid");

		if (lo > hi) {
			cp = hi;
			hi = lo;
			lo = cp;
		}

		/* Load ports sequentially. */
		for (cp = lo; cp <= hi; cp++) {
			portlist[x] = calloc(1, PORT_MAX_LEN);
			if (portlist[x] == NULL)
				err(1, NULL);
			snprintf(portlist[x], PORT_MAX_LEN, "%d", cp);
			x++;
		}

		/* Randomly swap ports. */
		if (rflag) {
			int y;
			char *c;

			for (x = 0; x <= (hi - lo); x++) {
				y = (arc4random() & 0xFFFF) % (hi - lo);
				c = portlist[x];
				portlist[x] = portlist[y];
				portlist[y] = c;
			}
		}
	} else {
		hi = (int)strtoul(p, &endp, 10);
		if (hi <= 0 || hi > PORT_MAX || *endp != '\0')
			errx(1, "port range not valid");
		portlist[0] = calloc(1, PORT_MAX_LEN);
		if (portlist[0] == NULL)
			err(1, NULL);
		strlcpy(portlist[0], p, PORT_MAX_LEN);
	}
}

/*
 * udptest()
 * Do a few writes to see if the UDP port is there.
 * XXX - Better way of doing this? Doesn't work for IPv6.
 * Also fails after around 100 ports checked.
 */
int
udptest(int s)
{
	int i, ret;

	for (i = 0; i <= 3; i++) {
		if (write(s, "X", 1) == 1)
			ret = 1;
		else
			ret = -1;
	}
	return (ret);
}

void
set_common_sockopts(int s, int af)
{
	int x = 1;

#ifndef __APPLE__
	if (Sflag) {
		if (setsockopt(s, IPPROTO_TCP, TCP_MD5SIG,
			&x, sizeof(x)) == -1)
			err(1, NULL);
	}
#endif /* !__APPLE__ */
	if (Dflag) {
		if (setsockopt(s, SOL_SOCKET, SO_DEBUG,
			&x, sizeof(x)) == -1)
			err(1, "SO_DEBUG");
	}
#ifndef __APPLE__
	if (jflag) {
		if (setsockopt(s, SOL_SOCKET, SO_JUMBO,
			&x, sizeof(x)) == -1)
			err(1, NULL);
	}
#endif /* !__APPLE__ */
#ifdef __APPLE__
	if (Aflag) {
		if (setsockopt(s, SOL_SOCKET, SO_RECV_ANYIF,
			&x, sizeof(x)) == -1)
			err(1, "SO_RECV_ANYIF");
	}
	if (aflag) {
		if (setsockopt(s, SOL_SOCKET, SO_AWDL_UNRESTRICTED,
			&x, sizeof(x)) == -1)
			err(1, "SO_AWDL_UNRESTRICTED");
	}

	if (mflag) {
		if (setsockopt(s, SOL_SOCKET, SO_INTCOPROC_ALLOW,
			&x, sizeof(x)) == -1)
			err(1, "SO_INTCOPROC_ALLOW");
	}

	if (boundif && (lflag || Oflag)) {
		/* Socket family could be AF_UNSPEC, so try both */
		if (setsockopt(s, IPPROTO_IP, IP_BOUND_IF, &ifscope,
		    sizeof (ifscope)) == -1 &&
		    setsockopt(s, IPPROTO_IPV6, IPV6_BOUND_IF, &ifscope,
		    sizeof (ifscope)) == -1)
			err(1, "{IP,IPV6}_BOUND_IF");
	}

	if (Cflag) {
		uint32_t restrictions = SO_RESTRICT_DENY_CELLULAR;
		if (setsockopt(s, SOL_SOCKET, SO_RESTRICTIONS, &restrictions,
		     sizeof (restrictions)) == -1)
			err(1, "SO_RESTRICTIONS: SO_RESTRICT_DENY_CELLULAR");
	}

	if (Eflag) {
		uint32_t restrictions = SO_RESTRICT_DENY_EXPENSIVE;
		if (setsockopt(s, SOL_SOCKET, SO_RESTRICTIONS, &restrictions,
		     sizeof (restrictions)) == -1)
			err(1, "SO_RESTRICTIONS: SO_RESTRICT_DENY_EXPENSIVE");
	}

	if (netsvctype_optarg != NULL) {
		if (setsockopt(s, SOL_SOCKET, SO_NET_SERVICE_TYPE,
			       &netsvctype, sizeof (netsvctype)) == -1)
			err(1, "SO_NET_SERVICE_TYPE netsvctype_optarg %s netsvctype %d",
			    netsvctype_optarg, netsvctype);
	}
	if (Kflag) {
		if (setsockopt(s, SOL_SOCKET, SO_TRAFFIC_CLASS,
			       &tclass, sizeof (tclass)) == -1)
			err(1, "SO_TRAFFIC_CLASS");
	}

	if (Gflag) {
		if (setsockopt(s, IPPROTO_TCP, TCP_CONNECTIONTIMEOUT, &tcp_conn_timeout,
			sizeof(tcp_conn_timeout)) == -1)
			err(1, "TCP_CONNECTIONTIMEOUT");
	}

	if (Hflag || Iflag || Jflag) {
		/* enable keep alives on this socket */
		int on = 1;
		if (setsockopt(s, SOL_SOCKET, SO_KEEPALIVE,
			&on, sizeof(on)) == -1)
			err(1, "SO_KEEPALIVE");
	}

	if (Hflag) {
		if (setsockopt(s, IPPROTO_TCP, TCP_KEEPALIVE,
			&tcp_conn_keepidle, sizeof(tcp_conn_keepidle)) == -1)
			err(1, "TCP_KEEPALIVE");
	}

	if (Iflag) {
		if (setsockopt(s, IPPROTO_TCP, TCP_KEEPINTVL,
			&tcp_conn_keepintvl, sizeof(tcp_conn_keepintvl)) == -1)
			err(1, "TCP_KEEPINTVL");
	}

	if (Jflag) {
		if (setsockopt(s, IPPROTO_TCP, TCP_KEEPCNT,
			&tcp_conn_keepcnt, sizeof(tcp_conn_keepcnt)) == -1)
			err(1, "TCP_KEEPCNT");
	}

	if (kao_optarg != NULL) {
		if (setsockopt(s, IPPROTO_TCP, TCP_KEEPALIVE_OFFLOAD,
			       &kao, sizeof(kao)) == -1)
			err(1, "TCP_KEEPALIVE_OFFLOAD");
	}

	if (Lflag) {
		if (setsockopt(s, IPPROTO_TCP, TCP_ADAPTIVE_READ_TIMEOUT,
			&tcp_conn_adaptive_rtimo,
			sizeof(tcp_conn_adaptive_rtimo)) == -1)
			err(1, "TCP_ADAPTIVE_READ_TIMEOUT");
	}

	if (Nflag) {
		if (setsockopt(s, IPPROTO_TCP,TCP_ADAPTIVE_WRITE_TIMEOUT,
			&tcp_conn_adaptive_wtimo,
			sizeof(tcp_conn_adaptive_wtimo)) == -1)
			err(1, "TCP_ADAPTIVE_WRITE_TIMEOUT");
	}

	if (pid_optarg != NULL) {
		if (setsockopt(s, SOL_SOCKET, SO_DELEGATED,
			       &pid, sizeof(pid)) == -1)
			err(1, "SO_DELEGATED");
	}

	if (uuid_optarg != NULL) {
		if (setsockopt(s, SOL_SOCKET, SO_DELEGATED_UUID,
			       uuid, sizeof(uuid)) == -1)
			err(1, "SO_DELEGATED_UUID");
	}

	if (extbkidle_flag) {
		if (setsockopt(s, SOL_SOCKET, SO_EXTENDED_BK_IDLE,
			       &extbkidle_flag, sizeof(int)) == -1)
			err(1, "SO_EXTENDED_BK_IDLE");
	}
	if (nowakefromsleep_flag) {
		if (setsockopt(s, SOL_SOCKET, SO_NOWAKEFROMSLEEP,
			       &nowakefromsleep_flag, sizeof(int)) == -1)
			err(1, "SO_NOWAKEFROMSLEEP");
	}
	if (ecn_mode_optarg != NULL) {
		if (setsockopt(s, IPPROTO_TCP, TCP_ECN_MODE,
			       &ecn_mode, sizeof(int)) == -1)
			err(1, "TCP_ECN_MODE");
	}
	if (Tflag != -1) {
		int proto, option;

		if (af == AF_INET6) {
			proto = IPPROTO_IPV6;
			option = IPV6_TCLASS;
		} else {
			proto = IPPROTO_IP;
			option = IP_TOS;
		}

		if (setsockopt(s, proto, option, &Tflag, sizeof(Tflag)) == -1)
			err(1, "set IP ToS");
	}
#endif /* __APPLE__ */
}

void
help(void)
{
	usage(0);
	fprintf(stderr, "\tCommand Summary:\n\
	\t-4		Use IPv4\n\
	\t-6		Use IPv6\n\
%s\
%s\
%s\
	\t-c		Send CRLF as line-ending\n\
%s\
	\t-D		Enable the debug socket option\n\
	\t-d		Detach from stdin\n\
%s\
%s\
%s\
	\t-h		This help text\n\
%s\
%s\
	\t-i secs\t	Delay interval for lines sent, ports scanned\n\
%s\
	\t-k		Keep inbound sockets open for multiple connects\n\
%s\
	\t-l		Listen mode, for inbound connects\n\
%s\
%s\
	\t-n		Suppress name/port resolutions\n\
%s\
%s\
%s\
	\t-p port\t	Specify local port for remote connects (cannot use with -l)\n\
	\t-r		Randomize remote ports\n\
	\t-s addr\t	Local source address\n\
	\t-t		Answer TELNET negotiation\n\
	\t-U		Use UNIX domain socket\n\
	\t-u		UDP mode\n\
	\t-v		Verbose\n\
	\t-w secs\t	Timeout for connects and final net reads\n\
	\t-X proto	Proxy protocol: \"4\", \"5\" (SOCKS) or \"connect\"\n\
	\t-x addr[:port]\tSpecify proxy address and port\n\
	\t-z		Zero-I/O mode [used for scanning]\n\
%s\
%s\
	Port numbers can be individual or ranges: lo-hi [inclusive]\n",
#ifndef __APPLE__
	"",
	"",
	"",
	"	\t-S		Enable the TCP MD5 signature option\n",
	"",
	"",
	"",
	"",
	""
#else /* __APPLE__ */
	"	\t-A		Set SO_RECV_ANYIF on socket\n",
	"	\t-a		Set SO_AWDL_UNRESTRICTED on socket\n",
	"	\t-b ifbound	Bind socket to interface\n",
	"	\t-C		Don't use cellular connection\n",
	"	\t-E		Don't use expensive interfaces\n",
	"	\t-F		Do not use flow advisory (flow adv enabled by default)\n",
	"	\t-G conntimo	Connection timeout in seconds\n",
	"	\t-H keepidle	Initial idle timeout in seconds\n",
	"	\t-I keepintvl	Interval for repeating idle timeouts in seconds\n",
	"	\t-J keepcnt	Number of times to repeat idle timeout\n",
	"	\t-K tclass	Specify traffic class\n",
	"	\t-L num_probes Number of probes to send before generating a read timeout event\n",
	"	\t-m		Set SO_INTCOPROC_ALLOW on socket\n",
	"	\t-M		Use MULTIPATH domain socket\n",
	"	\t-N num_probes Number of probes to send before generating a write timeout event\n",
	"	\t-O		Use old-style connect instead of connectx\n",
	"	\t-o		Issue socket options after connect/bind\n",
	"	\t--apple-delegate-pid pid\tSet socket as delegate using pid\n",
	"	\t--apple-delegate-uuid uuid\tSet socket as delegate using uuid\n"
	"	\t--apple-ext-bk-idle\tExtended background idle time\n"
	"	\t--apple-ecn\tSet the ECN mode\n"
	"	\t--apple-sockev\tReceive and print socket events\n"
	"	\t--apple-notify-ack\tReceive events when data gets acknowledged\n"
	"	\t--apple-tos\tSet the IP_TOS or IPV6_TCLASS option\n"
	"	\t--apple-netsvctype\tSet the network service type\n"
#endif /* !__APPLE__ */
	);
	exit(1);
}

void
usage(int ret)
{
#ifndef __APPLE__
	fprintf(stderr, "usage: nc [-46cDdhklnrStUuvz] [-i interval] [-p source_port]\n");
#else /* __APPLE__ */
	fprintf(stderr, "usage: nc [-46AacCDdEFhklMnOortUuvz] [-K tc] [-b boundif] [-i interval] [-p source_port] [--apple-delegate-pid pid] [--apple-delegate-uuid uuid]\n");
#endif /* !__APPLE__ */
	fprintf(stderr, "\t  [-s source_ip_address] [-w timeout] [-X proxy_version]\n");
	fprintf(stderr, "\t  [-x proxy_address[:port]] [hostname] [port[s]]\n");
	if (ret)
		exit(1);
}

int
wait_for_flowadv(int fd)
{
	static int kq = -1;
	int rc;
	struct kevent kev[2];
	bzero(kev, sizeof(kev));

	/* Got ENOBUFS.
	 * Now wait for flow advisory only if UDP is set and flow adv is enabled. */
	if (!uflag || !use_flowadv) {
		return (1);
	}

	if (kq < 0) {
		kq = kqueue();
		if (kq < 0) {
			errx(1, "failed to create kqueue for flow advisory");
			return (1);
		}
	}

	EV_SET(&kev[0], fd, EVFILT_WRITE, EV_ADD | EV_ENABLE, 0, 0, 0);
	rc = kevent(kq, &kev[0], 1, NULL, 0, NULL);
	if (rc < 0) {
		return (1);
	}

	rc = kevent(kq, NULL, 0, &kev[1], 1, NULL);
	if (rc < 0) {
		return (1);
	}

	if (kev[1].flags & EV_EOF) {
		return (2);
	}
	return (0);
}

void *
sockev_receive(void *arg)
{
	int kq = -1;
	struct kevent kev[2];
	int result, sock;

	if (arg == NULL) {
		printf("Bad argument to sockev_receive thread\n");
		return (NULL);
	}

	sock = *((int *) arg);
	kq = kqueue();
	if (kq < 0) {
		errx(1, "failed to create kqueue");
		return (NULL);
	}

loop:
	bzero(kev, sizeof(kev));
	EV_SET(&kev[0], sock, EVFILT_SOCK, (EV_ADD | EV_ENABLE | EV_CLEAR),
	    EVFILT_SOCK_ALL_MASK, NULL, NULL);

	/* Wait for events */
	result = kevent(kq, &kev[0], 1, &kev[1], 1, NULL);
	if (result < 0) {
		perror("kevent system call failed");
		return (NULL);
	}

	if (kev[1].fflags & NOTE_CONNRESET)
		printf("Received CONNRESET event, data 0x%lx\n",
		    kev[1].data);
	if (kev[1].fflags & NOTE_READCLOSED)
		printf("Received READCLOSED event, data 0x%lx\n",
		    kev[1].data);
	if (kev[1].fflags & NOTE_WRITECLOSED)
		printf("Received WRITECLOSED event, data 0x%lx\n",
		    kev[1].data);
	if (kev[1].fflags & NOTE_TIMEOUT)
		printf("Received TIMEOUT event, data 0x%lx\n", kev[1].data);
	if (kev[1].fflags & NOTE_NOSRCADDR)
		printf("Received NOSRCADDR event, data 0x%lx\n", kev[1].data);
	if (kev[1].fflags & NOTE_IFDENIED)
		printf("Received IFDENIED event, data 0x%lx\n", kev[1].data);
	if (kev[1].fflags & NOTE_SUSPEND)
		printf("Received SUSPEND event, data 0x%lx\n", kev[1].data);
	if (kev[1].fflags & NOTE_RESUME)
		printf("Received RESUME event, data 0x%lx\n", kev[1].data);
	if (kev[1].fflags & NOTE_KEEPALIVE)
		printf("Received KEEPALIVE event, data 0x%lx\n", kev[1].data);
	if (kev[1].fflags & NOTE_ADAPTIVE_WTIMO)
		printf("Received ADAPTIVE_WTIMO event, data 0x%lx\n",
		    kev[1].data);
	if (kev[1].fflags & NOTE_ADAPTIVE_RTIMO)
		printf("Received ADAPTIVE_RTIMO event, data 0x%lx\n",
		    kev[1].data);
	if (kev[1].fflags & NOTE_CONNECTED)
		printf("Received CONNECTED event, data 0x%lx\n",
		    kev[1].data);
	if (kev[1].fflags & NOTE_DISCONNECTED)
		printf("Received DISCONNECTED event, data 0x%lx\n",
		    kev[1].data);
	if (kev[1].fflags & NOTE_CONNINFO_UPDATED)
		printf("Received CONNINFO_UPDATED event, data 0x%lx\n",
		    kev[1].data);
	if (kev[1].fflags & NOTE_NOTIFY_ACK) {
		struct tcp_notify_ack_complete ack_ids;
		socklen_t optlen;
		printf("Received NOTIFY_ACK event, data 0x%lx\n",
		    kev[1].data);
		bzero(&ack_ids, sizeof (ack_ids));
		optlen = sizeof (ack_ids);
		result = getsockopt(sock, IPPROTO_TCP,
		    TCP_NOTIFY_ACKNOWLEDGEMENT,
		    &ack_ids, &optlen);
		if (result != 0) {
			perror("getsockopt TCP_NOTIFY_ACKNOWLEDGEMENT failed: ");
		} else {
			printf("pending: %u complete: %u id: %u \n",
			    ack_ids.notify_pending,
			    ack_ids.notify_complete_count,
			    ack_ids.notify_complete_id[0]);
		}
	}
	if (kev[1].flags & EV_EOF) {
		printf("reached EOF\n");
		return (NULL);
	} else {
		goto loop;
	}
}

/*
 * Print a value a la the %b format of the kernel's printf
 */
void
fprintb(FILE *stream, const char *s, unsigned v, const char *bits)
{
	int i, any = 0;
	char c;

	if (*bits == 8)
		fprintf(stderr, "%s=%o", s, v);
	else
		fprintf(stderr, "%s=%x", s, v);
	bits++;

	putc('<', stream);
	while ((i = *bits++) != '\0') {
		if (v & (1 << (i-1))) {
			if (any)
				putc(',', stream);
			any = 1;
			for (; (c = *bits) > 32; bits++)
				putc(c, stream);
		} else
			for (; *bits > 32; bits++)
				;
	}
	putc('>', stream);
}

#define	CIF_BITS	\
	"\020\1CONNECTING\2CONNECTED\3DISCONNECTING\4DISCONNECTED\5BOUND_IF" \
	"\6BOUND_IP\7BOUND_PORT\10PREFERRED\11MP_CAPABLE\12MP_READY" \
	"\13MP_DEGRADED"

int
showconninfo(int s, sae_connid_t cid)
{
	struct so_cordreq scor;
	char buf[INET6_ADDRSTRLEN];
	conninfo_t *cfo = NULL;
	int err;

	err = copyconninfo(s, cid, &cfo);
	if (err != 0) {
		warn("copyconninfo failed for cid %d\n", cid);
		goto out;
	}

	fprintf(stderr, "%6d:\t", cid);
	fprintb(stderr, "flags", cfo->ci_flags, CIF_BITS);
	fprintf(stderr, "\n");
	fprintf(stderr, "\toutif %s\n", if_indextoname(cfo->ci_ifindex, buf));
	if (cfo->ci_src != NULL) {
		fprintf(stderr, "\tsrc %s port %d\n", inet_ntop(cfo->ci_src->sa_family,
		    (cfo->ci_src->sa_family == AF_INET) ?
		    (void *)&((struct sockaddr_in *)cfo->ci_src)->
		    sin_addr.s_addr :
		    (void *)&((struct sockaddr_in6 *)cfo->ci_src)->sin6_addr,
		    buf, sizeof (buf)),
		    (cfo->ci_src->sa_family == AF_INET) ?
		    ntohs(((struct sockaddr_in *)cfo->ci_src)->sin_port) :
		    ntohs(((struct sockaddr_in6 *)cfo->ci_src)->sin6_port));
	}
	if (cfo->ci_dst != NULL) {
		fprintf(stderr, "\tdst %s port %d\n", inet_ntop(cfo->ci_dst->sa_family,
		    (cfo->ci_dst->sa_family == AF_INET) ?
		    (void *)&((struct sockaddr_in *)cfo->ci_dst)->
		    sin_addr.s_addr :
		    (void *)&((struct sockaddr_in6 *)cfo->ci_dst)->sin6_addr,
		    buf, sizeof (buf)),
		    (cfo->ci_dst->sa_family == AF_INET) ?
		    ntohs(((struct sockaddr_in *)cfo->ci_dst)->sin_port) :
		    ntohs(((struct sockaddr_in6 *)cfo->ci_dst)->sin6_port));
	}

	bzero(&scor, sizeof (scor));
	scor.sco_cid = cid;
	err = ioctl(s, SIOCGCONNORDER, &scor);
	if (err == 0) {
		fprintf(stderr, "\trank %d\n", scor.sco_rank);
	} else {
		fprintf(stderr, "\trank info not available\n");
	}

	if (cfo->ci_aux_data != NULL) {
		switch (cfo->ci_aux_type) {
		case CIAUX_TCP:
			fprintf(stderr, "\tTCP aux info available\n");
			break;
		default:
			fprintf(stderr, "\tUnknown aux type %d\n", cfo->ci_aux_type);
			break;
		}
	}
out:
	if (cfo != NULL)
		freeconninfo(cfo);

	return (err);
}

void
showmpinfo(int s)
{
	uint32_t aid_cnt, cid_cnt;
	sae_associd_t *aid = NULL;
	sae_connid_t *cid = NULL;
	int i, err;

	err = copyassocids(s, &aid, &aid_cnt);
	if (err != 0) {
		warn("copyassocids failed\n");
		goto done;
	} else {
		fprintf(stderr, "found %d associations", aid_cnt);
		if (aid_cnt > 0) {
			fprintf(stderr, " with IDs:");
			for (i = 0; i < aid_cnt; i++)
				fprintf(stderr, " %d\n", aid[i]);
		}
		fprintf(stderr, "\n");
	}

	/* just do an association for now */
	err = copyconnids(s, SAE_ASSOCID_ANY, &cid, &cid_cnt);
	if (err != 0) {
		warn("copyconnids failed\n");
		goto done;
	} else {
		fprintf(stderr, "found %d connections", cid_cnt);
		if (cid_cnt > 0) {
			fprintf(stderr, ":\n");
			for (i = 0; i < cid_cnt; i++) {
				if (showconninfo(s, cid[i]) != 0)
					break;
			}
		}
		fprintf(stderr, "\n");
	}

done:
	if (aid != NULL)
		freeassocids(aid);
	if (cid != NULL)
		freeconnids(cid);
}

int
str2sotc(const char *str)
{
	int svc;
	char *endptr;

	if (str == NULL || *str == '\0')
		svc = -1;
	else if (strcasecmp(str, "BK_SYS") == 0)
		svc = SO_TC_BK_SYS;
	else if (strcasecmp(str, "BK") == 0)
		svc = SO_TC_BK;
	else if (strcasecmp(str, "BE") == 0)
		svc = SO_TC_BE;
	else if (strcasecmp(str, "RD") == 0)
		svc = SO_TC_RD;
	else if (strcasecmp(str, "OAM") == 0)
		svc = SO_TC_OAM;
	else if (strcasecmp(str, "AV") == 0)
		svc = SO_TC_AV;
	else if (strcasecmp(str, "RV") == 0)
		svc = SO_TC_RV;
	else if (strcasecmp(str, "VI") == 0)
		svc = SO_TC_VI;
	else if (strcasecmp(str, "VO") == 0)
		svc = SO_TC_VO;
	else if (strcasecmp(str, "CTL") == 0)
		svc = SO_TC_CTL;
	else {
		svc = (int)strtol(str, &endptr, 0);
		if (*endptr != '\0')
			svc = -1;
	}
	return (svc);
}

int
str2netservicetype(const char *str)
{
	int svc = -1;
	char *endptr;

	if (str == NULL || *str == '\0')
		svc = -1;
	else if (strcasecmp(str, "BK") == 0)
		svc = NET_SERVICE_TYPE_BK;
	else if (strcasecmp(str, "BE") == 0)
		svc = NET_SERVICE_TYPE_BE;
	else if (strcasecmp(str, "VI") == 0)
		svc = NET_SERVICE_TYPE_VI;
	else if (strcasecmp(str, "SIG") == 0)
		return NET_SERVICE_TYPE_SIG;
	else if (strcasecmp(str, "VO") == 0)
		svc = NET_SERVICE_TYPE_VO;
	else if (strcasecmp(str, "RV") == 0)
		svc = NET_SERVICE_TYPE_RV;
	else if (strcasecmp(str, "AV") == 0)
		svc = NET_SERVICE_TYPE_AV;
	else if (strcasecmp(str, "OAM") == 0)
		svc = NET_SERVICE_TYPE_OAM;
	else if (strcasecmp(str, "RD") == 0)
		svc = NET_SERVICE_TYPE_RD;
	else {
		svc = (int)strtol(str, &endptr, 0);
		if (*endptr != '\0')
			svc = -1;
	}
	return (svc);
}

u_int8_t
str2tos(const char *str, bool *valid)
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
	/* DSCP occupies the 6 upper bits of the TOS field */
	return (dscp << 2);
}
