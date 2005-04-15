/* acconfig.h - autoheader configuration input
 * Rob Earhart
 */
/* 
 * Copyright (c) 1998-2003 Carnegie Mellon University.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer. 
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * 3. The name "Carnegie Mellon University" must not be used to
 *    endorse or promote products derived from this software without
 *    prior written permission. For permission or any other legal
 *    details, please contact  
 *      Office of Technology Transfer
 *      Carnegie Mellon University
 *      5000 Forbes Avenue
 *      Pittsburgh, PA  15213-3890
 *      (412) 268-4387, fax: (412) 268-7395
 *      tech-transfer@andrew.cmu.edu
 *
 * 4. Redistributions of any form whatsoever must retain the following
 *    acknowledgment:
 *    "This product includes software developed by Computing Services
 *     at Carnegie Mellon University (http://www.cmu.edu/computing/)."
 *
 * CARNEGIE MELLON UNIVERSITY DISCLAIMS ALL WARRANTIES WITH REGARD TO
 * THIS SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS, IN NO EVENT SHALL CARNEGIE MELLON UNIVERSITY BE LIABLE
 * FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN
 * AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING
 * OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#ifndef CONFIG_H
#define CONFIG_H

@TOP@

/* Our package */
#undef PACKAGE

/* Our version */
#undef VERSION

/* Set to the database name you want SASL to use for
 * username->secret lookups */
#undef SASL_DB_PATH

/* what db package are we using? */
#undef SASL_GDBM
#undef SASL_NDBM
#undef SASL_BERKELEYDB

/* which mechs can we link staticly? */
#undef STATIC_ANONYMOUS
#undef STATIC_CRAMMD5
#undef STATIC_DIGESTMD5
#undef STATIC_GSSAPIV2
#undef STATIC_KERBEROS4
#undef STATIC_LOGIN
#undef STATIC_MYSQL
#undef STATIC_NTLM
#undef STATIC_OTP
#undef STATIC_PLAIN
#undef STATIC_SASLDB
#undef STATIC_SRP

/* This is where plugins will live at runtime */
#undef PLUGINDIR

/* Make autoheader happy */
#undef WITH_SYMBOL_UNDERSCORE

/* should we use the internal rc4 library? */
#undef WITH_RC4

/* do we have des available? */
#undef WITH_DES
#undef WITH_SSL_DES

/* what about OpenSSL? */
#undef HAVE_OPENSSL

/* should we support srp_setpass */
#undef DO_SRP_SETPASS

/* do we have OPIE for server-side OTP support? */
#undef HAVE_OPIE

/* Do we have kerberos for plaintext password checking? */
#undef HAVE_KRB

/* do we have SIA for plaintext password checking? */
#undef HAVE_SIA

/* do we have PAM for plaintext password checking? */
#undef HAVE_PAM

/* what flavor of GSSAPI are we using? */
#undef HAVE_GSS_C_NT_HOSTBASED_SERVICE

/* does GSSAPI provide GSS_C_NT_USER_NAME? */
#undef HAVE_GSS_C_NT_USER_NAME

/* do we have gssapi.h or gssapi/gssapi.h? */
#undef HAVE_GSSAPI_H

/* do we have getsubopt()? */
#undef HAVE_GETSUBOPT

/* Does your system have the snprintf() call? */
#undef HAVE_SNPRINTF

/* Does your system have the vsnprintf() call? */
#undef HAVE_VSNPRINTF

/* does your system have gettimeofday()? */
#undef HAVE_GETTIMEOFDAY

/* should we include support for the pwcheck daemon? */
#undef HAVE_PWCHECK

/* where do we look for the pwcheck daemon? */
#undef PWCHECKDIR

/* should we include support for the saslauth daemon? */
#undef HAVE_SASLAUTHD

/* where does saslauthd look for the communication socket? */
#undef PATH_SASLAUTHD_RUNDIR

/* do we want alwaystrue (discouraged)? */
#undef HAVE_ALWAYSTRUE

/* are we linking against DMALLOC? */
#undef WITH_DMALLOC

/* should we support sasl_checkapop */
#undef DO_SASL_CHECKAPOP

/* do we pay attention to IP addresses in the kerberos 4 tickets? */
#undef KRB4_IGNORE_IP_ADDRESS

/* do we have a preferred mechanism, or should we just pick the highest ssf? */
#undef PREFER_MECH

/* define if your compile has __attribute__ */
#undef HAVE___ATTRIBUTE__

/* define if you have unistd.h */
#undef HAVE_UNISTD_H

/* define if your system has getpid() */
#undef HAVE_GETPID

/* do we have an inttypes.h? */
#undef HAVE_INTTYPES_H

/* do we have sys/uio.h? */
#undef HAVE_SYS_UIO_H

/* do we have sys/param.h? */
#undef HAVE_SYS_PARAM_H

/* do we have sysexits.h? */
#undef HAVE_SYSEXITS_H

/* stdarg.h? varargs.h? */
#undef HAVE_STDARG_H
#undef HAVE_VARARGS_H

/* Do we need a leading _ for dlsym? */
#undef DLSYM_NEEDS_UNDERSCORE

/* Does libtool support shared libs on this system? */
#undef HAVE_DLFCN_H
#undef DO_DLOPEN

/* Should we try to dlopen stuff when we are staticly compiled? */
#undef TRY_DLOPEN_WHEN_STATIC

/* define if your system has getaddrinfo() */
#undef HAVE_GETADDRINFO

/* define if your system has getnameinfo() */
#undef HAVE_GETNAMEINFO

/* define if your system has struct sockaddr_storage */
#undef HAVE_STRUCT_SOCKADDR_STORAGE

/* Define if you have ss_family in struct sockaddr_storage. */
#undef HAVE_SS_FAMILY

/* do we have socklen_t? */
#undef HAVE_SOCKLEN_T
#undef HAVE_SOCKADDR_SA_LEN

/* do we use doors for IPC? */
#undef USE_DOORS

/* SASL's concept of DEV_RANDOM */
#undef SASL_DEV_RANDOM

@BOTTOM@

/* Create a struct iovec if we need one */
#if !defined(_WIN32) && !defined(HAVE_SYS_UIO_H)
/* (win32 is handled in sasl.h) */
struct iovec {
    char *iov_base;
    long iov_len;
};
#else
#include <sys/types.h>
#include <sys/uio.h>
#endif

/* location of the random number generator */
#ifdef DEV_RANDOM
#undef DEV_RANDOM
#endif
#define DEV_RANDOM SASL_DEV_RANDOM

/* if we've got krb_get_err_txt, we might as well use it;
   especially since krb_err_txt isn't in some newer distributions
   (MIT Kerb for Mac 4 being a notable example). If we don't have
   it, we fall back to the krb_err_txt array */
#ifdef HAVE_KRB_GET_ERR_TEXT
#define get_krb_err_txt krb_get_err_text
#else
#define get_krb_err_txt(X) (krb_err_txt[(X)])
#endif

/* Make Solaris happy... */
#ifndef __EXTENSIONS__
#define __EXTENSIONS__
#endif

/* Make Linux happy... */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#ifndef HAVE___ATTRIBUTE__
/* Can't use attributes... */
#define __attribute__(foo)
#endif

#define SASL_PATH_ENV_VAR "SASL_PATH"

#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#ifndef WIN32
# include <netdb.h>
# ifdef HAVE_SYS_PARAM_H
#  include <sys/param.h>
# endif
#else /* WIN32 */
# include <winsock.h>
#endif /* WIN32 */
#include <string.h>

#include <netinet/in.h>

#ifndef HAVE_SOCKLEN_T
typedef unsigned int socklen_t;
#endif /* HAVE_SOCKLEN_T */

#ifndef HAVE_STRUCT_SOCKADDR_STORAGE
#define	_SS_MAXSIZE	128	/* Implementation specific max size */
#define	_SS_PADSIZE	(_SS_MAXSIZE - sizeof (struct sockaddr))

struct sockaddr_storage {
	struct	sockaddr ss_sa;
	char		__ss_pad2[_SS_PADSIZE];
};
# define ss_family ss_sa.sa_family
#endif /* !HAVE_STRUCT_SOCKADDR_STORAGE */

#ifndef AF_INET6
/* Define it to something that should never appear */
#define	AF_INET6	AF_MAX
#endif

#ifndef HAVE_GETADDRINFO
#define	getaddrinfo	sasl_getaddrinfo
#define	freeaddrinfo	sasl_freeaddrinfo
#define	getnameinfo	sasl_getnameinfo
#define	gai_strerror	sasl_gai_strerror
#include "gai.h"
#endif

/* Defined in RFC 1035. max strlen is only 253 due to length bytes. */
#ifndef MAXHOSTNAMELEN
#define        MAXHOSTNAMELEN  255
#endif

#ifndef HAVE_SYSEXITS_H
#include "exits.h"
#else
#include "sysexits.h"
#endif

#ifndef	NI_WITHSCOPEID
#define	NI_WITHSCOPEID	0
#endif

/* Get the correct time.h */
#if TIME_WITH_SYS_TIME
# include <sys/time.h>
# include <time.h>
#else
# if HAVE_SYS_TIME_H
#  include <sys/time.h>
# else
#  include <time.h>
# endif
#endif

#endif /* CONFIG_H */
