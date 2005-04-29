/*
 * csconndi.c -- low level connection maker for CSDPS
 * 
 * (c) Copyright 1990-1994 Adobe Systems Incorporated.
 * All rights reserved.
 * 
 * Permission to use, copy, modify, distribute, and sublicense this software
 * and its documentation for any purpose and without fee is hereby granted,
 * provided that the above copyright notices appear in all copies and that
 * both those copyright notices and this permission notice appear in
 * supporting documentation and that the name of Adobe Systems Incorporated
 * not be used in advertising or publicity pertaining to distribution of the
 * software without specific, written prior permission.  No trademark license
 * to use the Adobe trademarks is hereby granted.  If the Adobe trademark
 * "Display PostScript"(tm) is used to describe this software, its
 * functionality or for any other purpose, such use shall be limited to a
 * statement that this software works in conjunction with the Display
 * PostScript system.  Proper trademark attribution to reflect Adobe's
 * ownership of the trademark shall be given whenever any such reference to
 * the Display PostScript system is made.
 * 
 * ADOBE MAKES NO REPRESENTATIONS ABOUT THE SUITABILITY OF THE SOFTWARE FOR
 * ANY PURPOSE.  IT IS PROVIDED "AS IS" WITHOUT EXPRESS OR IMPLIED WARRANTY.
 * ADOBE DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING ALL
 * IMPLIED WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NON- INFRINGEMENT OF THIRD PARTY RIGHTS.  IN NO EVENT SHALL ADOBE BE LIABLE
 * TO YOU OR ANY OTHER PARTY FOR ANY SPECIAL, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER WHETHER IN AN ACTION OF CONTRACT,
 * NEGLIGENCE, STRICT LIABILITY OR ANY OTHER ACTION ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.  ADOBE WILL NOT
 * PROVIDE ANY TRAINING OR OTHER SUPPORT FOR THE SOFTWARE.
 * 
 * Adobe, PostScript, and Display PostScript are trademarks of Adobe Systems
 * Incorporated which may be registered in certain jurisdictions
 * 
 * Portions Copyright 1989 Massachusetts Institute of Technology
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the name of M.I.T. not be used in advertising or
 * publicity pertaining to distribution of the software without specific,
 * written prior permission.  M.I.T. makes no representations about the
 * suitability of this software for any purpose.  It is provided "as is"
 * without express or implied warranty.
 *  
 * Author:  Adobe Systems Incorporated and MIT X Consortium
 */
/* $XFree86: xc/lib/dps/csconndi.c,v 1.12 2003/05/27 22:26:43 tsi Exp $ */

#if defined(sun) && !defined(SVR4)
#define memmove(t,f,c) bcopy(f,t,c)
#endif

#define NEED_EVENTS

#include <stdio.h>
#include <stdlib.h>
#include <X11/Xos.h>
/* Include this first so that Xasync.h, included from Xlibint.h, can find
   the definition of NOFILE */
#include <sys/param.h>
#include <X11/Xlibint.h>
#include "Xlibnet.h"		/* New for R5, delete for R4 */
#include <arpa/inet.h>

#ifndef hpux	/* HP doesn't include Xauth.h :-( */
#include <X11/Xauth.h>
#else
#define FamilyLocal (256)
#endif

#include <ctype.h>
#ifdef DNETCONN
#include <netdnet/dn.h>
#include <netdnet/dnetdb.h>
#endif

#include <netdb.h>

#include "DPSCAPproto.h"
#include "cslibint.h"
#include "dpsassert.h"

#if defined(hpux) || defined(AIXV3)
#define SELECT_TYPE int *
#else
#define SELECT_TYPE fd_set *
#endif

#ifndef _XANYSET
#define _XANYSET N_XANYSET
#endif /* _XANYSET */

#ifndef X_CONNECTION_RETRIES		/* number retries on ECONNREFUSED */
#define X_CONNECTION_RETRIES 5
#endif

#define CONN_PARAMS char *, int , int , int *, int *, char **

#ifdef DNETCONN
static int MakeDECnetConnection(CONN_PARAMS);
#endif
#ifdef UNIXCONN
static int MakeUNIXSocketConnection(CONN_PARAMS);
#endif
#ifdef TCPCONN
static int MakeTCPConnection(CONN_PARAMS);
#endif
#ifdef STREAMSCONN
extern int _XMakeStreamsConnection(CONN_PARAMS);
#endif

/* This global controls the size of the output socket buffer.  Zero
   means to use the system default size. */
int gNXSndBufSize = 0;

static char *copystring (char *src, int len)
{
    char *dst = Xmalloc (len + 1);

    if (dst) {
	strncpy (dst, src, len);
	dst[len] = '\0';
    }

    return dst;
}


/* 
 * Attempts to connect to agent, given name. Returns file descriptor
 * (network socket) or -1 if connection fails.  Names may be of the
 * following format:
 *
 *     [hostname] : [:] agentnumber
 *
 * The second colon indicates a DECnet style name.  No hostname is interpretted
 * as the most efficient local connection to a server on the same machine.  
 * This is usually:
 *
 *     o  shared memory
 *     o  local stream
 *     o  UNIX domain socket
 *     o  TCP to local host
 */
 
int 
DPSCAPConnect(
    char *display_name,
    char **fullnamep,			/* RETURN */
    int *dpynump,			/* RETURN */
    int *familyp,			/* RETURN */
    int *saddrlenp,			/* RETURN */
    char **saddrp)			/* RETURN, freed by caller */
{
    char *lastp, *p;			/* char pointers */
    char *phostname = NULL;		/* start of host of display */
    char *pdpynum = NULL;		/* start of dpynum of display */
    char *pscrnum = NULL;		/* start of screen of display */
    Bool dnet = False;			/* if true, then DECnet format */
    int iagent;				/* required display number */
    int (*connfunc)(CONN_PARAMS);	/* method to create connection */
    int fd = -1;			/* file descriptor to return */
    int len;				/* length tmp variable */

    p = display_name;

    *saddrlenp = 0;			/* set so that we can clear later */
    *saddrp = NULL;

    /*
     * Step 1, find the hostname.  This is delimited by the required 
     * first colon.
     */
    for (lastp = p; *p && *p != ':'; p++) ;
    if (!*p) return -1;		/* must have a colon */

    if (p != lastp) {		/* no hostname given */
	phostname = copystring (lastp, p - lastp);
	if (!phostname) goto bad;	/* no memory */
    }


    /*
     * Step 2, see if this is a DECnet address by looking for the optional
     * second colon.
     */
    if (p[1] == ':') {			/* then DECnet format */
	dnet = True;
	p++;
    }

    /*
     * see if we're allowed to have a DECnet address
     */
#ifndef DNETCONN
    if (dnet) goto bad;
#endif

    
    /*
     * Step 3, find the port number.  This field is required.
     * Zero is acceptable as a port number, meaning use the default.
     */

    for (lastp = ++p; *p && isascii(*p) && isdigit(*p); p++) ;
    if ((p == lastp) ||			/* required field */
	(*p != '\0' && *p != '.') ||	/* invalid non-digit terminator */
	!(pdpynum = copystring (lastp, p - lastp)))  /* no memory */
      goto bad;
    iagent = atoi (pdpynum);


    /*
     * At this point, we know the following information:
     *
     *     phostname                hostname string or NULL
     *     iagent                   agent port number
     *     dnet                     DECnet boolean
     * 
     * We can now decide which transport to use based on the ConnectionFlags
     * build parameter the hostname string.  If phostname is NULL or equals
     * the string "local", then choose the best transport.  If phostname
     * is "unix", then choose BSD UNIX domain sockets (if configured).
     *
     * First, choose default transports:  DECnet else (TCP or STREAMS)
     */


#ifdef DNETCONN
    if (dnet)
      connfunc = MakeDECnetConnection;
    else
#endif
#ifdef TCPCONN
      connfunc = MakeTCPConnection;
#else
#ifdef STREAMSCONN
      connfunc = _XMakeStreamsConnection;
#else
      connfunc = NULL;
#endif
#endif

#ifdef UNIXCONN
    /*
     * Now that the defaults have been established, see if we have any 
     * special names that we have to override:
     *
     *     :N         =>     if UNIXCONN then unix-domain-socket
     *     ::N        =>     if UNIXCONN then unix-domain-socket
     *     unix:N     =>     if UNIXCONN then unix-domain-socket
     *
     * Note that if UNIXCONN isn't defined, then we can use the default
     * transport connection function set above.
     */
    if (!phostname) {
	connfunc = MakeUNIXSocketConnection;
    }
    else if (strcmp (phostname, "unix") == 0) {
	connfunc = MakeUNIXSocketConnection;
    }
#endif
    if (!connfunc)
	goto bad;


#ifdef UNIXCONN
#define LOCALCONNECTION (!phostname || connfunc == MakeUNIXSocketConnection)
#else
#define LOCALCONNECTION (!phostname)
#endif

    if (LOCALCONNECTION) {
	/*
	 * Get the auth info for local hosts so that it doesn't have to be
	 * repeated everywhere; the particular values in these fields are
	 * not part of the protocol.
	 */
	char hostnamebuf[256];
	int len = N_XGetHostname (hostnamebuf, sizeof hostnamebuf);

	*familyp = FamilyLocal;
	if (len > 0) {
	    *saddrp = Xmalloc (len + 1);
	    if (*saddrp) {
		strcpy (*saddrp, hostnamebuf);
		*saddrlenp = len;
	    } else {
		*saddrlenp = 0;
	    }
	}
    }
#undef LOCALCONNECTION

#ifndef ultrix
    /* Ultrix has a nasty bug in getservbyname(); if the name passed to
       it is not in the services list it seg faults!  *sigh*  So, we
       don't ever look in the services info for ultrix... */
    if (iagent == 0) {		/* find service default, if one's defined */
      struct servent *serventInfo;
      
      if ((serventInfo = getservbyname(DPS_NX_SERV_NAME,
      				       (char *) NULL)) != NULL) {
	if (strcmp("tcp", serventInfo->s_proto) != 0) {
	  DPSWarnProc(NULL, "Services database specifies a protocol other than tcp.  Using default port.\n");
	} else 			/* extract the port number */
          iagent = ntohs(serventInfo->s_port);
      }
    }
#endif /* ultrix */    
    /*
     * Make the connection, also need to get the auth address info for
     * non-local connections.  Do retries in case server host has hit its
     * backlog (which, unfortunately, isn't distinguishable from there not
     * being a server listening at all, which is why we have to not retry
     * too many times).
     */
    if ((fd = (*connfunc) (phostname, iagent, X_CONNECTION_RETRIES,
			   familyp, saddrlenp, saddrp)) < 0)
      goto bad;


    /*
     * Set the connection non-blocking since we use select() to block; also
     * set close-on-exec so that programs that fork() doesn't get confused.
     */
#ifdef FIOSNBIO
    {
	int arg = 1;
	ioctl (fd, FIOSNBIO, &arg);
    }
#else
#if defined(O_NONBLOCK)
    (void) fcntl (fd, F_SETFL, O_NONBLOCK);
#elif defined(FNDELAY)
    (void) fcntl (fd, F_SETFL, FNDELAY);
#else
    (void) fcntl (fd, F_SETFL, O_NDELAY);
#endif /* O_NONBLOCK/FNDELAY */
#endif /* FIOSNBIO */

    (void) fcntl (fd, F_SETFD, 1);


    /*
     * Build the expanded display name:
     *
     *     [host] : [:] agent \0
     */
    len = ((phostname ? strlen(phostname) : 0) + 1 + (dnet ? 1 : 0) +
	   strlen(pdpynum) + 1);
    *fullnamep = (char *) Xmalloc (len);
    if (!*fullnamep) goto bad;

    sprintf (*fullnamep, "%s%s%d",
	     (phostname ? phostname : ""), (dnet ? "::" : ":"),
	     iagent);

    *dpynump = iagent;
    if (phostname) Xfree (phostname);
    if (pdpynum) Xfree (pdpynum);
    if (pscrnum) Xfree (pscrnum);
    return fd;


    /*
     * error return; make sure everything is cleaned up.
     */
  bad:
    if (fd >= 0) (void) close (fd);
    if (*saddrp) {
	Xfree (*saddrp);
	*saddrp = NULL;
    }
    *saddrlenp = 0;
    if (phostname) Xfree (phostname);
    if (pdpynum) Xfree (pdpynum);
    return -1;

}


/*****************************************************************************
 *                                                                           *
 *			   Make Connection Routines                          *
 *                                                                           *
 *****************************************************************************/

#ifdef DNETCONN				/* stupid makedepend */
#define NEED_BSDISH
#endif
#ifdef UNIXCONN
#define NEED_BSDISH
#endif
#ifdef TCPCONN
#define NEED_BSDISH
#endif

#ifdef NEED_BSDISH			/* makedepend can't handle #if */
/*
 * 4.2bsd-based systems
 */
#include <sys/socket.h>

#ifndef hpux
#ifdef apollo			/* nest if(n)defs because makedepend is broken */
#ifndef NO_TCP_H
#include <netinet/tcp.h>
#endif /* NO_TCP_H */
#else  /* apollo */
#include <netinet/tcp.h>
#endif /* apollo */
#endif
#endif /* NEED_BSDISH */


#ifdef DNETCONN
static int MakeDECnetConnection (phostname, iagent, retries,
				 familyp, saddrlenp, saddrp)
    char *phostname;
    int iagent;
    int retries;
    int *familyp;			/* RETURN */
    int *saddrlenp;			/* RETURN */
    char **saddrp;			/* RETURN */
{
    int fd;
    char objname[20];
    extern int dnet_conn();
    struct dn_naddr *dnaddrp, dnaddr;
    struct nodeent *np;

    if (!phostname) phostname = "0";

    /*
     * build the target object name.
     */
    sprintf (objname, "DPS$EXECUTIVE_%d", iagent);

    /*
     * Attempt to open the DECnet connection, return -1 if fails; ought to
     * do some retries here....
     */
    if ((fd = dnet_conn (phostname, objname, SOCK_STREAM, 0, 0, 0, 0)) < 0) {
	return -1;
    }

    *familyp = FamilyDECnet;
    if (dnaddrp = dnet_addr (phostname)) {  /* stolen from xhost */
	dnaddr = *dnaddrp;
    } else {
	if ((np = getnodebyname (phostname)) == NULL) {
	    (void) close (fd);
	    return -1;
	}
	dnaddr.a_len = np->n_length;
	memmove(dnaddr.a_addr, np->n_addr, np->n_length);
    }

    *saddrlenp = sizeof (struct dn_naddr);
    *saddrp = Xmalloc (*saddrlenp);
    if (!*saddrp) {
	(void) close (fd);
	return -1;
    }
    memmove (*saddrp, (char *)&dnaddr, *saddrlenp);
    return fd;
}
#endif /* DNETCONN */


#ifdef UNIXCONN

#ifndef X_NO_SYS_UN
#include <sys/un.h>
#endif

#ifndef CSDPS_UNIX_PATH
#ifdef hpux
#define CSDPS_UNIX_DIR      "/usr/spool/sockets/DPSNX"
#define CSDPS_UNIX_PATH     "/usr/spool/sockets/DPSNX/"
#else
#define CSDPS_UNIX_DIR      "/tmp/.DPSNX-unix"
#define CSDPS_UNIX_PATH     "/tmp/.DPSNX-unix/AGENT"
#endif
#endif

static int MakeUNIXSocketConnection (
    char *phostname,
    int iagent,
    int retries,
    int *familyp,			/* RETURN */
    int *saddrlenp,			/* RETURN */
    char **saddrp)			/* RETURN */
{
    struct sockaddr_un unaddr;		/* UNIX socket data block */
    struct sockaddr *addr;		/* generic socket pointer */
    int addrlen;			/* length of addr */
    int fd;				/* socket file descriptor */
    int port = (iagent == 0) ? CSDPSPORT : iagent;

    unaddr.sun_family = AF_UNIX;
    sprintf (unaddr.sun_path, "%s_%d", CSDPS_UNIX_PATH, port);

    addr = (struct sockaddr *) &unaddr;
    addrlen = strlen(unaddr.sun_path) + sizeof(unaddr.sun_family);

    /*
     * Open the network connection.
     */
    do {
	if ((fd = socket ((int) addr->sa_family, SOCK_STREAM, 0)) < 0) {
	    return -1;
	}

        if (gNXSndBufSize > 0)
	    setsockopt(fd, SOL_SOCKET, SO_SNDBUF, (char *) &gNXSndBufSize, sizeof(gNXSndBufSize));

	if (connect (fd, addr, addrlen) < 0) {
	    int olderrno = errno;
	    (void) close (fd);
	    if (olderrno != ENOENT || retries <= 0) {
		errno = olderrno;
		return -1;
	    }
	    sleep (1);
	} else {
	    break;
	}
    } while (retries-- > 0);

    /*
     * Don't need to get auth info since we're local
     */
    return fd;
}
#endif /* UNIXCONN */


#ifdef TCPCONN
static int MakeTCPConnection (
    char *phostname,
    int iagent,
    int retries,
    int *familyp,			/* RETURN */
    int *saddrlenp,			/* RETURN */
    char **saddrp)			/* RETURN */
{
    char hostnamebuf[256];		/* tmp space */
    unsigned long hostinetaddr;		/* result of inet_addr of arpa addr */
    struct sockaddr_in inaddr;		/* IP socket */
    struct sockaddr *addr;		/* generic socket pointer */
    int addrlen;			/* length of addr */
    struct hostent *hp;			/* entry in hosts table */
    char *cp;				/* character pointer iterator */
    int fd;				/* file descriptor to return */
    int len;				/* length tmp variable */

#define INVALID_INETADDR ((unsigned long) -1)

    if (!phostname) {
	hostnamebuf[0] = '\0';
	(void) N_XGetHostname (hostnamebuf, sizeof hostnamebuf);
	phostname = hostnamebuf;
    }

    /*
     * if numeric host name then try to parse it as such; do the number
     * first because some systems return garbage instead of INVALID_INETADDR
     */
    if (isascii(phostname[0]) && isdigit(phostname[0])) {
	hostinetaddr = inet_addr (phostname);
    } else {
	hostinetaddr = INVALID_INETADDR;
    }

    /*
     * try numeric
     */
    if (hostinetaddr == INVALID_INETADDR) {
	if ((hp = gethostbyname(phostname)) == NULL) {
	    /* No such host! */
	    return -1;
	}
	if (hp->h_addrtype != AF_INET) {  /* is IP host? */
	    /* Not an Internet host! */
	    return -1;
	}
 
	/* Set up the socket data. */
	inaddr.sin_family = hp->h_addrtype;
#if defined(CRAY) && defined(OLDTCP)
	/* Only Cray UNICOS3 and UNICOS4 will define this */
	{
	    long t;
	    memmove ((char *)&t, (char *)hp->h_addr, sizeof(t));
	    inaddr.sin_addr = t;
	}
#else
	memmove ((char *)&inaddr.sin_addr, (char *)hp->h_addr, 
	       sizeof(inaddr.sin_addr));
#endif /* CRAY and OLDTCP */
    } else {
#if defined(CRAY) && defined(OLDTCP)
	/* Only Cray UNICOS3 and UNICOS4 will define this */
	inaddr.sin_addr = hostinetaddr;
#else
	inaddr.sin_addr.s_addr = hostinetaddr;
#endif /* CRAY and OLDTCP */
	inaddr.sin_family = AF_INET;
    }

    addr = (struct sockaddr *) &inaddr;
    addrlen = sizeof (struct sockaddr_in);
    inaddr.sin_port = (iagent == 0) ? CSDPSPORT : iagent;
    inaddr.sin_port = htons (inaddr.sin_port);	/* may be funky macro */


    /*
     * Open the network connection.
     */
    do {
	if ((fd = socket ((int) addr->sa_family, SOCK_STREAM, 0)) < 0) {
	    return -1;
	}

	/*
	 * turn off TCP coalescence
	 */
#ifdef TCP_NODELAY
	{
	    int tmp = 1;
	    setsockopt (fd, IPPROTO_TCP, TCP_NODELAY, (char *)&tmp, sizeof (int));
	}
#endif
        if (gNXSndBufSize > 0)
	    len = setsockopt(fd, SOL_SOCKET, SO_SNDBUF, (char *) &gNXSndBufSize, sizeof(gNXSndBufSize));

	/*
	 * connect to the socket; if there is no X server or if the backlog has
	 * been reached, then ECONNREFUSED will be returned.
	 */
	if (connect (fd, addr, addrlen) < 0) {
	    int olderrno = errno;
	    (void) close (fd);
	    if (olderrno != ECONNREFUSED || retries <= 0) {
		errno = olderrno;
		return -1;
	    }
	    sleep (1);
	} else {
	    break;
	}
    } while (retries-- > 0);


    /*
     * Success!  So, save the auth information
     */
#ifdef CRAY
#ifdef OLDTCP
    len = sizeof(inaddr.sin_addr);
#else
    len = SIZEOF_in_addr;
#endif /* OLDTCP */
    cp = (char *) &inaddr.sin_addr;
#else /* else not CRAY */
    len = sizeof(inaddr.sin_addr.s_addr);
    cp = (char *) &inaddr.sin_addr.s_addr;
#endif /* CRAY */

    /*
     * We are special casing the BSD hack localhost address
     * 127.0.0.1, since this address shouldn't be copied to
     * other machines.  So, we simply omit generating the auth info
     * since we set it to the local machine before calling this routine!
     */
    if (!((len == 4) && (cp[0] == 127) && (cp[1] == 0) &&
	  (cp[2] == 0) && (cp[3] == 1))) {
	*saddrp = Xmalloc (len);
	if (*saddrp) {
	    *saddrlenp = len;
	    memmove (*saddrp, cp, len);
	    *familyp = FamilyInternet;
	} else {
	    *saddrlenp = 0;
	}
    }

    return fd;
}
#undef INVALID_INETADDR
#endif /* TCPCONN */


/*****************************************************************************
 *                                                                           *
 *			  Connection Utility Routines                        *
 *                                                                           *
 *****************************************************************************/

/* 
 * Disconnect from server.
 */

int N_XDisconnectDisplay (int server)
{
    (void) close(server);
    return 0;
}


#include <stddef.h>
/*
 * This is an OS dependent routine which:
 * 1) returns as soon as the connection can be written on....
 * 2) if the connection can be read, must enqueue events and handle errors,
 * until the connection is writable.
 */
void N_XWaitForWritable(Display *dpy)
{
    unsigned long r_mask[MSKCNT];
    unsigned long w_mask[MSKCNT];
    int nfound;

    CLEARBITS(r_mask);
    CLEARBITS(w_mask);

    while (1) {
	BITSET(r_mask, dpy->fd);
        BITSET(w_mask, dpy->fd);

	do {
	    nfound = select (dpy->fd + 1, (SELECT_TYPE) r_mask,
			     (SELECT_TYPE) w_mask, (SELECT_TYPE) NULL,
			     (struct timeval *) NULL);
	    if (nfound < 0 && errno != EINTR)
		_XIOError(dpy);
	} while (nfound <= 0);

	if (_XANYSET(r_mask)) {
	    char buf[BUFSIZE];
	    long pend_not_register;
	    register long pend;
	    register xEvent *ev;

	    /* find out how much data can be read */
	    if (BytesReadable(dpy->fd, (char *) &pend_not_register) < 0)
		_XIOError(dpy);
	    pend = pend_not_register;

	    /* must read at least one xEvent; if none is pending, then
	       we'll just block waiting for it */
	    if (pend < SIZEOF(xEvent)) pend = SIZEOF(xEvent);
		
	    /* but we won't read more than the max buffer size */
	    if (pend > BUFSIZE) pend = BUFSIZE;

	    /* round down to an integral number of XReps */
	    pend = (pend / SIZEOF(xEvent)) * SIZEOF(xEvent);

	    N_XRead (dpy, buf, pend);

	    /* no space between comma and type or else macro will die */
	    STARTITERATE (ev,xEvent, buf, (pend > 0),
			  (pend -= SIZEOF(xEvent))) {
		if (ev->u.u.type == X_Error)
		    _XError (dpy, (xError *) ev);
		else		/* it's an event packet; die */
		    DPSFatalProc((DPSContext)0, "N_XWaitForWritable read bogus X event");
	    }
	    ENDITERATE
	}
	if (_XANYSET(w_mask))
	    return;
    }
}


void N_XWaitForReadable(Display *dpy)
{
    unsigned long r_mask[MSKCNT];
    int result;
	
    CLEARBITS(r_mask);
    do {
	BITSET(r_mask, dpy->fd);
	result = select(dpy->fd + 1, (SELECT_TYPE) r_mask, (SELECT_TYPE) NULL,
			(SELECT_TYPE) NULL, (struct timeval *) NULL);
	if (result == -1 && errno != EINTR) _XIOError(dpy);
    } while (result <= 0);
}

#ifdef XXX

static int padlength[4] = {0, 3, 2, 1};	 /* make sure auth is multiple of 4 */

_XSendClientPrefix (
     Display *dpy,
     xConnClientPrefix *client,		/* contains count for auth_* */
     char *auth_proto,			/* NOT null-terminated */
     char char *auth_string)		/* NOT null-terminated */
{
    int auth_length = client->nbytesAuthProto;
    int auth_strlen = client->nbytesAuthString;
    char padbuf[3];			/* for padding to 4x bytes */
    int pad;
    struct iovec iovarray[5], *iov = iovarray;
    int niov = 0;

#define add_to_iov(b,l) \
	  { iov->iov_base = (b); iov->iov_len = (l); iov++, niov++; }

    add_to_iov ((caddr_t) client, SIZEOF(xConnClientPrefix));

    /*
     * write authorization protocol name and data
     */
    if (auth_length > 0) {
	add_to_iov (auth_proto, auth_length);
	pad = padlength [auth_length & 3];
	if (pad) add_to_iov (padbuf, pad);
    }
    if (auth_strlen > 0) {
	add_to_iov (auth_string, auth_strlen);
	pad = padlength [auth_strlen & 3];
	if (pad) add_to_iov (padbuf, pad);
    }

#undef add_to_iov

    (void) WritevToServer (dpy->fd, iovarray, niov);
    return;
}

#endif /* XXX */
