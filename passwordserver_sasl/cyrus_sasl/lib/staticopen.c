/* dlopen.c--Unix dlopen() dynamic loader interface
 * Rob Siemborski
 * Rob Earhart
 * $Id: staticopen.c,v 1.2 2002/05/22 17:56:56 snsimon Exp $
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
#include <stdlib.h>
#include <limits.h>
#include <sys/param.h>
#include <sasl.h>
#include "saslint.h"
#include "staticopen.h"

const int _is_sasl_server_static = 1;

/* gets the list of mechanisms */
int _sasl_load_plugins(const add_plugin_list_t *entrypoints,
                       const sasl_callback_t *getpath_callback __attribute__((unused)),
                       const sasl_callback_t *verifyfile_callback __attribute__((unused)))
{
    int result = SASL_OK;
    const add_plugin_list_t *cur_ep;
    int (*add_plugin)(const char *, void *);
    enum {
	UNKNOWN = 0, SERVER = 1, CLIENT = 2, AUXPROP = 3, CANONUSER = 4
    } type;

    for(cur_ep = entrypoints; cur_ep->entryname; cur_ep++) {

	/* What type of plugin are we looking for? */
	if(!strcmp(cur_ep->entryname, "sasl_server_plug_init")) {
	    type = SERVER;
	    add_plugin = (void *)sasl_server_add_plugin;
	} else if (!strcmp(cur_ep->entryname, "sasl_client_plug_init")) {
	    type = CLIENT;
	    add_plugin = (void *)sasl_client_add_plugin;
	} else if (!strcmp(cur_ep->entryname, "sasl_auxprop_plug_init")) {
	    type = AUXPROP;
	    add_plugin = (void *)sasl_auxprop_add_plugin;
	} else if (!strcmp(cur_ep->entryname, "sasl_canonuser_init")) {
	    type = CANONUSER;
	    add_plugin = (void *)sasl_canonuser_add_plugin;
	} else {
	    /* What are we looking for then? */
	    return SASL_FAIL;
	}

#ifdef STATIC_ANONYMOUS
	if(type == SERVER) {
	    result = (*add_plugin)("ANONYMOUS",
				   SPECIFIC_SERVER_PLUG_INIT( anonymous ));
	} else if (type == CLIENT) {
	    result = (*add_plugin)("ANONYMOUS",
				   SPECIFIC_CLIENT_PLUG_INIT( anonymous ));
	}
#endif

#ifdef STATIC_CRAMMD5
	if(type == SERVER) {
	    result = (*add_plugin)("CRAM-MD5",
				   SPECIFIC_SERVER_PLUG_INIT( crammd5 ));
	} else if (type == CLIENT) {
	    result = (*add_plugin)("CRAM-MD5",
				   SPECIFIC_CLIENT_PLUG_INIT( crammd5 ));
	}
#endif

#ifdef STATIC_DIGESTMD5
	if(type == SERVER) {
	    result = (*add_plugin)("DIGEST-MD5",
				   SPECIFIC_SERVER_PLUG_INIT( digestmd5 ));
	} else if (type == CLIENT) {
	    result = (*add_plugin)("DIGEST-MD5",
				   SPECIFIC_CLIENT_PLUG_INIT( digestmd5 ));
	}
#endif

#ifdef STATIC_GSSAPIV2
	if(type == SERVER) {
	    result = (*add_plugin)("GSSAPI",
				   SPECIFIC_SERVER_PLUG_INIT( gssapiv2 ));
	} else if (type == CLIENT) {
	    result = (*add_plugin)("GSSAPI",
				   SPECIFIC_CLIENT_PLUG_INIT( gssapiv2 ));
	}
#endif

#ifdef STATIC_KERBEROS4
	if(type == SERVER) {
	    result = (*add_plugin)("KERBEROS_V4",
				   SPECIFIC_SERVER_PLUG_INIT( kerberos4 ));
	} else if (type == CLIENT) {
	    result = (*add_plugin)("KERBEROS_V4",
				   SPECIFIC_CLIENT_PLUG_INIT( kerberos4 ));
	}
#endif

#ifdef STATIC_LOGIN
	if(type == SERVER) {
	    result = (*add_plugin)("LOGIN",
				   SPECIFIC_SERVER_PLUG_INIT( login ));
	} else if (type == CLIENT) {
	    result = (*add_plugin)("LOGIN",
				   SPECIFIC_CLIENT_PLUG_INIT( login ));
	}
#endif

#ifdef STATIC_OTP
	if(type == SERVER) {
	    result = (*add_plugin)("OTP",
				   SPECIFIC_SERVER_PLUG_INIT( otp ));
	} else if (type == CLIENT) {
	    result = (*add_plugin)("OTP",
				   SPECIFIC_CLIENT_PLUG_INIT( otp ));
	}
#endif

#ifdef STATIC_PLAIN
	if(type == SERVER) {
	    result = (*add_plugin)("PLAIN",
				   SPECIFIC_SERVER_PLUG_INIT( plain ));
	} else if (type == CLIENT) {
	    result = (*add_plugin)("PLAIN",
				   SPECIFIC_CLIENT_PLUG_INIT( plain ));
	}
#endif

#ifdef STATIC_SRP
	if(type == SERVER) {
	    result = (*add_plugin)("SRP", SPECIFIC_SERVER_PLUG_INIT( srp ));
	} else if (type == CLIENT) {
	    result = (*add_plugin)("SRP", SPECIFIC_CLIENT_PLUG_INIT( srp ));
	}
#endif

#ifdef STATIC_SASLDB
	if(type == AUXPROP) {
	    result = (*add_plugin)("SASLDB",
				   SPECIFIC_AUXPROP_PLUG_INIT( sasldb ));
	}
#endif

    }
    
    return SASL_OK;
}


/* loads a single mechanism (or rather, fails to) */
int _sasl_get_plugin(const char *file __attribute__((unused)),
		     const sasl_callback_t *verifyfile_cb __attribute__((unused)),
		     void **libraryptr __attribute__((unused)))
{
    return SASL_FAIL;
}

/* fails to locate an entry point ;) */
int _sasl_locate_entry(void *library __attribute__((unused)),
		       const char *entryname __attribute__((unused)),
                       void **entry_point __attribute__((unused)))
{
    return SASL_FAIL;
}


int
_sasl_done_with_plugins() 
{
    return SASL_OK;
}
