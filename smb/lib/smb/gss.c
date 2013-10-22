/*
 * Copyright (c) 2010 - 2011  Apple Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */

#include <errno.h>
#include <dispatch/dispatch.h>
#include <smb_lib.h>
#include <smb_conn.h>
#include "gss.h"
#include <GSS/gssapi_spi.h>
#include <Heimdal/krb5.h>
#include <KerberosHelper/KerberosHelper.h>
#include <KerberosHelper/NetworkAuthenticationHelper.h>


#define MAX_GSS_HOSTBASE_NAME (SMB_MAX_DNS_SRVNAMELEN + 5 + 1)

static void
acquire_cred_complete(void *ctx, OM_uint32 maj, gss_status_id_t status __unused,
	 gss_cred_id_t creds, gss_OID_set oids, OM_uint32 time_rec __unused)
{
	uint32_t min;
	struct smb_gss_cred_ctx *aq_cred_ctx = ctx;

	gss_release_oid_set(&min, &oids);
	aq_cred_ctx->creds = creds;
	aq_cred_ctx->maj = maj;
	dispatch_semaphore_signal(aq_cred_ctx->sem);
}

static int
smb_acquire_cred(const char *user, const char *domain, const char *password, 
				 gss_OID mech, void **gssCreds)
{
	gss_auth_identity_desc identity;
	struct smb_gss_cred_ctx aq_cred_ctx;
	uint32_t maj = !GSS_S_COMPLETE;

	if (password == NULL || user == NULL || *user == '\0') 
		return 0;
		
	identity.type = GSS_AUTH_IDENTITY_TYPE_1;
	identity.flags = 0;
	identity.username = strdup(user);
	identity.realm = strdup(domain ? domain : "");
	identity.password = strdup(password);
	identity.credentialsRef = NULL;
	
	if (identity.username == NULL ||
	    identity.realm == NULL || 
	    identity.password == NULL)
	    goto out;
	
	aq_cred_ctx.sem = dispatch_semaphore_create(0);
	if (aq_cred_ctx.sem == NULL)
		goto out;

	maj = gss_acquire_cred_ex_f(NULL,
				    GSS_C_NO_NAME,
				    0,
				    GSS_C_INDEFINITE,
				    mech,
				    GSS_C_INITIATE,
				    &identity,
				    &aq_cred_ctx,
				    acquire_cred_complete);
	
	if (maj == GSS_S_COMPLETE) {
		dispatch_semaphore_wait(aq_cred_ctx.sem, DISPATCH_TIME_FOREVER);
		maj = aq_cred_ctx.maj;
		*gssCreds = aq_cred_ctx.creds;
	}
	
	if (maj != GSS_S_COMPLETE)
		smb_log_info("Acquiring NTLM creds for %s\%s failed. GSS returned %d",
			ASL_LEVEL_INFO, domain, user, maj);

	dispatch_release(aq_cred_ctx.sem);
out:
	free(identity.username);
	free(identity.realm);
	free(identity.password);
	
	return (maj == GSS_S_COMPLETE);
}

/* 
 * Create the target name using the host name. Note we will use the 
 * GSS_C_NT_HOSTBASE name type of cifs@<server> and will return a CFString
 */
CFStringRef TargetNameCreateWithHostName(struct smb_ctx *ctx)
{
	CFStringRef hostName;
	CFMutableStringRef kerbHintsHostname;
	
	/* We need to add "cifs@ server part" */
	kerbHintsHostname = CFStringCreateMutableCopy(kCFAllocatorDefault, 0, 
												  CFSTR("cifs@"));
	if (kerbHintsHostname == NULL) {
		return NULL;
	}
	/*
	 * The old code would return an IP dot address if the name was NetBIOS. This
	 * was done for Leopard gss. After talking this over with LHA we now always
	 * return the server name. The IP dot address never worked and was causing
	 * Dfs links to fail.
	 */
	hostName = CFStringCreateWithCString(kCFAllocatorDefault, ctx->serverName, kCFStringEncodingUTF8);
	if (hostName) {
		CFStringAppend(kerbHintsHostname, hostName);
		CFRelease(hostName);
	} else {
		CFRelease(kerbHintsHostname);
		kerbHintsHostname = NULL;
	}
	return kerbHintsHostname;
}

/* 
 * Create a GSS_C_NT_HOSTBASE target name
 */
void GetTargetNameUsingHostName(struct smb_ctx *ctx)
{
	CFStringRef targetNameRef = TargetNameCreateWithHostName(ctx);
	char		targetName[MAX_GSS_HOSTBASE_NAME];
	
	if (targetNameRef == NULL) {
		goto done;
	}
	targetName[0] = 0;
	CFStringGetCString(targetNameRef, targetName, sizeof(targetName), kCFStringEncodingUTF8);
	ctx->ct_setup.ioc_gss_target_name = CAST_USER_ADDR_T(strdup(targetName));
	if (ctx->ct_setup.ioc_gss_target_name == USER_ADDR_NULL) {
		goto done;
	}
	ctx->ct_setup.ioc_gss_target_size = (uint32_t)strnlen(targetName, sizeof(targetName));
done:
	if (targetNameRef) {
		CFRelease(targetNameRef);
	}
}

int serverSupportsKerberos(CFDictionaryRef mechDict)
{	
	if (mechDict == NULL) {
		return FALSE;
	}
	mechDict = CFDictionaryGetValue(mechDict, kSPNEGONegTokenInitMechs);
	if (mechDict == NULL) {
		return FALSE;
	}
	if (CFDictionaryGetValue(mechDict, kGSSAPIMechKerberosOID))
		return TRUE;
	if (CFDictionaryGetValue(mechDict, kGSSAPIMechKerberosU2UOID))
		return TRUE;
	if (CFDictionaryGetValue(mechDict, kGSSAPIMechKerberosMicrosoftOID))
		return TRUE;
	if (CFDictionaryGetValue(mechDict, kGSSAPIMechPKU2UOID))
		return TRUE;
	return FALSE;
}

void
smb_release_gss_cred(void *gssCreds, int error)
{
	OM_uint32 minor_status;

	if (gssCreds == NULL) {
		return;
	}
	if (error == 0) {
		(void)gss_cred_unhold(&minor_status, gssCreds);
		(void)gss_release_cred(&minor_status, (gss_cred_id_t *)&gssCreds);
	} else {
		(void)gss_destroy_cred(NULL, (gss_cred_id_t *)&gssCreds);
	}
}

int
smb_acquire_ntlm_cred(const char *user, const char *domain, const char *password, void **gssCreds)
{
	return (smb_acquire_cred(user, domain, password, GSS_NTLM_MECHANISM, gssCreds));
}


static char *
get_realm(void)
{
	int error;
	krb5_context ctx;
	const char *msg;
	char *realm;

	error = krb5_init_context(&ctx);
	if (error) {
		smb_log_info("%s: Couldn't initialize kerberos: %d", ASL_LEVEL_DEBUG, __FUNCTION__, error);
		return (NULL);
	}		
	error = krb5_get_default_realm(ctx, &realm);
	if (error) {
		msg = krb5_get_error_message(ctx, error);
		smb_log_info("%s: Couldn't get kerberos default realm: %s", ASL_LEVEL_DEBUG, __FUNCTION__, msg);
		krb5_free_error_message(ctx, msg);
		return (NULL);
	}		
	krb5_free_context(ctx);
	
	return (realm);
}

int
smb_acquire_krb5_cred(const char *user, const char *domain, const char *password, void **gssCreds)
{
	int status;
	char *default_realm = NULL;
	
	if (domain == NULL) {
		domain = default_realm = get_realm();
		if (domain == NULL)
			return (EAUTH);
	}
	status = smb_acquire_cred(user, domain, password, GSS_KRB5_MECHANISM, gssCreds);
	free(default_realm);
	return (status);
}

char *
smb_gss_principal_from_cred(void *smb_cred)
{
	gss_cred_id_t cred = (gss_cred_id_t)smb_cred;
	gss_buffer_desc buf;
	gss_name_t name;
	uint32_t M, m;
	char *principal = NULL;
	
	if (cred == GSS_C_NO_CREDENTIAL)
		return (NULL);
	M = gss_inquire_cred(&m, cred, &name, NULL, NULL, NULL);
	if (M != GSS_S_COMPLETE)
		return (NULL);
	M = gss_display_name(&m, name, &buf, NULL);
	(void) gss_release_name(&m, &name);
	if (M == GSS_S_COMPLETE) {
		asprintf(&principal, "%.*s", (int)buf.length, (char *)buf.value);
		(void) gss_release_buffer(&m, &buf);
	}
	
	return (principal);
}

static void
smb_gss_add_cred(struct smb_gss_cred_list *list, gss_OID oid, gss_cred_id_t cred)
{
	gss_buffer_desc buf;
	gss_name_t name;
	uint32_t M, m;
	uint32_t ltime;
	struct smb_gss_cred_list_entry *ep;
	
	if (cred == GSS_C_NO_CREDENTIAL)
		return;
	
	M = gss_inquire_cred(&m, cred, &name, &ltime, NULL, NULL);
	if (M != GSS_S_COMPLETE)
		goto out;
	if (ltime != GSS_C_INDEFINITE && ltime == 0)
		goto out;
	M = gss_display_name(&m, name, &buf, NULL);
	(void) gss_release_name(&m, &name);
	if (M != GSS_S_COMPLETE)
		goto out;
	
	ep = calloc(1, sizeof (struct smb_gss_cred_list_entry));
	if (ep) {
		ep->expire = ltime;
		ep->mech = oid;
		asprintf(&ep->principal, "%.*s", (int)buf.length, (char *)buf.value);
		if (ep->principal) {
			TAILQ_INSERT_TAIL(list, ep, next);
		} else {
			free(ep);
		}
	}
	(void) gss_release_buffer(&m, &buf);
	
out:
	(void)gss_release_cred(&m, &cred);
}

struct cred_iter_ctx {
	struct smb_gss_cred_list *clist;
	dispatch_semaphore_t s;
};

static void
cred_iter(void *ctx, gss_OID moid, gss_cred_id_t cred)
{
	struct cred_iter_ctx *context = (struct cred_iter_ctx *)ctx;
	if (cred == GSS_C_NO_CREDENTIAL) {
		dispatch_semaphore_signal(context->s);
		return;
	}
	smb_gss_add_cred(context->clist, moid, cred);
}

int
smb_gss_get_cred_list(struct smb_gss_cred_list **list, gss_OID mech)
{
	struct cred_iter_ctx ctx;
	
	ctx.s = dispatch_semaphore_create(0);
	if (ctx.s == NULL)
		return ENOMEM;
	
	ctx.clist = malloc(sizeof (struct smb_gss_cred_list));
	
	if (ctx.clist == NULL) {
		dispatch_release(ctx.s);
		return ENOMEM;
	}
	
	TAILQ_INIT(ctx.clist);
	gss_iter_creds_f(NULL, 0, mech, &ctx, cred_iter);
	
	dispatch_semaphore_wait(ctx.s, DISPATCH_TIME_FOREVER);
	dispatch_release(ctx.s);
	*list = ctx.clist;
	
	return 0;
}

void
smb_gss_free_cred_entry(struct smb_gss_cred_list_entry **entry)
{
	struct smb_gss_cred_list_entry *ep;
	
	if (entry == NULL)
		return;
	ep = *entry;
	free(ep->principal);
	*entry = NULL;
}

void
smb_gss_free_cred_list(struct smb_gss_cred_list **list)
{
	struct smb_gss_cred_list *cl;
	struct smb_gss_cred_list_entry *ep, *tmp;
	
	if (list == NULL)
		return;
	cl = *list;
	
	TAILQ_FOREACH_SAFE(ep, cl, next, tmp) {
		TAILQ_REMOVE(cl, ep, next);
		smb_gss_free_cred_entry(&ep);
	}
	free(cl);
	*list = NULL;
}

int
smb_gss_match_cred_entry(struct smb_gss_cred_list_entry *entry, const gss_OID mech, const char *name, const char *domain)
{
	int match = FALSE;
	char *n, *r, *principal, *tofree, *realm;
	
	if (mech == GSS_C_NO_OID)
		return (FALSE);
	
	if (entry->principal == NULL)
		return (FALSE);
	
	if (!gss_oid_equal(mech, entry->mech))
		return (FALSE);
	
	tofree = principal = strdup(entry->principal);
	if (tofree == NULL)
		return (FALSE); //XXX?
	
	
#if 0 /* Use mech specific format */
	if (gss_oid_equal(mech, GSS_KRB5_MECHANISM)) {
		n = strsep(&principal, "/@");	// Set principal to point to the 
						// instance part or realm. return
						// null terminated name
		realm = principal;
		r = strsep(&principal, "@");      // Find the realm if we don't already
						  // have it.
		if (r)
			realm = r;
	} else if (gss_oid_equal(mech, GSS_NTLM_MECHANISM)) {
		r = strsep(&principal, "\\");	// Null terminated the domain and set
						// principal to the name
		if (principal == NULL || *principal == '\0') { // No name means no realm 
			n = r;
			realm = "";
		} else {
			n = principal;
		}
	}
#else /* Currently ntlm is mapped to the kerberos syntax of user@domain */
	n = strsep(&principal, "/@");
	realm = principal;
	r = strsep(&principal, "@");
	if (r)
		realm = r;
#endif	
	if (name) {
		match = (strncmp(name, n, strlen(name) + 1) == 0);
	} else {
		match = TRUE;
	}
	if (match && domain) {
		match = (strncmp(realm, domain, strlen(domain) + 1) == 0);
	}
	
	free(tofree);
	
	return (match);
}
