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

#include "ssh.h"
#include "ssh2.h"
#include "xmalloc.h"
#include "buffer.h"
#include "bufaux.h"
#include "packet.h"
#include "compat.h"
#include <openssl/evp.h>
#include "cipher.h"
#include "kex.h"
#include "auth.h"
#include "log.h"
#include "channels.h"
#include "session.h"
#include "dispatch.h"
#include "servconf.h"
#include "compat.h"
#include "monitor_wrap.h"

#include "ssh-gss.h"

extern ServerOptions options;
extern u_char *session_id2;
extern int session_id2_len;

static ssh_gssapi_client gssapi_client =
	{ {0,NULL}, GSS_C_NO_CREDENTIAL, NULL, {NULL,NULL,NULL}};

ssh_gssapi_mech gssapi_null_mech 
  = {NULL, NULL, {0, NULL}, NULL, NULL, NULL, NULL};

#ifdef KRB5
extern ssh_gssapi_mech gssapi_kerberos_mech;
extern ssh_gssapi_mech gssapi_kerberos_mech_old;
#endif
#ifdef GSI
extern ssh_gssapi_mech gssapi_gsi_mech;
extern ssh_gssapi_mech gssapi_gsi_mech_old;
#endif

ssh_gssapi_mech* supported_mechs[]= {
#ifdef KRB5
  &gssapi_kerberos_mech,
  &gssapi_kerberos_mech_old, /* Support for legacy clients */
#endif
#ifdef GSI
  &gssapi_gsi_mech,
  &gssapi_gsi_mech_old,	/* Support for legacy clients */
#endif
  &gssapi_null_mech,
};

/* Return a list of the gss-group1-sha1-x mechanisms supported by this
 * program.
 *
 * We only support the mechanisms that we've indicated in the list above,
 * but we check that they're supported by the GSSAPI mechanism on the 
 * machine. We also check, before including them in the list, that
 * we have the necesary information in order to carry out the key exchange
 * (that is, that the user has credentials, the server's creds are accessible,
 * etc)
 *
 * The way that this is done is fairly nasty, as we do a lot of work that
 * is then thrown away. This should possibly be implemented with a cache
 * that stores the results (in an expanded Gssctxt structure), which are
 * then used by the first calls if that key exchange mechanism is chosen.
 */

/* Unpriviledged */ 
char * 
ssh_gssapi_server_mechanisms() {
	gss_OID_set 	supported;
	Gssctxt		*ctx = NULL;
	OM_uint32	maj_status, min_status;
	Buffer		buf;
	int 		i = 0;
	int		first = 0;
	int		present;
	char *		mechs;

	if (datafellows & SSH_OLD_GSSAPI) return NULL;
	
	ssh_gssapi_supported_oids(&supported);
	
	buffer_init(&buf);

	while(supported_mechs[i]->name != NULL) {
		if ((maj_status=gss_test_oid_set_member(&min_status,
						   	&supported_mechs[i]->oid,
						   	supported,
						   	&present))) {
			present=0;
		}

		if (present) {
		    if (!GSS_ERROR(PRIVSEP(ssh_gssapi_server_ctx(&ctx,
					    &supported_mechs[i]->oid)))) {
			/* Append gss_group1_sha1_x to our list */
			if (first++!=0)
				buffer_put_char(&buf,',');
			buffer_append(&buf, KEX_GSS_SHA1,
				      sizeof(KEX_GSS_SHA1)-1);
	        	buffer_append(&buf, 
	        		      supported_mechs[i]->enc_name,
        	      		      strlen(supported_mechs[i]->enc_name));
			debug("GSSAPI mechanism %s (%s%s) supported",
			      supported_mechs[i]->name, KEX_GSS_SHA1,
			      supported_mechs[i]->enc_name);
		    } else {
			debug("no credentials for GSSAPI mechanism %s",
			      supported_mechs[i]->name);
		    }
        	} else {
		    debug("GSSAPI mechanism %s not supported",
			  supported_mechs[i]->name);
        	}
        	ssh_gssapi_delete_ctx(&ctx);
        	i++;
	}
	
	buffer_put_char(&buf,'\0');
	
	mechs=xmalloc(buffer_len(&buf));
	buffer_get(&buf,mechs,buffer_len(&buf));
	buffer_free(&buf);
	if (strlen(mechs)==0)
	   return(NULL);
	else
	   return(mechs);
}

/* Unpriviledged */
void ssh_gssapi_supported_oids(gss_OID_set *oidset) {
	int i =0;
	OM_uint32 maj_status,min_status;
	int present;
	gss_OID_set supported;
	
	gss_create_empty_oid_set(&min_status,oidset);
	PRIVSEP(gss_indicate_mechs(&min_status, &supported));

	while (supported_mechs[i]->name!=NULL) {
		if ((maj_status=gss_test_oid_set_member(&min_status,
						       &supported_mechs[i]->oid,
						       supported,
						       &present))) {
			present=0;
		}
		if (present) {
			gss_add_oid_set_member(&min_status,
					       &supported_mechs[i]->oid,
				       	       oidset);	
		}
		i++;
	}
}	

/* Find out which GSS type (out of the list we define in ssh-gss.h) a
 * particular connection is using 
 */

/* Priviledged (called ssh_gssapi_accept_ctx -> ssh_gssapi_getclient ->) */
ssh_gssapi_mech *
ssh_gssapi_get_ctype(Gssctxt *ctxt) {
	int i=0;
	
	while(supported_mechs[i]->name!=NULL) {
	    if (supported_mechs[i]->oid.length == ctxt->oid->length &&
	      	(memcmp(supported_mechs[i]->oid.elements,
			ctxt->oid->elements,ctxt->oid->length)==0)) {
		return supported_mechs[i];
	    }
	   i++;
	}
	return NULL;
}

/* Return the OID that corresponds to the given context name */
 
/* Unpriviledged */
gss_OID 
ssh_gssapi_server_id_kex(char *name) {
  int i=0;
  
  if (strncmp(name, KEX_GSS_SHA1, sizeof(KEX_GSS_SHA1)-1) !=0) {
     return(NULL);
  }
  
  name+=sizeof(KEX_GSS_SHA1)-1; /* Move to the start of the MIME string */
  
  while (supported_mechs[i]->name!=NULL &&
  	 strcmp(name,supported_mechs[i]->enc_name)!=0) {
  	i++;
  }

  if (supported_mechs[i]->name==NULL)
     return (NULL);

  debug("using GSSAPI mechanism %s (%s%s)", supported_mechs[i]->name,
	KEX_GSS_SHA1, supported_mechs[i]->enc_name);

  return &supported_mechs[i]->oid;
}

/* Wrapper around accept_sec_context
 * Requires that the context contains:
 *    oid		
 *    credentials	(from ssh_gssapi_acquire_cred)
 */
/* Priviledged */
OM_uint32 ssh_gssapi_accept_ctx(Gssctxt *ctx,gss_buffer_desc *recv_tok,
				gss_buffer_desc *send_tok, OM_uint32 *flags) 
{
	OM_uint32 status;
	gss_OID mech;
	
	ctx->major=gss_accept_sec_context(&ctx->minor,
					  &ctx->context,
					  ctx->creds,
					  recv_tok,
					  GSS_C_NO_CHANNEL_BINDINGS,
					  &ctx->client,
					  &mech, /* read-only pointer */
					  send_tok,
					  flags,
					  NULL,
					  &ctx->client_creds);
	if (GSS_ERROR(ctx->major)) {
		ssh_gssapi_error(ctx);
	}
	
	if (ctx->client_creds) {
		debug("Received some client credentials");
	} else {
		debug("Got no client credentials");
	}

	/* FIXME: We should check that the me
	 * the one that we asked for (in ctx->oid) */

	status=ctx->major;
	
	/* Now, if we're complete and we have the right flags, then
	 * we flag the user as also having been authenticated
	 */
	
	if (((flags==NULL) || ((*flags & GSS_C_MUTUAL_FLAG) && 
	                       (*flags & GSS_C_INTEG_FLAG))) &&
	    (ctx->major == GSS_S_COMPLETE)) {
		if (ssh_gssapi_getclient(ctx,&gssapi_client.mech,
	  			         &gssapi_client.name,
	  			         &gssapi_client.creds))
	  		fatal("Couldn't convert client name");
	}

	/* Make sure that the getclient call hasn't stamped on this */
	return(status);
}

/* Extract the client details from a given context. This can only reliably
 * be called once for a context */

/* Priviledged (called from accept_secure_ctx) */
OM_uint32 
ssh_gssapi_getclient(Gssctxt *ctx, ssh_gssapi_mech **type,
		     gss_buffer_desc *name, gss_cred_id_t *creds) {

	*type=ssh_gssapi_get_ctype(ctx);
	if ((ctx->major=gss_display_name(&ctx->minor,ctx->client,name,NULL))) {
		ssh_gssapi_error(ctx);
		return(ctx->major);
	}
	
	/* This is icky. There appears to be no way to copy this structure,
	 * rather than the pointer to it, so we simply copy the pointer and
	 * mark the originator as empty so we don't destroy it. 
	 */
	*creds=ctx->client_creds;
	ctx->client_creds=GSS_C_NO_CREDENTIAL;
	return(ctx->major);
}

/* As user - called through fatal cleanup hook */
void
ssh_gssapi_cleanup_creds(void *ignored)
{
	if (gssapi_client.store.filename!=NULL) {
		/* Unlink probably isn't sufficient */
		debug("removing gssapi cred file\"%s\"",gssapi_client.store.filename);
		unlink(gssapi_client.store.filename);
	}
}

/* As user */
void 
ssh_gssapi_storecreds()
{
	if (gssapi_client.mech && gssapi_client.mech->storecreds) {
		(*gssapi_client.mech->storecreds)(&gssapi_client);
		if (options.gss_cleanup_creds) {
			fatal_add_cleanup(ssh_gssapi_cleanup_creds, NULL);
		}
	} else {
		debug("ssh_gssapi_storecreds: Not a GSSAPI mechanism");
	}
}

/* This allows GSSAPI methods to do things to the childs environment based
 * on the passed authentication process and credentials.
 */
/* As user */
void 
ssh_gssapi_do_child(char ***envp, u_int *envsizep) 
{

	if (gssapi_client.store.envvar!=NULL && 
	    gssapi_client.store.envval!=NULL) {
	    
		debug("Setting %s to %s", gssapi_client.store.envvar,
					  gssapi_client.store.envval);				  
		child_set_env(envp, envsizep, gssapi_client.store.envvar, 
					      gssapi_client.store.envval);
	}
}

/* Priviledged */
int
ssh_gssapi_userok(char *user)
{
	if (gssapi_client.name.length==0 || 
	    gssapi_client.name.value==NULL) {
		debug("No suitable client data");
		return 0;
	}
	if (gssapi_client.mech && gssapi_client.mech->userok) {
		return((*gssapi_client.mech->userok)(&gssapi_client,user));
	} else {
		debug("ssh_gssapi_userok: Unknown GSSAPI mechanism");
	}
	return(0);
}

/* Priviledged */
int
ssh_gssapi_localname(char **user)
{
    	*user = NULL;
	if (gssapi_client.name.length==0 || 
	    gssapi_client.name.value==NULL) {
		debug("No suitable client data");
		return(0);;
	}
	if (gssapi_client.mech && gssapi_client.mech->localname) {
		return((*gssapi_client.mech->localname)(&gssapi_client,user));
	} else {
		debug("Unknown client authentication type");
	}
	return(0);
}
#endif
