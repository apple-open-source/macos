/* config.h--SASL configuration for win32
 * Ryan Troll
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

#include <windows.h>

/* Our package */
#define PACKAGE "cyrus-sasl"

/* Our version */
#define VERSION "2.1.17"

/* Visual Studio supports prototypes */
#define PROTOTYPES     1

typedef unsigned char   *caddr_t;

/* Registry key that contains the locations of the plugins */
#define SASL_ROOT_KEY "SOFTWARE\\Carnegie Mellon\\Project Cyrus\\SASL Library"
#define SASL_KEY_V1 SASL_ROOT_KEY "\\Available Plugins"
#define SASL_PATH_SUBKEY "SearchPath"

/* : This should probably be replaced with a call to a function
   : that gets the proper value from Registry */
#define SASL_DB_PATH "c:\\CMU\\sasldb2"

/* what db package are we using? */
/* #undef SASL_GDBM */
/* #undef SASL_NDBM */
#define SASL_BERKELEYDB 1

/* which mechs can we link staticly? */
#define STATIC_ANONYMOUS 1
#define STATIC_CRAMMD5 1
#define STATIC_DIGESTMD5 1
#define STATIC_GSSAPIV2 1
/* #undef STATIC_KERBEROS4 */
/* #undef STATIC_LOGIN */
/* #undef STATIC_MYSQL */
/* #undef STATIC_OTP */
#define STATIC_PLAIN 1
#define STATIC_SASLDB 1
/* #undef STATIC_SRP */

/* ------------------------------------------------------------ */

/* Things that are fetched via autoconf under Unix
 */
#define HAVE_MEMCPY 1

#define SASL_PATH_ENV_VAR "SASL_PATH"
#define PLUGINDIR "C:\\CMU\\bin\\sasl2"

/* Windows calls these functions something else
 */
#define strcasecmp   stricmp
#define snprintf    _snprintf
#define strncasecmp  strnicmp

#define MAXHOSTNAMELEN 1024

/* ------------------------------------------------------------ */

#define WITHOUT_NANA
#define L_DEFAULT_GUARD (0)
#define I_DEFAULT_GUARD (0)
#define I(foo)
#define VL(foo)  printf foo;
#define VLP(foo,bar)

/* we're not gcc */
#define __attribute__(foo)

/* : Same as in tpipv6.h */
#ifndef HAVE_SOCKLEN_T
typedef int socklen_t;
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

#include <time.h>

typedef int ssize_t;

#define HIER_DELIMITER '\\'

#endif /* CONFIG_H */
