/* 
 * Copyright (c) 1995 NeXT Computer, Inc. All Rights Reserved
 *
 * Copyright (c) 1980, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * The NEXTSTEP Software License Agreement specifies the terms
 * and conditions for redistribution.
 *
 *	@(#)whois.c	8.1 (Berkeley) 6/6/93
 */


#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <stdio.h>

#define	NICHOST	"whois.internic.net"

main(argc, argv)
	int argc;
	char **argv;
{
	extern char *optarg;
	extern int optind;
	register FILE *sfi, *sfo;
	register int ch;
	struct sockaddr_in sin;
	struct hostent *hp;
	struct servent *sp;
	int s;
	char *host;

	host = NICHOST;
	while ((ch = getopt(argc, argv, "h:")) != EOF)
		switch((char)ch) {
		case 'h':
			host = optarg;
			break;
		case '?':
		default:
			usage();
		}
	argc -= optind;
	argv += optind;

	if (!argc)
		usage();

	hp = gethostbyname(host);
	if (hp == NULL) {
		(void)fprintf(stderr, "whois: %s: ", host);
		herror((char *)NULL);
		exit(1);
	}
	host = hp->h_name;
	s = socket(hp->h_addrtype, SOCK_STREAM, 0);
	if (s < 0) {
		perror("whois: socket");
		exit(1);
	}
	bzero((caddr_t)&sin, sizeof (sin));
	sin.sin_family = hp->h_addrtype;
	if (bind(s, (struct sockaddr *)&sin, sizeof(sin)) < 0) {
		perror("whois: bind");
		exit(1);
	}
	bcopy(hp->h_addr, (char *)&sin.sin_addr, hp->h_length);
	sp = getservbyname("whois", "tcp");
	if (sp == NULL) {
		(void)fprintf(stderr, "whois: whois/tcp: unknown service\n");
		exit(1);
	}
	sin.sin_port = sp->s_port;
	if (connect(s, (struct sockaddr *)&sin, sizeof(sin)) < 0) {
		perror("whois: connect");
		exit(1);
	}
	sfi = fdopen(s, "r");
	sfo = fdopen(s, "w");
	if (sfi == NULL || sfo == NULL) {
		perror("whois: fdopen");
		(void)close(s);
		exit(1);
	}
	while (argc-- > 1)
		(void)fprintf(sfo, "%s ", *argv++);
	(void)fprintf(sfo, "%s\r\n", *argv);
	(void)fflush(sfo);
	while ((ch = getc(sfi)) != EOF)
		putchar(ch);
	exit(0);
}

usage()
{
	(void)fprintf(stderr, "usage: whois [-h hostname] name ...\n");
	exit(1);
}
