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
#ifdef KRB5

#include "auth.h"
#include "auth-pam.h"
#include "xmalloc.h"
#include "log.h"
#include "servconf.h"

#include "ssh-gss.h"

extern ServerOptions options;

#ifdef HEIMDAL
#include <krb5.h>
#else
#include <gssapi/gssapi_krb5.h>
#define krb5_get_err_text(context,code) error_message(code)
#endif

static krb5_context krb_context = NULL;

/* Initialise the krb5 library, so we can use it for those bits that
 * GSSAPI won't do */

static int 
ssh_gssapi_krb5_init() {
	krb5_error_code problem;
	
	if (krb_context !=NULL)
		return 1;
		
	problem = krb5_init_context(&krb_context);
	if (problem) {
		log("Cannot initialize krb5 context");
		return 0;
	}
#ifndef __APPLE__ /* not required on OS X */
	krb5_init_ets(krb_context);
#endif

	return 1;	
}			

/* Check if this user is OK to login. This only works with krb5 - other 
 * GSSAPI mechanisms will need their own.
 * Returns true if the user is OK to log in, otherwise returns 0
 */

static int
ssh_gssapi_krb5_userok(ssh_gssapi_client *client, char *name) {
	krb5_principal princ;
	int retval;

	if (ssh_gssapi_krb5_init() == 0)
		return 0;
		
	if ((retval=krb5_parse_name(krb_context, client->name.value, 
				    &princ))) {
		log("krb5_parse_name(): %.100s", 
			krb5_get_err_text(krb_context,retval));
		return 0;
	}
	if (krb5_kuserok(krb_context, princ, name)) {
		retval = 1;
		log("Authorized to %s, krb5 principal %s (krb5_kuserok)",name,
		    (char *)client->name.value);
	}
	else
		retval = 0;
	
	krb5_free_principal(krb_context, princ);
	return retval;
}

/* Retrieve the local username associated with a set of Kerberos 
 * credentials. Hopefully we can use this for the 'empty' username
 * logins discussed in the draft  */
static int
ssh_gssapi_krb5_localname(ssh_gssapi_client *client, char **user) {
	krb5_principal princ;
	int retval;
	
	if (ssh_gssapi_krb5_init() == 0)
		return 0;

	if ((retval=krb5_parse_name(krb_context, client->name.value, 
				    &princ))) {
		log("krb5_parse_name(): %.100s", 
			krb5_get_err_text(krb_context,retval));
		return 0;
	}
	
	/* We've got to return a malloc'd string */
	*user = (char *)xmalloc(256);
	if (krb5_aname_to_localname(krb_context, princ, 256, *user)) {
		xfree(*user);
		*user = NULL;
		return(0);
	}
	
	return(1);
}
	
/* Make sure that this is called _after_ we've setuid to the user */

/* This writes out any forwarded credentials. Its specific to the Kerberos
 * GSSAPI mechanism
 *
 * We assume that our caller has made sure that the user has selected
 * delegated credentials, and that the client_creds structure is correctly
 * populated.
 */

static void
ssh_gssapi_krb5_storecreds(ssh_gssapi_client *client) {
	krb5_ccache ccache;
	krb5_error_code problem;
	krb5_principal princ;
	char ccname[35];
	static char name[40];
	int tmpfd;
	OM_uint32 maj_status,min_status;

	if (client->creds==NULL) {
		debug("No credentials stored"); 
		return;
	}
		
	if (ssh_gssapi_krb5_init() == 0)
		return;

#ifdef USE_CCAPI
	snprintf(ccname, sizeof(ccname), "krb5cc_%d", geteuid());
	snprintf(name, sizeof(name), "API:%s", ccname);
	debug ("Using ccache '%s' for principal '%s'", name, client->name.value);
#else
	if (options.gss_use_session_ccache) {
        	snprintf(ccname,sizeof(ccname),"/tmp/krb5cc_%d_XXXXXX",geteuid());
       
        	if ((tmpfd = mkstemp(ccname))==-1) {
                	log("mkstemp(): %.100s", strerror(errno));
                	return;
        	}
	        if (fchmod(tmpfd, S_IRUSR | S_IWUSR) == -1) {
	               	log("fchmod(): %.100s", strerror(errno));
	               	close(tmpfd);
	               	return;
	        }
        } else {
        	snprintf(ccname,sizeof(ccname),"/tmp/krb5cc_%d",geteuid());

        	tmpfd = open(ccname, O_TRUNC | O_CREAT, S_IRUSR | S_IWUSR);
        	if (tmpfd == -1) {
        		log("open(): %.100s", strerror(errno));
        		return;
        	}
        }

       	close(tmpfd);
        snprintf(name, sizeof(name), "FILE:%s",ccname);
#endif 
 
        if ((problem = krb5_cc_resolve(krb_context, name, &ccache))) {
                log("krb5_cc_resolve(): %.100s", 
                	krb5_get_err_text(krb_context,problem));
                return;
        }

	if ((problem = krb5_parse_name(krb_context, client->name.value, 
				       &princ))) {
		log("krb5_parse_name(): %.100s", 
			krb5_get_err_text(krb_context,problem));
		krb5_cc_destroy(krb_context,ccache);
		return;
	}
	
	if ((problem = krb5_cc_initialize(krb_context, ccache, princ))) {
		log("krb5_cc_initialize(): %.100s", 
			krb5_get_err_text(krb_context,problem));
		krb5_free_principal(krb_context,princ);
		krb5_cc_destroy(krb_context,ccache);
		return;
	}
	
	krb5_free_principal(krb_context,princ);

	#ifdef HEIMDAL
	if ((problem = krb5_cc_copy_cache(krb_context, 
					   client->creds->ccache,
					   ccache))) {
		log("krb5_cc_copy_cache(): %.100s", 
			krb5_get_err_text(krb_context,problem));
		krb5_cc_destroy(krb_context,ccache);
		return;
	}
	#else
	if ((maj_status = gss_krb5_copy_ccache(&min_status, 
					       client->creds, 
					       ccache))) {
		log("gss_krb5_copy_ccache() failed");
		krb5_cc_destroy(krb_context,ccache);
		return;
	}
	#endif
	
	krb5_cc_close(krb_context,ccache);

#ifdef USE_PAM
	do_pam_putenv("KRB5CCNAME",name);
#endif

	client->store.filename=strdup(ccname);
	client->store.envvar="KRB5CCNAME";
	client->store.envval=strdup(name);

	return;
}

/* We've been using a wrongly encoded mechanism ID for yonks */

ssh_gssapi_mech gssapi_kerberos_mech_old = {
	"Se3H81ismmOC3OE+FwYCiQ==",
	"Kerberos",
	{9, "\x2A\x86\x48\x86\xF7\x12\x01\x02\x02"},
	&ssh_gssapi_krb5_init,
	&ssh_gssapi_krb5_userok,
	&ssh_gssapi_krb5_localname,
	&ssh_gssapi_krb5_storecreds
};

ssh_gssapi_mech gssapi_kerberos_mech = {
	"toWM5Slw5Ew8Mqkay+al2g==",
	"Kerberos",
	{9, "\x2A\x86\x48\x86\xF7\x12\x01\x02\x02"},
	NULL,
	&ssh_gssapi_krb5_userok,
	&ssh_gssapi_krb5_localname,
	&ssh_gssapi_krb5_storecreds
};
	
#endif /* KRB5 */

#endif /* GSSAPI */
