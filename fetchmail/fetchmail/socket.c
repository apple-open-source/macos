/*
 * socket.c -- socket library functions
 *
 * Copyright 1998 by Eric S. Raymond.
 * For license terms, see the file COPYING in this directory.
 */

#include "config.h"
#include <stdio.h>
#include <errno.h>
#include <string.h>
#ifdef HAVE_MEMORY_H
#include <memory.h>
#endif /* HAVE_MEMORY_H */
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#if defined(STDC_HEADERS)
#include <stdlib.h>
#endif
#if defined(HAVE_UNISTD_H)
#include <unistd.h>
#endif
#if defined(HAVE_STDARG_H)
#include <stdarg.h>
#else
#include <varargs.h>
#endif
#include "socket.h"
#include "fetchmail.h"
#include "i18n.h"

#ifdef HAVE_RES_SEARCH
/* some versions of FreeBSD should declare this but don't */
extern int h_errno;
#else
/* pretend we have h_errno to avoid some #ifdef's later */
static int h_errno;
#endif

#if NET_SECURITY
#include <net/security.h>
#endif /* NET_SECURITY */

#ifdef HAVE_SOCKETPAIR
static int handle_plugin(const char *host,
			 const char *service, const char *plugin)
/* get a socket mediated through a given external command */
{
    int fds[2];
    if (socketpair(AF_UNIX,SOCK_STREAM,0,fds))
    {
	report(stderr, _("fetchmail: socketpair failed\n"));
	return -1;
    }
    switch (fork()) {
	case -1:
		/* error */
		report(stderr, _("fetchmail: fork failed\n"));
		return -1;
		break;
	case 0:	/* child */
		/* fds[1] is the parent's end; close it for proper EOF
		** detection */
		(void) close(fds[1]);
		if ( (dup2(fds[0],0) == -1) || (dup2(fds[0],1) == -1) ) {
			report(stderr, _("dup2 failed\n"));
			exit(1);
		}
		/* fds[0] is now connected to 0 and 1; close it */
		(void) close(fds[0]);
		if (outlevel >= O_VERBOSE)
		    report(stderr, _("running %s %s %s\n"), plugin, host, service);
		execlp(plugin,plugin,host,service,0);
		report(stderr, _("execl(%s) failed\n"), plugin);
		exit(0);
		break;
	default:	/* parent */
		/* NOP */
		break;
    }
    /* fds[0] is the child's end; close it for proper EOF detection */
    (void) close(fds[0]);
    return fds[1];
}
#endif /* HAVE_SOCKETPAIR */

#if INET6
int SockOpen(const char *host, const char *service, const char *options,
	     const char *plugin)
{
    struct addrinfo *ai, req;
#if NET_SECURITY
    void *request = NULL;
    int requestlen;
#endif /* NET_SECURITY */

#ifdef HAVE_SOCKETPAIR
    if (plugin)
	return handle_plugin(host,service,plugin);
#endif /* HAVE_SOCKETPAIR */
    memset(&req, 0, sizeof(struct addrinfo));
    req.ai_socktype = SOCK_STREAM;

    if (getaddrinfo(host, service, &req, &ai)) {
	report(stderr, _("fetchmail: getaddrinfo(%s.%s)\n"), host,service);
	return -1;
    };

#if NET_SECURITY
    if (!options)
	requestlen = 0;
    else
	if (net_security_strtorequest((char *)options, &request, &requestlen))
	    goto ret;

    i = inner_connect(ai, request, requestlen, NULL, NULL, "fetchmail", NULL);
    if (request)
	free(request);

 ret:
#else /* NET_SECURITY */
    i = inner_connect(ai, NULL, 0, NULL, NULL, "fetchmail", NULL);
#endif /* NET_SECURITY */

    freeaddrinfo(ai);

    return i;
};
#else /* INET6 */
#ifndef HAVE_INET_ATON
#ifndef  INADDR_NONE
#ifdef   INADDR_BROADCAST
#define  INADDR_NONE	INADDR_BROADCAST
#else
#define	 INADDR_NONE	-1
#endif
#endif
#endif /* HAVE_INET_ATON */

int SockOpen(const char *host, int clientPort, const char *options,
	     const char *plugin)
{
    int sock;
#ifndef HAVE_INET_ATON
    unsigned long inaddr;
#endif /* HAVE_INET_ATON */
    struct sockaddr_in ad;
    struct hostent *hp;

#ifdef HAVE_SOCKETPAIR
    if (plugin) {
      char buf[10];
      sprintf(buf,"%d",clientPort);
      return handle_plugin(host,buf,plugin);
    }
#endif /* HAVE_SOCKETPAIR */

    memset(&ad, 0, sizeof(ad));
    ad.sin_family = AF_INET;

    /* we'll accept a quad address */
#ifndef HAVE_INET_ATON
    inaddr = inet_addr(host);
    if (inaddr != INADDR_NONE)
        memcpy(&ad.sin_addr, &inaddr, sizeof(inaddr));
    else
#else
    if (!inet_aton(host, &ad.sin_addr))
#endif /* HAVE_INET_ATON */
    {
        hp = gethostbyname(host);

        if (hp == NULL)
	{
	    errno = 0;
	    return -1;
	}
	/*
	 * Add a check to make sure the address has a valid IPv4 or IPv6
	 * length.  This prevents buffer spamming by a broken DNS.
	 */
	if(hp->h_length != 4 && hp->h_length != 8)
	{
	    h_errno = errno = 0;
	    report(stderr, 
		   _("fetchmail: illegal address length received for host %s\n"),host);
	    return -1;
	}
	/*
	 * FIXME: make this work for multihomed hosts.
	 * We're toast if we get back multiple addresses and h_addrs[0]
	 * (aka h_addr) is not one we can actually connect to; this happens
	 * with multi-homed boxen.
	 */
        memcpy(&ad.sin_addr, hp->h_addr, hp->h_length);
    }
    ad.sin_port = htons(clientPort);
    
    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0)
    {
	h_errno = 0;
        return -1;
    }
    if (connect(sock, (struct sockaddr *) &ad, sizeof(ad)) < 0)
    {
	int olderr = errno;
	close(sock);
	h_errno = 0;
	errno = olderr;
        return -1;
    }

    return(sock);
}
#endif /* INET6 */


#if defined(HAVE_STDARG_H)
int SockPrintf(int sock, const char* format, ...)
{
#else
int SockPrintf(sock,format,va_alist)
int sock;
char *format;
va_dcl {
#endif

    va_list ap;
    char buf[8192];

#if defined(HAVE_STDARG_H)
    va_start(ap, format) ;
#else
    va_start(ap);
#endif
#ifdef HAVE_VSNPRINTF
    vsnprintf(buf, sizeof(buf), format, ap);
#else
    vsprintf(buf, format, ap);
#endif
    va_end(ap);
    return SockWrite(sock, buf, strlen(buf));

}

int SockWrite(int sock, char *buf, int len)
{
    int n, wrlen = 0;

    while (len)
    {
        n = write(sock, buf, len);
        if (n <= 0)
            return -1;
        len -= n;
	wrlen += n;
	buf += n;
    }
    return wrlen;
}

int SockRead(int sock, char *buf, int len)
{
    char *newline, *bp = buf;
    int n;

    if (--len < 1)
	return(-1);
    do {
	/* 
	 * The reason for these gymnastics is that we want two things:
	 * (1) to read \n-terminated lines,
	 * (2) to return the true length of data read, even if the
	 *     data coming in has embedded NULS.
	 */
	if ((n = recv(sock, bp, len, MSG_PEEK)) <= 0)
	    return(-1);
	if ((newline = memchr(bp, '\n', n)) != NULL)
	    n = newline - bp + 1;
	if ((n = read(sock, bp, n)) == -1)
	    return(-1);
	bp += n;
	len -= n;
    } while 
	    (!newline && len);
    *bp = '\0';
    return bp - buf;
}

int SockPeek(int sock)
/* peek at the next socket character without actually reading it */
{
    int n;
    char ch;

    if ((n = recv(sock, &ch, 1, MSG_PEEK)) == -1)
	return -1;
    else
	return(ch);
}

int SockClose(int sock)
/* close a socket (someday we may do other cleanup here) */
{
    return(close(sock));
}

#ifdef MAIN
/*
 * Use the chargen service to test input buffering directly.
 * You may have to uncomment the `chargen' service description in your
 * inetd.conf (and then SIGHUP inetd) for this to work. 
 */
main()
{
    int	 	sock = SockOpen("localhost", 19, NULL);
    char	buf[80];

    while (SockRead(sock, buf, sizeof(buf)-1))
	SockWrite(1, buf, strlen(buf));
    SockClose(sock);
}
#endif /* MAIN */

/* socket.c ends here */
