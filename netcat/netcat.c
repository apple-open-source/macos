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
#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include "atomicio.h"

#include <network/conninfo.h>

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
char	*boundif;				/* interface to bind to */
int	ifscope;				/* idx of bound to interface */
int	Cflag;					/* cellular connection OFF option */
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
int	tcp_conn_adaptive_rtimo;			/* Value of TCP adaptive timeout */
int	tcp_conn_adaptive_wtimo;			/* Value of TCP adaptive timeout */
#endif /* __APPLE__ */

int srcroute = 0;				/* Source routing IPv4/IPv6 options */
char *srcroute_hosts = NULL;

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
void    set_common_sockopts(int);
void	usage(int);
int	showconninfo(int, connid_t);
void	showmpinfo(int);

extern int sourceroute(struct addrinfo *, char *, char **, int *, int *, int *);

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

	while ((ch = getopt(argc, argv,
	    "46AcDCb:dhi:jFG:H:I:J:K:L:klMnN:Oop:rSs:tUuvw:X:x:z")) != -1) {
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
		case 'U':
			family = AF_UNIX;
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
			tclass = (int)strtoul(optarg, &endp, 10);
			if (tclass < 0 || *endp != '\0')
				errx(1, "invalid traffic class");
			break;
		case 'F':
			Fflag = 1;
			use_flowadv = 0;
			break;
		case 'G':
			Gflag = 1;
			tcp_conn_timeout = strtoumax(optarg, &endp, 10);
			if (tcp_conn_timeout < 0 || *endp != '\0')
				errx(1, "invalid tcp connection timeout");
			break;
		case 'H':
			Hflag = 1;
			tcp_conn_keepidle = strtoumax(optarg, &endp, 10);
			if (tcp_conn_keepidle < 0 || *endp != '\0')
				errx(1, "invalid tcp keep idle interval");
			break;
		case 'I':
			Iflag = 1;
			tcp_conn_keepintvl = strtoumax(optarg, &endp, 10);
			if (tcp_conn_keepintvl < 0 || *endp != '\0')
				errx(1, "invalid tcp keep interval");
			break;
		case 'J':
			Jflag = 1;
			tcp_conn_keepcnt = strtoumax(optarg, &endp, 10);
			if (tcp_conn_keepcnt < 0 || *endp != '\0')
				errx(1, "invalid tcp keep count");
			break;
		case 'L':
			Lflag = 1;
			tcp_conn_adaptive_rtimo = strtoumax(optarg, &endp, 10);
			if (tcp_conn_adaptive_rtimo < 0 || *endp != '\0')
				errx(1, "invalid tcp adaptive read timeout value");
			break;
		case 'N':
			Nflag = 1;
			tcp_conn_adaptive_wtimo = strtoumax(optarg, &endp, 10);
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
		default:
			usage(1);
		}
	}
	argc -= optind;
	argv += optind;

	/* Cruft to make sure options are clean, and used properly. */
	if (argv[0] && !argv[1] && family == AF_UNIX) {
		if (uflag)
			errx(1, "cannot use -u and -U");
		if (Mflag)
			errx(1, "cannot use -M and -U");
		host = argv[0];
		uport = NULL;
	} else if (argv[0] && !argv[1]) {
		if  (!lflag)
			usage(1);
		uport = argv[0];
		host = NULL;
	} else if (argv[0] && argv[1]) {
		host = argv[0];
		uport = argv[1];
	} else
		usage(1);

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

				printf("Connection to %s port %s [%s/%s] succeeded!\n",
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
			set_common_sockopts(s);

		if (connect(s, res0->ai_addr, res0->ai_addrlen) == 0) {
			if (oflag)
				set_common_sockopts(s);
			break;
		} else if (vflag) {
			warn("connect to %s port %s (%s) failed", host, port,
			    uflag ? "udp" : "tcp");
		}
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
	connid_t cid;
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
			set_common_sockopts(s);

		cid = CONNID_ANY;
		if (ares == NULL) {
			error = connectx(s, NULL, 0, res0->ai_addr,
			     res0->ai_addrlen, ifscope, ASSOCID_ANY, &cid);
		} else {
			error = connectx(s, ares->ai_addr, ares->ai_addrlen,
			    res0->ai_addr, res0->ai_addrlen, ifscope,
			    ASSOCID_ANY, &cid);
		}

		if (error == 0) {
			if (oflag)
				set_common_sockopts(s);
			if (vflag)
				showmpinfo(s);
			break;
		} else if (errno == EPROTO) {	/* PF_MULTIPATH specific */
			int ps;
			warn("connectx to %s port %s (%s) succeded without "
			    "multipath association (connid %d)",
			    host, port, uflag ? "udp" : "tcp", cid);
			if (vflag)
				showmpinfo(s);
			ps = peeloff(s, ASSOCID_ANY);
			if (ps != -1) {
				close(s);
				s = ps;
				if (oflag)
					set_common_sockopts(s);
				break;
			}
			warn("peeloff failed for connid %d", cid);
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
			set_common_sockopts(s);

		if (bind(s, (struct sockaddr *)res0->ai_addr,
		    res0->ai_addrlen) == 0) {
			if (oflag)
				set_common_sockopts(s);
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
	int plen;

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
			}
		}
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
		if (obuf) {
			p++;
			obuf[2] = *p;
			obuf[3] = '\0';
			if (atomicio(vwrite, nfd, obuf, 3) != 3)
				warn("Write Error!");
			obuf[0] = '\0';
		}
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
set_common_sockopts(int s)
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
	\t-D		Enable the debug socket option\n\
	\t-d		Detach from stdin\n\
	\t-h		This help text\n\
	\t-i secs\t	Delay interval for lines sent, ports scanned\n\
	\t-k		Keep inbound sockets open for multiple connects\n\
	\t-l		Listen mode, for inbound connects\n\
	\t-n		Suppress name/port resolutions\n\
	\t-p port\t	Specify local port for remote connects\n\
	\t-r		Randomize remote ports\n\
%s\
%s\
%s\
%s\
%s\
%s\
%s\
	\t-s addr\t	Local source address\n\
	\t-t		Answer TELNET negotiation\n\
	\t-U		Use UNIX domain socket\n\
	\t-u		UDP mode\n\
	\t-v		Verbose\n\
	\t-w secs\t	Timeout for connects and final net reads\n\
	\t-X proto	Proxy protocol: \"4\", \"5\" (SOCKS) or \"connect\"\n\
	\t-x addr[:port]\tSpecify proxy address and port\n\
	\t-z		Zero-I/O mode [used for scanning]\n\
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
	"	\t-C		Don't use cellular connection\n",
	"	\t-b ifbound	Bind socket to interface\n",
	"	\t-O		Use old-style connect instead of connectx\n",
	"	\t-o		Issue socket options after connect/bind\n",
	"	\t-K tclass	Specify traffic class\n",
	"	\t-F		Do not use flow advisory (flow adv enabled by default)\n",
	"	\t-G conntimo	Connection timeout in seconds\n",
	"	\t-H keepidle	Initial idle timeout in seconds\n",
	"	\t-I keepintvl	Interval for repeating idle timeouts in seconds\n",
	"	\t-J keepcnt	Number of times to repeat idle timeout\n",
	"	\t-L num_probes Number of probes to send before generating a read timeout event\n",
	"	\t-M		Use MULTIPATH domain socket\n",
	"	\t-N num_probes Number of probes to send before generating a write timeout event\n"
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
	fprintf(stderr, "usage: nc [-46AcCDdFhklMnOortUuvz] [-K tc] [-b boundif] [-i interval] [-p source_port]\n");
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

#define	CIF_BITS	\
	"\020\1CONNECTING\2CONNECTED\3DISCONNECTING\4DISCONNECTED\5BOUND_IF" \
	"\6BOUND_IP\7BOUND_PORT\10PREFERRED\11MP_CAPABLE\12MP_READY" \
	"\13MP_DEGRADED"

int
showconninfo(int s, connid_t cid)
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

	printf("%6d:\t", cid);
	printb("flags", cfo->ci_flags, CIF_BITS);
	printf("\n");
	printf("\toutif %s\n", if_indextoname(cfo->ci_ifindex, buf));
	if (cfo->ci_src != NULL) {
		printf("\tsrc %s port %d\n", inet_ntop(cfo->ci_src->sa_family,
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
		printf("\tdst %s port %d\n", inet_ntop(cfo->ci_dst->sa_family,
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
		printf("\trank %d\n", scor.sco_rank);
	} else {
		printf("\trank info not available\n");
	}

	if (cfo->ci_aux_data != NULL) {
		switch (cfo->ci_aux_type) {
		case CIAUX_TCP:
			printf("\tTCP aux info available\n");
			break;
		default:
			printf("\tUnknown aux type %d\n", cfo->ci_aux_type);
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
	associd_t *aid = NULL;
	connid_t *cid = NULL;
	int i, err;

	err = copyassocids(s, &aid, &aid_cnt);
	if (err != 0) {
		warn("copyassocids failed\n");
		goto done;
	} else {
		printf("found %d associations", aid_cnt);
		if (aid_cnt > 0) {
			printf(" with IDs:");
			for (i = 0; i < aid_cnt; i++)
				printf(" %d\n", aid[i]);
		}
		printf("\n");
	}

	/* just do an association for now */
	err = copyconnids(s, ASSOCID_ANY, &cid, &cid_cnt);
	if (err != 0) {
		warn("copyconnids failed\n");
		goto done;
	} else {
		printf("found %d connections", cid_cnt);
		if (cid_cnt > 0) {
			printf(":\n");
			for (i = 0; i < cid_cnt; i++) {
				if (showconninfo(s, cid[i]) != 0)
					break;
			}
		}
		printf("\n");
	}

done:
	if (aid != NULL)
		freeassocids(aid);
	if (cid != NULL)
		freeconnids(cid);
}
