/*
%%% copyright-cmetz-97

  The author(s) grant permission for redistribution and use in source and
binary forms, with or without modification, of the software and documentation
provided that the following conditions are met:

1. All terms of the all other applicable copyrights and licenses must be
   followed.
2. Redistributions of source code must retain the authors' copyright
   notice(s), this list of conditions, and the following disclaimer.
3. Redistributions in binary form must reproduce the authors' copyright
   notice(s), this list of conditions, and the following disclaimer in the
   documentation and/or other materials provided with the distribution.
4. Neither the name(s) of the author(s) nor the names of its contributors
   may be used to endorse or promote products derived from this software
   without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY ITS AUTHORS AND CONTRIBUTORS ``AS IS'' AND ANY
EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE AUTHORS OR CONTRIBUTORS BE LIABLE FOR ANY
DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

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
