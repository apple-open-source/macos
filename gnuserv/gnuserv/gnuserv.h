/* -*-C-*-

 Header file for the GNU Emacs server and client C code.

 This file is part of GNU Emacs.

 Copying is permitted under those conditions described by the GNU
 General Public License.

 Copyright (C) 1989 Free Software Foundation, Inc.

 Author: Andy Norman (ange@hplb.hpl.hp.com), based on 
         'etc/server.c' and 'etc/emacsclient.c' from the 18.52 GNU
         Emacs distribution.

 Please mail bugs and suggestions to the author at the above address.
*/

/* HISTORY 
 * 11-Nov-1990		bristor@simba	
 *    Added EOT stuff.
 */

/*
 * This file incorporates new features added by Bob Weiner <weiner@mot.com>,
 * Darrell Kindred <dkindred@cmu.edu> and Arup Mukherjee <arup@cmu.edu>.
 * Please see the note at the end of the README file for details.
 *
 * (If gnuserv came bundled with your emacs, the README file is probably
 * ../etc/gnuserv.README relative to the directory containing this file)
 */

#define GNUSERV_VERSION "3.12"

#define USE_TMPDIR

#define PATCHLEVEL 2

#define NO_SHORTNAMES
/* gnuserv should not be compiled using SOCKS */
#define DO_NOT_SOCKSIFY
#define DONT_ENCAPSULATE
#include <config.h>
#undef signal

/* Define the communication method between server and clients:
 *   You can have either or both kinds of sockets, but you can't mix
 *   sockets with sysv ipc
 */


#if defined(HAVE_INTERNET_DOMAIN_SOCKETS) || defined(HAVE_UNIX_DOMAIN_SOCKETS)
#if defined(HAVE_INTERNET_DOMAIN_SOCKETS)
#define INTERNET_DOMAIN_SOCKETS
#endif
#if defined(HAVE_UNIX_DOMAIN_SOCKETS)
#define UNIX_DOMAIN_SOCKETS
#endif
#else
#if defined(HAVE_SYSVIPC)
#define SYSV_IPC
#endif
#endif
/*
 * Define additional authentication protocols to be used. These methods will
 * be tried before falling back to the default gnuserv protocol (based on
 * the GNU_SECURE environment variable). Currently, only MIT-MAGIC-COOKIE-1
 * is also supported.
 *
 * Comment out the next line(s) if you don't want to enable the
 * appropriate authentication protocol.
 */

#if defined (HAVE_XAUTH)
#define AUTH_MAGIC_COOKIE
#endif /* HAVE_XAUTH */

/*
 * stuff related to supporting MIT-MAGIC-COOKIE-1
 */

#define MCOOKIE_SCREEN "999"     /* screen # to use as the gnuserv cookie */
#define MCOOKIE_NAME   "MAGIC-1" /* authentication protocol name */
#define MCOOKIE_X_NAME "MIT-MAGIC-COOKIE-1"  /* as needed by X */


#define DEFAUTH_NAME "GNU-SECURE"  /* name of default auth protocol */
#define AUTH_TIMEOUT  15           /* # seconds to wait for auth data */
#define AUTH_NAMESZ   15           /* max allows auth protocol name size */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <errno.h>

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
#endif

/*
 * If you are using SYSV_IPC, you might want to make the buffer size bigger
 * since it limits the size of requests and responses. Don't make it bigger
 * than your system's max message size though (usually a couple of k) or else
 * msgsend will start failing. For sockets, using the system BUFSIZ is usually
 * what you want. 
 */

# define GSERV_BUFSZ BUFSIZ


#ifdef SYSV_IPC
#include <sys/ipc.h>
#include <sys/msg.h>

#define send_string(s,str) \
  if (strlen(msgp->mtext) + strlen(str) < GSERV_BUFSZ) \
     strcat(msgp->mtext,str); \
  else \
  { \
    fprintf(stderr,"%s: not enough message buffer space\n",progname); \
     exit(1); \
  } \

#endif /* SYSV_IPC */

#if defined(INTERNET_DOMAIN_SOCKETS) || defined(UNIX_DOMAIN_SOCKETS)
#include <sys/socket.h>
#endif /* INTERNET_DOMAIN_SOCKETS || UNIX_DOMAIN_SOCKETS */

#ifdef INTERNET_DOMAIN_SOCKETS
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#define TABLE_SIZE 101		/* The number of entries in the hash table */
#define HASH(host) host		/* Rather simplistic hash function */
#define DEFAULT_PORT 21490	/* default port number to use is
				 * DEFAULT_PORT + uid */
#endif /* INTERNET_DOMAIN_SOCKETS */

#ifdef UNIX_DOMAIN_SOCKETS
#include <sys/un.h>
#define HIDE_UNIX_SOCKET	/* put the unix socket in a protected dir */
#endif /* UNIX_DOMAIN_SOCKETS */

/* On some platforms, we need to do the equivalent of "stty litout" to get
 * characters like ^D to pass through to emacs.  This problem has only
 * been observed under emacs18; fsf19 and lemacs are probably okay without it.
 */
#ifndef DONT_USE_LITOUT
#if !defined(HAVE_TERMIO) && !defined(HAVE_TERMIOS) && !defined(VMS)
#if !defined(BSD4_1)
#define USE_LITOUT
#endif
#endif
#endif


#define HOSTNAMSZ 255		/* max size of a hostname */
#define REPLYSIZ 300		/* max size of reply from server to client */
#undef FALSE
#define FALSE 0
#undef TRUE
#define TRUE 1

extern char *optarg;
extern int optind;
extern char *progname;
extern char *tmpdir;

/* The casts shut Sun's compiler up and are safe in the context these
   are actually used. */
#define max2(x,y) (((int) (x) > (int) (y)) ? (x) : (y))
#define min2(x,y) (((int) (x) < (int) (y)) ? (x) : (y))

#ifndef _NFILE            /* rough guess at maximum number of open files */
#define _NFILE 20
#endif

#define EOT_STR "\004"
#define EOT_CHR '\004'

/* connection types */
#define CONN_UNIX     0
#define CONN_INTERNET 1
#define CONN_IPC      2

/* function declarations */
int make_connection (char *hostarg, int portarg, int *s);
#ifdef SYSV_IPC
void disconnect_from_ipc_server();
#endif
#if defined(INTERNET_DOMAIN_SOCKETS) || defined(UNIX_DOMAIN_SOCKETS)
void send_string (int s, const char *msg);
void disconnect_from_server (int s, int echo);
int read_line (int s, char *dest);
#endif
#ifdef INTERNET_DOMAIN_SOCKETS
int internet_addr (char *host);
#endif
