/*
 * Copyright (c) 2013 Kungliga Tekniska Högskolan
 * (Royal Institute of Technology, Stockholm, Sweden).
 * All rights reserved.
 *
 * Portions Copyright (c) 2013 Apple Inc. All rights reserved.
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

#include "krb5_locl.h"
#include "heimcred.h"

#ifdef HAVE_XCC

#define CFRELEASE_NULL(x) do { if (x) { CFRelease(x); x = NULL; } } while(0)

typedef struct krb5_xcc {
    CFUUIDRef uuid;
    HeimCredRef cred;
    CFStringRef clientName;
    krb5_principal primary_principal;
    char *cache_name;
} krb5_xcc;

struct xcc_cursor {
    CFArrayRef array;
    CFIndex offset;
};

#define XCACHE(X) ((krb5_xcc *)(X)->data.data)

static void
free_cursor(struct xcc_cursor *c)
{
    if (c->array)
	CFRelease(c->array);
    free(c);
}

#if 0
static krb5_error_code
make_cred(krb5_context context,
	  HeimCredRef xcred,
	  krb5_creds *cred)
{
    memset(cred, 0, sizeof(*cred));
    return 0;
}
#endif

static CFStringRef
CFStringCreateFromPrincipal(krb5_context context, krb5_principal principal)
{
    CFStringRef str;
    char *p;
    
    if (krb5_unparse_name(context, principal, &p) != 0)
	return NULL;
    
    str = CFStringCreateWithCString(NULL, p, kCFStringEncodingUTF8);
    krb5_xfree(p);
    
    return str;
}

static krb5_principal
PrincipalFromCFString(krb5_context context, CFStringRef string)
{
    krb5_principal principal = NULL;
    char *p = rk_cfstring2cstring(string);
    if (p == NULL)
	return NULL;
    
    (void)krb5_parse_name(context, p, &principal);
    free(p);
    return principal;
}


static const char* KRB5_CALLCONV
xcc_get_name(krb5_context context,
	     krb5_ccache id)
{
    krb5_xcc *a = XCACHE(id);
    return a->cache_name;
}

static krb5_error_code KRB5_CALLCONV
xcc_alloc(krb5_context context, krb5_ccache *id)
{
    (*id)->data.data = calloc(1, sizeof(krb5_xcc));
    (*id)->data.length = sizeof(krb5_xcc);

    return 0;
}

static void
genName(krb5_xcc *x)
{
    if (x->cache_name)
	return;
    CFUUIDBytes bytes = CFUUIDGetUUIDBytes(x->uuid);
    x->cache_name = malloc(37);
    uuid_unparse((void *)&bytes, x->cache_name);
}

static krb5_error_code KRB5_CALLCONV
xcc_resolve(krb5_context context, krb5_ccache *id, const char *res)
{
    krb5_error_code ret;
    CFUUIDBytes bytes;
    krb5_xcc *x;

    if (uuid_parse(res, (void *)&bytes) != 0) {
	krb5_set_error_message(context, KRB5_CC_END, "failed to parse uuid: %s", res);
	return KRB5_CC_END;
    }
    
    CFUUIDRef uuidref = CFUUIDCreateFromUUIDBytes(NULL, bytes);
    if (uuidref == NULL) {
	krb5_set_error_message(context, KRB5_CC_END, "failed to create uuid from: %s", res);
	return KRB5_CC_END;
    }

    ret = xcc_alloc(context, id);
    if (ret) {
	CFRELEASE_NULL(uuidref);
	return ret;
    }
    
    x = XCACHE((*id));
    
    x->uuid = uuidref;
    genName(x);

    return 0;
}

static krb5_error_code
xcc_create(krb5_context context, krb5_xcc *x, CFUUIDRef uuid)
{
    const void *keys[] = {
    	(void *)kHEIMAttrCredentialGroupLead,
	(void *)kHEIMAttrType,
	(void *)kHEIMAttrUUID
    };
    const void *values[] = {
	(void *)kCFBooleanTrue,
	(void *)kHEIMTypeKerberos,
	(void *)uuid
    };
    CFDictionaryRef attrs;
    krb5_error_code ret;
    
    CFIndex num_keys = sizeof(keys)/sizeof(keys[0]);
    if (uuid == NULL)
	num_keys -= 1;
   
    attrs = CFDictionaryCreate(NULL, keys, values, num_keys, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    if (attrs == NULL) abort();
    
    /*
     * Contract with HeimCredCreate is that it set parent and group uuid's when they are not set on the credential
     */
    x->cred = HeimCredCreate(attrs, NULL);
    CFRelease(attrs);
    if (x->cred) {
	if (x->uuid) abort();
	x->uuid = HeimCredGetUUID(x->cred);
	if (x->uuid == NULL) abort();
	CFRetain(x->uuid);

	ret = 0;
	genName(x);
    } else
	ret = ENOMEM;

    return ret;
}

static krb5_error_code KRB5_CALLCONV
xcc_gen_new(krb5_context context, krb5_ccache *id)
{
    krb5_error_code ret;
    krb5_xcc *x;
    
    ret = xcc_alloc(context, id);
    if (ret)
	return ret;
    
    x = XCACHE(*id);

    ret = xcc_create(context, x, NULL);
	
    return ret;
}

static krb5_error_code KRB5_CALLCONV
xcc_initialize(krb5_context context,
	       krb5_ccache id,
	       krb5_principal primary_principal)
{
    krb5_xcc *x = XCACHE(id);
    krb5_error_code ret;

    if (x->primary_principal)
	krb5_free_principal(context, x->primary_principal);
    
    ret = krb5_copy_principal(context, primary_principal, &x->primary_principal);
    if (ret)
	return ret;

    CFRELEASE_NULL(x->clientName);

    x->clientName = CFStringCreateFromPrincipal(context, primary_principal);
    if (x->clientName == NULL)
	return krb5_enomem(context);

    if (x->cred == NULL) {
	ret = xcc_create(context, x, x->uuid);
	if (ret)
	    return ret;
    }
    if (!HeimCredSetAttribute(x->cred, kHEIMAttrClientName, x->clientName, NULL)) {\
	ret = EINVAL;
	krb5_set_error_message(context, ret, "failed to store credential to %s", x->cache_name);
    }


    return ret;
}

static krb5_error_code KRB5_CALLCONV
xcc_close(krb5_context context,
	  krb5_ccache id)
{
    krb5_xcc *x = XCACHE(id);
    krb5_free_principal(context, x->primary_principal);
    CFRELEASE_NULL(x->uuid);
    CFRELEASE_NULL(x->cred);
    free(x->cache_name);
    return 0;
}

static krb5_error_code KRB5_CALLCONV
xcc_destroy(krb5_context context,
	    krb5_ccache id)
{
    krb5_xcc *x = XCACHE(id);
    if (x->uuid)
	HeimCredDeleteByUUID(x->uuid);
    CFRELEASE_NULL(x->cred);

    return 0;
}

static krb5_error_code KRB5_CALLCONV
xcc_store_cred(krb5_context context,
	       krb5_ccache id,
	       krb5_creds *creds)
{
    krb5_xcc *x = XCACHE(id);
    krb5_storage *sp = NULL;
    CFDataRef dref = NULL;
    krb5_data data;
    CFStringRef principal = NULL;
    CFDictionaryRef query = NULL;
    krb5_error_code ret;
    
    krb5_data_zero(&data);
    
    sp = krb5_storage_emem();
    if (sp == NULL) {
	ret = krb5_enomem(context);
	goto out;
    }
    
    ret = krb5_store_creds(sp, creds);
    if (ret)
	goto out;
    
    krb5_storage_to_data(sp, &data);
    
    dref = CFDataCreateWithBytesNoCopy(NULL, data.data, data.length, kCFAllocatorNull);
    if (dref == NULL) {
	ret = krb5_enomem(context);
	goto out;
    }
    
    principal = CFStringCreateFromPrincipal(context, creds->server);
    if (principal == NULL) {
	ret = krb5_enomem(context);
	goto out;
    }
    
    const void *add_keys[] = {
	kHEIMAttrType,
	kHEIMAttrClientName,
	kHEIMAttrServerName,
	kHEIMAttrData,
	kHEIMAttrCredentialGroup,
	kHEIMAttrParentCredential,
    };
    const void *add_values[] = {
	kHEIMTypeKerberos,
	x->clientName,
	principal,
	dref,
	x->uuid,
	x->uuid,
    };
    
    query = CFDictionaryCreate(NULL, add_keys, add_values, sizeof(add_keys) / sizeof(add_keys[0]), NULL, NULL);
    heim_assert(query != NULL, "out of memory");
    
    HeimCredRef ccred = HeimCredCreate(query, NULL);
    if (ccred) {
	CFRelease(ccred);
    } else {
	_krb5_debugx(context, 5, "failed to add credential to %s\n", x->cache_name);
	ret = EINVAL;
	krb5_set_error_message(context, ret, "failed to store credential to %s", x->cache_name);
	goto out;
    }
    
out:
    if (sp)
	krb5_storage_free(sp);
    CFRELEASE_NULL(dref);
    CFRELEASE_NULL(principal);
    krb5_data_free(&data);
    
    return ret;
}

static krb5_error_code KRB5_CALLCONV
xcc_get_principal(krb5_context context,
		  krb5_ccache id,
		  krb5_principal *principal)
{
    krb5_xcc *x = XCACHE(id);
    if (x->cred == NULL) {
	x->cred = HeimCredCopyFromUUID(x->uuid);
	if (x->cred == NULL) {
	    krb5_set_error_message(context, KRB5_CC_END, "no credential for %s", x->cache_name);
	    return KRB5_CC_END;
	}
    }
    if (x->clientName == NULL) {
	x->clientName = HeimCredCopyAttribute(x->cred, kHEIMAttrClientName);
	if (x->clientName == NULL) {
	    krb5_set_error_message(context, KRB5_CC_END, "no principal name for %s", x->cache_name);
	    return KRB5_CC_END;
	}
    }
    if (x->primary_principal == NULL) {
	x->primary_principal = PrincipalFromCFString(context, x->clientName);
	if (x->primary_principal == NULL) {
	    krb5_set_error_message(context, KRB5_CC_END, "no principal for %s", x->cache_name);
	    return KRB5_CC_END;
	}
    }
	    
    return krb5_copy_principal(context, x->primary_principal, principal);
}

static krb5_error_code KRB5_CALLCONV
xcc_get_first (krb5_context context,
	       krb5_ccache id,
	       krb5_cc_cursor *cursor)
{
    CFDictionaryRef query;
    krb5_xcc *x = XCACHE(id);
    CFUUIDRef uuid = HeimCredGetUUID(x->cred);
    struct xcc_cursor *c;
    
    c = calloc(1, sizeof(*c));
    if (c == NULL)
	return krb5_enomem(context);
    
    const void *keys[] = { (void *)kHEIMAttrCredentialGroup, kHEIMAttrType };
    const void *values[] = { (void *)uuid, kHEIMTypeKerberos };
    
    query = CFDictionaryCreate(NULL, keys, values, sizeof(keys)/sizeof(keys[0]), &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    if (query == NULL) abort();
    
    c->array = HeimCredCopyQuery(query);
    CFRELEASE_NULL(query);
    if (c->array == NULL) {
	free_cursor(c);
	return KRB5_CC_END;
    }
    
    *cursor = c;
    
    return 0;
}


static krb5_error_code KRB5_CALLCONV
xcc_get_next (krb5_context context,
	      krb5_ccache id,
	      krb5_cc_cursor *cursor,
	      krb5_creds *creds)
{
    struct xcc_cursor *c = *cursor;
    krb5_error_code ret;
    krb5_storage *sp;
    HeimCredRef cred;
    CFDataRef data;

    if (c->array == NULL)
	return KRB5_CC_END;

next:
    if (c->offset >= CFArrayGetCount(c->array))
	return KRB5_CC_END;
    
    cred = (HeimCredRef)CFArrayGetValueAtIndex(c->array, c->offset++);
    if (cred == NULL)
	return KRB5_CC_END;
    
    data = HeimCredCopyAttribute(cred, kHEIMAttrData);
    if (data == NULL)
	goto next;
    
    sp = krb5_storage_from_readonly_mem(CFDataGetBytePtr(data), CFDataGetLength(data));
    if (sp == NULL) {
	CFRELEASE_NULL(data);
	return KRB5_CC_END;
    }
    
    ret = krb5_ret_creds(sp, creds);
    krb5_storage_free(sp);
    CFRELEASE_NULL(data);
    
    return ret;
}

static krb5_error_code KRB5_CALLCONV
xcc_end_get (krb5_context context,
	     krb5_ccache id,
	     krb5_cc_cursor *cursor)
{
    free_cursor((struct xcc_cursor *)*cursor);
    *cursor = NULL;
    
    return 0;
}

static krb5_error_code KRB5_CALLCONV
xcc_remove_cred(krb5_context context,
		krb5_ccache id,
		krb5_flags which,
		krb5_creds *cred)
{
    CFDictionaryRef query;
    krb5_xcc *x = XCACHE(id);

    CFStringRef servername = CFStringCreateFromPrincipal(context, cred->server);
    if (servername == NULL)
	return KRB5_CC_END;
    
    const void *keys[] = { (void *)kHEIMAttrCredentialGroup, kHEIMAttrType, kHEIMAttrServerName };
    const void *values[] = { (void *)x->uuid, kHEIMTypeKerberos, servername };
    
    /* XXX match enctype */
    
    query = CFDictionaryCreate(NULL, keys, values, sizeof(keys)/sizeof(keys[0]), &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    if (query == NULL) abort();
    
    CFRELEASE_NULL(servername);

    bool res = HeimCredDeleteQuery(query, NULL);
    CFRELEASE_NULL(query);

    if (!res)
	return KRB5_CC_END;
    return 0;
}

static krb5_error_code KRB5_CALLCONV
xcc_set_flags(krb5_context context,
	      krb5_ccache id,
	      krb5_flags flags)
{
    return 0;
}

static int KRB5_CALLCONV
xcc_get_version(krb5_context context,
		krb5_ccache id)
{
    return 0;
}

static krb5_error_code KRB5_CALLCONV
xcc_get_cache_first(krb5_context context, krb5_cc_cursor *cursor)
{
    CFDictionaryRef query;
    struct xcc_cursor *c;

    const void *keys[] = {
    	(void *)kHEIMAttrCredentialGroupLead,
	(void *)kHEIMAttrType,
    };
    const void *values[] = {
	(void *)kCFBooleanTrue,
	(void *)kHEIMTypeKerberos,
    };
    
    c = calloc(1, sizeof(*c));
    if (c == NULL)
	return krb5_enomem(context);

    query = CFDictionaryCreate(NULL, keys, values, sizeof(keys)/sizeof(keys[0]), &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    if (query == NULL) abort();
    
    c->array = HeimCredCopyQuery(query);
    CFRELEASE_NULL(query);
    if (c->array == NULL) {
	free_cursor(c);
	return KRB5_CC_END;
    }
    *cursor = c;
    return 0;
}

static krb5_error_code KRB5_CALLCONV
xcc_get_cache_next(krb5_context context, krb5_cc_cursor cursor, krb5_ccache *id)
{
    struct xcc_cursor *c = cursor;
    krb5_error_code ret;
    HeimCredRef cred;
    krb5_xcc *x;
    
    if (c->array == NULL)
	return KRB5_CC_END;
    
    if (c->offset >= CFArrayGetCount(c->array))
	return KRB5_CC_END;
    
    cred = (HeimCredRef)CFArrayGetValueAtIndex(c->array, c->offset++);
    if (cred == NULL)
	return KRB5_CC_END;
    
    ret = _krb5_cc_allocate(context, &krb5_xcc_ops, id);
    if (ret)
	return ret;
    
    xcc_alloc(context, id);
    x = XCACHE((*id));
    
    x->uuid = HeimCredGetUUID(cred);
    CFRetain(x->uuid);
    x->cred = cred;
    CFRetain(cred);
    genName(x);
    
    return ret;
}

static krb5_error_code KRB5_CALLCONV
xcc_end_cache_get(krb5_context context, krb5_cc_cursor cursor)
{
    free_cursor((struct xcc_cursor *)cursor);
    return 0;
}

static krb5_error_code KRB5_CALLCONV
xcc_move(krb5_context context, krb5_ccache from, krb5_ccache to)
{
    krb5_xcc *xfrom = XCACHE(from);
    krb5_xcc *xto = XCACHE(to);
    
    if (!HeimCredMove(xfrom->uuid, xto->uuid))
	return KRB5_CC_END;

    CFRELEASE_NULL(xto->cred);
    CFRELEASE_NULL(xfrom->cred);

    free(xto->cache_name);
    xto->cache_name = NULL;
    genName(xto);

    CFRELEASE_NULL(xto->clientName);
    xto->clientName = xfrom->clientName;
    xfrom->clientName = NULL;

    if (xto->primary_principal)
	krb5_free_principal(context, xto->primary_principal);
    xto->primary_principal = xfrom->primary_principal;
    xfrom->primary_principal = NULL;
    
    return 0;
}

static krb5_error_code KRB5_CALLCONV
xcc_get_default_name(krb5_context context, char **str)
{
    krb5_set_error_message(context, EINVAL, "XCACHE doesn't have a default name");
    return EINVAL;
}

static krb5_error_code KRB5_CALLCONV
xcc_set_default(krb5_context context, krb5_ccache id)
{
    krb5_set_error_message(context, EINVAL, "XCACHE doesn't have a default name");
    return EINVAL;
}

static krb5_error_code KRB5_CALLCONV
xcc_lastchange(krb5_context context, krb5_ccache id, krb5_timestamp *mtime)
{
    *mtime = 0;
    return 0;
}

static krb5_error_code
xcc_get_uuid(krb5_context context, krb5_ccache id, krb5_uuid uuid)
{
    krb5_xcc *x = XCACHE(id);
    CFUUIDBytes bytes = CFUUIDGetUUIDBytes(x->uuid);
    memcpy(uuid, &bytes, sizeof(krb5_uuid));
    return 0;
}

static krb5_error_code
xcc_resolve_by_uuid(krb5_context context, krb5_ccache id, krb5_uuid uuid)
{
    krb5_error_code ret;
    CFUUIDBytes bytes;
    krb5_xcc *x;

    memcpy(&bytes, uuid, sizeof(bytes));
    
    CFUUIDRef uuidref = CFUUIDCreateFromUUIDBytes(NULL, bytes);
    if (uuidref == NULL) {
	krb5_set_error_message(context, KRB5_CC_END, "failed to create uuid");
	return KRB5_CC_END;
    }
    
    ret = xcc_alloc(context, &id);
    if (ret) {
	CFRELEASE_NULL(uuidref);
	return ret;
    }
    
    x = XCACHE(id);
    
    x->uuid = uuidref;
    genName(x);
    
    return 0;
}

static krb5_error_code
xcc_set_acl(krb5_context context, krb5_ccache id, const char *type, /* heim_object_t */ void *obj)
{
    krb5_xcc *x = XCACHE(id);
    bool res;

    CFStringRef t = CFStringCreateWithCString(NULL, type, kCFStringEncodingUTF8);
    if (t == NULL)
	return krb5_enomem(context);

    res = HeimCredSetAttribute(x->cred, t, obj, NULL);
    CFRELEASE_NULL(t);

    if (!res)
	return KRB5_CC_END;

    return 0;
}

/**
 * Variable containing the XCACHE based credential cache implemention.
 *
 * @ingroup krb5_ccache
 */

KRB5_LIB_VARIABLE const krb5_cc_ops krb5_xcc_ops = {
    KRB5_CC_OPS_VERSION,
    "XCACHE",
    xcc_get_name,
    xcc_resolve,
    xcc_gen_new,
    xcc_initialize,
    xcc_destroy,
    xcc_close,
    xcc_store_cred,
    NULL, /* acc_retrieve */
    xcc_get_principal,
    xcc_get_first,
    xcc_get_next,
    xcc_end_get,
    xcc_remove_cred,
    xcc_set_flags,
    xcc_get_version,
    xcc_get_cache_first,
    xcc_get_cache_next,
    xcc_end_cache_get,
    xcc_move,
    xcc_get_default_name,
    xcc_set_default,
    xcc_lastchange,
    NULL, /* set_kdc_offset */
    NULL, /* get_kdc_offset */
    NULL, /* hold */
    NULL, /* unhold */
    xcc_get_uuid,
    xcc_resolve_by_uuid,
    NULL,
    NULL,
    xcc_set_acl
};

#endif /* HAVE_XCC */
