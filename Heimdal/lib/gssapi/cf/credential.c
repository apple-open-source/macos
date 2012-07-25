/*-
 * Copyright (c) 2009 - 2011 Kungliga Tekniska Högskolan
 * (Royal Institute of Technology, Stockholm, Sweden).
 * All rights reserved.
 *
 * Portions Copyright (c) 2009 - 2011 Apple Inc. All rights reserved.
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

#include <Security/Security.h>

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
 * * kGSSICVerifyCredential - validate the credential with a trusted source that there was no MITM
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
		      gss_const_OID desired_mech,
		      CFDictionaryRef attributes,
		      gss_cred_id_t * output_cred_handle,
		      CFErrorRef *error)
{
    OM_uint32 major_status, minor_status;
    gss_buffer_desc credential;
    CFStringRef usage;
    CFTypeRef password, certificate;
    gss_cred_usage_t cred_usage = GSS_C_INITIATE;
    gss_const_OID cred_type;
    void *cred_value;

    credential.value = NULL;
    credential.length = 0;

    HEIM_WARN_BLOCKING("gss_aapl_initial_cred", warn_once);

    if (error)
	*error = NULL;

    if (desired_mech == GSS_C_NO_OID)
	return GSS_S_BAD_MECH;
    if (desired_name == GSS_C_NO_NAME)
	return GSS_S_BAD_NAME;

    if (output_cred_handle == NULL)
	return GSS_S_CALL_INACCESSIBLE_READ;

    *output_cred_handle = GSS_C_NO_CREDENTIAL;

    /* only support password right now */
    password = CFDictionaryGetValue(attributes, kGSSICPassword);
    certificate = CFDictionaryGetValue(attributes, kGSSICCertificate);
    if (password == NULL && certificate == NULL)
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

    if (password && CFGetTypeID(password) == CFStringGetTypeID()) {
	char *str = rk_cfstring2cstring(password);
	if (str == NULL)
	    return GSS_S_FAILURE;

	credential.value = str;
	credential.length = strlen(str);
	cred_value = &credential;
	cred_type = GSS_C_CRED_PASSWORD;

    } else if (password && CFGetTypeID(password) == CFDataGetTypeID()) {
	credential.value = malloc(CFDataGetLength(password));
	if (credential.value == NULL)
	    return GSS_S_FAILURE;

	credential.length = CFDataGetLength(password);
	memcpy(credential.value, CFDataGetBytePtr(password), CFDataGetLength(password));

	cred_value = &credential;
	cred_type = GSS_C_CRED_PASSWORD;
    } else if (certificate && CFGetTypeID(certificate) == SecIdentityGetTypeID()) {
	cred_value = rk_UNCONST(certificate);
	cred_type = GSS_C_CRED_SecIdentity;
    } else if (certificate && CFGetTypeID(certificate) == SecCertificateGetTypeID()) {
	cred_value = rk_UNCONST(certificate);
	cred_type = GSS_C_CRED_SecIdentity;
    } else
	return GSS_S_FAILURE;

    major_status = gss_acquire_cred_ext(&minor_status,
					desired_name,
					cred_type,
					cred_value,
					GSS_C_INDEFINITE,
					desired_mech,
					cred_usage,
					output_cred_handle);
    if (credential.length) {
	memset(credential.value, 0, credential.length);
	free(credential.value);
    }
	
    if (major_status && error) {
	*error = _gss_mg_cferror(major_status, minor_status, desired_mech);
	return major_status;
    }
    
    /**
     * The credential can be validated by adding kGSSICVerifyCredential to the attributes with any value.
     */

    if (CFDictionaryGetValue(attributes, kGSSICVerifyCredential)) {
	gss_buffer_set_t bufferset = GSS_C_NO_BUFFER_SET;

	major_status = gss_inquire_cred_by_oid(&minor_status, *output_cred_handle,
					       GSS_C_CRED_VALIDATE, &bufferset);
	if (major_status == GSS_S_COMPLETE)
	    gss_release_buffer_set(&minor_status, &bufferset);
	else {
	    if (error)
		*error = _gss_mg_cferror(major_status, minor_status, desired_mech);
	    gss_destroy_cred(&minor_status, output_cred_handle);
	}
    }

    return major_status;
}

OM_uint32 GSSAPI_LIB_FUNCTION
gss_aapl_change_password(const gss_name_t name,
			 gss_const_OID mech,
			 CFDictionaryRef attributes,
			 CFErrorRef *error)
{
    struct _gss_mechanism_name *mn = NULL;
    char *oldpw = NULL, *newpw = NULL;
    OM_uint32 maj_stat, min_stat;
    gssapi_mech_interface m;
    CFStringRef old, new;

    _gss_load_mech();

    m = __gss_get_mechanism(mech);
    if (m == NULL) {
	maj_stat = GSS_S_BAD_MECH;
	min_stat = 0;
	goto out;
    }

    if (m->gm_aapl_change_password == NULL) {
	maj_stat = GSS_S_UNAVAILABLE;
	min_stat = 0;
	goto out;
    }

    maj_stat = _gss_find_mn(&min_stat, (struct _gss_name *)name, mech, &mn);
    if (maj_stat != GSS_S_COMPLETE)
	goto out;

    old = CFDictionaryGetValue(attributes, kGSSChangePasswordOldPassword);
    new = CFDictionaryGetValue(attributes, kGSSChangePasswordNewPassword);

    heim_assert(old != NULL, "old password missing");
    heim_assert(new != NULL, "new password missing");

    oldpw = rk_cfstring2cstring(old);
    newpw = rk_cfstring2cstring(new);

    if (oldpw == NULL || newpw == NULL) {
	maj_stat = GSS_S_FAILURE;
	min_stat = 0;
	goto out;
    }

    maj_stat = m->gm_aapl_change_password(&min_stat,
					  mn->gmn_name,
					  oldpw, newpw);
    if (maj_stat)
	_gss_mg_error(m, min_stat);

 out:
    if (maj_stat && error)
	*error = _gss_mg_cferror(maj_stat, min_stat, mech);

    if (oldpw) {
	memset(oldpw, 0, strlen(oldpw));
	free(oldpw);
    }
    if (newpw) {
	memset(newpw, 0, strlen(newpw));
	free(newpw);
    }

    return maj_stat;
}
