/*
 * Copyright (c) 2001-2003 Simon Wilkinson. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR `AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "includes.h"

#ifdef GSSAPI
#ifdef GSI

#include "auth.h"
#include "auth-pam.h"
#include "xmalloc.h"
#include "log.h"
#include "servconf.h"

#include "ssh-gss.h"

#include <globus_gss_assist.h>

static int ssh_gssapi_gsi_userok(ssh_gssapi_client *client, char *name);
static int ssh_gssapi_gsi_localname(ssh_gssapi_client *client, char **user);
static void ssh_gssapi_gsi_storecreds(ssh_gssapi_client *client);

ssh_gssapi_mech gssapi_gsi_mech_old = {
	"N3+k7/4wGxHyuP8Yxi4RhA==",
	"GSI",
	{9, "\x2B\x06\x01\x04\x01\x9B\x50\x01\x01"},
	NULL,
	&ssh_gssapi_gsi_userok,
	&ssh_gssapi_gsi_localname,
	&ssh_gssapi_gsi_storecreds
};

ssh_gssapi_mech gssapi_gsi_mech = {
	"dZuIebMjgUqaxvbF7hDbAw==",
	"GSI",
	{9, "\x2B\x06\x01\x04\x01\x9B\x50\x01\x01"},
	NULL,
	&ssh_gssapi_gsi_userok,
	&ssh_gssapi_gsi_localname,
	&ssh_gssapi_gsi_storecreds
};

/*
 * Check if this user is OK to login under GSI. User has been authenticated
 * as identity in global 'client_name.value' and is trying to log in as passed
 * username in 'name'.
 *
 * Returns non-zero if user is authorized, 0 otherwise.
 */
static int
ssh_gssapi_gsi_userok(ssh_gssapi_client *client, char *name)
{
    int authorized = 0;
    
#ifdef GLOBUS_GSI_GSS_ASSIST_MODULE
    if (globus_module_activate(GLOBUS_GSI_GSS_ASSIST_MODULE) != 0) {
	return 0;
    }
#endif

    /* This returns 0 on success */
    authorized = (globus_gss_assist_userok(client->name.value,
					   name) == 0);
    
    log("GSI user %s is%s authorized as target user %s",
	(char *) client->name.value, (authorized ? "" : " not"), name);
    
    return authorized;
}

/*
 * Return the local username associated with the GSI credentials.
 */
int
ssh_gssapi_gsi_localname(ssh_gssapi_client *client, char **user)
{
#ifdef GLOBUS_GSI_GSS_ASSIST_MODULE
    if (globus_module_activate(GLOBUS_GSI_GSS_ASSIST_MODULE) != 0) {
	return 0;
    }
#endif
    return(globus_gss_assist_gridmap(client->name.value, user) == 0);
}

/*
 * Export GSI credentials to disk.
 */
static void
ssh_gssapi_gsi_storecreds(ssh_gssapi_client *client)
{
	OM_uint32	major_status;
	OM_uint32	minor_status;
	gss_buffer_desc	export_cred = GSS_C_EMPTY_BUFFER;
	char *		p;
	
	if (!client || !client->creds) {
	    return;
	}
	
	major_status = gss_export_cred(&minor_status,
					 client->creds,
				       GSS_C_NO_OID,
				       1,
				       &export_cred);
	if (GSS_ERROR(major_status) && major_status != GSS_S_UNAVAILABLE) {
	    Gssctxt *ctx;
	    ssh_gssapi_build_ctx(&ctx);
	    ctx->major = major_status;
	    ctx->minor = minor_status;
	    ssh_gssapi_set_oid(ctx, &gssapi_gsi_mech.oid);
	    ssh_gssapi_error(ctx);
	    ssh_gssapi_delete_ctx(&ctx);
				return;
			}
	
	p = strchr((char *) export_cred.value, '=');
	if (p == NULL) {
	    log("Failed to parse exported credentials string '%.100s'",
		(char *)export_cred.value);
	    gss_release_buffer(&minor_status, &export_cred);
	    return;
			}
	*p++ = '\0';
	if (strcmp((char *)export_cred.value,"X509_USER_DELEG_PROXY") == 0) {
	    client->store.envvar = strdup("X509_USER_PROXY");
	} else {
	    client->store.envvar = strdup((char *)export_cred.value);
		}
	client->store.envval = strdup(p);
#ifdef USE_PAM
	do_pam_putenv(client->store.envvar, client->store.envval);
#endif
	if (strncmp(p, "FILE:", 5) == 0) {
	    p += 5;
		}
	if (access(p, R_OK) == 0) {
	    client->store.filename = strdup(p);
	}	
	gss_release_buffer(&minor_status, &export_cred);
}

#endif /* GSI */
#endif /* GSSAPI */
