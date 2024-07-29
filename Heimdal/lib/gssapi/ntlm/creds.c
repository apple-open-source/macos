/*
 * Copyright (c) 2006 Kungliga Tekniska Högskolan
 * (Royal Institute of Technology, Stockholm, Sweden).
 * All rights reserved.
 *
 * Portions Copyright (c) 2009 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the Institute nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "ntlm.h"
#include <heimbase.h>
#include "heimcred.h"

OM_uint32 _gss_ntlm_inquire_cred
           (OM_uint32 * minor_status,
            const gss_cred_id_t cred_handle,
            gss_name_t * name,
            OM_uint32 * lifetime,
            gss_cred_usage_t * cred_usage,
            gss_OID_set * mechanisms
           )
{
    OM_uint32 ret, junk;

    *minor_status = 0;

    if (cred_handle == NULL)
	return GSS_S_NO_CRED;

    if (name) {
	ret = _gss_ntlm_duplicate_name(minor_status,
				       (gss_name_t)cred_handle,
				       name);
	if (ret)
	    goto out;
    }
    if (lifetime)
	*lifetime = GSS_C_INDEFINITE;
    if (cred_usage)
	*cred_usage = 0;
    if (mechanisms)
	*mechanisms = GSS_C_NO_OID_SET;

    if (cred_handle == GSS_C_NO_CREDENTIAL)
	return GSS_S_NO_CRED;

    if (mechanisms) {
        ret = gss_create_empty_oid_set(minor_status, mechanisms);
        if (ret)
	    goto out;
	ret = gss_add_oid_set_member(minor_status,
				     GSS_NTLM_MECHANISM,
				     mechanisms);
        if (ret)
	    goto out;
    }

    return GSS_S_COMPLETE;
out:
    gss_release_oid_set(&junk, mechanisms);
    return ret;
}

OM_uint32
_gss_ntlm_destroy_cred(OM_uint32 *minor_status,
		       gss_cred_id_t *cred_handle)
{
    krb5_error_code ret;
#ifdef HAVE_KCM
    krb5_storage *request = NULL, *response = NULL;
    krb5_data response_data;
    krb5_context context;
    ssize_t sret;
#endif
    ntlm_cred cred;

    if (cred_handle == NULL || *cred_handle == GSS_C_NO_CREDENTIAL)
	return GSS_S_COMPLETE;

    cred = (ntlm_cred)*cred_handle;

    if ((cred->flags & NTLM_UUID) == 0)
	return _gss_ntlm_release_cred(minor_status, cred_handle);
#ifdef HAVE_KCM
    ret = krb5_init_context(&context);
    if (ret) {
	*minor_status = ret;
	return GSS_S_FAILURE;
    }

    ret = krb5_kcm_storage_request(context, KCM_OP_DESTROY_CRED, &request);
    if (ret) {
	request = NULL;
	goto out;
    }

    sret = krb5_storage_write(request, cred->uuid, sizeof(cred->uuid));
    if (sret != sizeof(cred->uuid)) {
	ret = KRB5_CC_IO;
	goto out;
    }

    ret = krb5_kcm_call(context, request, &response, &response_data);

 out:
    if (request)
        krb5_storage_free(request);
    if (response) {
        krb5_storage_free(response);
	krb5_data_free(&response_data);
    }

    krb5_free_context(context);
    if (ret) {
	*minor_status = ret;
	return GSS_S_FAILURE;
    }
#else /* !HAVE_KCM */
    CFUUIDBytes cfuuid;
    CFUUIDRef uuid_cfuuid;
    memcpy(&cfuuid, &cred->uuid, sizeof(cred->uuid));
    uuid_cfuuid = CFUUIDCreateFromUUIDBytes(kCFAllocatorDefault, cfuuid );
    if (!uuid_cfuuid) {
		ret = KRB5_CC_IO;
		*minor_status = ret;
		return GSS_S_FAILURE;
	} else {
		*minor_status = 0;
	}

	HeimCredDeleteByUUID(uuid_cfuuid);
	CFRelease(uuid_cfuuid);
#endif /* HAVE_KCM */
    return _gss_ntlm_release_cred(minor_status, cred_handle);
}

#ifdef HAVE_KCM
static OM_uint32
change_hold(OM_uint32 *minor_status, ntlm_cred cred, int op)
{
    krb5_storage *request = NULL, *response = NULL;
    krb5_data response_data;
    krb5_context context;
    krb5_error_code ret;
    ssize_t sret;

    *minor_status = 0;

    if (cred == NULL)
	return GSS_S_COMPLETE;

    if ((cred->flags & NTLM_UUID) == 0)
	return GSS_S_COMPLETE;

    ret = krb5_init_context(&context);
    if (ret) {
	*minor_status = ret;
	return GSS_S_FAILURE;
    }

    ret = krb5_kcm_storage_request(context, op, &request);
    if (ret) {
	request = NULL;
	goto out;
    }

    sret = krb5_storage_write(request, cred->uuid, sizeof(cred->uuid));
    if (sret != sizeof(cred->uuid)) {
	ret = KRB5_CC_IO;
	goto out;
    }

    ret = krb5_kcm_call(context, request, &response, &response_data);
    if (ret)
	goto out;

 out:
    if (request)
	krb5_storage_free(request);
    if (response) {
	krb5_storage_free(response);
	krb5_data_free(&response_data);
    }
    krb5_free_context(context);
    if (ret) {
	*minor_status = ret;
	return GSS_S_FAILURE;
    }

    return GSS_S_COMPLETE;
}
#endif

OM_uint32
_gss_ntlm_cred_hold(OM_uint32 *minor_status, gss_cred_id_t incred)
{
#ifdef HAVE_KCM
    return change_hold(minor_status, (ntlm_cred)incred, KCM_OP_RETAIN_CRED);
#else
    krb5_error_code ret;
    ntlm_cred cred = (ntlm_cred)incred;

    if ((cred->flags & NTLM_UUID) == 0)
	return GSS_S_FAILURE;
    
    CFUUIDBytes cfuuid;
    CFUUIDRef uuid_cfuuid;
    memcpy(&cfuuid, &cred->uuid, sizeof(cred->uuid));
    uuid_cfuuid = CFUUIDCreateFromUUIDBytes(kCFAllocatorDefault, cfuuid);
    if (!uuid_cfuuid) {
		ret = KRB5_CC_IO;
		*minor_status = ret;
		return GSS_S_FAILURE;
	} else {
		*minor_status = 0;
	}
    HeimCredRef credential_cfcred = HeimCredCopyFromUUID(uuid_cfuuid);
    if (credential_cfcred) {
	HeimCredRetainTransient(credential_cfcred);
    }
    CFRELEASE_NULL(credential_cfcred);
    CFRELEASE_NULL(uuid_cfuuid);
    
    return GSS_S_COMPLETE;
#endif
}

OM_uint32
_gss_ntlm_cred_unhold(OM_uint32 *minor_status, gss_cred_id_t incred)
{
#ifdef HAVE_KCM
    return change_hold(minor_status, (ntlm_cred)incred, KCM_OP_RELEASE_CRED);
#else
    krb5_error_code ret;
    ntlm_cred cred = (ntlm_cred)incred;

    if ((cred->flags & NTLM_UUID) == 0)
	return GSS_S_FAILURE;
    
    CFUUIDBytes cfuuid;
    CFUUIDRef uuid_cfuuid;
    memcpy(&cfuuid, &cred->uuid, sizeof(cred->uuid));
    uuid_cfuuid = CFUUIDCreateFromUUIDBytes(kCFAllocatorDefault, cfuuid);
    if (!uuid_cfuuid) {
		ret = KRB5_CC_IO;
		*minor_status = ret;
		return GSS_S_FAILURE;
	} else {
		*minor_status = 0;
	}
    HeimCredRef credential_cfcred = HeimCredCopyFromUUID(uuid_cfuuid);
    if (credential_cfcred) {
	HeimCredReleaseTransient(credential_cfcred);
    }
    CFRELEASE_NULL(credential_cfcred);
    CFRELEASE_NULL(uuid_cfuuid);
    
    return GSS_S_COMPLETE;
#endif
}

OM_uint32
_gss_ntlm_cred_label_get(OM_uint32 *minor_status, gss_cred_id_t cred_handle,
			const char *label, gss_buffer_t value)
{
#ifdef HAVE_KCM
    ntlm_cred cred = (ntlm_cred)cred_handle;
    krb5_storage *request = NULL, *response = NULL;
    krb5_data response_data, data;
    krb5_error_code ret;
    krb5_context context;
    ssize_t sret;

    *minor_status = 0;

    if (cred == NULL)
	return GSS_S_COMPLETE;

    if ((cred->flags & NTLM_UUID) == 0)
	return GSS_S_COMPLETE;

    ret = krb5_init_context(&context);
    if (ret) {
	*minor_status = ret;
	return GSS_S_FAILURE;
    }

    ret = krb5_kcm_storage_request(context, KCM_OP_CRED_LABEL_GET, &request);
    if (ret) {
	request = NULL;
	goto out;
    }

    sret = krb5_storage_write(request, cred->uuid, sizeof(cred->uuid));
    if (sret != sizeof(cred->uuid)) {
	ret = KRB5_CC_IO;
	goto out;
    }

    ret = krb5_store_stringz(request, label);
    if (ret)
	goto out;

    ret = krb5_kcm_call(context, request, &response, &response_data);
    if (ret)
	goto out;

    ret = krb5_ret_data(response, &data);
    if (ret)
	goto out;

    value->value = data.data;
    value->length = data.length;

 out:
    if (request)
	krb5_storage_free(request);
    if (response) {
	krb5_storage_free(response);
	krb5_data_free(&response_data);
    }
    krb5_free_context(context);
    if (ret) {
	*minor_status = ret;
	return GSS_S_FAILURE;
    }
#else /* GSSCred */
    krb5_error_code ret;
    ntlm_cred cred = (ntlm_cred)cred_handle;
    
    CFUUIDBytes cfuuid;
    CFUUIDRef uuid_cfuuid = NULL;
    memcpy(&cfuuid, &cred->uuid, sizeof(cred->uuid));
    uuid_cfuuid = CFUUIDCreateFromUUIDBytes(kCFAllocatorDefault, cfuuid);
    if (!uuid_cfuuid) {
	ret = KRB5_CC_IO;
	*minor_status = ret;
	return GSS_S_FAILURE;
    } else {
	*minor_status = 0;
    }
    
    CFDictionaryRef query = NULL;
    CFArrayRef query_result = NULL;
    CFStringRef label_cfstr = NULL;
    
    label_cfstr = CFStringCreateWithCString(kCFAllocatorDefault, label, kCFStringEncodingUTF8);
    if (label_cfstr == NULL) {
	CFRELEASE_NULL(uuid_cfuuid);
	return GSS_S_FAILURE;
    }
    
    const void *add_keys[] = {
	(void *)kHEIMObjectType,
	kHEIMAttrType,
	kHEIMAttrParentCredential,
	kHEIMAttrServerName,
    };
    const void *add_values[] = {
	(void *)kHEIMObjectNTLM,
	kHEIMTypeNTLM,
	uuid_cfuuid,
	label_cfstr,
    };

    query = CFDictionaryCreate(NULL, add_keys, add_values, sizeof(add_keys) / sizeof(add_keys[0]), &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    if (query == NULL)
	errx(1, "out of memory");

    query_result = HeimCredCopyQuery(query);
    CFRELEASE_NULL(query);
    
    if (CFArrayGetCount(query_result) < 1) {
	CFRELEASE_NULL(query_result);
	return 0;
    }
    
    HeimCredRef result_cred = (HeimCredRef)CFArrayGetValueAtIndex(query_result, 0);
    if (!result_cred) {
	CFRELEASE_NULL(query_result);
	return GSS_S_FAILURE;
    }
    CFRetain(result_cred);
    CFRELEASE_NULL(query_result);
    
    CFDataRef cfdata = HeimCredCopyAttribute(result_cred, kHEIMAttrLabelValue);
    CFRELEASE_NULL(result_cred);
    if (!cfdata) {
	return GSS_S_FAILURE;
    }
    
    value->value = malloc(CFDataGetLength(cfdata));
    if (value->value == NULL) {
	CFRELEASE_NULL(cfdata);
	return GSS_S_FAILURE;
    }
    memcpy(value->value, CFDataGetBytePtr(cfdata), CFDataGetLength(cfdata));
    value->length = CFDataGetLength(cfdata);
    
    CFRELEASE_NULL(cfdata);
    
    return GSS_S_COMPLETE;
#endif
    return GSS_S_COMPLETE;
}

OM_uint32
_gss_ntlm_cred_label_set(OM_uint32 *minor_status, gss_cred_id_t cred_handle,
			 const char *label, gss_buffer_t value)
{
#ifdef HAVE_KCM
    ntlm_cred cred = (ntlm_cred)cred_handle;
    krb5_storage *request = NULL, *response = NULL;
    krb5_data response_data;
    krb5_context context;
    krb5_error_code ret;
    krb5_data data;
    ssize_t sret;

    *minor_status = 0;

    if (cred == NULL)
	return GSS_S_COMPLETE;

    if ((cred->flags & NTLM_UUID) == 0)
	return GSS_S_COMPLETE;

    ret = krb5_init_context(&context);
    if (ret) {
	*minor_status = ret;
	return GSS_S_FAILURE;
    }

    ret = krb5_kcm_storage_request(context, KCM_OP_CRED_LABEL_SET, &request);
    if (ret) {
	request = NULL;
	goto out;
    }

    sret = krb5_storage_write(request, cred->uuid, sizeof(cred->uuid));
    if (sret != sizeof(cred->uuid)) {
	ret = KRB5_CC_IO;
	goto out;
    }

    ret = krb5_store_stringz(request, label);
    if (ret)
	goto out;

    if (value) {
	data.data = value->value;
	data.length = value->length;
    } else {
	krb5_data_zero(&data);
    }

    ret = krb5_store_data(request, data);
    if (ret)
	goto out;

    ret = krb5_kcm_call(context, request, &response, &response_data);
    if (ret)
	goto out;

 out:
    if (request)
	krb5_storage_free(request);
    if (response) {
	krb5_storage_free(response);
	krb5_data_free(&response_data);
    }

    krb5_free_context(context);
    if (ret) {
	*minor_status = ret;
	return GSS_S_FAILURE;
    }
#else /* GSSCred */
    krb5_error_code ret;
    ntlm_cred cred = (ntlm_cred)cred_handle;

    if ((cred->flags & NTLM_UUID) == 0)
	return GSS_S_FAILURE;
    
    CFUUIDBytes cfuuid;
    CFUUIDRef uuid_cfuuid = NULL;
    memcpy(&cfuuid, &cred->uuid, sizeof(cred->uuid));
    uuid_cfuuid = CFUUIDCreateFromUUIDBytes(kCFAllocatorDefault, cfuuid);
    if (!uuid_cfuuid) {
	ret = KRB5_CC_IO;
	*minor_status = ret;
	return GSS_S_FAILURE;
    } else {
	*minor_status = 0;
    }

    HeimCredRef credential_cfcred = NULL;
    CFErrorRef cferr = NULL;
    CFStringRef user_cfstr = NULL;
    CFStringRef domain_cfstr = NULL;
    CFStringRef label_cfstr = NULL;
    CFDataRef dref = NULL;
    CFDictionaryRef attrs = NULL;
    
    CFDictionaryRef query;
    
    label_cfstr = CFStringCreateWithCString(kCFAllocatorDefault, label, kCFStringEncodingUTF8);
    if (label_cfstr == NULL) {
	ret = GSS_S_FAILURE;
	goto out;
    }
    
    //remove the existing value, if it exists
    
    const void *keys[] = { (const void *)kHEIMAttrParentCredential, kHEIMAttrType, kHEIMAttrServerName };
    const void *values[] = { (const void *)uuid_cfuuid, kHEIMTypeNTLM, label_cfstr };
    
    query = CFDictionaryCreate(NULL, keys, values, sizeof(keys)/sizeof(keys[0]), &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    heim_assert(query != NULL, "Failed to create dictionary");

    HeimCredDeleteQuery(query, NULL);
    CFRELEASE_NULL(query);
    
    if (value != NULL) {
	
	user_cfstr = CFStringCreateWithCString(kCFAllocatorDefault, cred->user,kCFStringEncodingUTF8);
	if (user_cfstr == NULL) {
	    ret = GSS_S_FAILURE;
	    goto out;
	}
	
	domain_cfstr = CFStringCreateWithCString(kCFAllocatorDefault, cred->domain,kCFStringEncodingUTF8);
	if (domain_cfstr == NULL) {
	    ret = GSS_S_FAILURE;
	    goto out;
	}
	
	dref = CFDataCreateWithBytesNoCopy(NULL, value->value, value->length, kCFAllocatorNull);
	if (dref == NULL) {
	    ret = GSS_S_FAILURE;
	    goto out;
	}
	
	const void *add_keys[] = {
	    (void *)kHEIMObjectType,
	    kHEIMAttrType,
	    kHEIMAttrParentCredential,
	    kHEIMAttrNTLMUsername,
	    kHEIMAttrNTLMDomain,
	    kHEIMAttrServerName,
	    kHEIMAttrLabelValue,  // the label value is used because the data attr is write only
	};
	const void *add_values[] = {
	    (void *)kHEIMObjectNTLM,
	    kHEIMTypeNTLM,
	    uuid_cfuuid,
	    user_cfstr,
	    domain_cfstr,
	    label_cfstr,
	    dref,
	};
	
	attrs = CFDictionaryCreate(NULL, add_keys, add_values, sizeof(add_keys) / sizeof(add_keys[0]), &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
	heim_assert(attrs != NULL, "Failed to create dictionary");
	
	// TODO check for duplicate in GSSCred? HeimCredCopyQuery <OR>  handled in _gss_ntlm_have_cred
	credential_cfcred = HeimCredCreate(attrs, &cferr);
	if (credential_cfcred == NULL){
	    ret = GSS_S_FAILURE;
	    goto out;
	}
    }
    
    ret = GSS_S_COMPLETE;
    
out:
    CFRELEASE_NULL(credential_cfcred);
    CFRELEASE_NULL(uuid_cfuuid);
    CFRELEASE_NULL(user_cfstr);
    CFRELEASE_NULL(domain_cfstr);
    CFRELEASE_NULL(label_cfstr);
    CFRELEASE_NULL(dref);
    CFRELEASE_NULL(attrs);
    
    return ret;
#endif

    return GSS_S_COMPLETE;
}
