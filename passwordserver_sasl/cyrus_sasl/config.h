/* config.h.  Generated automatically by configure.  */
/* config.h.in.  Generated automatically from configure.in by autoheader.  */
/* acconfig.h - autoheader configuration input
 * Rob Earhart
 */
/* 
 * Copyright (c) 2001 Carnegie Mellon University.  All rights reserved.
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

#include <machine/ansi.h>

/* Define to empty if the keyword does not work.  */
/* #undef const */

/* Define if you have <sys/wait.h> that is POSIX.1 compatible.  */
#define HAVE_SYS_WAIT_H 1

/* Define as __inline if that's what the C compiler calls it.  */
/* #undef inline */

/* Define to `int' if <sys/types.h> doesn't define.  */
/* #undef mode_t */

/* Define to `int' if <sys/types.h> doesn't define.  */
/* #undef pid_t */

/* Define as the return type of signal handlers (int or void).  */
#define RETSIGTYPE void

/* Define if you have the ANSI C header files.  */
#define STDC_HEADERS 1

/* Define if you can safely include both <sys/time.h> and <time.h>.  */
#define TIME_WITH_SYS_TIME 1

/* Set to the database name you want SASL to use for
 * username->secret lookups */
#define SASL_DB_PATH "/etc/sasldb2"

/* what db package are we using? */
/* #undef SASL_GDBM */
#define SASL_NDBM 1
/* #undef SASL_BERKELEYDB */

/* which mechs can we link staticly? */
#define STATIC_ANONYMOUS 1
#define STATIC_CRAMMD5 1
#define STATIC_DIGESTMD5 1
/* #undef STATIC_GSSAPIV2 */
#define STATIC_KERBEROS4 1
/* #undef STATIC_LOGIN */
#define STATIC_OTP 1
#define STATIC_PLAIN 1
/* #undef STATIC_SRP */
#define STATIC_SASLDB 1

/* This is where plugins will live at runtime */
#define PLUGINDIR "/usr/lib/sasl2"

/* Do we need a leading _ for dlsym? */
/* #undef DLSYM_NEEDS_UNDERSCORE */

/* should we use the internal rc4 library? */
#define WITH_RC4 1

/* do we have des available? */
#define WITH_DES 1
#define WITH_SSL_DES 1

/* should we support srp_setpass */
/* #undef DO_SRP_SETPASS */

/* do we have OPIE for server-side OTP support? */
/* #undef HAVE_OPIE */

/* Do we have kerberos for plaintext password checking? */
#define HAVE_KRB 1

/* do we have PAM for plaintext password checking? */
#define HAVE_PAM 1

/* what flavor of GSSAPI are we using? */
/* #undef HAVE_GSS_C_NT_HOSTBASED_SERVICE */

/* do we have gssapi.h or gssapi/gssapi.h? */
/* #undef HAVE_GSSAPI_H */

/* do we have getsubopt()? */
#define HAVE_GETSUBOPT 1

/* Does your system have the snprintf() call? */
#define HAVE_SNPRINTF 1

/* Does your system have the vsnprintf() call? */
#define HAVE_VSNPRINTF 1

/* should we include support for the pwcheck daemon? */
/* #undef HAVE_PWCHECK */

/* where do we look for the pwcheck daemon? */
/* #undef PWCHECKDIR */

/* should we include support for the saslauth daemon? */
/* #undef HAVE_SASLAUTHD */

/* where does saslauthd look for the communication socket? */
/* #undef PATH_SASLAUTHD_RUNDIR */

/* do we want alwaystrue (discouraged)? */
/* #undef HAVE_ALWAYSTRUE */

/* are we linking against DMALLOC? */
/* #undef WITH_DMALLOC */

/* should we support sasl_checkapop */
#define DO_SASL_CHECKAPOP 1

/* do we pay attention to IP addresses in the kerberos 4 tickets? */
/* #undef KRB4_IGNORE_IP_ADDRESS */

/* do we have a preferred mechanism, or should we just pick the highest ssf? */
/* #undef PREFER_MECH */

/* do we have a wierd location of db.h? */
/* #undef HAVE_DB3_DB_H */

/* define if your system has getnameinfo() */
#define HAVE_GETADDRINFO 1

/* define if your system has getnameinfo() */
#define HAVE_GETNAMEINFO 1

/* define if your system has struct sockaddr_storage */
#define HAVE_STRUCT_SOCKADDR_STORAGE 1

/* Define if you have ss_family in struct sockaddr_storage. */
#define HAVE_SS_FAMILY 1

/* do we have socklen_t? */
#ifdef _BSD_SOCKLEN_T_
#define HAVE_SOCKLEN_T
#endif
#define HAVE_SOCKADDR_SA_LEN 1

/* Define if you have the dn_expand function.  */
#define HAVE_DN_EXPAND 1

/* Define if you have the dns_lookup function.  */
/* #undef HAVE_DNS_LOOKUP */

/* Define if you have the getdomainname function.  */
#define HAVE_GETDOMAINNAME 1

/* Define if you have the gethostname function.  */
#define HAVE_GETHOSTNAME 1

/* Define if you have the getpwnam function.  */
#define HAVE_GETPWNAM 1

/* Define if you have the getspnam function.  */
/* #undef HAVE_GETSPNAM */

/* Define if you have the gettimeofday function.  */
#define HAVE_GETTIMEOFDAY 1

/* Define if you have the gsskrb5_register_acceptor_identity function.  */
/* #undef HAVE_GSSKRB5_REGISTER_ACCEPTOR_IDENTITY */

/* Define if you have the jrand48 function.  */
#define HAVE_JRAND48 1

/* Define if you have the krb_get_err_text function.  */
/* #undef HAVE_KRB_GET_ERR_TEXT */

/* Define if you have the memcpy function.  */
#define HAVE_MEMCPY 1

/* Define if you have the mkdir function.  */
#define HAVE_MKDIR 1

/* Define if you have the select function.  */
#define HAVE_SELECT 1

/* Define if you have the socket function.  */
#define HAVE_SOCKET 1

/* Define if you have the strchr function.  */
#define HAVE_STRCHR 1

/* Define if you have the strdup function.  */
#define HAVE_STRDUP 1

/* Define if you have the strerror function.  */
#define HAVE_STRERROR 1

/* Define if you have the strspn function.  */
#define HAVE_STRSPN 1

/* Define if you have the strstr function.  */
#define HAVE_STRSTR 1

/* Define if you have the strtol function.  */
#define HAVE_STRTOL 1

/* Define if you have the syslog function.  */
#define HAVE_SYSLOG 1

/* Define if you have the <dirent.h> header file.  */
#define HAVE_DIRENT_H 1

/* Define if you have the <fcntl.h> header file.  */
#define HAVE_FCNTL_H 1

/* Define if you have the <inttypes.h> header file.  */
#define HAVE_INTTYPES_H 1

/* Define if you have the <limits.h> header file.  */
#define HAVE_LIMITS_H 1

/* Define if you have the <malloc.h> header file.  */
/* #undef HAVE_MALLOC_H */

/* Define if you have the <ndir.h> header file.  */
/* #undef HAVE_NDIR_H */

/* Define if you have the <paths.h> header file.  */
#define HAVE_PATHS_H 1

/* Define if you have the <strings.h> header file.  */
#define HAVE_STRINGS_H 1

/* Define if you have the <sys/dir.h> header file.  */
/* #undef HAVE_SYS_DIR_H */

/* Define if you have the <sys/file.h> header file.  */
#define HAVE_SYS_FILE_H 1

/* Define if you have the <sys/ndir.h> header file.  */
/* #undef HAVE_SYS_NDIR_H */

/* Define if you have the <sys/time.h> header file.  */
#define HAVE_SYS_TIME_H 1

/* Define if you have the <sys/uio.h> header file.  */
#define HAVE_SYS_UIO_H 1

/* Define if you have the <syslog.h> header file.  */
#define HAVE_SYSLOG_H 1

/* Define if you have the <unistd.h> header file.  */
#define HAVE_UNISTD_H 1

/* Define if you have the db library (-ldb).  */
/* #undef HAVE_LIBDB */

/* Define if you have the resolv library (-lresolv).  */
/* #undef HAVE_LIBRESOLV */

/* Name of package */
#define PACKAGE "cyrus-sasl"

/* Version number of package */
#define VERSION "2.1.1"

/* define if your compiler has __attribute__ */
#define HAVE___ATTRIBUTE__ 1


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
#ifndef DEV_RANDOM
#define DEV_RANDOM "/dev/random"
#endif

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
# include <sys/param.h>
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

#ifndef	NI_WITHSCOPEID
#define	NI_WITHSCOPEID	0
#endif

#endif /* CONFIG_H */
