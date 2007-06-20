/** \file servport.c Resolve service name to port number.
 * \author Matthias Andree
 * \date 2005 - 2006
 *
 * Copyright (C) 2005 by Matthias Andree
 * For license terms, see the file COPYING in this directory.
 */
#include "fetchmail.h"
#include "getaddrinfo.h"
#include "i18n.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <netdb.h>
#if defined(HAVE_NETINET_IN_H)
#include <netinet/in.h>
#endif
#ifdef HAVE_ARPA_INET_H
#include <arpa/inet.h>
#endif
#include <sys/socket.h>

int servport(const char *service) {
    int port, e;
    unsigned long u;
    char *end;

    if (service == 0)
	return -1;

    /*
     * Check if the service is a number. If so, convert it.
     * If it isn't a number, call getservbyname to resolve it.
     */
    errno = 0;
    u = strtoul(service, &end, 10);
    if (errno || end[strspn(end, POSIX_space)] != '\0') {
	struct addrinfo hints, *res;

	/* hardcode kpop to port 1109 as per fetchmail(1)
	 * manual page, it's not a IANA registered service */
	if (strcmp(service, "kpop") == 0)
	    return 1109;

	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;
	e = fm_getaddrinfo(NULL, service, &hints, &res);
	if (e) {
	    report(stderr, GT_("getaddrinfo(NULL, \"%s\") error: %s\n"),
		    service, gai_strerror(e));
	    goto err;
	} else {
	    switch(res->ai_addr->sa_family) {
		case AF_INET:
		    port = ntohs(((struct sockaddr_in *)res->ai_addr)->sin_port);
		break;
#ifdef AF_INET6
		case AF_INET6:
		    port = ntohs(((struct sockaddr_in6 *)res->ai_addr)->sin6_port);
		break;
#endif
		default:
		    fm_freeaddrinfo(res);
		    goto err;
	    }
	    fm_freeaddrinfo(res);
	}
    } else {
	if (u == 0 || u > 65535)
	    goto err;
	port = u;
    }

    return port;
err:
    report(stderr, GT_("Cannot resolve service %s to port number.\n"), service);
    report(stderr, GT_("Please specify the service as decimal port number.\n"));
    return -1;
}
/* end of servport.c */
