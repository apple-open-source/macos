/* staticopen.h
 * Rob Siemborski
 * $Id: staticopen.h,v 1.2 2002/05/22 17:56:56 snsimon Exp $
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

#include <config.h>
#ifndef __hpux
#include <dlfcn.h>
#endif /* !__hpux */
#include <stdlib.h>
#include <limits.h>
#include <sys/param.h>
#include <sasl.h>
#include "saslint.h"

/* For static linking */
#define SPECIFIC_CLIENT_PLUG_INIT_PROTO( x ) \
int x##_client_plug_init(const sasl_utils_t *utils, \
                         int maxversion, int *out_version, \
			 sasl_client_plug_t **pluglist, \
                         int *plugcount, \
                         const char *plugname)

#define SPECIFIC_SERVER_PLUG_INIT_PROTO( x ) \
int x##_server_plug_init(const sasl_utils_t *utils, \
                         int maxversion, int *out_version, \
			 sasl_server_plug_t **pluglist, \
                         int *plugcount, \
                         const char *plugname)

#define SPECIFIC_AUXPROP_PLUG_INIT_PROTO( x ) \
int x##_auxprop_plug_init(const sasl_utils_t *utils, \
		     	  int maxversion, int *out_version, \
		     	  sasl_auxprop_plug_t **plug, \
			  const char *plugname)

/* Static Compillation Foo */
#define SPECIFIC_CLIENT_PLUG_INIT( x ) x##_client_plug_init
#define SPECIFIC_SERVER_PLUG_INIT( x ) x##_server_plug_init
#define SPECIFIC_AUXPROP_PLUG_INIT( x ) x##_auxprop_plug_init

#ifdef STATIC_ANONYMOUS
extern SPECIFIC_SERVER_PLUG_INIT_PROTO( anonymous );
extern SPECIFIC_CLIENT_PLUG_INIT_PROTO( anonymous );
#endif
#ifdef STATIC_CRAMMD5
extern SPECIFIC_SERVER_PLUG_INIT_PROTO( crammd5 );
extern SPECIFIC_CLIENT_PLUG_INIT_PROTO( crammd5 );
#endif
#ifdef STATIC_DIGESTMD5
extern SPECIFIC_SERVER_PLUG_INIT_PROTO( digestmd5 );
extern SPECIFIC_CLIENT_PLUG_INIT_PROTO( digestmd5 );
#endif
#ifdef STATIC_GSSAPIV2
extern SPECIFIC_SERVER_PLUG_INIT_PROTO( gssapiv2 );
extern SPECIFIC_CLIENT_PLUG_INIT_PROTO( gssapiv2 );
#endif
#ifdef STATIC_KERBEROS4
extern SPECIFIC_SERVER_PLUG_INIT_PROTO( kerberos4 );
extern SPECIFIC_CLIENT_PLUG_INIT_PROTO( kerberos4 );
#endif
#ifdef STATIC_LOGIN
extern SPECIFIC_SERVER_PLUG_INIT_PROTO( login );
extern SPECIFIC_CLIENT_PLUG_INIT_PROTO( login );
#endif
#ifdef STATIC_PLAIN
extern SPECIFIC_SERVER_PLUG_INIT_PROTO( plain );
extern SPECIFIC_CLIENT_PLUG_INIT_PROTO( plain );
#endif
#ifdef STATIC_SRP
extern SPECIFIC_SERVER_PLUG_INIT_PROTO( srp );
extern SPECIFIC_CLIENT_PLUG_INIT_PROTO( srp );
#endif
#ifdef STATIC_OTP
extern SPECIFIC_SERVER_PLUG_INIT_PROTO( otp );
extern SPECIFIC_CLIENT_PLUG_INIT_PROTO( otp );
#endif
#ifdef STATIC_SASLDB
extern SPECIFIC_AUXPROP_PLUG_INIT_PROTO( sasldb );
#endif
