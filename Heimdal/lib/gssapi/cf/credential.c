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
#include "krb5.h"

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
 * * kGSSICPassword - CFStringRef password
 * * kGSSICCertificate - SecIdentityRef to the certificate to use with PKINIT/PKU2U
 *
 * optional keys
 * * kGSSCredentialUsage - one of kGSS_C_INITIATE, kGSS_C_ACCEPT, kGSS_C_BOTH, default if not given is kGSS_C_INITIATE
 * * kGSSICVerifyCredential - validate the credential with a trusted source that there was no MITM
 * * kGSSICLKDCHostname - CFStringRef hostname of LKDC hostname
 * * kGSSICKerberosCacheName - CFStringRef name of cache that will be created (including type)
 * * kGSSICAppIdentifierACL - CFArrayRef[CFStringRef] prefix of bundle ID allowed to access this credential
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

    if (gss_oid_equal(desired_mech, GSS_KRB5_MECHANISM)) {

	cred_value = (void *)attributes;
	cred_type = GSS_C_CRED_HEIMBASE;
	
    } else if (password && CFGetTypeID(password) == CFStringGetTypeID()) {
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

/**
 * Returns a copy of the UUID of the GSS credential
 *
 * @param credential credential
 *
 * @returns CFUUIDRef that can be used to turn into a credential,
 * normal CoreFoundaton rules for rules applies so the CFUUIDRef needs
 * to be released.
 *
 * @ingroup gssapi
 */

CFUUIDRef
GSSCredentialCopyUUID(gss_cred_id_t cred)
{
    OM_uint32 major, minor;
    gss_buffer_set_t dataset = GSS_C_NO_BUFFER_SET;
    krb5_error_code ret;
    krb5_uuid uuid;
    CFUUIDBytes cfuuid;

    major = gss_inquire_cred_by_oid(&minor, cred, GSS_C_NT_UUID, &dataset);
    if (major || dataset->count != 1) {
	gss_release_buffer_set(&minor, &dataset);
	return NULL;
    }
	    
    if (dataset->elements[0].length != 36) {
	gss_release_buffer_set(&minor, &dataset);
	return NULL;
    }

    ret = krb5_string_to_uuid(dataset->elements[0].value, uuid);
    gss_release_buffer_set(&minor, &dataset);
    if (ret)
	return NULL;
	
    memcpy(&cfuuid, uuid, sizeof(uuid));

    return CFUUIDCreateFromUUIDBytes(NULL, cfuuid);
}

/**
 * Returns a GSS credential for a given UUID if the credential exists.
 *
 * @param uuid the UUID of the credential to fetch
 *
 * @returns a gss_cred_id_t, normal CoreFoundaton rules for rules
 * applies so the CFUUIDRef needs to be released with either CFRelease() or gss_release_name().
 *
 * @ingroup gssapi
 */

gss_cred_id_t GSSAPI_LIB_FUNCTION
GSSCreateCredentialFromUUID(CFUUIDRef uuid)
{
    OM_uint32 min_stat, maj_stat;
    gss_cred_id_t cred;
    CFStringRef name;
    gss_name_t gname;

    name = CFUUIDCreateString(NULL, uuid);
    if (name == NULL)
	return NULL;
    
    gname = GSSCreateName(name, GSS_C_NT_UUID, NULL);
    CFRelease(name);
    if (gname == NULL)
	return NULL;

    maj_stat = gss_acquire_cred(&min_stat, gname, GSS_C_INDEFINITE, NULL,
				GSS_C_INITIATE, &cred, NULL, NULL);
    gss_release_name(&min_stat, &gname);
    if (maj_stat != GSS_S_COMPLETE)
	return NULL;

    return cred;
}

static CFStringRef
CopyFoldString(CFStringRef host)
{
    CFMutableStringRef string = CFStringCreateMutableCopy(NULL, 0, host);
    static dispatch_once_t once;
    static CFLocaleRef locale;
    dispatch_once(&once, ^{
	    locale = CFLocaleCreate(NULL, CFSTR("C"));
	});
    CFStringFold(string, kCFCompareCaseInsensitive, locale);
    return string;
}

static CFStringRef
CopyFoldedHostName(CFStringRef stringOrURL, CFStringRef *path)
{
    CFStringRef string, hn = NULL;
    CFURLRef url;

    *path = NULL;

    /*
     * Try paring the hostname as an URL first
     */

    url = CFURLCreateWithString(NULL, stringOrURL, NULL);
    if (url) {
	CFStringRef host = CFURLCopyHostName(url);
	CFStringRef scheme = CFURLCopyScheme(url);
	if (host == NULL)
	    host = CFSTR("");
	if (scheme == NULL)
	    scheme = CFSTR("");
	hn = CFStringCreateWithFormat(NULL, NULL, CFSTR("%@-%@"), host, scheme);
	*path = CFURLCopyPath(url);
	if (CFStringCompare(*path, CFSTR(""), 0) == kCFCompareEqualTo) {
	    CFRelease(*path);
	    *path = CFSTR("/");
	}
	CFRelease(url);
	CFRelease(scheme);
	CFRelease(host);
    }
    if (hn == NULL) {
	hn = CFStringCreateWithFormat(NULL, NULL, CFSTR("%@-host"), stringOrURL);
	*path = CFSTR("/");
    }

    string = CopyFoldString(hn);
    CFRelease(hn);
    return string;
}

/*
 *
 */

void
GSSRuleAddMatch(CFMutableDictionaryRef rules, CFStringRef host, CFStringRef value)
{
    CFMutableDictionaryRef match;
    CFStringRef hostname, path;

    hostname = CopyFoldedHostName(host, &path);
    if (hostname) {

	match = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
	if (match) {
	    CFMutableArrayRef mutable;

	    CFDictionarySetValue(match, CFSTR("path"), path);
	    CFDictionarySetValue(match, CFSTR("value"), value);

	    CFArrayRef array = CFDictionaryGetValue(rules, hostname);
	    if (array) {
		mutable = CFArrayCreateMutableCopy(NULL, 0, array);
	    } else {
		mutable = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
	    }
	    if (mutable) {
		
		CFIndex n, count = CFArrayGetCount(mutable);

		for (n = 0; n < count; n++) {
		    CFDictionaryRef item = (CFDictionaryRef)CFArrayGetValueAtIndex(mutable, n);
		    CFStringRef p = CFDictionaryGetValue(item, CFSTR("path"));

		    if (CFStringHasPrefix(path, p)) {
			CFArrayInsertValueAtIndex(mutable, n, match);
			break;
		    }
		}
		if (n >= count)
		    CFArrayAppendValue(mutable, match);

		CFDictionarySetValue(rules, hostname, mutable);
		CFRelease(mutable);
	    }
	    CFRelease(hostname);
	    CFRelease(path);
	}
	CFRelease(match);
    }
}

/*
 * host is a URL string or hostname string
 */

CFStringRef
GSSRuleGetMatch(CFDictionaryRef rules, CFStringRef host)
{
    CFTypeRef result = NULL;
    CFStringRef hostFolded, path;
    const char *p;

    hostFolded = CopyFoldedHostName(host, &path);
    if (hostFolded == NULL)
	return NULL;

    char *str = rk_cfstring2cstring(hostFolded);
    CFRelease(hostFolded);
    if (str == NULL) {
	CFRelease(path);
	return NULL;
    }
    
    if (str[0] == '\0') {
	free(str);
	return NULL;
    }
    
    for (p = str; p != NULL && result == NULL; p = strchr(p + 1, '.')) {
	CFStringRef partial = CFStringCreateWithCString(NULL, p, kCFStringEncodingUTF8);
	CFArrayRef array = (CFArrayRef)CFDictionaryGetValue(rules, partial);

	CFRelease(partial);

	if (array) {
	    CFIndex n, count = CFArrayGetCount(array);

	    for (n = 0; n < count && result == NULL; n++) {
		CFDictionaryRef item = (CFDictionaryRef)CFArrayGetValueAtIndex(array, n);

		CFStringRef matchPath = CFDictionaryGetValue(item, CFSTR("path"));
		if (CFStringHasPrefix(path, matchPath)) {
		    result = CFDictionaryGetValue(item, CFSTR("value"));
		    if (result)
			CFRetain(result);
		}
	    }
	}
    }
    CFRelease(path);
    free(str);
    return result;
}

/**
 * Create a GSS name from a buffer and type.
 *
 * @param name name buffer describing a credential, can be either a CFDataRef or CFStringRef of a name.
 * @param name_type on OID of the GSS_C_NT_* OIDs constants specifiy the name type.
 * @param error if an error happen, this may be set to a CFErrorRef describing the failure futher.
 *
 * @returns returns gss_name_t or NULL on failure. Must be freed using gss_release_name() or CFRelease(). Follows CoreFoundation Create/Copy rule.
 *
 * @ingroup gssapi
 */

gss_name_t
GSSCreateName(CFTypeRef name, gss_const_OID name_type, CFErrorRef *error)
{
    OM_uint32 maj_stat, min_stat;
    gss_buffer_desc buffer;
    int free_data = 0;
    gss_name_t n;

    if (error)
	*error = NULL;

    if (CFGetTypeID(name) == CFStringGetTypeID()) {
	buffer.value = rk_cfstring2cstring(name);
	if (buffer.value == NULL)
	    return GSS_S_FAILURE;
	buffer.length = strlen((char *)buffer.value);
	free_data = 1;
    } else if (CFGetTypeID(name) == CFDataGetTypeID()) {
	buffer.value = (void *)CFDataGetBytePtr(name);
	buffer.length = (void *)CFDataGetLength(name);
    } else {
	return GSS_C_NO_NAME;
    }

    maj_stat = gss_import_name(&min_stat, &buffer, name_type, &n);

    if (free_data)
	free(buffer.value);

    if (maj_stat)
	return GSS_C_NO_NAME;

    return n;
}

/**
 * Copy the name describing the credential
 *
 * @param cred the credential to get the name from
 *
 * @returns returns gss_name_t or NULL on failure. Must be freed using gss_release_name() or CFRelease(). Follows CoreFoundation Create/Copy rule.
 *
 * @ingroup gssapi
 */

gss_name_t
GSSCredentialCopyName(gss_cred_id_t cred)
{
    OM_uint32 major, minor;
    gss_name_t name;
                
    major = gss_inquire_cred(&minor, cred, &name, NULL, NULL, NULL);
    if (major != GSS_S_COMPLETE)
	return NULL;
	
    return name;
}

/**
 * Return the lifetime (in seconds) left of the credential.
 *
 * @param cred the credential to get the name from
 *
 * @returns the lifetime of the credentials. 0 on failure and
 * GSS_C_INDEFINITE on credentials that never expire.
 *
 * @ingroup gssapi
 */

OM_uint32
GSSCredentialGetLifetime(gss_cred_id_t cred)
{
    OM_uint32 maj_stat, min_stat;
    OM_uint32 lifetime;
                
    maj_stat = gss_inquire_cred(&min_stat, cred, NULL, &lifetime, NULL, NULL);
    if (maj_stat != GSS_S_COMPLETE)
	return 0;
	
    return lifetime;
}

/**
 * Returns a string that is suitable for displaying to user, must not
 * be used for verify subjects on an ACLs.
 *
 * @param name to get a display strings from
 *
 * @returns a string that is printable. Follows CoreFoundation Create/Copy rule.
 *
 * @ingroup gssapi
 */

CFStringRef
GSSNameCreateDisplayString(gss_name_t name)
{
    OM_uint32 maj_stat, min_stat;
    gss_buffer_desc buffer;
    CFStringRef str;

    maj_stat = gss_display_name(&min_stat, name, &buffer, NULL);
    if (maj_stat != GSS_S_COMPLETE)
	return NULL;

    str = CFStringCreateWithBytes(NULL, (const UInt8 *)buffer.value, buffer.length, kCFStringEncodingUTF8, false);
    gss_release_buffer(&min_stat, &buffer);

    return str;
}

/* deprecated */
OM_uint32
GSSCredGetLifetime(gss_cred_id_t cred)
{
    return GSSCredentialGetLifetime(cred);
}

gss_name_t
GSSCredCopyName(gss_cred_id_t cred)
{
    return GSSCredentialCopyName(cred);
}
