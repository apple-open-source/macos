/*
 * Copyright (c) 2009 Kungliga Tekniska Högskolan
 * (Royal Institute of Technology, Stockholm, Sweden).
 * All rights reserved.
 *
 * Portions Copyright (c) 2009 - 2011 Apple Inc. All rights reserved.
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


#include "hdb_locl.h"
#include <heimbase.h>
#include <hx509.h>
#include <base64.h>
#include <rfc2459_asn1.h>
#include <ifaddrs.h>
#include <heimbase.h>
#include <roken.h>

#ifdef HAVE_OPENDIRECTORY

#include <CoreFoundation/CoreFoundation.h>
#include <OpenDirectory/OpenDirectory.h>
#ifdef __APPLE_PRIVATE__
#include <OpenDirectory/OpenDirectoryPriv.h>
#include <PasswordServer/KerberosInterface.h>
#endif
#include <SystemConfiguration/SCPreferences.h>

struct iter_ctx {
    CFIndex idx;
    CFArrayRef odRecordArray;
} iter;

struct hdb_od;

typedef krb5_error_code (*hod_locate_record)(krb5_context, struct hdb_od *, krb5_principal, unsigned, int, int *, ODRecordRef *);

typedef struct hdb_od {
    CFStringRef rootName;
    ODNodeRef rootNode;
    CFArrayRef inNodeAttributes;
    CFArrayRef inKDCAttributes;
    CFArrayRef inComputerOrUsers;
    CFStringRef restoreRoot;
    char *LKDCRealm;
    SCDynamicStoreRef *store;
    char *ntlmDomain;
    struct iter_ctx iter;
    hod_locate_record locate_record;
} *hdb_od;

static CFStringRef kRealName = CFSTR("dsAttrTypeStandard:RealName");

static const char wellknown_lkdc[] = "WELLKNOWN:COM.APPLE.LKDC";

/*
 *
 */

typedef struct hdb_entry_ex_ctx {
    krb5_principal principal;
    ODRecordRef record;
} hdb_entry_ex_ctx;

static void
free_hod_ctx(krb5_context context, hdb_entry_ex *entry)
{
    hdb_entry_ex_ctx *ctx = entry->ctx;
    if (ctx) {
	if (ctx->record)
	    CFRelease(ctx->record);
	free(ctx);
    }
    entry->ctx = NULL;
}


static hdb_entry_ex_ctx *
get_ex_ctx(hdb_entry_ex *entry)
{
    if (entry->ctx == NULL) {
	entry->ctx = calloc(1, sizeof(*entry->ctx));
	if (entry->ctx == NULL)
	    return NULL;
	entry->free_entry = free_hod_ctx;
    }
    return entry->ctx;
}

/*
 *
 */

static int
is_lkdc(hdb_od d, const char *realm)
{
    return d->LKDCRealm != NULL && krb5_realm_is_lkdc(realm);
}

static krb5_error_code
map_service(krb5_context context, krb5_const_principal principal, krb5_principal *eprinc)
{
    const char *service;
    krb5_error_code ret;

    ret = krb5_copy_principal(context, principal, eprinc);
    if (ret)
	return ret;

    service = krb5_principal_get_comp_string(context, principal, 0);
    if (strcasecmp(service, "afpserver") == 0 ||
	strcasecmp(service, "cifs") == 0 ||
	strcasecmp(service, "smb") == 0 ||
	strcasecmp(service, "vnc") == 0) {

	free((*eprinc)->name.name_string.val[0]);
	(*eprinc)->name.name_string.val[0] = strdup("host");
    }
    return 0;
}

static bool
tryAgainP(krb5_context context, int *tryAgain, CFErrorRef *error)
{
    if (*tryAgain <= 1)
	return 0;

    if (error && *error) {
	CFRelease(*error);
	*error = NULL;
    }
    (*tryAgain)--;

    krb5_warnx(context, "try fetching result again");

    return 1;
}

#define MAX_TRIES	3


/*
 * Returns the matching User or Computer record, if not, returns the node itself
 */

static ODRecordRef
HODODNodeCopyLinkageRecordFromAuthenticationData(krb5_context context, ODNodeRef node, ODRecordRef record)
{
    CFMutableArrayRef types = NULL;
    CFArrayRef linkageArray = NULL, resultArray = NULL;
    CFTypeRef userLinkage;
    ODQueryRef query = NULL;
    CFErrorRef error = NULL;
    ODRecordRef res;
    int tryAgain = MAX_TRIES;

    types = CFArrayCreateMutable(NULL, 2, &kCFTypeArrayCallBacks);
    if (types == NULL)
	goto out;
    
    CFArrayAppendValue(types, kODRecordTypeUsers);
    CFArrayAppendValue(types, kODRecordTypeComputers);
    
    linkageArray = ODRecordCopyValues(record, CFSTR("dsAttrTypeNative:userLinkage"), &error);
    if (linkageArray == NULL)
	goto out;

    if (CFArrayGetCount(linkageArray) == 0)
	goto out;

    userLinkage = CFArrayGetValueAtIndex(linkageArray, 0);
    if (userLinkage == NULL)
	goto out;
    
    do {
	query = ODQueryCreateWithNode(NULL, node, types,
				      CFSTR("dsAttrTypeNative:entryUUID"),
				      kODMatchEqualTo,
				      userLinkage, NULL, 1, &error);
	if (query == NULL)
	    goto out;

	resultArray = ODQueryCopyResults(query, FALSE, &error);
        CFRelease(query);
	if (resultArray == NULL && error == NULL)
	    tryAgain = 0;
    } while(resultArray == NULL && tryAgainP(context, &tryAgain, &error));
    if (resultArray == NULL)
	goto out;

    if (CFArrayGetCount(resultArray) == 0)
	goto out;

    res = (ODRecordRef)CFArrayGetValueAtIndex(resultArray, 0);
    if (res)
	record = res;

out:
    CFRetain(record);
    if (error)
	CFRelease(error);
    if (linkageArray)
	CFRelease(linkageArray);
    if (resultArray)
	CFRelease(resultArray);
    if (types)
	CFRelease(types);
    return record;
	
}

/*
 *
 */

static krb5_error_code
od2hdb_usercert(krb5_context context, hdb_od d,
		CFArrayRef data, unsigned flags, hdb_entry_ex *entry)
{
    HDB_extension ext;
    void *ptr;
    krb5_error_code ret;
    CFIndex i;

    memset(&ext, 0, sizeof(ext));

    ext.data.element = choice_HDB_extension_data_pkinit_cert;

    for (i = 0; i < CFArrayGetCount(data); i++) {
	CFDataRef c = (CFDataRef) CFArrayGetValueAtIndex(data, i);
	krb5_data cd;

	if (CFGetTypeID((CFTypeRef)c) != CFDataGetTypeID())
	    continue;

	ret = krb5_data_copy(&cd, CFDataGetBytePtr(c),
			     CFDataGetLength(c));
	if (ret) {
	    free_HDB_extension(&ext);
	    return ENOMEM;
	}

	ext.data.u.pkinit_cert.len += 1;

	ptr = realloc(ext.data.u.pkinit_cert.val,
		      sizeof(ext.data.u.pkinit_cert.val[0]) * ext.data.u.pkinit_cert.len);
	if (ptr == NULL) {
	    krb5_data_free(&cd);
	    free_HDB_extension(&ext);
	    return ENOMEM;
	}
	ext.data.u.pkinit_cert.val = ptr;

	memset(&ext.data.u.pkinit_cert.val[ext.data.u.pkinit_cert.len - 1],
	       0, sizeof(ext.data.u.pkinit_cert.val[0]));

	ext.data.u.pkinit_cert.val[ext.data.u.pkinit_cert.len - 1].cert = cd;
    }

    ret = hdb_replace_extension(context, &entry->entry, &ext);
    free_HDB_extension(&ext);

    return ret;
}

/*
 *
 */

static krb5_error_code
nodeCreateWithName(krb5_context context, hdb_od d, ODNodeRef *node)
{
    ODSessionRef session = kODSessionDefault;

    *node = NULL;

#ifdef __APPLE_PRIVATE__
    if (d->restoreRoot) {
	CFMutableDictionaryRef options;

	options = CFDictionaryCreateMutable(NULL, 0,
					    &kCFTypeDictionaryKeyCallBacks,
					    &kCFTypeDictionaryValueCallBacks);
	if (options == NULL)
	    return ENOMEM;

	CFDictionaryAddValue(options, kODSessionLocalPath, d->restoreRoot);

	session = ODSessionCreate(kCFAllocatorDefault, options, NULL);
	CFRelease(options);
	if (session == NULL) {
	    krb5_set_error_message(context, HDB_ERR_DB_INUSE, "failed to create session");
	    return HDB_ERR_DB_INUSE;
	}
    }
#endif

    *node = ODNodeCreateWithName(kCFAllocatorDefault,
				 session,
				 d->rootName,
				 NULL);
    if (session)
	CFRelease(session);

    if (*node == NULL) {
	char *restoreRoot = NULL, *path = NULL;

	if (d->restoreRoot)
	    restoreRoot = rk_cfstring2cstring(d->restoreRoot);
	path = rk_cfstring2cstring(d->rootName);
	krb5_set_error_message(context, HDB_ERR_DB_INUSE, "failed to create root node: %s %s",
			       path ? path : "<nopath>",
			       restoreRoot ? restoreRoot : "");
	free(restoreRoot);
	free(path);
	return HDB_ERR_DB_INUSE;
    }

    return 0;
}



/*
 *
 */

static krb5_error_code
od2hdb_null(krb5_context context, hdb_od d,
	    CFArrayRef data, unsigned flags, hdb_entry_ex *entry)
{
    return 0;
}

/*
 *
 */

static krb5_error_code
map_lkdc_principal(krb5_context context, krb5_principal principal, const char *from, const char *to)
{
    size_t num = krb5_principal_get_num_comp(context, principal);
    krb5_error_code ret;

    if (strcasecmp(principal->realm, from) == 0) {
	ret = krb5_principal_set_realm(context, principal, to);
	if (ret)
	    return ret;
    }

    if (num == 2 && strcasecmp(principal->name.name_string.val[1], from) == 0) {
	free(principal->name.name_string.val[1]);
	principal->name.name_string.val[1] = strdup(to);
	if (principal->name.name_string.val[1] == NULL)
	    return ENOMEM;
    }
    return 0;
}

/*
 *
 */

static krb5_error_code
od2hdb_principal(krb5_context context, hdb_od d,
		 CFArrayRef data, unsigned flags, hdb_entry_ex *entry)
{
    hdb_entry_ex_ctx *ctx = entry->ctx;
    krb5_error_code ret;
    char *str;

    /* the magic store principal in hdb entry was found, lets skip this step */
    if (ctx && ctx->principal)
	return 0;
    
    if ((flags & HDB_F_CANON) == 0)
	return 0;

    if (entry->entry.principal) {
	krb5_free_principal(context, entry->entry.principal);
	entry->entry.principal = NULL;
    }

    str = rk_cfstring2cstring((CFStringRef)CFArrayGetValueAtIndex(data, 0));
    if (str == NULL)
	return ENOMEM;

    ret = krb5_parse_name(context, str, &entry->entry.principal);
    free(str);
    if (ret)
	return ret;

    if (d->LKDCRealm) {
	ret = map_lkdc_principal(context, entry->entry.principal, wellknown_lkdc, d->LKDCRealm);
	if (ret)
	    return ret;
    }

    return 0;
}

static krb5_error_code
od2hdb_def_principal(krb5_context context, hdb_od d,
		     int flags, hdb_entry_ex *entry)
{
    if (entry->entry.principal)
	return 0;
    return HDB_ERR_NOENTRY;
}

static krb5_error_code
hdb2od_principal_lkdc(krb5_context context, hdb_od d, ODAttributeType type,
		      unsigned flags, hdb_entry_ex *entry, ODRecordRef record)
{
    size_t num = krb5_principal_get_num_comp(context, entry->entry.principal);
    CFMutableArrayRef array = NULL;
    krb5_principal principal = NULL;
    krb5_error_code ret = 0;
    CFStringRef element;
    char *user;

    if (d->LKDCRealm == NULL)
	return 0;

    /*
     * "User" LKDC names are implicitly stored
     */
    if (num == 1)
	return 0;

    ret = krb5_copy_principal(context, entry->entry.principal, &principal);
    if (ret)
	return ret;

    ret = map_lkdc_principal(context, principal, d->LKDCRealm, wellknown_lkdc);
    if (ret)
	goto out;

    /* 2 entry nodes are servers :/ */
    if (num == 2) {
	krb5_principal eprinc;
	type = CFSTR("dsAttrTypeNative:KerberosServerName");
	
	ret = map_service(context, principal, &eprinc);
	if (ret)
	    goto out;
	krb5_free_principal(context, principal);
	principal = eprinc;
    }

    array = CFArrayCreateMutable(kCFAllocatorDefault, 1,
				 &kCFTypeArrayCallBacks);
    if (array == NULL) {
	ret = ENOMEM;
	goto out;
    }

    ret = krb5_unparse_name(context, principal, &user);
    if (ret)
	goto out;

    element = CFStringCreateWithCString(kCFAllocatorDefault, user, kCFStringEncodingUTF8);
    if (element == NULL) {
	ret = ENOMEM;
	goto out;
    }

    CFArrayAppendValue(array, element);
    CFRelease(element);
    
    bool r = ODRecordSetValue(record, type, array, NULL);
    if (!r)
	ret = HDB_ERR_UK_SERROR;

 out:
    if (principal)
	krb5_free_principal(context, principal);
    if (array)
	CFRelease(array);

    return ret;
}


static krb5_error_code
hdb2od_principal_server(krb5_context context, hdb_od d, ODAttributeType type,
			unsigned flags, hdb_entry_ex *entry, ODRecordRef record)
{
    krb5_error_code ret;
    CFStringRef name;
    char *user;
    
    if (d->LKDCRealm)
	return 0;
    
    ret = krb5_unparse_name(context, entry->entry.principal, &user);
    if (ret)
	return ret;

    name = CFStringCreateWithCString(NULL, user, kCFStringEncodingUTF8);
    free(user);
    if (name == NULL)
	return ENOMEM;
    
    bool r = ODRecordSetValue(record, type, name, NULL);
    CFRelease(name);
    if (!r)
	return HDB_ERR_UK_SERROR;

    return 0;
}

/*
 *
 */

static krb5_error_code
od2hdb_alias(krb5_context context, hdb_od d,
	     CFArrayRef data, unsigned flags, hdb_entry_ex *entry)
{
    hdb_entry_ex_ctx *ctx = entry->ctx;
    krb5_error_code ret;
    krb5_principal principal;
    char *str;

    /* skip aliases on magic principal since they don't have aliases */
    if (ctx && ctx->principal)
	return 0;

    str = rk_cfstring2cstring((CFStringRef)CFArrayGetValueAtIndex(data, 0));
    if (str == NULL)
	return ENOMEM;

    ret = krb5_parse_name(context, str, &principal);
    free(str);
    if (ret)
	return ret;

    krb5_free_principal(context, principal);

    return 0;
}
/*
 *
 */

static krb5_error_code
od2hdb_keys(krb5_context context, hdb_od d,
	    CFArrayRef akeys, unsigned flags, hdb_entry_ex *entry)
{
    CFIndex i, count = CFArrayGetCount(akeys);
    krb5_error_code ret;
    hdb_keyset_aapl *keys;
    int specific_match = -1, general_match = -1;
    uint32_t specific_kvno = 0, general_kvno = 0;

    if (count == 0)
	return ENOMEM;

    keys = calloc(count, sizeof(keys[0]));
    if (keys == NULL)
	return ENOMEM;

    ret = 0;
    for (i = 0; i < count; i++) {
	CFTypeRef type = CFArrayGetValueAtIndex(akeys, i);
	void *ptr;

	if (CFGetTypeID(type) == CFDataGetTypeID()) {
	    CFDataRef data = (CFDataRef) type;

	    ret = CFDataGetLength(data);
	    ptr = malloc(ret);
	    if (ptr == NULL) {
		ret = ENOMEM;
		goto out;
	    }
	    memcpy(ptr, CFDataGetBytePtr(data), ret);
	} else if (CFGetTypeID(type) == CFStringGetTypeID()) {
	    CFStringRef cfstr = (CFStringRef)type;
	    char *str;

	    str = rk_cfstring2cstring(cfstr);
	    if (str == NULL) {
		ret = ENOMEM;
		goto out;
	    }
	    ptr = malloc(strlen(str));
	    if (ptr == NULL) {
		free(str);
		ret = ENOMEM;
		goto out;
	    }

	    ret = base64_decode(str, ptr);
	    free(str);
	    if (ret < 0) {
		free(ptr);
		ret = EINVAL;
		goto out;
	    }
	} else {
	    ret = EINVAL; /* XXX */
	    goto out;
	}

	ret = decode_hdb_keyset_aapl(ptr, ret, &keys[i], NULL);
	free(ptr);
	if (ret)
	    goto out;

	if (keys[i].principal) {
	    if (krb5_principal_compare(context, keys[i].principal, entry->entry.principal)) {

		if (keys[i].kvno > specific_kvno) {
		    hdb_entry_ex_ctx *ctx = get_ex_ctx(entry);
		    if (ctx == NULL) {
			ret = ENOMEM;
			goto out;
		    }
		    ctx->principal = entry->entry.principal;

		    specific_match = i;
		    specific_kvno = keys[i].kvno;
		}
	    }		
	} else {
	    if (keys[i].kvno > general_kvno) {
		general_match = i;
		general_kvno = keys[i].kvno;
	    }
	}
    }

    if (specific_match != -1) {
	i = specific_match;
	entry->entry.kvno = specific_kvno;
    } else if (general_match != -1) {
	i = general_match;
	entry->entry.kvno = general_kvno;
    } else {
	ret = HDB_ERR_NOENTRY;
	goto out;
    }

    entry->entry.keys.len = keys[i].keys.len;
    entry->entry.keys.val = keys[i].keys.val;
    keys[i].keys.val = NULL;
    keys[i].keys.len = 0;

 out:
    for (i = 0; i < count; i++)
	free_hdb_keyset_aapl(&keys[i]);
    free(keys);

    return ret;
}

static krb5_error_code
hdb2od_keys(krb5_context context, hdb_od d, ODAttributeType type,
	    unsigned flags, hdb_entry_ex *entry, ODRecordRef record)
{
    CFMutableArrayRef array;
    heim_octet_string data;
    krb5_error_code ret;
    CFDataRef element;
    hdb_keyset_aapl key;
    size_t size;
    
    /* if this is a change password operation, the keys have already been updated with ->hdb_password */
    if (flags & HDB_F_CHANGE_PASSWORD)
	return 0;
 
    /* this overwrites other keys that are stored in the special "alias" keyset for server */
    
    key.kvno = entry->entry.kvno;
    key.keys.len = entry->entry.keys.len;
    key.keys.val = entry->entry.keys.val;
    key.principal = NULL;

    array = CFArrayCreateMutable(kCFAllocatorDefault, 1,
				 &kCFTypeArrayCallBacks);
    if (array == NULL) {
	ret = ENOMEM;
	goto out;
    }

    ASN1_MALLOC_ENCODE(hdb_keyset_aapl, data.data, data.length, &key, &size, ret);
    if (ret)
	goto out;
    if (data.length != size)
	krb5_abortx(context, "internal asn.1 encoder error");

    element = CFDataCreate(kCFAllocatorDefault, data.data, data.length);
    if (element == NULL) {
	ret = ENOMEM;
	goto out;
    }

    CFArrayAppendValue(array, element);
    CFRelease(element);

    bool r = ODRecordSetValue(record, type, array, NULL);
    if (!r)
	ret = HDB_ERR_UK_SERROR;

 out:
    if (array)
	CFRelease(array);

    return ret;
}

/*
 *
 */

static krb5_error_code
od2hdb_flags(krb5_context context, hdb_od d,
	     CFArrayRef data, unsigned qflags, hdb_entry_ex *entry)
{
    int flags;
    char *str;

    str = rk_cfstring2cstring((CFStringRef)CFArrayGetValueAtIndex(data, 0));
    if (str == NULL)
	return ENOMEM;

    flags = atoi(str);
    free(str);
    entry->entry.flags = int2HDBFlags(flags);
    return 0;
}

static krb5_error_code
od2hdb_def_flags(krb5_context context, hdb_od d,
		 int flags, hdb_entry_ex *entry)
{
    entry->entry.flags.client = 1;
    entry->entry.flags.forwardable = 1;
    entry->entry.flags.renewable = 1;
    entry->entry.flags.proxiable = 1;

    return 0;
}

static krb5_error_code
hdb2od_flags(krb5_context context, hdb_od d, ODAttributeType type,
	     unsigned flags, hdb_entry_ex *entry, ODRecordRef record)
{
    int eflags = HDBFlags2int(entry->entry.flags);
    CFStringRef value;

    value = CFStringCreateWithFormat(kCFAllocatorDefault, NULL,
				       CFSTR("%d"), eflags);
    if (value == NULL)
	return ENOMEM;

    bool r = ODRecordSetValue(record, type, value, NULL);
    CFRelease(value);
    if (!r)
	return HDB_ERR_UK_SERROR;

    return 0;
}

static krb5_error_code
od2hdb_altsecids(krb5_context context, hdb_od d,
		 CFArrayRef akeys, unsigned flags, hdb_entry_ex *entry)
{
    CFIndex i, count = CFArrayGetCount(akeys);
    krb5_error_code ret;

    if (count == 0)
	return ENOMEM;

    for (i = 0; i < count; i++) {
	CFStringRef name = CFArrayGetValueAtIndex(akeys, i);
	char *str, *str0, *subj;

	if (CFGetTypeID(name) != CFStringGetTypeID())
	    continue;

	if (!CFStringHasPrefix(name, CFSTR("X509:<T>")))
	    continue;

	/* split out X509:<T>(.*)<S>(.*) from the string and store */

	str0 = str = rk_cfstring2cstring(name);
	str += 8; /* skip X509:<T> */

	subj = strstr(str, "<S>");
	if (subj == NULL) {
	    free(str0);
	    continue;
	}
	*subj = '\0';
	subj += 3; /* skip <S> */

	/* make sure that there is no <T> or <S> in the string */
	if (strstr(subj, "<T>") || strstr(subj, "<S>") ||
	    strstr(str, "<T>") || strstr(str, "<S>"))
	{
	    free(str0);
	    continue;
	}

	ret = hdb_entry_set_pkinit_acl(&entry->entry, subj, NULL, str);
	free(str0);
	if (ret)
	    return ret;
    }
    return 0;
}

/*
 *
 */

static krb5_error_code
od2hdb_acl_rights(krb5_context context, hdb_od d,
		  CFArrayRef data, unsigned flags, hdb_entry_ex *entry)
{
    char *str;

    str = rk_cfstring2cstring((CFStringRef)CFArrayGetValueAtIndex(data, 0));
    if (str == NULL)
	return ENOMEM;

    entry->entry.acl_rights = malloc(sizeof(*entry->entry.acl_rights));    
    if (entry->entry.acl_rights == NULL) {
	free(str);
	return ENOMEM;
    }

    *entry->entry.acl_rights = atoi(str);
    free(str);

    return 0;
}

/*
 *
 */

typedef krb5_error_code
(*od2hdb_func)(krb5_context, hdb_od, CFArrayRef, unsigned flags, hdb_entry_ex *);

typedef krb5_error_code
(*od2hdb_default)(krb5_context, hdb_od, int, hdb_entry_ex *);

typedef krb5_error_code
(*hdb2od_func)(krb5_context, hdb_od, ODAttributeType,
	       unsigned flags, hdb_entry_ex *, ODRecordRef);


static const struct key {
    const char *desc;
    CFStringRef name;
    int flags;
#define MULTI_VALUE	1
#define OPTIONAL_VALUE	2
#define SERVER_VALUE	4
#define DELETE_KEY	8
    od2hdb_func od2hdb;
    od2hdb_default od2hdb_def;
    hdb2od_func hdb2od;
} keys[] = {
    {
	"keyset",
	CFSTR("dsAttrTypeNative:draft-krbKeySet"), OPTIONAL_VALUE | MULTI_VALUE | SERVER_VALUE | DELETE_KEY,
	od2hdb_keys,
	NULL,
	hdb2od_keys
    },
    {
	"KerberosKeys",
	CFSTR("dsAttrTypeNative:KerberosKeys"), OPTIONAL_VALUE | MULTI_VALUE | DELETE_KEY,
	od2hdb_keys,
	NULL,
	hdb2od_keys
    },
    {
	"username",
	CFSTR("dsAttrTypeNative:KerberosUserName"), OPTIONAL_VALUE | DELETE_KEY,
	od2hdb_principal,
	od2hdb_def_principal,
	hdb2od_principal_lkdc
    },
    {
	"principalName",
	CFSTR("dsAttrTypeNative:draft-krbPrincipalName"), OPTIONAL_VALUE | SERVER_VALUE | DELETE_KEY,
	od2hdb_principal,
	NULL,
	hdb2od_principal_server
    },
    {
	"principalAlias",
	CFSTR("dsAttrTypeNative:draft-krbPrincipalAliases"), MULTI_VALUE | OPTIONAL_VALUE | DELETE_KEY,
	od2hdb_alias,
	NULL,
	NULL
    },
    {
	"KerberosFlags",
	CFSTR("dsAttrTypeNative:KerberosFlags"), DELETE_KEY,
	od2hdb_flags,
	od2hdb_def_flags,
	hdb2od_flags
    },
    {
	"KerberosPolicy",
	CFSTR("dsAttrTypeNative:draft-krbTicketPolicy"), OPTIONAL_VALUE | SERVER_VALUE | DELETE_KEY,
	od2hdb_flags,
	NULL,
	hdb2od_flags
    },
    {
	"MaxLife",
	CFSTR("dsAttrTypeNative:KerberosMaxLife"), OPTIONAL_VALUE | DELETE_KEY,
	od2hdb_null,
	NULL,
	NULL
    },
    {
	"MaxRenewLife",
	CFSTR("dsAttrTypeNative:KerberosMaxRenew"), OPTIONAL_VALUE | DELETE_KEY,
	od2hdb_null,
	NULL,
	NULL
    },
    {
	"UserCertificate",
	CFSTR("dsAttrTypeStandard:UserCertificate"), MULTI_VALUE | OPTIONAL_VALUE,
	od2hdb_usercert,
	NULL,
	NULL
    },
    {
	"AltSecurityIdentities",
	CFSTR("dsAttrTypeStandard:AltSecurityIdentities"), MULTI_VALUE | OPTIONAL_VALUE,
	od2hdb_altsecids,
	NULL,
	NULL
    },
    {
	"principalACL",
	CFSTR("dsAttrTypeNative:draft-krbPrincipalACL"), OPTIONAL_VALUE,
	od2hdb_acl_rights,
	NULL,
	NULL
    }
	
};
static const int num_keys = sizeof(keys)/sizeof(keys[0]);

/*
 * Policy mappings
 */

static int
booleanPolicy(CFDictionaryRef policy, CFStringRef key)
{
    CFTypeRef val = CFDictionaryGetValue(policy, key);
    
    if (val == NULL)
	return 0;
    
    if (CFGetTypeID(val) == CFStringGetTypeID()) {
	if (CFStringCompare(val, CFSTR("0"), kCFCompareNumerically) != kCFCompareEqualTo)
	    return 1;
    } else if (CFGetTypeID(val) == CFNumberGetTypeID()) {
	int num;
	if (CFNumberGetValue(val, kCFNumberIntType, &num))
	    return num;
    }
    return 0;
}

static time_t
timePolicy(CFDictionaryRef policy, CFStringRef key)
{
    CFTypeRef val = CFDictionaryGetValue(policy, key);
    
    if (val == NULL)
	return 0;
    
    if (CFGetTypeID(val) == CFStringGetTypeID()) {
	return (time_t)CFStringGetIntValue(val);
    } else if (CFGetTypeID(val) == CFNumberGetTypeID()) {
	long num;
	if (CFNumberGetValue(val, kCFNumberLongType, &num))
	    return num;
    }
    return 0;
}

static krb5_error_code
apply_policy(hdb_entry *entry, CFDictionaryRef policy)
{
    /*
     * Historically, we have not applied any policies to admin users.
     */
    if (booleanPolicy(policy, CFSTR("isAdminUser")))
	return 0;

    entry->flags.invalid = booleanPolicy(policy, CFSTR("isDisabled"));
    
    if (booleanPolicy(policy, CFSTR("newPasswordRequired"))) {
	if (entry->pw_end == NULL) {
	    entry->pw_end = malloc(sizeof(*entry->pw_end));
	    if (entry->pw_end == NULL)
		return ENOMEM;
	}
	*entry->pw_end = time(NULL) - 60; /* expired on 60s ago */
    } else if (booleanPolicy(policy, CFSTR("usingExpirationDate"))) {
	time_t t = timePolicy(policy, CFSTR("expirationDateGMT"));
	if (t > 0) {
	    if (entry->pw_end == NULL) {
		entry->pw_end = malloc(sizeof(*entry->pw_end));
		if (entry->pw_end == NULL)
		    return ENOMEM;
	    }
	    *entry->pw_end = t;
	}
    }
    if (booleanPolicy(policy, CFSTR("usingHardExpirationDate"))) {
	time_t t = timePolicy(policy, CFSTR("hardExpireDateGMT"));
	if (t > 0) {
	    if (entry->valid_end == NULL) {
		entry->valid_end = malloc(sizeof(*entry->valid_end));
		if (entry->valid_end == NULL)
		    return ENOMEM;
	    }
	    *entry->valid_end = t;
	}
    }
    return 0;
}

/*
 * HDB entry points
 */

static krb5_error_code
hod_close(krb5_context context, HDB *db)
{
    return 0;
}

static krb5_error_code
hod_destroy(krb5_context context, HDB *db)
{
    hdb_od d = (hdb_od)db->hdb_db;
    krb5_error_code ret;

    if (d->rootNode)
	CFRelease(d->rootNode);
    if (d->inNodeAttributes)
	CFRelease(d->inNodeAttributes);
    if (d->inKDCAttributes)
	CFRelease(d->inKDCAttributes);
    if (d->LKDCRealm)
	free(d->LKDCRealm);

    ret = hdb_clear_master_key (context, db);
    free(db->hdb_name);
    free(db);
    return ret;
}

static krb5_error_code
hod_lock(krb5_context context, HDB *db, int operation)
{
    return 0;
}

static krb5_error_code
hod_unlock(krb5_context context, HDB *db)
{
    return 0;
}

static krb5_error_code
od_record2entry(krb5_context context, HDB * db, ODRecordRef cfRecord,
		unsigned flags, hdb_entry_ex * entry)
{
    hdb_od d = (hdb_od)db->hdb_db;
    krb5_error_code ret = HDB_ERR_NOENTRY;
    int i;

    for (i = 0; i < num_keys; i++) {
	CFArrayRef data;

	data = ODRecordCopyValues(cfRecord, keys[i].name, NULL);
	if (data == NULL) {
	    if (keys[i].flags & OPTIONAL_VALUE)
		continue;
	    if (keys[i].od2hdb_def) {
		ret = (*keys[i].od2hdb_def)(context, d, flags, entry);
		if (ret)
		    goto out;
		continue;
	    }
	    krb5_warnx(context, "Failed to copy non-optional value %s", keys[i].desc);
	    ret = HDB_ERR_NOENTRY;
	    goto out;
	}

	if ((keys[i].flags & MULTI_VALUE) && CFArrayGetCount(data) < 1) {
	    krb5_warnx(context, "Expected multivalue got zero %s for", keys[i].desc);
	    CFRelease(data);
	    ret = HDB_ERR_NOENTRY;
	    goto out;
	} else if ((keys[i].flags & MULTI_VALUE) == 0 && CFArrayGetCount(data) != 1) {
	    krb5_warnx(context, "Expected single-value got non zero %d for %s",
		       (int)CFArrayGetCount(data), keys[i].desc);
	    CFRelease(data);
	    ret = HDB_ERR_NOENTRY;
	    goto out;
	}

	ret = (*keys[i].od2hdb)(context, d, data, flags, entry);
	CFRelease(data);
	if (ret) {
	    krb5_warn(context, ret, "od2hdb failed for %s", keys[i].desc);
	    goto out;
	}
    }

    /*
     * If entry didn't contain a principal, something is wrong with
     * the entry, lets skip it.
     */
    if (entry->entry.principal == NULL) {
	krb5_warnx(context, "principal missing");
	ret = HDB_ERR_NOENTRY;
	goto out;
    }

    /* Not recorded in the OD backend, make something up */
    ret = krb5_parse_name(context, "hdb/od@WELL-KNOWN:OD-BACKEND",
			  &entry->entry.created_by.principal);
    if (ret)
	goto out;

    entry->entry.created_by.time = time(NULL);
    
    if ((flags & HDB_F_GET_KRBTGT) == 0 && !krb5_principal_is_krbtgt(context, entry->entry.principal)) {
        ODRecordRef userrecord;
	
	userrecord = HODODNodeCopyLinkageRecordFromAuthenticationData(context, d->rootNode, cfRecord);
	if (userrecord) {
	    CFDictionaryRef policy;
	    
	    policy = ODRecordCopyPasswordPolicy(NULL, userrecord, NULL);
	    CFRelease(userrecord);
	    
	    if (policy) {
		ret = apply_policy(&entry->entry, policy);
		CFRelease(policy);
		if (ret)
		    goto out;
	    }
	}
    }

    if (db->hdb_master_key_set && (flags & HDB_F_DECRYPT)) {
        ret = hdb_unseal_keys(context, db, &entry->entry);
        if(ret) {
	    krb5_warn(context, ret, "unseal keys failed");
	    goto out;
	}
    }

out:
    return ret;
}

static krb5_error_code
hod_nextkey(krb5_context context, HDB * db, unsigned flags,
	     hdb_entry_ex * entry)
{
    ODRecordRef cfRecord = NULL;
    hdb_od d = (hdb_od)db->hdb_db;
    krb5_error_code ret;

    memset(entry, 0, sizeof(*entry));

    heim_assert(d->iter.odRecordArray != NULL, "hdb: hod_next w/o hod_first");

    while (d->iter.idx < CFArrayGetCount(d->iter.odRecordArray)) {
	cfRecord = (ODRecordRef) CFArrayGetValueAtIndex(d->iter.odRecordArray, d->iter.idx);
	d->iter.idx++;
	if (cfRecord == NULL) {
	    ret = HDB_ERR_NOENTRY;
	    goto out;
	}

	CFArrayRef kerberosKeysArray = ODRecordCopyValues(cfRecord, CFSTR("dsAttrTypeNative:KerberosKeys"), NULL);
	if (kerberosKeysArray == NULL) {
	    cfRecord = NULL;
	    continue;
	}
	if (CFArrayGetCount(kerberosKeysArray) == 0) {
	    CFRelease(kerberosKeysArray);
	    cfRecord = NULL;
	    continue;
	}
	CFRelease(kerberosKeysArray);

	break;
    }

    if (cfRecord) {
	CFStringRef name = ODRecordGetRecordName(cfRecord);
	char *str;

	str = rk_cfstring2cstring(name);
	if (str == NULL) {
	    ret = ENOENT;
	    goto out;
	}

	ret = krb5_make_principal(context, &entry->entry.principal,
				  d->LKDCRealm, str, NULL);
	free(str);
	if (ret)
	    goto out;
	
	ret = od_record2entry(context, db, cfRecord, flags, entry);
    } else
	ret = HDB_ERR_NOENTRY;

out:
    if (ret) {
	free_hdb_entry(&entry->entry);

	if (d->iter.odRecordArray) {
	    CFRelease(d->iter.odRecordArray);
	    d->iter.odRecordArray = NULL;
	    d->iter.idx = 0;
	}
    }

    return ret;
}

static krb5_error_code
hod_firstkey(krb5_context context, HDB *db,
	     unsigned flags, hdb_entry_ex *entry)
{
    int ret = 0;
    ODQueryRef query = NULL;
    hdb_od d = (hdb_od)db->hdb_db;
    int tryAgain = MAX_TRIES;

    if (d->LKDCRealm == NULL) {
	krb5_set_error_message(context, EINVAL, "iteration over database only supported for DSLocal");
	return EINVAL;
    }

    heim_assert(d->iter.odRecordArray == NULL, "hdb: more then one iteration at the same time");

    do {
	CFErrorRef error = NULL;

	query = ODQueryCreateWithNode(NULL, d->rootNode,
				      kODRecordTypeUsers, NULL,
				      kODMatchAny,
				      NULL,
				      kODAttributeTypeAllAttributes,
				      0,
				      &error);
	if (query == NULL) {
	    ret = HDB_ERR_NOENTRY;
	    goto out;
	}

	d->iter.odRecordArray = ODQueryCopyResults(query, FALSE, &error);
	CFRelease(query);
	if (d->iter.odRecordArray == NULL && error == NULL)
	    tryAgain = 0;
	if (error)
	    CFRelease(error);
    } while(d->iter.odRecordArray == NULL && tryAgainP(context, &tryAgain, NULL));

    if (d->iter.odRecordArray == NULL) {
	ret = HDB_ERR_NOENTRY;
	goto out;
    }
    d->iter.idx = 0;

    return hod_nextkey(context, db, flags, entry);
out:
    return ret;
}

static krb5_error_code
hod_open(krb5_context context, HDB * db, int flags, mode_t mode)
{
    hdb_od d = (hdb_od)db->hdb_db;

    if (d->rootNode == NULL)
	return nodeCreateWithName(context, d, &d->rootNode);

    return 0;
}

static krb5_error_code
lkdc_locate_record(krb5_context context, hdb_od d, krb5_principal principal,
		   unsigned flags, int createp, int *createdp, ODRecordRef *result)
{
    ODMatchType matchtype = kODMatchEqualTo;
    CFStringRef queryString = NULL;
    ODRecordRef cfRecord;
    ODQueryRef query = NULL;
    CFArrayRef res = NULL;
    CFStringRef attr;
    krb5_error_code ret = 0;
    krb5_principal eprinc = NULL;
    char *kuser = NULL;
    const char *node = NULL;
    CFTypeRef rtype = NULL;
    int upflags = 0;
    int tryAgain = MAX_TRIES;

    *result = NULL;

    /*
     * Really need something like CrackName for uniform unparsing
     */

    if (principal->name.name_type == KRB5_NT_X509_GENERAL_NAME) {
#ifdef PKINIT
	char *anode = NULL;
	hx509_name name = NULL;
	GeneralName gn;
	ssize_t len;
	void *buf;
	Name n;

	memset(&gn, 0, sizeof(gn));
	memset(&n, 0, sizeof(n));

	if (krb5_principal_get_num_comp(context, principal) != 1) {
	    krb5_warnx(context, "wrong length of x509 name");
	    return HDB_ERR_NOENTRY;
	}

	len = strlen(principal->name.name_string.val[0]);
	buf = malloc(len);
	if (buf == NULL)
	    return HDB_ERR_NOENTRY;

	len = base64_decode(principal->name.name_string.val[0], buf);
	if (len <= 0) {
	    free(buf);
	    return HDB_ERR_NOENTRY;
	}

	ret = decode_GeneralName(buf, len, &gn, NULL);
	free(buf);
	if (ret) {
	    krb5_warnx(context, "x500 GeneralName malformed: %d", ret);
	    return HDB_ERR_NOENTRY;
	}

	if (gn.element != choice_GeneralName_directoryName) {
	    krb5_warnx(context, "x500 name not directory Name");
	    free_GeneralName(&gn);
	    return HDB_ERR_NOENTRY;
	}

	n.element = choice_Name_rdnSequence;
	n.u.rdnSequence.len = gn.u.directoryName.u.rdnSequence.len;
	n.u.rdnSequence.val = gn.u.directoryName.u.rdnSequence.val;

	ret = hx509_name_from_Name(&n, &name);
	free_GeneralName(&gn);
	if (ret)
	    return HDB_ERR_NOENTRY;

	attr = kODAttributeTypeRecordName;
	rtype = kODRecordTypeUsers;

	ret = hx509_name_to_string(name, &anode);
	hx509_name_free(&name);
	if (ret)
	    return HDB_ERR_NOENTRY;

	queryString = CFStringCreateWithCString(NULL, anode, kCFStringEncodingUTF8);
	free(anode);
	if (queryString == NULL) {
	    ret = HDB_ERR_NOENTRY;
	    goto out;
	}
	createp = 0;
#else
	return HDB_ERR_NOENTRY;
#endif
    } else if (principal->name.name_type == KRB5_NT_NTLM) {
	attr = kODAttributeTypeRecordName;
	rtype = kODRecordTypeUsers;
	matchtype = kODMatchInsensitiveEqualTo;

	if (krb5_principal_get_num_comp(context, principal) != 1) {
	    krb5_warnx(context, "wrong length of ntlm name");
	    return HDB_ERR_NOENTRY;
	}
	
	if (d->ntlmDomain == NULL) {
	    krb5_warnx(context, "NTLM domain not configured");
	    return HDB_ERR_NOENTRY;
	}

	krb5_principal_set_realm(context, principal, d->ntlmDomain);
	node = krb5_principal_get_comp_string(context, principal, 0);

    } else if (krb5_principal_is_pku2u(context, principal)) {
	attr = kODAttributeTypeRecordName;
	if (flags & HDB_F_GET_CLIENT) {
	    int num = krb5_principal_get_num_comp(context, principal);
	    if (num != 1) {
		ret = HDB_ERR_NOENTRY;
		goto out;
	    }
	    rtype = kODRecordTypeUsers;
	    upflags = KRB5_PRINCIPAL_UNPARSE_NO_REALM;
	    upflags |= KRB5_PRINCIPAL_UNPARSE_DISPLAY;
	} else {
	    node = "localhost";
	    rtype = kODRecordTypeComputers;
	}
    } else if (is_lkdc(d, principal->realm)) {
	/* check if user is a LKDC user, just look for RecordName then */
	int num = krb5_principal_get_num_comp(context, principal);
	const char *comp0;
	size_t lencomp0;

	if (num < 1) {
	    ret = HDB_ERR_NOENTRY;
	    goto out;
	}

	ret = map_lkdc_principal(context, principal, d->LKDCRealm, wellknown_lkdc);
	if (ret)
	    goto out;

	comp0 = krb5_principal_get_comp_string(context, principal, 0);
	lencomp0 = strlen(comp0);
	if (lencomp0 == 0) {
	    ret = HDB_ERR_NOENTRY;
	    goto out;
	}

	if (num == 2 || (num == 1 && comp0[lencomp0 - 1] == '$')) {
	    const char *host;

	    attr = kODAttributeTypeRecordName;

	    ret = map_service(context, principal, &eprinc);
	    if (ret) {
		ret = HDB_ERR_NOENTRY;
		goto out;
	    }
	    principal = eprinc;

	    host = krb5_principal_get_comp_string(context, principal, 1);
	    comp0 = krb5_principal_get_comp_string(context, principal, 0);

	    if (strcasecmp(KRB5_TGS_NAME, comp0) == 0) {
		node = "_krbtgt";
		rtype = kODRecordTypeUsers;
	    } else if (num == 2 && is_lkdc(d, host)) {
		node = "localhost";
		rtype = kODRecordTypeComputers;
	    } else if (num == 2) {
		node = host;
		rtype = kODRecordTypeComputers;
	    } else {
		node = comp0;
		rtype = kODRecordTypeComputers;
	    }

	} else if (num == 1) {
	    attr = kODAttributeTypeRecordName;
	    rtype = kODRecordTypeUsers;
	    upflags = KRB5_PRINCIPAL_UNPARSE_NO_REALM;
	    upflags |= KRB5_PRINCIPAL_UNPARSE_DISPLAY;
	} else {
	    ret = HDB_ERR_NOENTRY;
	    goto out;
	}
    } else {
	/* managed realm, lets check for real entries */

	ret = krb5_unparse_name_flags(context, principal, KRB5_PRINCIPAL_UNPARSE_DISPLAY, &kuser);
	if (ret)
	    goto out;

	rtype = d->inComputerOrUsers;
	matchtype = kODMatchCompoundExpression;
	attr = NULL;

	queryString = CFStringCreateWithFormat(NULL, NULL, CFSTR("(|(%@=%s)(%@=%s))"), 
					       CFSTR("dsAttrTypeNative:KerberosUserName"),
					       kuser,
					       CFSTR("dsAttrTypeNative:KerberosServerName"),
					       kuser);
    }

    if (queryString == NULL) {
	if (node) {
	    queryString = CFStringCreateWithCString(NULL, node, kCFStringEncodingUTF8);
	} else {
	    ret = krb5_unparse_name_flags(context, principal, upflags, &kuser);
	    if (ret)
		goto out;

	    queryString = CFStringCreateWithCString(NULL, kuser, kCFStringEncodingUTF8);
	}
    }
    if (queryString == NULL) {
	ret = ENOMEM;
	goto out;
    }

    do {
	CFErrorRef error = NULL;

	query = ODQueryCreateWithNode(NULL, d->rootNode,
				      rtype, attr,
				      matchtype,
				      queryString,
				      d->inNodeAttributes,
				      1,
				      NULL);
	if (query == NULL) {
	    CFRelease(queryString);
	    ret = ENOMEM;
	    goto out;
	}
	
	res = ODQueryCopyResults(query, FALSE, &error);
	if (res == NULL && error == NULL)
	    tryAgain = 0;
	if (error)
	    CFRelease(error);
    } while(res == NULL && tryAgainP(context, &tryAgain, NULL));

    CFRelease(queryString);
    if (res == NULL) {
	ret = HDB_ERR_NOENTRY;
	goto out;
    }

    if (CFArrayGetCount(res) == 0 && node && createp) {

	queryString = CFStringCreateWithCString(NULL, node, kCFStringEncodingUTF8);

	cfRecord = ODNodeCopyRecord(d->rootNode, rtype, queryString, d->inNodeAttributes, NULL);
	if (cfRecord == NULL) {
	    CFMutableDictionaryRef attributes;

	    attributes = CFDictionaryCreateMutable(NULL, 0,
						   &kCFTypeDictionaryKeyCallBacks,
						   &kCFTypeDictionaryValueCallBacks);

	    cfRecord = ODNodeCreateRecord(d->rootNode,
					  rtype,
					  queryString,
					  attributes,
					  NULL);
	    CFRelease(attributes);
	}
	CFRelease(queryString);

	if (cfRecord == NULL) {
	    ret = HDB_ERR_UK_RERROR;
	    goto out;
	}

	if (createdp)
	    *createdp = TRUE;

    } else if (CFArrayGetCount(res) == 1) {
	cfRecord = (ODRecordRef) CFArrayGetValueAtIndex(res, 0);
	if (cfRecord == NULL) {
	    ret = HDB_ERR_NOENTRY;
	    goto out;
	}
	CFRetain(cfRecord);

    } else {
	ret = HDB_ERR_NOENTRY;
	goto out;
    }

    *result = cfRecord;

 out:
    if (kuser)
	free(kuser);
    if (eprinc)
	krb5_free_principal(context, eprinc);
    if (res)
	CFRelease(res);
    if (query)
	CFRelease(query);
    return ret;
}

static krb5_error_code
server_locate_record(krb5_context context, hdb_od d, krb5_principal principal,
		     unsigned flags, int createp, int *createdp, ODRecordRef *result)
{
    ODRecordRef cfRecord = NULL;
    krb5_error_code ret;
    ODMatchType matchtype;
    CFStringRef queryString = NULL;
    ODQueryRef query = NULL;
    CFArrayRef res = NULL;
    CFStringRef attr;
    char *kuser = NULL;
    CFStringRef client_key, server_key;
    int tryAgain = MAX_TRIES;

    *result = NULL;
    
    ret = krb5_unparse_name_flags(context, principal, KRB5_PRINCIPAL_UNPARSE_DISPLAY, &kuser);
    if (ret)
	goto out;
    
    client_key = CFSTR("dsAttrTypeNative:draft-krbPrincipalName");
    server_key = CFSTR("dsAttrTypeNative:draft-krbPrincipalAliases");
    
    /* XXX support referrals here */
    
    if (flags & HDB_F_GET_KRBTGT) {
	matchtype = kODMatchEqualTo;
	attr = client_key;
	queryString = CFStringCreateWithCString(NULL, kuser, kCFStringEncodingUTF8);
    } else {
	matchtype = kODMatchCompoundExpression;
	attr = NULL;
	queryString = CFStringCreateWithFormat(NULL, NULL, CFSTR("(|(%@=%s)(%@=%s))"), 
					       server_key,
					       kuser,
					       client_key,
					       kuser);
    }
    if (queryString == NULL) {
	ret = HDB_ERR_NOENTRY;
	goto out;
    }
    
    do {
	CFErrorRef error = NULL;

	query = ODQueryCreateWithNode(NULL, d->rootNode,
				      kODRecordTypeUserAuthenticationData,
				      attr,
				      matchtype,
				      queryString,
				      d->inNodeAttributes,
				      2,
				      NULL);
	if (query == NULL) {
	    CFRelease(queryString);
	    ret = ENOMEM;
	    goto out;
	}
    
	res = ODQueryCopyResults(query, FALSE, &error);
	if (res == NULL && error == NULL)
	    tryAgain = 0;
	if (error)
	    CFRelease(error);
    } while(res == NULL && tryAgainP(context, &tryAgain, NULL));
    CFRelease(queryString);

    if (res == NULL) {
	ret = HDB_ERR_NOENTRY;
	goto out;
    }

    
    CFIndex count = CFArrayGetCount(res);
    if (count == 0 && createp) {
	CFMutableDictionaryRef attributes;
	CFStringRef name;
	CFUUIDRef uuid;

	uuid = CFUUIDCreate(NULL);
	if (uuid == NULL) {
	    ret = HDB_ERR_NOENTRY;
	    goto out;
	}
	
	name = CFUUIDCreateString(NULL, uuid);
	CFRelease(uuid);
	if (name == NULL) {
	    ret = HDB_ERR_NOENTRY;
	    goto out;
	}
	
	attributes = CFDictionaryCreateMutable(NULL, 0,
					       &kCFTypeDictionaryKeyCallBacks,
					       &kCFTypeDictionaryValueCallBacks);
	if (attributes == NULL) {
	    CFRelease(name);
	    ret = HDB_ERR_NOENTRY;
	    goto out;
	}
	    
	cfRecord = ODNodeCreateRecord(d->rootNode,
				      kODRecordTypeUserAuthenticationData,
				      name,
				      attributes,
				      NULL);
	CFRelease(name);
	CFRelease(attributes);
	if (cfRecord == NULL) {
	    ret = HDB_ERR_NOENTRY;
	    goto out;
	}
	
	if (createdp)
	    *createdp = 1;
	    
    } else if (count != 1) {
	ret = HDB_ERR_NOENTRY;
	goto out;
    } else {
	cfRecord = (ODRecordRef) CFArrayGetValueAtIndex(res, 0);
	if (cfRecord == NULL) {
	    ret = HDB_ERR_NOENTRY;
	    goto out;
	}
	CFRetain(cfRecord);
    }
    *result = cfRecord;
    
out:
    if (res)
	CFRelease(res);
    if (query)
	CFRelease(query);
    if (kuser)
	free(kuser);
    
    return ret;
}


static krb5_error_code
hod_lkdc_fetch(krb5_context context, HDB * db, krb5_const_principal principal,
	       unsigned flags, krb5_kvno kvno, hdb_entry_ex * entry)
{
    krb5_principal eprincipal = NULL;
    hdb_od d = (hdb_od)db->hdb_db;
    ODRecordRef cfRecord = NULL;
    krb5_error_code ret;

    ret = krb5_copy_principal(context, principal, &eprincipal);
    if (ret)
	goto out;

    ret = d->locate_record(context, d, eprincipal, flags, FALSE, NULL, &cfRecord);
    if (ret)
	goto out;

    if (is_lkdc(d, eprincipal->realm))
	map_lkdc_principal(context, eprincipal, wellknown_lkdc, d->LKDCRealm);

    /* set the principal to the username for users in pku2u and lkdc */
    if (krb5_principal_is_pku2u(context, eprincipal) ||
	(krb5_principal_is_lkdc(context, eprincipal) && krb5_principal_get_num_comp(context, eprincipal) == 1))
    {
	CFStringRef name = ODRecordGetRecordName(cfRecord);
	char *str;

	str = rk_cfstring2cstring(name);
	if (str == NULL) {
	    ret = ENOENT;
	    goto out;
	}

	ret = krb5_make_principal(context, &entry->entry.principal,
				  eprincipal->realm, str, NULL);
	free(str);
	if (ret)
	    goto out;
    } else {
	ret = krb5_copy_principal(context, eprincipal, &entry->entry.principal);
	if (ret)
	    goto out;
    }

    ret = od_record2entry(context, db, cfRecord, flags, entry);
    if (ret)
	goto out;

 out:
    if (eprincipal)
	krb5_free_principal(context, eprincipal);
    if (cfRecord)
	CFRelease(cfRecord);
    if (ret) {
	free_hdb_entry(&entry->entry);
	memset(&entry->entry, 0, sizeof(entry->entry));
    }

    return ret;
}

static krb5_error_code
hod_server_fetch(krb5_context context, HDB * db, krb5_const_principal principal,
		 unsigned flags, krb5_kvno kvno, hdb_entry_ex * entry)
{
    krb5_principal eprincipal = NULL;
    hdb_od d = (hdb_od)db->hdb_db;
    ODRecordRef cfRecord = NULL;
    krb5_error_code ret;
    
    
    ret = krb5_copy_principal(context, principal, &eprincipal);
    if (ret)
	goto out;
    
    ret = d->locate_record(context, d, eprincipal, flags, FALSE, NULL, &cfRecord);
    if (ret)
        goto out;
    
    ret = krb5_copy_principal(context, eprincipal, &entry->entry.principal);
    if (ret)
	goto out;
    
    ret = od_record2entry(context, db, cfRecord, flags, entry);
    if (ret)
	goto out;
    
out:
    if (cfRecord)
        CFRelease(cfRecord);
    if (eprincipal)
	krb5_free_principal(context, eprincipal);
    
    if (ret) {
	free_hdb_entry(&entry->entry);
	memset(&entry->entry, 0, sizeof(entry->entry));
    }
    
    return ret;
}


static krb5_error_code
hod_store(krb5_context context, HDB * db, unsigned flags,
	  hdb_entry_ex * entry)
{
    krb5_principal eprincipal = NULL;
    hdb_od d = (hdb_od)db->hdb_db;
    ODRecordRef record = NULL;
    krb5_error_code ret;
    int i, created = 0;

    ret = krb5_copy_principal(context, entry->entry.principal, &eprincipal);
    if (ret)
	goto out;

    ret = d->locate_record(context, d, eprincipal, flags, (flags & HDB_F_REPLACE) == 0, &created, &record);
    if (ret)
	goto out;

    for (i = 0; i < num_keys; i++) {
	if (keys[i].hdb2od == NULL)
	    continue;

	/* logical XOR between server values and LKDC connection */
	if ((!!(keys[i].flags & SERVER_VALUE)) ^ (!d->LKDCRealm))
	    continue;

	ret = (*keys[i].hdb2od)(context, d, keys[i].name, flags, entry, record);
	if (ret)
	    goto out;
    }

    if ((flags & HDB_F_CHANGE_PASSWORD) == 0) {
	ret = hdb_seal_keys(context, db, &entry->entry);
	if(ret)
	    goto out;
    }
    
    ODRecordSynchronize(record, NULL);

 out:
    if (record) {
	if (ret && created)
	    ODRecordDelete(record, NULL);
	CFRelease(record);
    }
    if (eprincipal)
	krb5_free_principal(context, eprincipal);

    return ret;
}

static krb5_error_code
hod_remove(krb5_context context, HDB *db, krb5_const_principal principal)
{
    krb5_principal eprincipal = NULL;
    hdb_od d = (hdb_od)db->hdb_db;
    ODRecordRef record = NULL;
    CFMutableArrayRef array = NULL;
    krb5_error_code ret;
    int i;

    ret = krb5_copy_principal(context, principal, &eprincipal);
    if (ret)
	goto out;

    ret = d->locate_record(context, d, eprincipal, 0, FALSE, NULL, &record);
    if (ret)
	goto out;

    array = CFArrayCreateMutable(kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks);
    if (array == NULL) {
	ret = ENOMEM;
	goto out;
    }

    for (i = 0; i < num_keys; i++) {

	if ((keys[i].flags & DELETE_KEY) == 0)
	    continue;

	if (!ODRecordSetValue(record, keys[i].name, array, NULL)) {
	    ret = HDB_ERR_UK_SERROR;
	    break;
	}
    }

    ODRecordSynchronize(record, NULL);

 out:
    if (eprincipal)
	krb5_free_principal(context, eprincipal);
    if (array)
	CFRelease(array);
    if (record)
	CFRelease(record);
    return ret;
}


static krb5_error_code
hod_get_realms(krb5_context context, HDB *db, krb5_realm **realms)
{
    hdb_od d = (hdb_od)db->hdb_db;

    *realms = NULL;

    if (d->LKDCRealm) {
	*realms = calloc(2, sizeof(realms[0]));
	if (*realms == NULL)
	    return ENOMEM;
	(*realms)[0] = strdup(d->LKDCRealm);
	if ((*realms)[0] == NULL) {
	    free(*realms);
	    *realms = NULL;
	    return ENOMEM;
	}
	(*realms)[1] = NULL;
    }

    return 0;
}

static void
update_ntlm(SCDynamicStoreRef store, CFArrayRef changedKeys, void *info)
{
    hdb_od d = info;
    CFDictionaryRef settings;
    CFStringRef n;

    if (store == NULL || info == NULL)
	return;

    settings = (CFDictionaryRef)SCDynamicStoreCopyValue(store, CFSTR("com.apple.smb"));
    if (settings == NULL)
	return;

    n = CFDictionaryGetValue(settings, CFSTR("NetBIOSName"));
    if (n == NULL || CFGetTypeID(n) != CFStringGetTypeID())
	goto fin;

    if (d->ntlmDomain)
	free(d->ntlmDomain);
    d->ntlmDomain = rk_cfstring2cstring(n);
    strupr(d->ntlmDomain);

fin:
    CFRelease(settings);
    return;
}

static void
ntlm_notification(hdb_od d)
{
    SCDynamicStoreRef store;
    dispatch_queue_t queue;
    SCDynamicStoreContext context;

    memset(&context, 0, sizeof(context));
    context.info = (void*)d;

    store = SCDynamicStoreCreate(kCFAllocatorDefault, CFSTR("com.apple.Kerberos.hdb-od"), update_ntlm, (void *)&context);
    if (store == NULL)
	return;

    CFTypeRef key[] = {CFSTR("com.apple.smb")};
    CFArrayRef nkeys = CFArrayCreate(kCFAllocatorDefault, key, 1, NULL);
    SCDynamicStoreSetNotificationKeys(store, nkeys, NULL);
    CFRelease(nkeys);

    queue = dispatch_queue_create("com.apple.hdb-od.ntlm-name", NULL);
    if (queue == NULL) {
	CFRelease(store);
	errx(1, "dispatch_queue_create");
    }

    SCDynamicStoreSetDispatchQueue(store, queue);
    CFRelease(store);

    dispatch_sync(queue, ^{ update_ntlm(store, NULL, d); });
}

static krb5_error_code
hod_get_ntlm_domain(krb5_context context, struct HDB *db, char **name)
{
    hdb_od d = (hdb_od)db->hdb_db;
    if (d->ntlmDomain == NULL) {
	krb5_set_error_message(context, EINVAL, "no ntlm domain");
	return EINVAL;
    }
    *name = strdup(d->ntlmDomain);
    return 0;
}

static krb5_error_code
hod_password(krb5_context context, struct HDB *db, hdb_entry_ex *entry, const char *password, int is_admin)
{
    int ret;
    char *user;
    
    if (krb5_principal_get_num_comp(context, entry->entry.principal) != 1)
	return EINVAL;
    
    user = (char *)krb5_principal_get_comp_string(context, entry->entry.principal, 0);
    
    ret = pwsf_PasswordServerChangePassword(user, password, is_admin);
    if (ret != 0)
	return EINVAL;
    
    return 0;
}


krb5_error_code
hdb_od_create(krb5_context context, HDB ** db, const char *arg)
{
    CFMutableArrayRef attrs;
    ODNodeRef localRef = NULL;
    ODRecordRef kdcConfRef = NULL;
    CFArrayRef data = NULL;
    krb5_error_code ret;
    hdb_od d = NULL;
    char *path = NULL;
    int i;

    *db = calloc(1, sizeof(**db));
    if (*db == NULL) {
	ret = ENOMEM;
	krb5_set_error_message(context, ret, "malloc: out of memory");
	goto out;
    }
    memset(*db, 0, sizeof(**db));

    path = strdup(arg);
    if (path == NULL) {
	ret = ENOMEM;
	krb5_set_error_message(context, ret, "malloc: out of memory");
	goto out;
    }

    d = calloc(1, sizeof(*d));
    if (d == NULL) {
	ret = ENOMEM;
	krb5_set_error_message(context, ENOMEM, "malloc: out of memory");
	goto out;
    }

    {
	char *p = strchr(path, '&');
	if (p) {
	    *p++ = '\0';
	    d->restoreRoot = CFStringCreateWithCString(NULL, p, kCFStringEncodingUTF8);
	}
    }

    d->rootName = CFStringCreateWithCString(NULL, path, kCFStringEncodingUTF8);
    if (d == NULL) {
	ret = ENOMEM;
	krb5_set_error_message(context, ENOMEM, "malloc: out of memory");
	goto out;
    }

    attrs = CFArrayCreateMutable(NULL, num_keys, &kCFTypeArrayCallBacks);
    for (i = 0; i < num_keys; i++)
	CFArrayAppendValue(attrs, keys[i].name);
    d->inNodeAttributes = attrs;

    attrs = CFArrayCreateMutable(NULL, 1, &kCFTypeArrayCallBacks);
    CFArrayAppendValue(attrs, kRealName);
    d->inKDCAttributes = attrs;
    
    attrs = CFArrayCreateMutable(NULL, 2, &kCFTypeArrayCallBacks);
    CFArrayAppendValue(attrs, kODRecordTypeComputers);
    CFArrayAppendValue(attrs, kODRecordTypeUsers);
    
    d->inComputerOrUsers = attrs;

    (*db)->hdb_db = d;

    (*db)->hdb_capability_flags = HDB_CAP_F_HANDLE_ENTERPRISE_PRINCIPAL;
    (*db)->hdb_master_key_set = 0;
    (*db)->hdb_openp = 0;
    (*db)->hdb_open = hod_open;
    (*db)->hdb_close = hod_close;
    (*db)->hdb_store = hod_store;
    (*db)->hdb_remove = hod_remove;
    (*db)->hdb_firstkey = hod_firstkey;
    (*db)->hdb_nextkey = hod_nextkey;
    (*db)->hdb_lock = hod_lock;
    (*db)->hdb_unlock = hod_unlock;
    (*db)->hdb_rename = NULL;
    (*db)->hdb__get = NULL;
    (*db)->hdb__put = NULL;
    (*db)->hdb__del = NULL;
    (*db)->hdb_destroy = hod_destroy;
    (*db)->hdb_get_realms = hod_get_realms;
    (*db)->hdb_get_ntlm_domain = hod_get_ntlm_domain;


    /*
     * The /Local/Default realm is the LKDC realm, so lets pick up the
     * LKDC configuration that we will use later.
     */

    if (strncmp(path, "/Local/", 7) == 0) {

	/*
	 * Pull out KerberosLocalKDC configuration
	 */

	ret = nodeCreateWithName(context, d, &localRef);
	if (ret)
	    goto out;

	kdcConfRef = ODNodeCopyRecord(localRef, kODRecordTypeConfiguration,
				      CFSTR("KerberosKDC"),
				      d->inKDCAttributes, NULL);
	if (kdcConfRef == NULL) {
	    ret = HDB_ERR_NOENTRY;
	    krb5_set_error_message(context, ret, "Failed to find KerberosKDC node");
	    goto out;
	}

	data = ODRecordCopyValues(kdcConfRef, kRealName, NULL);
	if (data == NULL) {
	    ret = HDB_ERR_NOENTRY;
	    krb5_set_error_message(context, ret, "Failed to copy RealName from KerberosKDC node");
	    goto out;
	}

	if (CFArrayGetCount(data) != 1) {
	    ret = HDB_ERR_NOENTRY;
	    krb5_set_error_message(context, ret, "Found RealName %d from KerberosKDC",
				   (int)CFArrayGetCount(data));
	    goto out;
	}
	d->LKDCRealm = rk_cfstring2cstring((CFStringRef)CFArrayGetValueAtIndex(data, 0));
	if (d->LKDCRealm == NULL) {
	    ret = HDB_ERR_NOENTRY;
	    krb5_set_error_message(context, ret, "failed to find realm");
	    goto out;
	}

	CFRelease(data); data = NULL;
	CFRelease(kdcConfRef); kdcConfRef = NULL;
	CFRelease(localRef); localRef = NULL;

	ntlm_notification(d);
	
	(*db)->hdb_fetch_kvno = hod_lkdc_fetch;
	d->locate_record = lkdc_locate_record;

    } else {
	/* hod_password interface is stupid, pipe can fail and we might not expect it to */
	/* Not the right place for this. */
#ifdef HAVE_SIGACTION
	struct sigaction sa;
	
	sa.sa_flags = 0;
	sa.sa_handler = SIG_IGN;
	sigemptyset(&sa.sa_mask);

	sigaction(SIGPIPE, &sa, NULL);
#else
	signal(SIGPIPE, SIG_IGN);
#endif /* HAVE_SIGACTION */

	(*db)->hdb_capability_flags |= HDB_CAP_F_HANDLE_PASSWORDS;
	(*db)->hdb_password = hod_password;
	(*db)->hdb_fetch_kvno = hod_server_fetch;
	d->locate_record = server_locate_record;
    
    }


    free(path);

    return 0;

  out:
    if (path)
	free(path);
    if (data)
	CFRelease(data);
    if (kdcConfRef)
	CFRelease(kdcConfRef);
    if (localRef)
	CFRelease(localRef);

    if (*db) {
	free(*db);
	*db = NULL;
    }
    if (d) {
	if (d->rootName)
	    CFRelease(d->rootName);
	if (d->restoreRoot)
	    CFRelease(d->restoreRoot);
	free(d);
    }

    return ret;
}

#endif
