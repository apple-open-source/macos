/*
%%% copyright-cmetz-97
This software is Copyright 1997-1998 by Craig Metz, All Rights Reserved.
The Inner Net License Version 2 applies to this software.
You should have received a copy of the license with this software. If
you didn't get a copy, you may request one from <license@inner.net>.

*/


#include "config.h"
#ifdef INET6_ENABLE
#include <sys/types.h>
#include <stdio.h>
#ifdef HAVE_NET_SOCKET_H
#include <net/socket.h>
#else
#include <sys/socket.h>
#endif
#include <netinet/in.h>
#include <errno.h>
#include <netdb.h>
#include <signal.h>

/* This patch, supplying SA_LEN if it's undefined, is from Red Hat */
#ifndef SA_LEN
#define SA_LEN(sa)	sa_len(sa)

static size_t sa_len(struct sockaddr *sa)
{
	switch(sa->sa_family) {
#if defined(AF_INET)
		case AF_INET:
			return sizeof(struct sockaddr_in);
#endif
#if defined(AF_INET6) && defined(INET6_ENABLE)
		case AF_INET6:
			return sizeof(struct sockaddr_in6);
#endif
		default:
			return sizeof(struct sockaddr);
	}
}
#endif /* SA_LEN */

static int default_trying_callback(struct sockaddr *sa)
{
  char hbuf[NI_MAXHOST], sbuf[NI_MAXSERV];

  if (getnameinfo(sa, SA_LEN(sa), hbuf, sizeof(hbuf), sbuf, sizeof(sbuf), NI_NUMERICHOST | NI_NUMERICSERV)) {
    fprintf(stderr, "inner_getstream: getnameinfo failed\n");
    return -1;
  };

  fprintf(stderr, "Trying %s.%s...\n", hbuf, sbuf);
  return 0;
};

static int default_error_callback(char *myname, char *message)
{
  fprintf(stderr, "%s: %s\n", myname, message);
  return 0;
};

int inner_connect(struct addrinfo *ai, void *request, int requestlen, int (*trying_callback)(struct sockaddr *sa), int (*error_callback)(char *myname, char *message), char *myname, struct addrinfo **pai)
{
  int fd;
  char errorbuf[128];

  if (!trying_callback)
    trying_callback = default_trying_callback;

  if (!error_callback)
    error_callback = default_error_callback;

  for (; ai; ai = ai->ai_next) {
    if (trying_callback(ai->ai_addr))
      continue;

    if ((fd = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol)) < 0) {
#ifdef HAVE_SNPRINTF
     snprintf(errorbuf, sizeof(errorbuf),
#else
     sprintf(errorbuf,
#endif
      	"socket: %s(%d)", strerror(errno), errno);
      error_callback(myname, errorbuf);
      continue;
    };

    if (connect(fd, ai->ai_addr, ai->ai_addrlen) < 0) {
#ifdef HAVE_SNPRINTF
     snprintf(errorbuf, sizeof(errorbuf),
#else
     sprintf(errorbuf,
#endif
         "connect: %s(%d)", strerror(errno), errno);
      error_callback(myname, errorbuf);
      close(fd);	/* just after a connect; no reads or writes yet */
      continue;
    }
    break;
  };

  if (ai) {
    if (pai)
      *pai = ai;
  } else {
#ifdef HAVE_SNPRINTF
     snprintf(errorbuf, sizeof(errorbuf),
#else
     sprintf(errorbuf,
#endif
       "no connections result");
    error_callback(myname, errorbuf);
    fd = -1;
  };

  return fd;
};

#endif /* INET6_ENABLE */
