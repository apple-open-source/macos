/*
 * Copyright (c) 2010 Kungliga Tekniska Högskolan
 * (Royal Institute of Technology, Stockholm, Sweden).
 * All rights reserved.
 *
 * Portions Copyright (c) 2010 Apple Inc. All rights reserved.
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

#ifdef __APPLE__

#include <CoreFoundation/CoreFoundation.h>
#include <Security/Security.h>
#include <uuid/uuid.h>

/*
 *  kSecAttrDescription
 *          'Kerberos ticket for %@'
 *  kSecAttrServer
 *          the name of the credentials cache
 *  kSecAttrCreator
 *       'GSSL'
 *  kSecAttrType
 *       'iTGT'
 *       'oTGT'
 *  kSecAttrLabel
 *       'Kerberos ticket for %@'
 *  kSecAttrIsInvisible
 *       'Yes'
 *  kSecAttrIsNegative
 *       set on the keychain that is default
 *  kSecAttrAccount
 *       The service principal name
 *  kSecAttrService
 *       'GSS-API'
 *  kSecAttrGeneric
 *       iTGT
 *          kdc offset: int32_t
 *       oTGT
 *       'The kerberos ticket'
 */


typedef struct krb5_kcache{
    char *name;
    CFStringRef cname;
    int version;
}krb5_kcache;

#define KCACHE(X) ((krb5_kcache*)(X)->data.data)

struct kcc_cursor {
    CFArrayRef array;
    CFIndex offset;
};

static CFTypeRef kGSSiTGT;
static CFTypeRef kGSSoTGT;
static CFTypeRef kGSSCreator;
#ifndef __APPLE_TARGET_EMBEDDED__
static SecAccessRef kGSSAnyAccess;
#endif

static void
create_constants(void)
{
    static dispatch_once_t once;
    dispatch_once(&once, ^{
	    int32_t num;

	    num = 'iTGT';
	    kGSSiTGT = CFNumberCreate(NULL, kCFNumberSInt32Type, &num);
	    num = 'oTGT';
	    kGSSoTGT = CFNumberCreate(NULL, kCFNumberSInt32Type, &num);
	    num = 'GSSL';
	    kGSSCreator = CFNumberCreate(NULL, kCFNumberSInt32Type, &num);
		
#ifndef __APPLE_TARGET_EMBEDDED__
	    OSStatus ret;

	    ret = SecAccessCreate(CFSTR("GSS-credential"), NULL, &kGSSAnyAccess);
	    if (ret)
		return;
		
	    SecKeychainPromptSelector promptSelector;
	    CFStringRef promptDescription = NULL;
	    CFArrayRef appList = NULL, aclList = NULL;
	    SecACLRef acl;

	    aclList = SecAccessCopyMatchingACLList(kGSSAnyAccess, kSecACLAuthorizationDecrypt);
	    if (aclList == NULL)
		return;

	    if (CFArrayGetCount(aclList) < 1) {
		CFRelease(aclList);
		return;
	    }

	    acl = (SecACLRef)CFArrayGetValueAtIndex(aclList, 0);

	    ret = SecACLCopyContents(acl, &appList, &promptDescription, &promptSelector);
	    if (ret == 0) {
		promptSelector &= ~kSecKeychainPromptRequirePassphase;
		(void)SecACLSetContents(acl, NULL, promptDescription, promptSelector);
	    }

	    CFRelease(promptDescription);
	    CFRelease(appList);
	    CFRelease(aclList);
#endif
	});
}

static void
delete_type(krb5_context context, krb5_kcache *k)
{
#ifndef __APPLE_TARGET_EMBEDDED__
#define USE_DELETE 1 /* rdar://8736785 */
#endif

    const void *keys[] = {
#ifdef USE_DELETE
	kSecReturnRef,
#endif
	kSecClass,
	kSecAttrCreator,
	kSecAttrServer,
    };
    const void *values[] = {
#ifdef USE_DELETE
	kCFBooleanTrue,
#endif
	kSecClassInternetPassword,
	kGSSCreator,
	k->cname,
    };
    CFDictionaryRef query;
    OSStatus ret;
	
    query = CFDictionaryCreate(NULL, keys, values, sizeof(keys) / sizeof(keys[0]), NULL, NULL);
    heim_assert(query != NULL, "out of memory");

#ifdef USE_DELETE
    CFArrayRef array;

    ret = SecItemCopyMatching(query, (CFTypeRef *)&array);
    if (ret == 0) {
	CFIndex n, count = CFArrayGetCount(array);

	for (n = 0; n < count; n++)
	    SecKeychainItemDelete((SecKeychainItemRef)CFArrayGetValueAtIndex(array, n));
	CFRelease(array);
    }

#else
    ret = SecItemDelete(query);
    if (ret)
	_krb5_debugx(context, 5, "failed to delete credential %s: %d\n", k->name, (int)ret);
#endif
    CFRelease(query);
}

static CFDictionaryRef
find_cache(krb5_kcache *k)
{
    CFDictionaryRef result = NULL, query = NULL;
    OSStatus ret;
	
    create_constants();
	
    const void *keys[] = {
	kSecClass,
	kSecReturnAttributes,
	kSecAttrType,
	kSecAttrServer
    };
    const void *values[] = {
	kSecClassInternetPassword,
	kCFBooleanTrue,
	kGSSiTGT,
	k->cname
    };
	
    query = CFDictionaryCreate(NULL, keys, values, sizeof(keys) / sizeof(keys[0]),
			       &kCFTypeDictionaryKeyCallBacks,
			       &kCFTypeDictionaryValueCallBacks);
    heim_assert(query != NULL, "out of memory");
	
    ret = SecItemCopyMatching(query, (CFTypeRef *)&result);
    CFRelease(query);
    if (ret)
	return NULL;

    return result;
}

static const char*
kcc_get_name(krb5_context context,
	     krb5_ccache id)
{
    krb5_kcache *k = KCACHE(id);
    return k->name;
}

static krb5_error_code
kcc_resolve(krb5_context context, krb5_ccache *id, const char *res)
{
    krb5_kcache *k;
	
    k = malloc(sizeof(*k));
    if (k == NULL) {
	krb5_set_error_message(context, KRB5_CC_NOMEM,
			       N_("malloc: out of memory", ""));
	return KRB5_CC_NOMEM;
    }
    k->name = strdup(res);
    if (k->name == NULL){
	free(k);
	krb5_set_error_message(context, KRB5_CC_NOMEM,
			       N_("malloc: out of memory", ""));
	return KRB5_CC_NOMEM;
    }
    k->cname = CFStringCreateWithCString(NULL, k->name, kCFStringEncodingUTF8);
    if (k->cname == NULL) {
	free(k->name);
	free(k);
	krb5_set_error_message(context, KRB5_CC_NOMEM,
			       N_("malloc: out of memory", ""));
	return KRB5_CC_NOMEM;
		
    }
    (*id)->data.data = k;
    (*id)->data.length = sizeof(*k);
    return 0;
}

static krb5_error_code
kcc_gen_new(krb5_context context, krb5_ccache *id)
{
    char *file = NULL;
    krb5_error_code ret;
    krb5_kcache *k;
    uuid_t uuid;
    uuid_string_t uuidstr;

    k = malloc(sizeof(*k));
    if (k == NULL) {
	krb5_set_error_message(context, KRB5_CC_NOMEM,
			       N_("malloc: out of memory", ""));
	return KRB5_CC_NOMEM;
    }
    
    uuid_generate_random(uuid);
    uuid_unparse(uuid, uuidstr);

    ret = asprintf(&file, "unique-%s", uuidstr);
    if (ret <= 0 || file == NULL) {
	free(k);
	krb5_set_error_message(context, KRB5_CC_NOMEM,
			       N_("malloc: out of memory", ""));
	return KRB5_CC_NOMEM;
    }

    k->name = file;
    k->cname = CFStringCreateWithCString(NULL, k->name, kCFStringEncodingUTF8);
    if (k->cname == NULL) {
	free(k->name);
	free(k);
	krb5_set_error_message(context, KRB5_CC_NOMEM,
			       N_("malloc: out of memory", ""));
	return KRB5_CC_NOMEM;
		
    }
	
    (*id)->data.data = k;
    (*id)->data.length = sizeof(*k);
    return 0;
}

static CFStringRef
CFStringCreateFromPrincipal(CFAllocatorRef alloc, krb5_context context, krb5_principal principal)
{
    CFStringRef str;
    char *p;
	
    if (krb5_unparse_name(context, principal, &p) != 0)
	return NULL;

    str = CFStringCreateWithCString(alloc, p, kCFStringEncodingUTF8);
    krb5_xfree(p);

    return str;
}


static krb5_error_code
kcc_initialize(krb5_context context,
	       krb5_ccache id,
	       krb5_principal primary_principal)
{
    krb5_error_code ret = 0;
    krb5_kcache *k = KCACHE(id);
    CFDictionaryRef query;
    CFTypeRef item = NULL;
    CFStringRef principal = NULL;
    CFStringRef label = NULL;
    OSStatus osret;

    create_constants();
   
    /* 
       delete all entries for
	 
       kSecAttrServer == k->name
    */
	
    delete_type(context, k);
	
    /*
      add entry with
	 
      kSecAttrServer, k->name
      kSecAttrLabel, "credential for %@", primary_principal
      kSecAttrType, ITGT
      kSecAttrAccount, primary_principal
    */
	
    principal = CFStringCreateFromPrincipal(NULL, context, primary_principal);
    if (principal == NULL) {
	ret = ENOMEM;
	goto out;
    }

    label = CFStringCreateWithFormat(NULL, NULL, CFSTR("Kerberos credentials for %@"), principal);
    if (label == NULL) {
	ret = ENOMEM;
	goto out;
    }

    const void *add_keys[] = {
	kSecAttrCreator,
#ifdef __APPLE_TARGET_EMBEDDED__
	kSecAttrAccessible,
#else
	kSecAttrAccess,
#endif
	kSecClass,
	kSecAttrType,
	kSecAttrServer,
	kSecAttrAccount,
	kSecAttrLabel,
	kSecAttrIsInvisible
    };
    const void *add_values[] = {
	kGSSCreator,
#ifdef __APPLE_TARGET_EMBEDDED__
	kSecAttrAccessibleAfterFirstUnlock,
#else
	kGSSAnyAccess,
#endif
	kSecClassInternetPassword,
	kGSSiTGT,
	k->cname,
	principal,
	label,
	kCFBooleanTrue
    };
	
    query = CFDictionaryCreate(NULL, add_keys, add_values, sizeof(add_keys) / sizeof(add_keys[0]), NULL, NULL);
    heim_assert(query != NULL, "out of memory");


    osret = SecItemAdd(query, &item);
    CFRelease(query);
    if (osret) {
	_krb5_debugx(context, 5, "failed to store credential: %d\n", (int)osret);
	ret = EINVAL;
	krb5_set_error_message(context, ret, "failed to store credential: %d", (int)osret);
	goto out;
    }

 out:
    if (label)
	CFRelease(label);
    if (item)
	CFRelease(item);
    if (principal)
	CFRelease(principal);

    return ret;
}

static krb5_error_code
kcc_close(krb5_context context,
	  krb5_ccache id)
{
    krb5_kcache *k = KCACHE(id);

    if (k->cname)
	CFRelease(k->cname);
    free(k->name);
    krb5_data_free(&id->data);
    return 0;
}

static krb5_error_code
kcc_destroy(krb5_context context,
	    krb5_ccache id)
{
    krb5_kcache *k = KCACHE(id);

    create_constants();

    delete_type(context, k);
	
    return 0;
}

static krb5_error_code
kcc_store_cred(krb5_context context,
	       krb5_ccache id,
	       krb5_creds *creds)
{
    krb5_kcache *k = KCACHE(id);
    krb5_storage *sp = NULL;
    CFDataRef dref = NULL;
    krb5_data data;
    CFStringRef principal = NULL;
    CFStringRef label = NULL;
    CFDictionaryRef query = NULL;
    krb5_error_code ret;
    CFTypeRef item = NULL;
	
    create_constants();

    krb5_data_zero(&data);

    sp = krb5_storage_emem();
    if (sp == NULL) {
	ret = ENOMEM;
	goto out;
    }

    ret = krb5_store_creds(sp, creds);
    if (ret)
	goto out;

    krb5_storage_to_data(sp, &data);
	
    dref = CFDataCreateWithBytesNoCopy(NULL, data.data, data.length, kCFAllocatorNull);
    if (dref == NULL) {
	ret = ENOMEM;
	goto out;
    }
	
    principal = CFStringCreateFromPrincipal(NULL, context, creds->server);
    if (principal == NULL) {
	ret = ENOMEM;
	goto out;
    }

    label = CFStringCreateWithFormat(NULL, NULL, CFSTR("kerberos credentials for %@"), principal);
    if (label == NULL) {
	ret = ENOMEM;
	goto out;
    }

    /*
      store

      kSecAttrServer, k->name
      kSecAttrLabel, "credential for %@", cred.server
      kSecAttrType, ITGT
      kSecAttrAccount, cred.server
      kSecAttrGeneric, data
    */
	
    const void *add_keys[] = {
	kSecAttrCreator,
#ifdef __APPLE_TARGET_EMBEDDED__
	kSecAttrAccessible,
#else
	kSecAttrAccess,
#endif
	kSecClass,
	kSecAttrType,
	kSecAttrServer,
	kSecAttrAccount,
	kSecAttrLabel,
	kSecValueData,
	kSecAttrIsInvisible
    };
    const void *add_values[] = {
	kGSSCreator,
#ifdef __APPLE_TARGET_EMBEDDED__
	kSecAttrAccessibleAfterFirstUnlock,
#else
	kGSSAnyAccess,
#endif
	kSecClassInternetPassword,
	kGSSoTGT,
	k->cname,
	principal,
	label,
	dref,
	kCFBooleanTrue
    };
	
    query = CFDictionaryCreate(NULL, add_keys, add_values, sizeof(add_keys) / sizeof(add_keys[0]), NULL, NULL);
    heim_assert(query != NULL, "out of memory");
	
    ret = SecItemAdd(query, &item);
    CFRelease(query);
    if (ret) {
	_krb5_debugx(context, 5, "failed to add credential to %s: %d\n", k->name, (int)ret);
	ret = EINVAL;
	krb5_set_error_message(context, ret, "failed to store credential to %s", k->name);
	goto out;
    }

 out:
    if (item)
	CFRelease(item);
    if (sp)
	krb5_storage_free(sp);
    if (dref)
	CFRelease(dref);
    if (principal)
	CFRelease(principal);
    if (label)
	CFRelease(label);
    krb5_data_free(&data);

    return ret;
}

static krb5_error_code
kcc_get_principal(krb5_context context,
		  krb5_ccache id,
		  krb5_principal *principal)
{
    krb5_error_code ret = 0;
    krb5_kcache *k = KCACHE(id);
    CFDictionaryRef result;

    /*
      lookup:
      kSecAttrServer, k->name
      kSecAttrType, ITGT

      to get:

      kSecAttrAccount, primary_principal
    */
	
	
    result = find_cache(k);
    if (result == NULL) {
	krb5_set_error_message(context, KRB5_CC_END,
			       "failed finding cache: %s", k->name);
	ret = KRB5_CC_END;
	goto out;
    }
	
    CFStringRef p = CFDictionaryGetValue(result, kSecAttrAccount);
    if (p == NULL) {
	ret = KRB5_CC_END;
	goto out;
    }
	
    char *str = rk_cfstring2cstring(p);
    if (str == NULL) {
	ret = ENOMEM;
	goto out;
    }
	
    krb5_parse_name(context, str, principal);
    free(str);
	
 out:	
    if (result)
	CFRelease(result);
	
    return ret;
}

static void
free_cursor(struct kcc_cursor *c)
{
    if (c->array)
	CFRelease(c->array);
    free(c);
}

static krb5_error_code
kcc_get_first (krb5_context context,
	       krb5_ccache id,
	       krb5_cc_cursor *cursor)
{
    krb5_error_code ret;
    krb5_kcache *k = KCACHE(id);
    CFDictionaryRef query = NULL;
    CFArrayRef result = NULL;
    struct kcc_cursor *c;
	
    create_constants();
	
    c = calloc(1, sizeof(*c));
    if (c == NULL) {
        krb5_set_error_message(context, ENOMEM, N_("malloc: out of memory", ""));
	return ENOMEM;
    }

    /*
      lookup:
      kSecAttrServer, k->name
      kSecAttrType, OTGT
    */

    const void *keys[] = {
	kSecClass,
	kSecReturnData,
	kSecMatchLimit,
	kSecAttrType,
	kSecAttrServer
    };
    const void *values[] = {
	kSecClassInternetPassword,
	kCFBooleanTrue,
	kSecMatchLimitAll,
	kGSSoTGT,
	k->cname
    };
	
    query = CFDictionaryCreate(NULL, keys, values, sizeof(keys) / sizeof(keys[0]),
			       &kCFTypeDictionaryKeyCallBacks, & kCFTypeDictionaryValueCallBacks);
    heim_assert(query != NULL, "out of memory");
	
    ret = SecItemCopyMatching(query, (CFTypeRef *)&result);
    if (ret) {
	result = NULL;
	ret = 0;
	goto out;
    }
    if (CFGetTypeID(result) != CFArrayGetTypeID()) {
	CFRelease(result);
	ret = 0;
	result = NULL;
    }
	
    /* sort by creation date */
	
    c->array = result;
    c->offset = 0;
	
 out:
    if (query)
	CFRelease(query);
    if (ret) {
	free_cursor(c);
    } else {
	*cursor = c;
    }
	
    return ret;
}

static krb5_error_code
kcc_get_next (krb5_context context,
	      krb5_ccache id,
	      krb5_cc_cursor *cursor,
	      krb5_creds *creds)
{
    struct kcc_cursor *c = *cursor;
    krb5_error_code ret;
    krb5_storage *sp;
    CFDataRef data;
	
    if (c->array == NULL)
	return KRB5_CC_END;

    if (c->offset >= CFArrayGetCount(c->array))
	return KRB5_CC_END;
	
    data = CFArrayGetValueAtIndex(c->array, c->offset++);
    if (data == NULL)
	return KRB5_CC_END;

    sp = krb5_storage_from_readonly_mem(CFDataGetBytePtr(data), CFDataGetLength(data));
    if (sp == NULL)
	return KRB5_CC_END;
	    
    ret = krb5_ret_creds(sp, creds);
    krb5_storage_free(sp);

    return ret;
}

static krb5_error_code
kcc_end_get (krb5_context context,
	     krb5_ccache id,
	     krb5_cc_cursor *cursor)
{
    free_cursor((struct kcc_cursor *)*cursor);
    *cursor = NULL;

    return 0;
}

static krb5_error_code
kcc_remove_cred(krb5_context context,
		krb5_ccache id,
		krb5_flags which,
		krb5_creds *cred)
{
    /* lookup cred and delete */

    return 0;
}

static krb5_error_code
kcc_set_flags(krb5_context context,
	      krb5_ccache id,
	      krb5_flags flags)
{
    return 0; /* XXX */
}

static int
kcc_get_version(krb5_context context,
		krb5_ccache id)
{
    return 0;
}
		
static krb5_error_code
kcc_get_cache_first(krb5_context context, krb5_cc_cursor *cursor)
{
    struct kcc_cursor *c;
    CFArrayRef result;
    CFDictionaryRef query;
    OSStatus osret;
	
    create_constants();

    c = calloc(1, sizeof(*c));
    if (c == NULL) {
	return ENOMEM;
    }

    const void *keys[] = {
	kSecClass,
	kSecReturnAttributes,
	kSecMatchLimit,
	kSecAttrType,
    };
    const void *values[] = {
	kSecClassInternetPassword,
	kCFBooleanTrue,
	kSecMatchLimitAll,
	kGSSiTGT,
    };
	
    query = CFDictionaryCreate(NULL, keys, values, sizeof(keys) / sizeof(keys[0]),
			       &kCFTypeDictionaryKeyCallBacks, & kCFTypeDictionaryValueCallBacks);
    heim_assert(query != NULL, "out of memory");
	
    osret = SecItemCopyMatching(query, (CFTypeRef *)&result);
    CFRelease(query);
    if (osret)
	result = NULL;
	
    /* sort by creation date */

    c->array = result;
    c->offset = 0;
	
    *cursor = c;
    return 0;
}

static krb5_error_code
kcc_get_cache_next(krb5_context context, krb5_cc_cursor cursor, krb5_ccache *id)
{
    struct kcc_cursor *c = cursor;
    CFDictionaryRef item;
    krb5_error_code ret;
    CFStringRef sref;
    char *name;
	
    if (c->array == NULL)
	return KRB5_CC_END;

    if (c->offset >= CFArrayGetCount(c->array))
	return KRB5_CC_END;
	
    item = CFArrayGetValueAtIndex(c->array, c->offset++);
    if (item == NULL)
	return KRB5_CC_END;
	
    sref = CFDictionaryGetValue(item, kSecAttrServer);
    if (sref == NULL)
	return KRB5_CC_END;
	
    name = rk_cfstring2cstring(sref);
    if (name == NULL)
	return KRB5_CC_NOMEM;
	
    ret = _krb5_cc_allocate(context, &krb5_kcc_ops, id);
    if (ret) {
	free(name);
	return ret;
    }

    ret = kcc_resolve(context, id, name);
    free(name);
    if (ret)
	krb5_cc_close(context, *id);
	
    return ret;
}

static krb5_error_code
kcc_end_cache_get(krb5_context context, krb5_cc_cursor cursor)
{
    free_cursor((struct kcc_cursor *)cursor);
    return 0;
}

static krb5_error_code
kcc_move(krb5_context context, krb5_ccache from, krb5_ccache to)
{
    krb5_kcache *kfrom = KCACHE(from), *kto = KCACHE(to);
    CFDictionaryRef query = NULL, update = NULL;
	
    create_constants();
	
    delete_type(context, kto);

    /* lookup
       kSecAttrServer, k->name

       and rename
    */

    const void *keys[] = {
	kSecClass,
	kSecReturnRef,
	kSecMatchLimit,
	kSecAttrServer
    };
    const void *values[] = {
	kSecClassInternetPassword,
	kCFBooleanTrue,
	kSecMatchLimitAll,
	kfrom->cname
    };
	
    query = CFDictionaryCreate(NULL, keys, values, sizeof(keys) / sizeof(keys[0]),
			       &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    heim_assert(query != NULL, "out of memory");
	

    const void *update_keys[] = {
	kSecAttrServer
    };
    const void *update_values[] = {
	kto->cname
    };

    update = CFDictionaryCreate(NULL, update_keys, update_values, sizeof(update_keys) / sizeof(update_keys[0]),
				&kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    heim_assert(query != NULL, "out of memory");

    (void)SecItemUpdate(query, update);

    CFRelease(query);
    CFRelease(update);

    return 0;
}

static void
set_default(CFStringRef name, bool clear_first)
{
    CFDictionaryRef update = NULL, query = NULL;

    /* search for all
       kSecAttrType, ITGT
       kSecAttrIsNegative, True

       and change to
       to -> kSecAttrIsNegative, false
    */

    /* change kSecAttrIsNegative -> true for id */

    const void *keys[] = {
	kSecClass,
	kSecMatchLimit,
	kSecAttrCreator,
#ifdef __APPLE_TARGET_EMBEDDED__
	kSecAttrAccessible,
#endif
	kSecAttrType,
	kSecAttrServer
    };
    const void *values[] = {
	kSecClassInternetPassword,
	kSecMatchLimitAll,
	kGSSCreator,
#ifdef __APPLE_TARGET_EMBEDDED__
	kSecAttrAccessibleAfterFirstUnlock,
#endif
	kGSSiTGT,
	name
    };
	
    const void *update_keys[] = {
	kSecAttrIsNegative
    };
    const void *update_values_false[] = {
	kCFBooleanFalse
    };
    const void *update_values_true[] = {
	kCFBooleanTrue
    };

    /*
     * Clear all default flags
     */
    if (clear_first) {

	query = CFDictionaryCreate(NULL, keys, values, (sizeof(keys) / sizeof(keys[0])) - 1,
				   &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
	heim_assert(query != NULL, "out of memory");
	

	update = CFDictionaryCreate(NULL, update_keys, update_values_false, sizeof(update_keys) / sizeof(update_keys[0]),
				    &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
	heim_assert(update != NULL, "out of memory");
	
	(void)SecItemUpdate(query, update);

	CFRelease(query);
	CFRelease(update);
    }

    /*
     * Set default flag
     */

    query = CFDictionaryCreate(NULL, keys, values, (sizeof(keys) / sizeof(keys[0])),
			       &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    heim_assert(query != NULL, "out of memory");

    update = CFDictionaryCreate(NULL, update_keys, update_values_true, sizeof(update_keys) / sizeof(update_keys[0]),
				&kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    heim_assert(update != NULL, "out of memory");

    (void)SecItemUpdate(query, update);
    CFRelease(query);
    CFRelease(update);
}


static krb5_error_code
kcc_get_default_name(krb5_context context, char **str)
{
    CFDictionaryRef result = NULL, query = NULL;
    OSStatus ret;
	
    *str = NULL;

    create_constants();

    const void *keys[] = {
	kSecClass,
	kSecReturnAttributes,
#ifdef __APPLE_TARGET_EMBEDDED__
	kSecAttrAccessible,
#endif
	kSecAttrType,
	kSecAttrIsNegative
    };
    const void *values[] = {
	kSecClassInternetPassword,
	kCFBooleanTrue,
#ifdef __APPLE_TARGET_EMBEDDED__
	kSecAttrAccessibleAfterFirstUnlock,
#endif
	kGSSiTGT,
	kCFBooleanTrue
    };
	
    /* lookup
       kSecAttrType, ITGT
       kSecAttrIsNegative, True
    */

    query = CFDictionaryCreate(NULL, keys, values, sizeof(keys) / sizeof(keys[0]),
			       &kCFTypeDictionaryKeyCallBacks,
			       &kCFTypeDictionaryValueCallBacks);
    heim_assert(query != NULL, "out of memory");
	
    ret = SecItemCopyMatching(query, (CFTypeRef *)&result);
    CFRelease(query);

    if (ret) {
	/*
	 * Didn't find one, just pick one
	 */
	query = CFDictionaryCreate(NULL, keys, values, sizeof(keys) / sizeof(keys[0]) - 1,
				   &kCFTypeDictionaryKeyCallBacks,
				   &kCFTypeDictionaryValueCallBacks);
	heim_assert(query != NULL, "out of memory");

	ret = SecItemCopyMatching(query, (CFTypeRef *)&result);
	CFRelease(query);

	if (ret == 0) {
	    CFStringRef name = CFDictionaryGetValue(result, kSecAttrServer);
	    set_default(name, false);
	}
    }

    if (ret == 0) {
	CFStringRef name = CFDictionaryGetValue(result, kSecAttrServer);
	char *n;
	
	if (name == NULL)
	    return ENOMEM;

	n = rk_cfstring2cstring(name);
	if (n == NULL)
	    return ENOMEM;

	asprintf(str, "KCC:%s", n);
	free(n);
	if (*str == NULL)
	    return ENOMEM;
	return 0;
    } else {

	*str = strdup("KCC:default-kcache");
	if (*str == NULL)
	    return ENOMEM;
    }
    return 0;
}

static krb5_error_code
kcc_set_default(krb5_context context, krb5_ccache id)
{
    krb5_kcache *k = KCACHE(id);

    create_constants();
	
    set_default(k->cname, true);

    return 0;
}

static krb5_error_code
kcc_lastchange(krb5_context context, krb5_ccache id, krb5_timestamp *mtime)
{
    /* lookup modification date */
    *mtime = 0;
    return 0;
}

static krb5_error_code
kcc_set_kdc_offset(krb5_context context, krb5_ccache id, krb5_deltat kdc_offset)
{
    return 0;
}

static krb5_error_code
kcc_get_kdc_offset(krb5_context context, krb5_ccache id, krb5_deltat *kdc_offset)
{
    *kdc_offset = 0;
    return 0;
}


/**
 * Variable containing the KEYCHAIN based credential cache implemention.
 *
 * @ingroup krb5_ccache
 */

KRB5_LIB_VARIABLE const krb5_cc_ops krb5_kcc_ops = {
    KRB5_CC_OPS_VERSION,
    "KCC",
    kcc_get_name,
    kcc_resolve,
    kcc_gen_new,
    kcc_initialize,
    kcc_destroy,
    kcc_close,
    kcc_store_cred,
    NULL,
    kcc_get_principal,
    kcc_get_first,
    kcc_get_next,
    kcc_end_get,
    kcc_remove_cred,
    kcc_set_flags,
    kcc_get_version,
    kcc_get_cache_first,
    kcc_get_cache_next,
    kcc_end_cache_get,
    kcc_move,
    kcc_get_default_name,
    kcc_set_default,
    kcc_lastchange,
    kcc_set_kdc_offset,
    kcc_get_kdc_offset
};

#endif /* __APPLE__ */
