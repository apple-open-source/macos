#include <stdio.h>

@TOP@

/* Symbols that need defining */
/* do we have gssapi.h or gssapi/gssapi.h? */
#undef HAVE_GSSAPI_H

/* define if your compile has __attribute__ */
#undef HAVE___ATTRIBUTE__

/* what flavor of GSSAPI are we using? */
#undef HAVE_GSS_C_NT_HOSTBASED_SERVICE

/* Do we have kerberos for plaintext password checking? */
#undef HAVE_KRB

/* do we have SIA for plaintext password checking? */
#undef HAVE_SIA

/* do we have ldap support */
#undef HAVE_LDAP

/* do we want to enable the GSSAPI module */
#undef AUTH_KRB5
#undef KRB5_HEIMDAL

/* Do we want to enable the experimental sasldb authentication module? */
#undef AUTH_SASLDB

/* do we have a getuserpw? */
#undef HAVE_GETUSERPW

/* do we have a getspnam? */
#undef HAVE_GETSPNAM

/* Path to saslauthd rundir */
#undef PATH_SASLAUTHD_RUNDIR

/* do we have pam? */
#undef HAVE_PAM

/* do we have a sys/sio.h? */
#undef HAVE_SYS_UIO_H

/* Things SASLAUTHd doesn't really care about */
#undef HAVE_SASLAUTHD
#undef WITH_DES
#undef WITH_SSL_DES
#undef STATIC_GSSAPIV2
#undef STATIC_KERBEROS4
#undef STATIC_PLAIN
#undef SASL_BERKELEYDB
#undef SASL_DB_PATH
#undef SASL_GDBM
#undef SASL_NDBM
#undef STATIC_SASLDB

/* define if your system has getnameinfo() */
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

@BOTTOM@

#ifndef HAVE___ATTRIBUTE__
/* Can't use attributes... */
#define __attribute__(foo)
#endif

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
#define _SS_MAXSIZE     128     /* Implementation specific max size */
#define _SS_PADSIZE     (_SS_MAXSIZE - sizeof (struct sockaddr))

struct sockaddr_storage {
        struct  sockaddr ss_sa;
        char            __ss_pad2[_SS_PADSIZE];
};
# define ss_family ss_sa.sa_family
#endif /* !HAVE_STRUCT_SOCKADDR_STORAGE */

#ifndef AF_INET6
/* Define it to something that should never appear */
#define AF_INET6        AF_MAX
#endif

/* Create a struct iovec if we need one */
#if !defined(HAVE_SYS_UIO_H)
struct iovec {
    long iov_len;
    char *iov_base;
};
#else
#include <sys/types.h>
#include <sys/uio.h>
#endif

#ifndef HAVE_GETADDRINFO
#define getaddrinfo     sasl_getaddrinfo
#define freeaddrinfo    sasl_freeaddrinfo
#define getnameinfo     sasl_getnameinfo
#define gai_strerror    sasl_gai_strerror
#include "gai.h"
#endif

#ifndef NI_WITHSCOPEID
#define NI_WITHSCOPEID  0
#endif

