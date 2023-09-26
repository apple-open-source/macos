/*
 * Copyright (c) 1997 - 2004 Kungliga Tekniska Högskolan
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

#include "gssdigest.h"
#include <gssapi_spi.h>
#include "heimcred.h"
#include "heimbase.h"
#include "scram.h"
#import <Security/SecCFAllocator.h>

OM_uint32
_gss_scram_have_cred(OM_uint32 *minor,
		     const char *name,
		     gss_cred_id_t *rcred)
{
#ifdef HAVE_KCM
    krb5_context context;
    krb5_error_code ret;
    krb5_storage *request = NULL, *response;
    krb5_data response_data;
    OM_uint32 major;

    ret = krb5_init_context(&context);
    if (ret) {
	*minor = ret;
	return GSS_S_FAILURE;
    }


    ret = krb5_kcm_storage_request(context, KCM_OP_HAVE_SCRAM_CRED, &request);
    if (ret)
	goto out;

    ret = krb5_store_stringz(request, name);
    if (ret)
	goto out;

    ret = krb5_kcm_call(context, request, &response, &response_data);
    if (ret)
	goto out;

    request = NULL;

    if (rcred) {
        *rcred = (gss_cred_id_t)strdup(name);
	if (*rcred == NULL)
	    ret = ENOMEM;
    }

 out:
    if (request)
	krb5_storage_free(request);
    krb5_free_context(context);
    if (ret) {
	*minor = ret;
	major = GSS_S_FAILURE;
    } else
	major = GSS_S_COMPLETE;

    return major;
#else
    krb5_error_code ret = KRB5_CC_IO;
    OM_uint32 major = GSS_S_FAILURE;
    krb5_uuid uuid = { 0 };
    scram_cred cred = NULL;

    CFDictionaryRef query = NULL;
    CFArrayRef query_result = NULL;
    CFIndex query_count = 0;
    CFStringRef user_cfstr = NULL;
    CFUUIDRef uuid_cfuuid = NULL;
    CFUUIDBytes uuid_bytes;

    HeimCredRef result = NULL;

    user_cfstr = CFStringCreateWithCString(kCFAllocatorDefault, name, kCFStringEncodingUTF8);
    if (user_cfstr == NULL)
	    goto out;


    const void *add_keys[] = {
    (void *)kHEIMObjectType,
	    kHEIMAttrType,
	    kHEIMAttrSCRAMUsername
    };
    const void *add_values[] = {
    (void *)kHEIMObjectSCRAM,
	    kHEIMTypeSCRAM,
	    user_cfstr
    };

    query = CFDictionaryCreate(NULL, add_keys, add_values, sizeof(add_keys) / sizeof(add_keys[0]), &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    heim_assert(query != NULL, "Failed to create dictionary");

    query_result = HeimCredCopyQuery(query);

    if (query_result == NULL) {
	goto out;
    }

    query_count = CFArrayGetCount(query_result);
    if (query_count == 0) {
	goto out;
    }

    result = (HeimCredRef) CFArrayGetValueAtIndex(query_result, 0);
    if (result == NULL) {
	goto out;
    }

    uuid_cfuuid = HeimCredGetUUID(result);
    if (uuid_cfuuid == NULL)
	    goto out;
    uuid_bytes = CFUUIDGetUUIDBytes(uuid_cfuuid);
    memcpy(&uuid, &uuid_bytes, sizeof (uuid));
    uuid_string_t uuid_cstr;
    uuid_unparse(uuid, uuid_cstr);
    _gss_mg_log(1, "_gss_scram_have_cred UUID(%s)", uuid_cstr);

    if (rcred) {
	cred = calloc(1, sizeof(*cred));
	cred->name = strdup(name);
	memcpy(cred->uuid, uuid, sizeof(cred->uuid));
	*rcred = (gss_cred_id_t)cred;
	if (*rcred == NULL) {
	    ret = ENOMEM;
	} else {
	    ret = 0;
	}
    } else {
	ret = 0;
    }
out:
    if (ret) {
	*minor = ret;
	major = GSS_S_FAILURE;
    } else
	major = GSS_S_COMPLETE;

    if (user_cfstr) {
	CFRelease(user_cfstr);
    }
    if (query) {
	CFRelease(query);
    }
    if (query_result) {
	CFRelease(query_result);
    }

    return major;
#endif
}

OM_uint32
_gss_scram_acquire_cred(OM_uint32 * min_stat,
		       const gss_name_t desired_name,
		       OM_uint32 time_req,
		       const gss_OID_set desired_mechs,
		       gss_cred_usage_t cred_usage,
		       gss_cred_id_t * output_cred_handle,
		       gss_OID_set * actual_mechs,
		       OM_uint32 * time_rec)
{
    char *name = (char *) desired_name;
    OM_uint32 maj_stat;

    *min_stat = 0;
    *output_cred_handle = GSS_C_NO_CREDENTIAL;
    if (actual_mechs)
	*actual_mechs = GSS_C_NO_OID_SET;
    if (time_rec)
	*time_rec = GSS_C_INDEFINITE;

    if (desired_name == NULL)
	return GSS_S_NO_CRED;

    if (cred_usage == GSS_C_BOTH || cred_usage == GSS_C_ACCEPT) {
	/* we have all server names */
    }	
    if (cred_usage == GSS_C_BOTH || cred_usage == GSS_C_INITIATE) {
	maj_stat = _gss_scram_have_cred(min_stat, name, output_cred_handle);
	if (maj_stat)
	    return maj_stat;
    }

    return (GSS_S_COMPLETE);
}

OM_uint32
_gss_scram_acquire_cred_ext(OM_uint32 * minor_status,
			    const gss_name_t desired_name,
			    gss_const_OID credential_type,
			    const void *credential_data,
			    OM_uint32 time_req,
			    gss_const_OID desired_mech,
			    gss_cred_usage_t cred_usage,
			    gss_cred_id_t * output_cred_handle)
{
#ifdef HAVE_KCM
    char *name = (char *) desired_name;
    krb5_storage *request, *response;
    gss_buffer_t credential_buffer;
    krb5_data response_data;
    krb5_context context;
    krb5_error_code ret;
    char *password;
    char *cred;

    *output_cred_handle = GSS_C_NO_CREDENTIAL;

    krb5_data_zero(&response_data);

    if (!gss_oid_equal(credential_type, GSS_C_CRED_PASSWORD))
	return GSS_S_FAILURE;

    if (name == NULL)
	return GSS_S_FAILURE;

    if (credential_data == NULL)
	return GSS_S_FAILURE;
	
    ret = krb5_init_context(&context);
    if (ret)
	return GSS_S_FAILURE;

    ret = krb5_kcm_storage_request(context, KCM_OP_ADD_SCRAM_CRED, &request);
    if (ret)
	goto fail;
	
    ret = krb5_store_stringz(request, name);
    if (ret)
	goto fail;

    credential_buffer = (gss_buffer_t)credential_data;
    password = malloc(credential_buffer->length + 1);
    if (password == NULL) {
	ret = ENOMEM;
	goto fail;
    }
    memcpy(password, credential_buffer->value, credential_buffer->length);
    password[credential_buffer->length] = '\0';

    ret = krb5_store_stringz(request, password);
    memset(password, 0, strlen(password));
    free(password);
    if (ret)
	goto fail;
    
    ret = krb5_kcm_call(context, request, &response, &response_data);
    if (ret)
	goto fail;
    
    request = NULL;

    cred = strdup(name);

    *output_cred_handle = (gss_cred_id_t)cred;

    if (response)
	krb5_storage_free(response);
    krb5_data_free(&response_data);
    krb5_free_context(context);
    
    return GSS_S_COMPLETE;

 fail:
    if (request)
	krb5_storage_free(request);
    if (minor_status)
	*minor_status = ret;
    return GSS_S_FAILURE;
#else

    char *name = (char *) desired_name;
    CFStringRef user_cfstr = NULL;
    CFStringRef password_cfstring = NULL;
    CFDataRef password_cfdata = NULL;
    CFDictionaryRef attrs = NULL;
    HeimCredRef cred = NULL;
    CFErrorRef cferr = NULL;
    CFUUIDRef uuid_cfuuid = NULL;
    CFUUIDBytes uuid_bytes;
    scram_cred scramCred = NULL;

    gss_buffer_t credential_buffer;
    krb5_error_code ret = 0;

    *output_cred_handle = GSS_C_NO_CREDENTIAL;

    if (!gss_oid_equal(credential_type, GSS_C_CRED_PASSWORD))
	return GSS_S_FAILURE;

    if (name == NULL)
	return GSS_S_FAILURE;

    if (credential_data == NULL)
	return GSS_S_FAILURE;

    user_cfstr = CFStringCreateWithCString(kCFAllocatorDefault, name, kCFStringEncodingUTF8);
    if (user_cfstr == NULL)
	goto fail;

    credential_buffer = (gss_buffer_t)credential_data;
    password_cfstring = CFStringCreateWithBytes(SecCFAllocatorZeroize(), credential_buffer->value, credential_buffer->length, kCFStringEncodingUTF8, false);

    password_cfdata = CFStringCreateExternalRepresentation(SecCFAllocatorZeroize(), password_cfstring, kCFStringEncodingUTF8, 0);

    CFRELEASE_NULL(password_cfstring);

    const void *add_keys[] = {
    (void *)kHEIMObjectType,
	kHEIMAttrType,
	kHEIMAttrSCRAMUsername,
	kHEIMAttrData
    };
    const void *add_values[] = {
	(void *)kHEIMObjectSCRAM,
	kHEIMTypeSCRAM,
	user_cfstr,
	password_cfdata
    };

    attrs = CFDictionaryCreate(NULL, add_keys, add_values, sizeof(add_keys) / sizeof(add_keys[0]), &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    heim_assert(attrs != NULL, "Failed to create dictionary");

    cred = HeimCredCreate(attrs, &cferr);
    if (cred == NULL) {
	goto fail;
    }
    uuid_cfuuid = HeimCredGetUUID(cred);
    if (uuid_cfuuid == NULL) {
	goto fail;
    }

    scramCred = calloc(1, sizeof(*scramCred));

    uuid_bytes = CFUUIDGetUUIDBytes(uuid_cfuuid);
    memcpy(&scramCred->uuid, &uuid_bytes, sizeof (scramCred->uuid));
    uuid_string_t uuid_cstr;
    uuid_unparse(scramCred->uuid, uuid_cstr);
    _gss_mg_log(1, "_gss_scram_acquire_cred_ext name(%s) UUID(%s)", name, uuid_cstr);

    scramCred->name = strdup(name);

    *output_cred_handle = (gss_cred_id_t)scramCred;

    CFRELEASE_NULL(user_cfstr);
    CFRELEASE_NULL(password_cfstring);
    CFRELEASE_NULL(password_cfdata);
    CFRELEASE_NULL(attrs);
    CFRELEASE_NULL(cred);
    CFRELEASE_NULL(cferr);
    CFRELEASE_NULL(uuid_cfuuid);

    return GSS_S_COMPLETE;

 fail:
    CFRELEASE_NULL(user_cfstr);
    CFRELEASE_NULL(password_cfstring);
    CFRELEASE_NULL(password_cfdata);
    CFRELEASE_NULL(attrs);
    CFRELEASE_NULL(cred);
    CFRELEASE_NULL(cferr);
    CFRELEASE_NULL(uuid_cfuuid);

    if (minor_status)
	*minor_status = ret;
    return GSS_S_FAILURE;

#endif
}
