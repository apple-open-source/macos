/* tcp.c
   Code to handle TCP connections.

   Copyright (C) 1991, 1992, 1993, 1995, 2002 Ian Lance Taylor

   This file is part of the Taylor UUCP package.

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   This program is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307, USA.

   The author of the program may be contacted at ian@airs.com.
   */

#include "uucp.h"

#if USE_RCS_ID
const char tcp_rcsid[] = "$Id: tcp.c,v 1.12 2002/03/05 19:10:42 ian Rel $";
#endif

#if HAVE_TCP

#include "uudefs.h"
#include "uuconf.h"
#include "sysdep.h"
#include "conn.h"
#include "system.h"

#include <errno.h>

#if HAVE_SYS_TYPES_TCP_H
#include <sys/types.tcp.h>
#endif
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#if HAVE_FCNTL_H
#include <fcntl.h>
#else
#if HAVE_SYS_FILE_H
#include <sys/file.h>
#endif
#endif

#ifndef FD_CLOEXEC
#define FD_CLOEXEC 1
#endif

#if HAVE_STRUCT_SOCKADDR_STORAGE
typedef struct sockaddr_storage sockaddr_storage;
#else
typedef struct sockaddr_in sockaddr_storage;
#endif

/* This code handles TCP connections.  It assumes a Berkeley socket
   interface.  */

/* The normal "uucp" port number.  */
#define IUUCP_PORT (540)
#define ZUUCP_PORT ("540")

/* Local functions.  */
static void utcp_free P((struct sconnection *qconn));
#if HAVE_GETADDRINFO
static boolean ftcp_set_hints P((int iversion, struct addrinfo *qhints));
#endif
static boolean ftcp_set_flags P((struct ssysdep_conn *qsysdep));
static boolean ftcp_open P((struct sconnection *qconn, long ibaud,
			    boolean fwait, boolean fuser));
static boolean ftcp_close P((struct sconnection *qconn,
			     pointer puuconf,
			     struct uuconf_dialer *qdialer,
			     boolean fsuccess));
static boolean ftcp_dial P((struct sconnection *qconn, pointer puuconf,
			    const struct uuconf_system *qsys,
			    const char *zphone,
			    struct uuconf_dialer *qdialer,
			    enum tdialerfound *ptdialer));
static int itcp_port_number P((const char *zport));

/* The command table for a TCP connection.  */
static const struct sconncmds stcpcmds =
{
  utcp_free,
  NULL, /* pflock */
  NULL, /* pfunlock */
  ftcp_open,
  ftcp_close,
  ftcp_dial,
  fsysdep_conn_read,
  fsysdep_conn_write,
  fsysdep_conn_io,
  NULL, /* pfbreak */
  NULL, /* pfset */
  NULL, /* pfcarrier */
  fsysdep_conn_chat,
  NULL /* pibaud */
};

/* Initialize a TCP connection.  */

boolean
fsysdep_tcp_init (qconn)
     struct sconnection *qconn;
{
  struct ssysdep_conn *q;

  q = (struct ssysdep_conn *) xmalloc (sizeof (struct ssysdep_conn));
  q->o = -1;
  q->ord = -1;
  q->owr = -1;
  q->zdevice = NULL;
  q->iflags = -1;
  q->iwr_flags = -1;
  q->fterminal = FALSE;
  q->ftli = FALSE;
  q->ibaud = 0;

  qconn->psysdep = (pointer) q;
  qconn->qcmds = &stcpcmds;
  return TRUE;
}

/* Free a TCP connection.  */

static void
utcp_free (qconn)
     struct sconnection *qconn;
{
  xfree (qconn->psysdep);
}

#if HAVE_GETADDRINFO

/* Set the hints for an addrinfo structure from the IP version.  */

static boolean
ftcp_set_hints (iversion, qhints)
     int iversion;
     struct addrinfo *qhints;
{
  switch (iversion)
    {
    case 0:
      qhints->ai_family = 0;
      break;
    case 4:
      qhints->ai_family = PF_INET;
      break;
    case 6:
#ifdef PF_INET6
      qhints->ai_family = PF_INET6;
#else
      ulog (LOG_ERROR, "IPv6 requested but not supported");
      return FALSE;
#endif
      break;
    default:
      ulog (LOG_ERROR, "Invalid IP version number %d", iversion);
      return FALSE;
    }
  return TRUE;
}

#endif /* HAVE_GETADDRINFO */

/* Set the close on exec flag for a socket.  */

static boolean
ftcp_set_flags (qsysdep)
     struct ssysdep_conn *qsysdep;
{
  if (fcntl (qsysdep->o, F_SETFD,
	     fcntl (qsysdep->o, F_GETFD, 0) | FD_CLOEXEC) < 0)
    {
      ulog (LOG_ERROR, "fcntl (FD_CLOEXEC): %s", strerror (errno));
      (void) close (qsysdep->o);
      qsysdep->o = -1;
      return FALSE;
    }

  qsysdep->iflags = fcntl (qsysdep->o, F_GETFL, 0);
  if (qsysdep->iflags < 0)
    {
      ulog (LOG_ERROR, "fcntl: %s", strerror (errno));
      (void) close (qsysdep->o);
      qsysdep->o = -1;
      return FALSE;
    }

  return TRUE;
}

/* Open a TCP connection.  If the fwait argument is TRUE, we are
   running as a server.  Otherwise we are just trying to reach another
   system.  */

static boolean
ftcp_open (qconn, ibaud, fwait, fuser)
     struct sconnection *qconn;
     long ibaud ATTRIBUTE_UNUSED;
     boolean fwait;
     boolean fuser ATTRIBUTE_UNUSED;
{
  struct ssysdep_conn *qsysdep;
  const char *zport;
  uid_t ieuid;
  gid_t iegid;
  boolean fswap;
#if HAVE_GETADDRINFO
  struct addrinfo shints;
  struct addrinfo *qres;
  struct addrinfo *quse;
  int ierr;
#endif

  ulog_device ("TCP");

  qsysdep = (struct ssysdep_conn *) qconn->psysdep;

  qsysdep->o = -1;

  /* We save our process ID in the qconn structure.  This is checked
     in ftcp_close.  */
  qsysdep->ipid = getpid ();

  /* If we aren't waiting for a connection, we're done.  */
  if (! fwait)
    return TRUE;

  zport = qconn->qport->uuconf_u.uuconf_stcp.uuconf_zport;

#if HAVE_GETADDRINFO
  bzero ((pointer) &shints, sizeof shints);
  if (! ftcp_set_hints (qconn->qport->uuconf_u.uuconf_stcp.uuconf_iversion,
			&shints))
    return FALSE;
  shints.ai_socktype = SOCK_STREAM;
  shints.ai_flags = AI_PASSIVE;
  ierr = getaddrinfo (NULL, zport, &shints, &qres);
  if (ierr == EAI_SERVICE && strcmp (zport, "uucp") == 0)
    ierr = getaddrinfo (NULL, ZUUCP_PORT, &shints, &qres);

  /* If getaddrinfo failed (i.e., ierr != 0), just fall back on using
     an IPv4 port.  Likewise if we can't create a socket using any of
     the resulting addrinfo structures.  */

  if (ierr != 0)
    {
      ulog (LOG_ERROR, "getaddrinfo: %s", gai_strerror (ierr));
      qres = NULL;
      quse = NULL;
    }
  else
    {
      for (quse = qres; quse != NULL; quse = quse->ai_next)
	{
	  qsysdep->o = socket (quse->ai_family, quse->ai_socktype,
			       quse->ai_protocol);
	  if (qsysdep->o >= 0)
	    break;
	}
    }
#endif /* HAVE_GETADDRINFO */

  if (qsysdep->o < 0)
    {
      if (qconn->qport->uuconf_u.uuconf_stcp.uuconf_iversion != 0
	  && qconn->qport->uuconf_u.uuconf_stcp.uuconf_iversion != 4)
	{
#ifdef HAVE_GETADDRINFO
	  ulog (LOG_ERROR, "Could not get IPv6 socket");
#else
	  ulog (LOG_ERROR, "Only IPv4 sockets are supported");
#endif
	  return FALSE;
	}

      qsysdep->o = socket (AF_INET, SOCK_STREAM, 0);
      if (qsysdep->o < 0)
	{
	  ulog (LOG_ERROR, "socket: %s", strerror (errno));
	  return FALSE;
	}
    }

  if (! ftcp_set_flags (qsysdep))
    return FALSE;

#if HAVE_GETADDRINFO
#ifdef IPV6_BINDV6ONLY
  if (quse != NULL && quse->ai_family == AF_INET6)
    {
      int iflag;

      if (qconn->qport->uuconf_u.uuconf_stcp.uuconf_iversion == 0)
	iflag = 0;
      else
	iflag = 1;
      if (setsockopt (qsysdep->o, IPPROTO_IPV6, IPV6_BINDV6ONLY,
		      (char *) &iflag, sizeof (iflag)) < 0)
	{
	  ulog (LOG_ERROR, "setsockopt (IPV6_BINDONLY): %s",
		strerror (errno));
	  (void) close (qsysdep->o);
	  qsysdep->o = -1;
	  freeaddrinfo (qres);
	  return FALSE;
	}
    }
#endif /* defined (IPV6_BINDV6ONLY) */
#endif /* HAVE_GETADDRINFO */

  /* Run as a server and wait for a new connection.  The code in
     uucico.c has already detached us from our controlling terminal.
     From this point on if the server gets an error we exit; we only
     return if we have received a connection.  It would be more robust
     to respawn the server if it fails; someday.  */

  /* Swap to our real user ID when doing the bind call.  This will
     permit the server to use privileged TCP ports when invoked by
     root.  We only swap if our effective user ID is not root, so that
     the program can also be made suid root in order to get privileged
     ports when invoked by anybody.  */
  fswap = geteuid () != 0;
  if (fswap)
    {
      if (! fsuser_perms (&ieuid, &iegid))
	{
	  (void) close (qsysdep->o);
	  qsysdep->o = -1;
#ifdef HAVE_GETADDRINFO
	  if (qres != NULL)
	    freeaddrinfo (qres);
#endif
	  return FALSE;
	}
    }

#if HAVE_GETADDRINFO
  if (quse != NULL)
    {
      if (bind (qsysdep->o, quse->ai_addr, quse->ai_addrlen) < 0)
	{
	  if (fswap)
	    (void) fsuucp_perms ((long) ieuid, (long) iegid);
	  ulog (LOG_FATAL, "bind: %s", strerror (errno));
	}
    }
  else
#endif
    {
      struct sockaddr_in sin;

      bzero ((pointer) &sin, sizeof sin);
      sin.sin_family = AF_INET;
      sin.sin_port = itcp_port_number (zport);
      sin.sin_addr.s_addr = htonl (INADDR_ANY);

      if (bind (qsysdep->o, (struct sockaddr *) &sin, sizeof sin) < 0)
	{
	  if (fswap)
	    (void) fsuucp_perms ((long) ieuid, (long) iegid);
	  ulog (LOG_FATAL, "bind: %s", strerror (errno));
	}
    }

#if HAVE_GETADDRINFO
  if (qres != NULL)
    freeaddrinfo (qres);
#endif

  /* Now swap back to the uucp user ID.  */
  if (fswap)
    {
      if (! fsuucp_perms ((long) ieuid, (long) iegid))
	ulog (LOG_FATAL, "Could not swap back to UUCP user permissions");
    }

  if (listen (qsysdep->o, 5) < 0)
    ulog (LOG_FATAL, "listen: %s", strerror (errno));

  while (! FGOT_SIGNAL ())
    {
      sockaddr_storage speer;
      size_t clen;
      int onew;
      pid_t ipid;

      DEBUG_MESSAGE0 (DEBUG_PORT,
		      "ftcp_open: Waiting for connections");

      clen = sizeof speer;
      onew = accept (qsysdep->o, (struct sockaddr *) &speer, &clen);
      if (onew < 0)
	ulog (LOG_FATAL, "accept: %s", strerror (errno));

      DEBUG_MESSAGE0 (DEBUG_PORT,
		      "ftcp_open: Got connection; forking");

      ipid = ixsfork ();
      if (ipid < 0)
	ulog (LOG_FATAL, "fork: %s", strerror (errno));
      if (ipid == 0)
	{
	  (void) close (qsysdep->o);
	  qsysdep->o = onew;

	  /* Now we fork and let our parent die, so that we become
	     a child of init.  This lets the main server code wait
	     for its child and then continue without accumulating
	     zombie children.  */
	  ipid = ixsfork ();
	  if (ipid < 0)
	    {
	      ulog (LOG_ERROR, "fork: %s", strerror (errno));
	      _exit (EXIT_FAILURE);
	    }
	      
	  if (ipid != 0)
	    _exit (EXIT_SUCCESS);

	  ulog_id (getpid ());

	  return TRUE;
	}

      (void) close (onew);

      /* Now wait for the child.  */
      (void) ixswait ((unsigned long) ipid, (const char *) NULL);
    }

  /* We got a signal.  */
  usysdep_exit (FALSE);

  /* Avoid compiler warnings.  */
  return FALSE;
}

/* Close the port.  */

/*ARGSUSED*/
static boolean
ftcp_close (qconn, puuconf, qdialer, fsuccess)
     struct sconnection *qconn;
     pointer puuconf ATTRIBUTE_UNUSED;
     struct uuconf_dialer *qdialer ATTRIBUTE_UNUSED;
     boolean fsuccess ATTRIBUTE_UNUSED;
{
  struct ssysdep_conn *qsysdep;
  boolean fret;

  qsysdep = (struct ssysdep_conn *) qconn->psysdep;
  fret = TRUE;
  if (qsysdep->o >= 0 && close (qsysdep->o) < 0)
    {
      ulog (LOG_ERROR, "close: %s", strerror (errno));
      fret = FALSE;
    }
  qsysdep->o = -1;

  /* If the current pid is not the one we used to open the port, then
     we must have forked up above and we are now the child.  In this
     case, we are being called from within the fendless loop in
     uucico.c.  We return FALSE to force the loop to end and the child
     to exit.  This should be handled in a cleaner fashion.  */
  if (qsysdep->ipid != getpid ())
    fret = FALSE;

  return fret;
}

/* Dial out on a TCP port, so to speak: connect to a remote computer.  */

/*ARGSUSED*/
static boolean
ftcp_dial (qconn, puuconf, qsys, zphone, qdialer, ptdialer)
     struct sconnection *qconn;
     pointer puuconf;
     const struct uuconf_system *qsys;
     const char *zphone;
     struct uuconf_dialer *qdialer;
     enum tdialerfound *ptdialer;
{
  struct ssysdep_conn *qsysdep;
  const char *zhost;
  const char *zport;
  char **pzdialer;
#if HAVE_GETADDRINFO
  struct addrinfo shints;
  struct addrinfo *qres;
  struct addrinfo *quse;
  int ierr;
#endif

  qsysdep = (struct ssysdep_conn *) qconn->psysdep;

  qsysdep->o = -1;

  *ptdialer = DIALERFOUND_FALSE;

  zhost = zphone;
  if (zhost == NULL)
    {
      if (qsys == NULL)
	{
	  ulog (LOG_ERROR, "No address for TCP connection");
	  return FALSE;
	}
      zhost = qsys->uuconf_zname;
    }

  zport = qconn->qport->uuconf_u.uuconf_stcp.uuconf_zport;

#if HAVE_GETADDRINFO
  bzero ((pointer) &shints, sizeof shints);
  if (! ftcp_set_hints (qconn->qport->uuconf_u.uuconf_stcp.uuconf_iversion,
			&shints))
    return FALSE;
  shints.ai_socktype = SOCK_STREAM;
  ierr = getaddrinfo (zhost, zport, &shints, &qres);
  if (ierr == EAI_SERVICE && strcmp (zport, "uucp") == 0)
    ierr = getaddrinfo (zhost, ZUUCP_PORT, &shints, &qres);

  /* If getaddrinfo failed (i.e., ierr != 0), just fall back on using
     an IPv4 port.  Likewise if we can't connect using any of the
     resulting addrinfo structures.  */

  if (ierr != 0)
    {
      ulog (LOG_ERROR, "getaddrinfo: %s", gai_strerror (ierr));
      qres = NULL;
      quse = NULL;
      ierr = 0;
    }
  else
    {
      ierr = 0;
      for (quse = qres; quse != NULL; quse = quse->ai_next)
	{
	  qsysdep->o = socket (quse->ai_family, quse->ai_socktype,
			       quse->ai_protocol);
	  if (qsysdep->o >= 0)
	    {
	      if (connect (qsysdep->o, quse->ai_addr, quse->ai_addrlen) >= 0)
		break;
	      ierr = errno;
	      close (qsysdep->o);
	      qsysdep->o = -1;
	    }
	}
      if (qres != NULL)
	freeaddrinfo (qres);
    }
#endif

  if (qsysdep->o < 0)
    {
      struct hostent *qhost;
      struct sockaddr_in sin;

      if (qconn->qport->uuconf_u.uuconf_stcp.uuconf_iversion != 0
	  && qconn->qport->uuconf_u.uuconf_stcp.uuconf_iversion != 4)
	{
#if HAVE_GETADDRINFO
	  if (ierr != 0)
	    ulog (LOG_ERROR, "connect: %s", strerror (ierr));
	  else
	    ulog (LOG_ERROR, "Could not get IPv6 address or socket");
#else
	  ulog (LOG_ERROR, "Only IPv4 sockets are supported");
#endif
	  return FALSE;
	}

      qsysdep->o = socket (AF_INET, SOCK_STREAM, 0);
      if (qsysdep->o < 0)
	{
	  ulog (LOG_ERROR, "socket: %s", strerror (errno));
	  return FALSE;
	}

      errno = 0;
      bzero ((pointer) &sin, sizeof sin);
      qhost = gethostbyname ((char *) zhost);
      if (qhost != NULL)
	{
	  sin.sin_family = qhost->h_addrtype;
	  memcpy (&sin.sin_addr.s_addr, qhost->h_addr,
		  (size_t) qhost->h_length);
	}
      else
	{
	  if (errno != 0)
	    {
	      ulog (LOG_ERROR, "gethostbyname (%s): %s", zhost,
		    strerror (errno));
	      return FALSE;
	    }

	  sin.sin_family = AF_INET;
	  sin.sin_addr.s_addr = inet_addr ((char *) zhost);
	  if ((long) sin.sin_addr.s_addr == (long) -1)
	    {
	      ulog (LOG_ERROR, "%s: unknown host name", zhost);
	      return FALSE;
	    }
	}

      sin.sin_port = itcp_port_number (zport);

      if (connect (qsysdep->o, (struct sockaddr *) &sin, sizeof sin) < 0)
	{
	  ulog (LOG_ERROR, "connect: %s", strerror (errno));
	  (void) close (qsysdep->o);
	  qsysdep->o = -1;
	  return FALSE;
	}
    }

  if (! ftcp_set_flags (qsysdep))
    return FALSE;

  /* Handle the dialer sequence, if any.  */
  pzdialer = qconn->qport->uuconf_u.uuconf_stcp.uuconf_pzdialer;
  if (pzdialer != NULL && *pzdialer != NULL)
    {
      if (! fconn_dial_sequence (qconn, puuconf, pzdialer, qsys, zphone,
				 qdialer, ptdialer))
	return FALSE;
    }

  return TRUE;
}

/* Get the port number given a name.  The argument will almost always
   be "uucp" so we cache that value.  The return value is always in
   network byte order.  This returns -1 on error.  */

static int
itcp_port_number (zname)
     const char *zname;
{
  boolean fuucp;
  static int iuucp;
  int i;
  char *zend;
  struct servent *q;

  fuucp = strcmp (zname, "uucp") == 0;
  if (fuucp && iuucp != 0)
    return iuucp;

  /* Try it as a number first.  */
  i = strtol ((char *) zname, &zend, 10);
  if (i != 0 && *zend == '\0')
    return htons (i);

  q = getservbyname ((char *) zname, (char *) "tcp");
  if (q == NULL)
    {
      /* We know that the "uucp" service should be 540, even if isn't
	 in /etc/services.  */
      if (fuucp)
	{
	  iuucp = htons (IUUCP_PORT);
	  return iuucp;
	}
      ulog (LOG_ERROR, "getservbyname (%s): %s", zname, strerror (errno));
      return -1;
    }

  if (fuucp)
    iuucp = q->s_port;

  return q->s_port;
}

#endif /* HAVE_TCP */
