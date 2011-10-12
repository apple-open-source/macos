/*-
 * Copyright (c) 2005 Doug Rabson
 * All rights reserved.
 *
 * Portions Copyright (c) 2009 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "mech_locl.h"
#include <heim_threads.h>

#ifdef __BLOCKS__
#include <Block.h>
#endif

struct _gss_ac_ex {
    HEIMDAL_MUTEX mutex;
    struct _gss_cred *cred;
    gss_status_id_t status;
    gss_OID_set mechs;
    OM_uint32 min_time;
    unsigned int count;
    void *userctx;
    void (*usercomplete)(void *, OM_uint32, gss_status_id_t, gss_cred_id_t, gss_OID_set, OM_uint32);
};

#ifdef __BLOCKS__

static void 
complete_block(void *ctx, OM_uint32 maj_stat,
	       gss_status_id_t status, gss_cred_id_t cred,
	       gss_OID_set set, OM_uint32 min_time)
{
    gss_acquire_cred_complete complete = ctx;

    complete(status, cred, set, min_time);
    Block_release(complete);
}

OM_uint32 GSSAPI_LIB_FUNCTION
gss_acquire_cred_ex(const gss_name_t desired_name,
		    OM_uint32 flags,
		    OM_uint32 time_req,
		    const gss_OID desired_mech,
		    gss_cred_usage_t cred_usage,
		    gss_auth_identity_t identity,
		    gss_acquire_cred_complete complete)
{
    OM_uint32 ret;

    complete = (gss_acquire_cred_complete)Block_copy(complete);

    ret = gss_acquire_cred_ex_f(NULL,
				desired_name,
				flags,
				time_req,
				desired_mech,
				cred_usage,
				identity,
				complete,
				complete_block);
    if (ret != GSS_S_COMPLETE)
	Block_release(complete);
    return ret;
}
#endif


static void
acquire_deref(struct _gss_ac_ex *ctx)
{
    if (--ctx->count == 0) {
	OM_uint32 major, junk;

	/* did we fail getting all credentials */
	if (ctx->mechs->count == 0) {
	    gss_cred_id_t cred = (gss_cred_id_t)ctx->cred;
	    gss_release_oid_set(&junk, &ctx->mechs);
	    gss_release_cred(&junk, &cred);
	    ctx->cred = NULL;
	    major = GSS_S_NO_CRED;
	} else
	    major = GSS_S_COMPLETE;

	(ctx->usercomplete)(ctx->userctx, major, ctx->status, 
			    (gss_cred_id_t)ctx->cred, 
			    ctx->mechs, ctx->min_time);

	HEIMDAL_MUTEX_unlock(&ctx->mutex);
	HEIMDAL_MUTEX_destroy(&ctx->mutex);
	free(ctx);
    } else
	HEIMDAL_MUTEX_unlock(&ctx->mutex);

}


static void
complete_acquire(void *cctx,
		 OM_uint32 major,
		 gss_status_id_t status,
		 gss_cred_id_t mech_cred,
		 OM_uint32 time_rec)
{
    struct _gss_mechanism_cred *mc = cctx;
    struct _gss_ac_ex *ctx = (struct _gss_ac_ex *)mc->gmc_cred;
    OM_uint32 junk;

    HEIMDAL_MUTEX_lock(&ctx->mutex);

    if (major) {
	free(mc);
	goto out;
    }

    mc->gmc_cred = mech_cred;

    if (time_rec < ctx->min_time)
	ctx->min_time = time_rec;
    
    major = gss_add_oid_set_member(&junk, mc->gmc_mech_oid, &ctx->mechs);
    if (major) {
	mc->gmc_mech->gm_release_cred(&junk, &mc->gmc_cred);
	free(mc);
	goto out;
    }
    
    SLIST_INSERT_HEAD(&ctx->cred->gc_mc, mc, gmc_link);

 out:
    acquire_deref(ctx);
}

OM_uint32 GSSAPI_LIB_FUNCTION
gss_acquire_cred_ex_f(gss_status_id_t status,
		      const gss_name_t desired_name,
		      OM_uint32 flags,
		      OM_uint32 time_req,
		      const gss_OID desired_mech,
		      gss_cred_usage_t cred_usage,
		      gss_auth_identity_t identity,
		      void * userctx,
		      void (*usercomplete)(void *, OM_uint32, gss_status_id_t, gss_cred_id_t, gss_OID_set, OM_uint32))
{
	struct _gss_ac_ex *ctx;
	OM_uint32 major_status;
	struct _gss_name *name = (struct _gss_name *) desired_name;
	gssapi_mech_interface m;
	struct _gss_mechanism_cred *mc;
	OM_uint32 junk;
	int i;

	if (usercomplete == NULL)
	    return GSS_S_CALL_INACCESSIBLE_READ;

	_gss_load_mech();

	/*
	 * First make sure that at least one of the requested
	 * mechanisms is one that we support.
	 */
	if (desired_mech) {
		int t;
		gss_test_oid_set_member(&junk, desired_mech, _gss_mech_oids, &t);
		if (!t)
			return (GSS_S_BAD_MECH);
	}

	ctx = malloc(sizeof(struct _gss_ac_ex));
	if (ctx == NULL)
		return GSS_S_FAILURE;

	HEIMDAL_MUTEX_init(&ctx->mutex);
	ctx->status = status;
	ctx->min_time = GSS_C_INDEFINITE;
	ctx->count = 1;
	ctx->userctx = userctx;
	ctx->usercomplete = usercomplete;

	major_status = gss_create_empty_oid_set(&junk, &ctx->mechs);
	if (major_status) {
	    HEIMDAL_MUTEX_destroy(&ctx->mutex);
	    free(ctx);
	    return major_status;
	}

	ctx->cred = malloc(sizeof(struct _gss_cred));
	if (ctx->cred == NULL) {
		gss_release_oid_set(&junk, &ctx->mechs);
		free(ctx);
		return (GSS_S_FAILURE);
	}
	SLIST_INIT(&ctx->cred->gc_mc);

	for (i = 0; i < _gss_mech_oids->count; i++) {
		const gss_OID mech = &_gss_mech_oids->elements[i];
		struct _gss_mechanism_name *mn = NULL;

		if (desired_mech && !gss_oid_equal(desired_mech, mech))
			continue;

		m = __gss_get_mechanism(mech);
		if (!m)
			continue;

		if (m->gm_acquire_cred_ex == NULL)
			continue;

		if (desired_name != GSS_C_NO_NAME) {
			major_status = _gss_find_mn(&junk, name, mech, &mn);
			if (major_status != GSS_S_COMPLETE)
				continue;
		}

		mc = malloc(sizeof(struct _gss_mechanism_cred));
		if (!mc)
			continue;

		mc->gmc_mech = m;
		mc->gmc_mech_oid = &m->gm_mech_oid;
		mc->gmc_cred = (void *)ctx;

		HEIMDAL_MUTEX_lock(&ctx->mutex);
		ctx->count += 1;
		HEIMDAL_MUTEX_unlock(&ctx->mutex);

		major_status = (*m->gm_acquire_cred_ex)(status, mn ? mn->gmn_name : GSS_C_NO_NAME,
							flags, time_req, cred_usage, identity,
							mc, complete_acquire);
		if (major_status != GSS_S_COMPLETE) {
		    HEIMDAL_MUTEX_lock(&ctx->mutex);
		    ctx->count -= 1;
		    HEIMDAL_MUTEX_unlock(&ctx->mutex);
		}
	}

	HEIMDAL_MUTEX_lock(&ctx->mutex);
	acquire_deref(ctx);

	return (GSS_S_COMPLETE);
}



static void
complete_nothing(void *cctx,
		 OM_uint32 major,
		 gss_status_id_t status,
		 gss_cred_id_t mech_cred,
		 OM_uint32 time_rec)
{
    struct _gss_mechanism_cred *mc = cctx;

    if (major)
	    return;

    mc->gmc_cred = mech_cred;
}

#if 0
OM_uint32 GSSAPI_LIB_FUNCTION
gss_acquire_cred_ex2(OM_uint32 *min_stat,
		     const gss_name_t desired_name,
		     OM_uint32 flags,
		     OM_uint32 time_req,
		     const gss_OID desired_mech,
		     gss_cred_usage_t cred_usage,
		     gss_OID credential_type,
		     const void *credential,
		     gss_cred_id_t *output_cred_handle)
{
    *min_stat = 0
    *output_cred_handle = GSS_C_NO_CREDENTIAL;
    return GSS_S_FAILURE;
}
#endif


/**
 * Acquire a new initial credentials using long term credentials (password, certificate).
 *
 * Credentials acquired should be free-ed with gss_release_cred() or
 * destroyed with (removed from storage) gss_destroy_cred().
 *
 * Some mechanism types can not directly acquire or validate
 * credential (for example PK-U2U, SCRAM, NTLM or IAKERB), for those
 * mechanisms its instead the gss_init_sec_context() that will either acquire or
 * force validation of the credential.
 *
 * This function is blocking and should not be used on threads used for UI updates.
 *
 * @param desired_name name to use to acquire credential. Import the name using gss_import_name(). The type of the name has to be supported by the desired_mech used.
 *
 * @param mech mechanism to use to acquire credential. GSS_C_NO_OID is not valid input and a mechanism must be selected. For example GSS_KRB5_MECHANISM, GSS_NTLM_MECHNISM or any other mechanisms supported by the implementation. See gss_indicate_mechs().
 *
 * @param attributes CFDictionary that contains how to acquire the credential, see below for examples
 *
 * @param output_cred_handle the resulting credential handle, value is set to GSS_C_NO_CREDENTIAL on failure.
 *
 * @param error an CFErrorRef returned in case of an error, that needs to be released with CFRelease() by the caller, input can be NULL.
 *
 * @returns a gss_error code, see the CFErrorRef passed back in error for the failure message.
 *
 * attributes must contains one of the following keys
 * * kGSSICPasssword - CFStringRef password
 * * kGSSICCertificate - SecIdentityRef to the certificate to use with PKINIT/PKU2U
 *
 * optional keys
 * * kGSSCredentialUsage - one of kGSS_C_INITIATE, kGSS_C_ACCEPT, kGSS_C_BOTH, default if not given is kGSS_C_INITIATE
 * * kGSSRequestedLifeTime - CFNumberRef life time of credentials, default is dependant of the mechanism
 * * kGSSICLKDCHostname - CFStringRef hostname of LKDC hostname
 * * kGSSICKerberosRenewTime - CFNumberRef rewnable time of credentials
 * * kGSSICKerberosForwardable - CFBooleanRef if credentials should be forwardable, if not set default value is used
 * * kGSSICKerberosProxiable - CFBooleanRef if credentials should be allowed to be proxied, if not set default value is used
 * * kGSSICSessionPersistent - CFBooleanRef store long term credential in cache, and delete on session end
 *
 *
 *	  
 * @ingroup gssapi
 */


OM_uint32 GSSAPI_LIB_FUNCTION
gss_aapl_initial_cred(const gss_name_t desired_name,
		      const gss_OID mech,
		      CFDictionaryRef attributes,
		      gss_cred_id_t * output_cred_handle,
		      CFErrorRef *error)
{
	OM_uint32 major_status;
	struct _gss_name *name = (struct _gss_name *) desired_name;
	gssapi_mech_interface m;
	struct _gss_mechanism_cred *mc;
	OM_uint32 junk;
	struct _gss_mechanism_name *mn = NULL;
	struct gss_auth_identity identity;
	struct _gss_cred *cred;
	CFStringRef password, usage;
	gss_cred_usage_t cred_usage = GSS_C_INITIATE;

	HEIM_WARN_BLOCKING("gss_aapl_initial_cred", warn_once);

	if (error)
	    *error = NULL;

	*output_cred_handle = GSS_C_NO_CREDENTIAL;

	if (mech == GSS_C_NO_OID)
		return GSS_S_BAD_MECH;
	if (name == NULL)
		return GSS_S_BAD_NAME;

	if (output_cred_handle == NULL)
		return GSS_S_CALL_INACCESSIBLE_READ;

	/* only support password right now */
	password = CFDictionaryGetValue(attributes, kGSSICPassword);
	if (password == NULL)
	    return GSS_S_CALL_INACCESSIBLE_READ;

	/* check usage */
	usage = CFDictionaryGetValue(attributes, kGSSCredentialUsage);
	if (usage && CFGetTypeID(usage) == CFStringGetTypeID()) {
	    if (CFStringCompare(usage, kGSS_C_INITIATE, 0) == kCFCompareEqualTo)
		cred_usage = GSS_C_INITIATE;
	    else if (CFStringCompare(usage, kGSS_C_ACCEPT, 0) == kCFCompareEqualTo)
		cred_usage = GSS_C_ACCEPT;
	    else if (CFStringCompare(usage, kGSS_C_BOTH, 0) == kCFCompareEqualTo)
		cred_usage = GSS_C_BOTH;
	    else
		return GSS_S_FAILURE;
	}

	_gss_load_mech();

	m = __gss_get_mechanism(mech);
	if (m == NULL)
		return GSS_S_BAD_MECH;

	if (m->gm_acquire_cred_ex == NULL)
		return GSS_S_UNAVAILABLE;

	major_status = _gss_find_mn(&junk, name, mech, &mn);
	if (major_status != GSS_S_COMPLETE)
		return GSS_S_BAD_NAME;
	
	cred = calloc(1, sizeof(struct _gss_cred));
	if (cred == NULL)
		return (GSS_S_FAILURE);

	SLIST_INIT(&cred->gc_mc);
	
	mc = malloc(sizeof(struct _gss_mechanism_cred));
	if (!mc) {
		free(cred);
		return GSS_S_FAILURE;
	}

	mc->gmc_mech = m;
	mc->gmc_mech_oid = &m->gm_mech_oid;
	mc->gmc_cred = NULL;

	memset(&identity, 0, sizeof(identity));
	
	identity.password = rk_cfstring2cstring(password);
	if (identity.password == NULL) {
	    free(mc);
	    free(cred);
	    return GSS_S_FAILURE;
	}

	major_status = m->gm_acquire_cred_ex(NULL, mn->gmn_name,
					     0, GSS_C_INDEFINITE, cred_usage, &identity,
					     mc, complete_nothing);
	memset(identity.password, 0, strlen(identity.password));
	free(identity.password);
	if (major_status != GSS_S_COMPLETE) {
		free(mc);
		free(cred);
	} else {
		heim_assert(mc->gmc_cred != NULL, "gm_acquire_cred_ex was async");

		SLIST_INSERT_HEAD(&cred->gc_mc, mc, gmc_link);
  
		*output_cred_handle = (gss_cred_id_t)cred;
	}


	return major_status;
}
