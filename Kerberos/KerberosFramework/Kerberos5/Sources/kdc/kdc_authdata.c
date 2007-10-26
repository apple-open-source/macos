/*
 * Copyright (c) 2007 Apple Inc.  All Rights Reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *  * Neither the name of Apple Inc, nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS
 * IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 * PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER
 * OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * kdc/kdc_authdata.c
 *
 * AuthorizationData routines for the KDC.
 */

#include "k5-int.h"
#include "kdc_util.h"
#include "extern.h"
#include <stdio.h>
#include "adm_proto.h"

#include <syslog.h>

#include <assert.h>
#include "../include/krb5/authdata_plugin.h"

#if TARGET_OS_MAC
static const char *objdirs[] = { KRB5_AUTHDATA_PLUGIN_BUNDLE_DIR, LIBDIR "/krb5/plugins/authdata", NULL }; /* should be a list */
#else
static const char *objdirs[] = { LIBDIR "/krb5/plugins/authdata", NULL };
#endif

typedef krb5_error_code (*authdata_proc)
    (krb5_context, krb5_db_entry *client,
		    krb5_data *req_pkt,
		    krb5_kdc_req *request,
		    krb5_enc_tkt_part * enc_tkt_reply);

typedef krb5_error_code (*init_proc)
    (krb5_context, void **);
typedef void (*fini_proc)
    (krb5_context, void *);

typedef struct _krb5_authdata_systems {
    const char *name;
    int		type;
    int		flags;
    void       *plugin_context;
    init_proc   init;
    fini_proc   fini;
    authdata_proc	handle_authdata;
} krb5_authdata_systems;

static krb5_authdata_systems static_authdata_systems[] = {
    { "[end]", -1,}
};

static krb5_authdata_systems *authdata_systems;
static int n_authdata_systems;
static struct plugin_dir_handle authdata_plugins;

krb5_error_code
load_authdata_plugins(krb5_context context)
{
    struct errinfo err;
    void **authdata_plugins_ftables = NULL;
    struct krb5plugin_authdata_ftable_v0 *ftable = NULL;
    int module_count, i, k;
    init_proc server_init_proc = NULL;

    memset(&err, 0, sizeof(err));

    /* Attempt to load all of the authdata plugins we can find. */
    PLUGIN_DIR_INIT(&authdata_plugins);
    if (PLUGIN_DIR_OPEN(&authdata_plugins) == 0) {
	if (krb5int_open_plugin_dirs(objdirs, NULL,
				     &authdata_plugins, &err) != 0) {
	    return KRB5_PLUGIN_NO_HANDLE;
	}
    }

    /* Get the method tables provided by the loaded plugins. */
    authdata_plugins_ftables = NULL;
    n_authdata_systems = 0;
    if (krb5int_get_plugin_dir_data(&authdata_plugins,
				    "authdata_server_0",
				    &authdata_plugins_ftables, &err) != 0) {
	return KRB5_PLUGIN_NO_HANDLE;
    }

    /* Count the valid modules. */ 
    module_count = sizeof(static_authdata_systems)
		   / sizeof(static_authdata_systems[0]);
    if (authdata_plugins_ftables != NULL) {
		for (i = 0; authdata_plugins_ftables[i] != NULL; i++) {
			ftable = authdata_plugins_ftables[i];
			if ((ftable->authdata_proc != NULL)) {
				module_count++;
			}
		}
    }

    /* Build the complete list of supported authdata options, and
     * leave room for a terminator entry. */
    authdata_systems = calloc((module_count + 1), sizeof(krb5_authdata_systems) );
    if (authdata_systems == NULL) {
		krb5int_free_plugin_dir_data(authdata_plugins_ftables);
		return ENOMEM;
    }

    /* Add the locally-supplied mechanisms to the dynamic list first. */
    for (i = 0, k = 0;
	 i < sizeof(static_authdata_systems) / sizeof(static_authdata_systems[0]);
	 i++) {
			if (static_authdata_systems[i].type == -1)
				break;
			authdata_systems[k] = static_authdata_systems[i];
			/* Try to initialize the authdata system.  If it fails, we'll remove it
			 * from the list of systems we'll be using. */
			server_init_proc = static_authdata_systems[i].init;
			if ((server_init_proc != NULL) &&
				((*server_init_proc)(context, NULL /* &plugin_context */) != 0)) {
				memset(&authdata_systems[k], 0, sizeof(authdata_systems[k]));
				continue;
			}
			k++;
    }

    /* Now add the dynamically-loaded mechanisms to the list. */
    if (authdata_plugins_ftables != NULL) {
		for (i = 0; authdata_plugins_ftables[i] != NULL; i++) {
			ftable = authdata_plugins_ftables[i];
			if ((ftable->authdata_proc == NULL)) {
			continue;
			}
			server_init_proc = ftable->init_proc;
			krb5_error_code initerr;
			if ((server_init_proc != NULL) &&
				((initerr = (*server_init_proc)(context, NULL /* &plugin_context */)) != 0)) {
					const char *emsg;
					emsg = krb5_get_error_message(context, initerr);
					if (emsg) {
						krb5_klog_syslog(LOG_ERR,
							"authdata %s failed to initialize: %s",
							ftable->name, emsg);
						krb5_free_error_message(context, emsg);
					}
					memset(&authdata_systems[k], 0, sizeof(authdata_systems[k]));
		
					continue;
			}
	
			authdata_systems[k].name = ftable->name;
			authdata_systems[k].init = server_init_proc;
			authdata_systems[k].fini = ftable->fini_proc;
			authdata_systems[k].handle_authdata = ftable->authdata_proc;
			k++;
		}
    }
    n_authdata_systems = k;
    /* Add the end-of-list marker. */
    authdata_systems[k].name = "[end]";
	authdata_systems[k].type = -1;
    return 0;
}

krb5_error_code
unload_authdata_plugins(krb5_context context)
{
    int i;
    if (authdata_systems != NULL) {
	for (i = 0; i < n_authdata_systems; i++) {
	    if (authdata_systems[i].fini != NULL) {
	        (*authdata_systems[i].fini)(context,
					   authdata_systems[i].plugin_context);
	    }
	    memset(&authdata_systems[i], 0, sizeof(authdata_systems[i]));
	}
	free(authdata_systems);
	authdata_systems = NULL;
	n_authdata_systems = 0;
	krb5int_close_plugin_dirs(&authdata_plugins);
    }
    return 0;
}

krb5_error_code
handle_authdata (krb5_context context, krb5_db_entry *client, krb5_data *req_pkt,
	      krb5_kdc_req *request, krb5_enc_tkt_part *enc_tkt_reply)
{
    krb5_error_code retval = 0;
    krb5_authdata_systems *authdata_sys;
	int i;
	const char *emsg;
	
    krb5_klog_syslog (LOG_DEBUG, "handling authdata");

    for (authdata_sys = authdata_systems, i = 0; authdata_sys != NULL && i < n_authdata_systems; i++) {
      	if (authdata_sys[i].handle_authdata && authdata_sys[i].type != -1) {
			retval = authdata_sys[i].handle_authdata(context, client, req_pkt, request,
							   enc_tkt_reply);
			if (retval) {
				emsg = krb5_get_error_message (context, retval);
				krb5_klog_syslog (LOG_INFO, "authdata (%s) handling failure: %s",
						  authdata_sys[i].name, emsg);
				krb5_free_error_message (context, emsg);
			} else {
				krb5_klog_syslog (LOG_DEBUG, ".. .. ok");
			}
		}
    }

	return 0;
 }
