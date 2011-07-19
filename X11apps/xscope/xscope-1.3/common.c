/* ************************************************************ *\
 *								*
 *    Common support routines for sockets			*
 *								*
 *       James L. Peterson	 				*
 * Copyright (C) 1987 MCC
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the name of MCC not be used in
 * advertising or publicity pertaining to distribution of the software without
 * specific, written prior permission.  MCC makes no
 * representations about the suitability of this software for any purpose.  It
 * is provided "as is" without express or implied warranty.
 *
 * MCC DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO
 * EVENT SHALL MCC BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
 * DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 * 				  				*
 * 				  				*
 * Copyright (c) 2002, Oracle and/or its affiliates. All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 * 				  				*
 \* *********************************************************** */

#include "scope.h"
#include <fcntl.h>
#include <unistd.h>

/* ********************************************** */
/*						  */
/*       Debugging support routines               */
/*						  */
/* ********************************************** */

void
enterprocedure (char *s)
{
  debug(2,(stderr, "-> %s\n", s));
}

void
warn (char *s)
{
  fprintf(stderr, "####### %s\n", s);
}

void
panic (char *s)
{
  fprintf(stderr, "%s\n", s);
  exit(1);
}

/* ********************************************** */
/*						  */
/*  Debugging forms of memory management          */
/*						  */
/* ********************************************** */

void *
Malloc (long    n)
{
  void   *p;
  p = malloc(n);
  debug(64,(stderr, "%lx = malloc(%ld)\n", (unsigned long) p, n));
  if (p == NULL)
    panic("no more malloc space");
  return(p);
}

void 
Free(void   *p)
{
  debug(64,(stderr, "%lx = free\n", (unsigned long) p));
  free(p);
}



/* ************************************************************ */
/*								*/
/*    Signal Handling support					*/
/*								*/
/* ************************************************************ */

#define __USE_BSD_SIGNAL
#include <signal.h>

static void SignalURG(int sig)
{
  debug(1,(stderr, "==> SIGURG received\n"));
}

static void SignalPIPE(int sig)
{
  signal (SIGPIPE, SignalPIPE);
  debug(1,(stderr, "==> SIGPIPE received\n"));
}

static void SignalINT(int sig)
{
  signal (SIGINT, SignalINT);
  debug(1,(stderr, "==> SIGINT received\n"));
  Interrupt = 1;
}

static void SignalQUIT(int sig)
{
  debug(1,(stderr, "==> SIGQUIT received\n"));
  exit(1);
}

static void SignalTERM(int sig)
{
  debug(1,(stderr, "==> SIGTERM received\n"));
  exit(1);
}

static void SignalTSTP(int sig)
{
  debug(1,(stderr, "==> SIGTSTP received\n"));
}

static void SignalCONT(int sig)
{
  debug(1,(stderr, "==> SIGCONT received\n"));
}

static void SignalUSR1(int sig)
{
  debug(1,(stderr, "==> SIGUSR1 received\n"));
  ScopeEnabled = ! ScopeEnabled;
}

void
SetSignalHandling(void)
{
  enterprocedure("SetSignalHandling");
  (void)signal(SIGURG, SignalURG);
  (void)signal(SIGPIPE, SignalPIPE);
  (void)signal(SIGINT, SignalINT);
  (void)signal(SIGQUIT, SignalQUIT);
  (void)signal(SIGTERM, SignalTERM);
  (void)signal(SIGTSTP, SignalTSTP);
  (void)signal(SIGCONT, SignalCONT);
  if (HandleSIGUSR1)
    (void)signal(SIGUSR1, SignalUSR1);
}



/* ************************************************************ */
/*								*/
/*   Create a socket for a service to listen for clients        */
/*								*/
/* ************************************************************ */

#ifdef USE_XTRANS

#define TRANS_CLIENT
#define TRANS_SERVER
#define X11_t
#include <X11/Xtrans/Xtrans.h>
static XtransConnInfo  *ListenTransConns = NULL;
static int             *ListenTransFds = NULL;
static int              ListenTransCount;

#else

#include <sys/types.h>	       /* needed by sys/socket.h and netinet/in.h */
#include <sys/uio.h>	       /* for struct iovec, used by socket.h */
#include <sys/socket.h>	       /* for AF_INET, SOCK_STREAM, ... */
#include <sys/ioctl.h>	       /* for FIONCLEX, FIONBIO, ... */
#include <sys/fcntl.h>	       /* for FIONCLEX, FIONBIO, ... */
#if !defined(FIOCLEX) && defined(HAVE_SYS_FILIO_H)
#include <sys/filio.h>
#endif

#include <netinet/in.h>	       /* struct sockaddr_in */
#include <netdb.h>	       /* struct servent * and struct hostent *  */

static int  ON = 1 /* used in ioctl */ ;
#define	BACKLOG	5
#endif

void
SetUpConnectionSocket(
    int     iport,
    void     (*connectionFunc)(int))
{
#ifdef USE_XTRANS
  char	port[20];
  int	partial;     
  int   i;
#else
  FD ConnectionSocket;
  struct sockaddr_in  sin;
  short port;
  int one = 1;
#ifndef	SO_DONTLINGER
  struct linger linger;
#endif /* SO_DONTLINGER */
#endif

  enterprocedure("SetUpConnectionSocket");

#ifdef USE_XTRANS
  ScopePort = iport - ServerBasePort;
  sprintf (port, "%d", ScopePort);
  if ((_X11TransMakeAllCOTSServerListeners (port, &partial,
        &ListenTransCount, &ListenTransConns) >= 0) &&
        (ListenTransCount >= 1)) {
      if (partial) {
	  debug(4,(stderr, 
	    "Warning: Failed to establish listening connections on some transports\n"));
      } 
      ListenTransFds = (int *) malloc (ListenTransCount * sizeof (int));

      for (i = 0; i < ListenTransCount; i++)
      {
	  int fd = _X11TransGetConnectionNumber (ListenTransConns[i]);
                
	  ListenTransFds[i] = fd;
	  debug(4,(stderr, "Listening on FD %d\n", fd));
	  UsingFD(fd, NewConnection, NULL, ListenTransConns[i]);
      }
  } else {
      panic("Could not open any listening connections");
  }
#else

  /* create the connection socket and set its parameters of use */
  ConnectionSocket = socket(AF_INET, SOCK_STREAM, 0);
  if (ConnectionSocket < 0)
    {
      perror("socket");
      exit(-1);
    }
  (void)setsockopt(ConnectionSocket, SOL_SOCKET, SO_REUSEADDR,   (char *)&one, sizeof (int));
#ifdef SO_USELOOPBACK
  (void)setsockopt(ConnectionSocket, SOL_SOCKET, SO_USELOOPBACK, (char *)NULL, 0);
#endif
#ifdef	SO_DONTLINGER
  (void)setsockopt(ConnectionSocket, SOL_SOCKET, SO_DONTLINGER,  (char *)NULL, 0);
#else /* SO_DONTLINGER */
  linger.l_onoff = 0;
  linger.l_linger = 0;
  (void)setsockopt(ConnectionSocket, SOL_SOCKET, SO_LINGER, (char *)&linger, sizeof linger);
#endif /* SO_DONTLINGER */

  /* define the name and port to be used with the connection socket */
  bzero((char *)&sin, sizeof(sin));
  sin.sin_family = AF_INET;

  /* the address of the socket is composed of two parts: the host machine and
     the port number.  We need the host machine address for the current host
   */
  {
    /* define the host part of the address */
    char    MyHostName[256];
    struct hostent *hp;

    (void) gethostname(MyHostName, sizeof(MyHostName));
    ScopeHost = (char *) Malloc((long)(1+strlen(MyHostName)));
    strcpy(ScopeHost, MyHostName);
    hp = gethostbyname(MyHostName);
    if (hp == NULL)
      panic("No address for our host");
    bcopy((char *)hp->h_addr, (char*)&sin.sin_addr, hp->h_length);
  }
    /* new code -- INADDR_ANY should be better than using the name of the
       host machine.  The host machine may have several different network
       addresses.  INADDR_ANY should work with all of them at once. */
  sin.sin_addr.s_addr = INADDR_ANY;

  port = iport;
  sin.sin_port = htons (port);
  ScopePort = port;

  /* bind the name and port number to the connection socket */
  if (bind(ConnectionSocket, (struct sockaddr *)&sin, sizeof(sin)) < 0)
    {
      perror("bind");
      exit(-1);
    }

  debug(4,(stderr, "Socket is FD %d for %s,%d\n",
	   ConnectionSocket, ScopeHost, ScopePort));

  /* now activate the named connection socket to get messages */
  if (listen(ConnectionSocket, BACKLOG) < 0)
    {
      perror("listen");
      exit(-1);
    };

  /* a few more parameter settings */
#ifdef FD_CLOEXEC
  (void)fcntl(ConnectionSocket, F_SETFD, FD_CLOEXEC);
#else
  (void)ioctl(ConnectionSocket, FIOCLEX, 0);
#endif
  /* ultrix reads hang on Unix sockets, hpux reads fail */
#if defined(O_NONBLOCK) && (!defined(ultrix) && !defined(hpux))
  (void) fcntl (ConnectionSocket, F_SETFL, O_NONBLOCK);
#else
#ifdef FIOSNBIO
  (void) ioctl (ConnectionSocket, FIOSNBIO, &ON);
#else
  (void) fcntl (ConnectionSocket, F_SETFL, FNDELAY);
#endif
#endif

  debug(4,(stderr, "Listening on FD %d\n", ConnectionSocket));
  UsingFD(ConnectionSocket, connectionFunc, NULL, NULL);
#endif
}

