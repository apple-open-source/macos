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

#include "config.h"

#include "HeimODAdmin.h"

#ifdef __APPLE_PRIVATE__
#include <OpenDirectory/OpenDirectoryPriv.h>
#endif

#include <krb5.h>
#include <heimbase.h>
#include <hx509.h>
#include <asn1-common.h>
#include <hdb.h>
#include "admin.h"


#define kHeimODKerberosKeys CFSTR("dsAttrTypeNative:KerberosKeys")
#define kHeimODKerberosFlags CFSTR("dsAttrTypeNative:KerberosFlags")
#define kHeimODKerberosUserName CFSTR("dsAttrTypeNative:KerberosUserName")
#define kHeimODKerberosServerName CFSTR("dsAttrTypeNative:KerberosServerName")
static CFStringRef kHeimASI = CFSTR("dsAttrTypeStandard:AltSecurityIdentities");

static CFStringRef kRealName = CFSTR("dsAttrTypeStandard:RealName");

#define kHeimLDAPKerberosUserName CFSTR("dsAttrTypeNative:draft-krbPrincipalName")
#define kHeimLDAPKerberosServerName CFSTR("dsAttrTypeNative:draft-krbPrincipalAliases")
#define kHeimLDAPKerberosKeys CFSTR("dsAttrTypeNative:draft-krbKeySet")
#define kHeimLDAPKerberosFlags CFSTR("dsAttrTypeNative:draft-krbTicketPolicy")



static CFStringRef remove_keys_client[] = {
    kHeimODKerberosUserName,
    kHeimODKerberosServerName,
    kHeimODKerberosKeys,
    kHeimODKerberosFlags,
    CFSTR("dsAttrTypeNative:KerberosMaxLife"),
    CFSTR("dsAttrTypeNative:KerberosMaxRenew"),
    CFSTR("dsAttrTypeNative:KerberosValidStart"),
    CFSTR("dsAttrTypeNative:KerberosValidEnd")
};
static const int num_remove_keys_client = sizeof(remove_keys_client) / sizeof(remove_keys_client[0]);

static CFStringRef remove_keys_server[] = {
    kHeimLDAPKerberosUserName,
    kHeimLDAPKerberosServerName,
    kHeimLDAPKerberosKeys,
    kHeimLDAPKerberosFlags
};
static const int num_remove_keys_server = sizeof(remove_keys_server) / sizeof(remove_keys_server[0]);


static CFStringRef server_fetch[] = {
    kHeimLDAPKerberosUserName,
    kHeimLDAPKerberosServerName,
    kHeimLDAPKerberosKeys,
    kHeimLDAPKerberosFlags
};
static const int num_server_fetch = sizeof(server_fetch) / sizeof(server_fetch[0]);


#define kHeimODKerberosACL CFSTR("dsAttrTypeNative:KerberosACL")
#define kHeimLDAPKerberosACL CFSTR("dsAttrTypeNative:draft-krbPrincipalACL")

struct s2k {
    CFStringRef s;
    unsigned int k;
};

struct s2k FlagsKeys[] = {
    { kPrincipalFlagInitial, 		(1<<0) },
    { kPrincipalFlagForwardable,	(1<<1) },
    { kPrincipalFlagProxyable,		(1<<2) },
    { kPrincipalFlagRenewable,		(1<<3) },
    { kPrincipalFlagServer,		(1<<5) },
    { kPrincipalFlagInvalid,		(1<<7) },
    { kPrincipalFlagRequireStrongPreAuthentication, (1<<8) },
    { kPrincipalFlagPasswordChangeService, (1<<9) },
    { kPrincipalFlagOKAsDelegate,	(1<<11) },
    { kPrincipalFlagImmutable,		(1<<13) },
    { NULL }
};

struct s2k ACLKeys[] = {
    { kHeimODACLChangePassword,		KADM5_PRIV_CPW },
    { kHeimODACLList,			KADM5_PRIV_LIST },
    { kHeimODACLDelete,			KADM5_PRIV_DELETE },
    { kHeimODACLModify,			KADM5_PRIV_MODIFY },
    { kHeimODACLAdd,			KADM5_PRIV_ADD },
    { kHeimODACLGet,			KADM5_PRIV_GET },
    { NULL }
};

static CFIndex
createError(CFAllocatorRef alloc, CFErrorRef *error, CFIndex errorcode, CFStringRef fmt, ...)
{
    void *keys[1] = { (void *)CFSTR("HODErrorMessage") };
    void *values[1];
    va_list va;

    if (error == NULL)
	return errorcode;

    va_start(va, fmt);
    values[0] = (void *)CFStringCreateWithFormatAndArguments(alloc, NULL, fmt, va);
    va_end(va);
    if (values[0] == NULL) {
	*error = NULL;
	return errorcode;
    }
    
    *error = CFErrorCreateWithUserInfoKeysAndValues(alloc, CFSTR("com.apple.Heimdal.ODAdmin"), errorcode,
						    (const void * const *)keys,
						    (const void * const *)values, 1);
    CFRelease(values[0]);
    return errorcode;
}

static char *
cfstring2cstring(CFStringRef string)
{
    CFIndex l;
    char *s;
	
    s = (char *)CFStringGetCStringPtr(string, kCFStringEncodingUTF8);
    if (s)
	return strdup(s);

    l = CFStringGetMaximumSizeForEncoding(CFStringGetLength(string), kCFStringEncodingUTF8) + 1;
    s = malloc(l);
    if (s == NULL)
	return NULL;
    
    if (!CFStringGetCString(string, s, l, kCFStringEncodingUTF8)) {
	free (s);
	return NULL;
    }

    return s;
}

static unsigned int
string2int(const struct s2k *t, CFStringRef s)
{
    while (t->s) {
	if (CFStringCompare(s, t->s, kCFCompareCaseInsensitive) == kCFCompareEqualTo)
	    return t->k;
	t++;
    }
	    
    return 0;
}

static bool
is_record_server_location(ODRecordRef record)
{
    CFArrayRef values;
    bool is_server = false;

    values = ODRecordCopyValues(record, kODAttributeTypeMetaNodeLocation, NULL);
    if (values == NULL)
	return false;

    if (CFArrayGetCount(values) > 0) {
	CFStringRef str = (CFStringRef)CFArrayGetValueAtIndex(values, 0);
	if (CFStringGetTypeID() == CFGetTypeID(str) &&
	    CFStringHasPrefix(str, CFSTR("/LDAPv3/")))
	{
	    is_server = true;
	}
    }
    CFRelease(values);

    return is_server;
}

static bool
force_server(void)
{
    static dispatch_once_t once;
    static bool force = false;

    dispatch_once(&once, ^{
	    CFBooleanRef b;
	    b = CFPreferencesCopyAppValue(CFSTR("ForceHeimODServerMode"),
					  CFSTR("com.apple.Kerberos"));
	    if (b && CFGetTypeID(b) == CFBooleanGetTypeID())
		force = CFBooleanGetValue(b);
	    if (b)
		CFRelease(b);
	});
    return force;
}

static bool
is_record_server(ODRecordRef record)
{
    if (force_server())
	return true;

    return is_record_server_location(record);
}

static ODRecordRef
copyDataRecord(ODNodeRef node, ODRecordRef record,
	       CFStringRef ckey, CFStringRef skey,
	       CFStringRef *key, bool *server_record_p,
	       CFErrorRef *error)
{
    /* XXX copy user node */
    bool is_server = is_record_server_location(record);

    if (is_server) {

	if (force_server()) {
	    CFRetain(record);
	} else {
#ifdef __APPLE_PRIVATE__
            if (CFStringCompare(ODRecordGetRecordType(record), kODRecordTypeUserAuthenticationData, 0) == kCFCompareEqualTo) {
                CFRetain(record);
            } else {
	        record = ODNodeCopyRecordAuthenticationData(node, record, error);
	        if (record == NULL && error != NULL && *error == NULL)
		    createError(NULL, error, 1, CFSTR("can't map user record to UserAuthenticationRecord"));
            }
#else
	    CFRetain(record);
#endif
	}

	if (key)
	    *key = skey;
    } else {
	CFRetain(record);

	if (key)
	    *key = ckey;
    }

    if (server_record_p)
	*server_record_p = is_server;

    return record;
}


static unsigned int
flags2int(const struct s2k *t, CFTypeRef type)
{
    unsigned int nflags = 0;
    
    if (t == NULL)
	abort();
    
    if (CFGetTypeID(type) == CFStringGetTypeID()) {
	
	nflags = string2int(t, type);
	
    } else if (CFGetTypeID(type) == CFArrayGetTypeID()) {
	CFIndex n;
	
	for (n = 0; n < CFArrayGetCount(type); n++) {
	    CFTypeRef el = CFArrayGetValueAtIndex(type, n);

	    if (CFGetTypeID(el) == CFStringGetTypeID())
		nflags |= string2int(t, el);
	}
    }
    return nflags;
}

static CFArrayRef
int2flags(const struct s2k *t, unsigned long n)
{
    CFMutableArrayRef array;

    array = CFArrayCreateMutable(kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks);
    if (array == NULL)
	return NULL;

    while (t->s) {
	if (t->k & n)
	    CFArrayAppendValue(array, t->s);
	t++;
    }

    return array;
}

/*
 *
 */

int
HeimODCreateRealm(ODNodeRef node, CFStringRef realm, CFErrorRef *error)
{
    /* create
     * - krbtgt/realm
     * - kadmin/admin
     * - kadmin/changepw
     * - WELLKNOWN/ANONYMOUS
     */
    if (error)
	*error = NULL;
    return 0;
}

/*
 *
 */

int
HeimODCreatePrincipalData(ODNodeRef node, ODRecordRef record, CFTypeRef flags, CFStringRef principal, CFErrorRef *error)
{
    CFStringRef element;
    bool r = false;
    bool is_server = false;
    unsigned int nflags =
	(1<<1) /* forwardable */ | 
	(1<<2) /* proxiable */ | 
	(1<<6) /* client */ |
	(1<<7) /* invalid */;
    CFStringRef kflags, kusername;
    ODRecordRef datarecord = NULL;
    
    if (error)
	*error = NULL;
    
    datarecord = copyDataRecord(node, record, NULL, NULL, NULL, &is_server, error);
    if (datarecord == NULL)
	goto out;

    if (is_server) {
	kflags = kHeimLDAPKerberosFlags;
	kusername = kHeimLDAPKerberosUserName;
    } else {
	kflags = kHeimODKerberosFlags;
	kusername = kHeimODKerberosUserName;
    }

    if (flags)
	nflags |= flags2int(FlagsKeys, flags);
    
    element = CFStringCreateWithFormat(kCFAllocatorDefault,
				       NULL, CFSTR("%lu"), nflags);
    if (element == NULL) {
	createError(NULL, error, ENOMEM, CFSTR("out of memory"));
	goto out;
    }
    
    r = ODRecordSetValue(datarecord, kflags, element, error);
    CFRelease(element);
    if (!r)
	goto out;
    
    if (principal) {
	r = ODRecordSetValue(datarecord, kusername, principal, error);
	if (!r)
	    goto out;
    }
    
    if (!ODRecordSynchronize(datarecord, NULL))
	r = false;

out:
    if (datarecord)
	CFRelease(datarecord);
    return r ? 0 : 1;
}

/*
 *
 */

int
HeimODRemovePrincipalData(ODNodeRef node, ODRecordRef record, CFStringRef principal, CFErrorRef *error)
{
    CFMutableArrayRef array;
    ODRecordRef datarecord;
    CFStringRef *remove_keys;
    int num_remove_keys;
    bool is_server = false;
    CFIndex i;
    int ret = 1;

    datarecord = copyDataRecord(node, record, NULL, NULL, NULL, &is_server, error);
    if (datarecord == NULL)
	goto out;

    if (is_server) {
	remove_keys = remove_keys_server;
	num_remove_keys = num_remove_keys_server;
    } else {
	remove_keys = remove_keys_client;
	num_remove_keys = num_remove_keys_client;
    }

    array = CFArrayCreateMutable(kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks);
    if (array == NULL) {
	createError(NULL, error, 1, CFSTR("out of memory"));
	goto out;
    }
    
    for (i = 0; i < num_remove_keys; i++)
	ODRecordSetValue(datarecord, remove_keys[i], array, NULL);
    
    CFRelease(array);

    if (!ODRecordSynchronize(datarecord, error))
	ret = 1;
    else
	ret = 0;

 out:
    if (datarecord)
	CFRelease(datarecord);

    return ret;
}

/*
 *
 */

static unsigned long
getflags(ODRecordRef datarecord, CFStringRef kflags, CFErrorRef *error)
{
    CFArrayRef array;
    CFStringRef s;
    int32_t n;

    array = ODRecordCopyValues(datarecord, kflags, error);
    if (array == NULL)
	return 0;

    if (CFArrayGetCount(array) < 1) {
	CFRelease(array);
	return 0;
    }
    s = CFArrayGetValueAtIndex(array, 0);

    if (CFStringGetTypeID() != CFGetTypeID(s)) {
	CFRelease(array);
	return 0;
    }

    n = CFStringGetIntValue(s);
    CFRelease(array);

    if (n == INT_MAX || n == INT_MIN)
	return 0;

    return n;
}

static int
flagop(ODRecordRef datarecord,
       CFStringRef kflags,
       CFTypeRef flags, 
       const struct s2k *keys, CFErrorRef *error,
       unsigned long (^op)(unsigned long oldflags, unsigned long newflags))
{
    unsigned long uflags, oldflags;
    CFStringRef element;
    bool r = false;

    uflags = flags2int(keys, flags);
    if (uflags == 0) {
	createError(NULL, error, 1, CFSTR("failed to parse input flags"));
	goto out;
    }
    
    oldflags = getflags(datarecord, kflags, error);

    uflags = op(oldflags, uflags);
    
    element = CFStringCreateWithFormat(kCFAllocatorDefault,
				       NULL, CFSTR("%lu"), uflags);
    if (element == NULL) {
	createError(NULL, error, ENOMEM, CFSTR("out of memory"));
	goto out;
    }
    
    r = ODRecordSetValue(datarecord, kflags, element, error);
    CFRelease(element);

 out:
    if (!r)
	return 1;
    return 0;
}

/*
 *
 */

int
HeimODSetKerberosFlags(ODNodeRef node, ODRecordRef record, CFTypeRef flags, CFErrorRef *error)
{
    int ret;
    ODRecordRef datarecord;
    bool is_server;
    CFStringRef kflags;

    datarecord = copyDataRecord(node, record,
				kHeimODKerberosFlags, kHeimLDAPKerberosFlags,
				&kflags, &is_server, error);
    if (datarecord == NULL)
	return 1;

    ret = flagop(datarecord, kflags, flags, FlagsKeys, error, ^(unsigned long oldflags, unsigned long newflags) {
	    return (oldflags | newflags);
	});

    if (ret == 0 && !ODRecordSynchronize(datarecord, error))
	ret = 1;

    CFRelease(datarecord);

    return ret;
}

/*
 *
 */

CFArrayRef
HeimODCopyKerberosFlags(ODNodeRef node, ODRecordRef record, CFErrorRef *error)
{
    unsigned long uflags;
    CFArrayRef flags = NULL;
    CFStringRef kflags;

    ODRecordRef datarecord;

    datarecord = copyDataRecord(node, record, 
				kHeimODKerberosFlags, kHeimLDAPKerberosFlags, 
				&kflags, NULL, error);
    if (datarecord == NULL)
	goto out;

    uflags = getflags(datarecord, kflags, error);
    if (uflags == 0) {
	createError(NULL, error, 1, CFSTR("failed to parse input flags"));
	goto out;
    }

    flags = int2flags(FlagsKeys, uflags);

 out:
    if (datarecord)
	CFRelease(datarecord);
    return flags;
}

/*
 *
 */

int
HeimODClearKerberosFlags(ODNodeRef node, ODRecordRef record, CFTypeRef flags, CFErrorRef *error)
{
    ODRecordRef datarecord;
    int ret;
    bool is_server;
    CFStringRef kflags;

    datarecord = copyDataRecord(node, record, NULL, NULL, NULL, &is_server, error);
    if (datarecord == NULL)
	return 1;

    if (is_server) {
	kflags = kHeimLDAPKerberosFlags;
    } else {
	kflags = kHeimODKerberosFlags;
    }

    ret = flagop(datarecord, kflags, flags, FlagsKeys, error, ^(unsigned long oldflags, unsigned long newflags) {
	    return (oldflags & (~newflags));
	});

    if (ret == 0 && !ODRecordSynchronize(datarecord, error))
	ret = 1;

    CFRelease(datarecord);
    return ret;
}

/*
 *
 */

static CFTypeRef
remapacl(CFTypeRef flags, CFErrorRef *error)
{
    if (CFGetTypeID(flags) == CFStringGetTypeID() && CFStringCompare(flags, kHeimODACLAll, kCFCompareCaseInsensitive) == kCFCompareEqualTo) {
	CFMutableArrayRef nflags;
	CFIndex n;
	
	nflags = CFArrayCreateMutable(NULL, sizeof(ACLKeys) / sizeof(ACLKeys[0]), &kCFTypeArrayCallBacks);
	if (nflags == NULL) {
	    createError(NULL, error, ENOMEM, CFSTR("out of memory"));
	    return NULL;
	}
	for (n = 0; ACLKeys[n].s; n++)
	    CFArrayAppendValue(nflags, ACLKeys[n].s);
	flags = nflags;
    } else {
	CFRetain(flags);
    }
    return flags;
}

/*
 *
 */

int
HeimODSetACL(ODNodeRef node, ODRecordRef record, CFTypeRef flags, CFErrorRef *error)
{
    int ret;
    ODRecordRef datarecord;
    CFStringRef kflags;

    datarecord = copyDataRecord(node, record,
				kHeimODKerberosACL, kHeimLDAPKerberosACL,
				&kflags, NULL, error);
    if (datarecord == NULL)
	return 1;
    
    flags = remapacl(flags, error);
    if (flags == NULL)
	return 1;

    ret = flagop(datarecord, kflags, flags, ACLKeys, error, ^(unsigned long oldflags, unsigned long newflags) {
	    return newflags;
	});

    if (ret == 0 && !ODRecordSynchronize(datarecord, error))
	ret = 1;

    CFRelease(datarecord);

    CFRelease(flags);

    return ret;
}

CFArrayRef
HeimODCopyACL(ODNodeRef node, ODRecordRef record, CFErrorRef *error)
{
    unsigned long uflags;
    CFArrayRef flags = NULL;
    CFStringRef kflags;

    ODRecordRef datarecord;

    datarecord = copyDataRecord(node, record, 
				kHeimODKerberosACL, kHeimLDAPKerberosACL, 
				&kflags, NULL, error);
    if (datarecord == NULL)
	goto out;

    uflags = getflags(datarecord, kflags, error);
    if (uflags == 0) {
	createError(NULL, error, 1, CFSTR("failed to parse input flags"));
	goto out;
    }

    flags = int2flags(ACLKeys, uflags);

 out:
    if (datarecord)
	CFRelease(datarecord);
    return flags;
}


int
HeimODClearACL(ODNodeRef node, ODRecordRef record, CFTypeRef flags, CFErrorRef *error)
{
    ODRecordRef datarecord;
    int ret;
    bool is_server;
    CFStringRef kflags;
    
    datarecord = copyDataRecord(node, record, kHeimODKerberosACL, kHeimLDAPKerberosACL, &kflags, &is_server, error);
    if (datarecord == NULL)
	return 1;

    flags = remapacl(flags, error);
    if (flags == NULL)
	return 1;
    
    ret = flagop(datarecord, kflags, flags, ACLKeys, error, ^(unsigned long oldflags, unsigned long newflags) {
	return (oldflags & (~newflags));
    });
    
    if (ret == 0 && !ODRecordSynchronize(datarecord, error))
	ret = 1;
    
    CFRelease(flags);
    CFRelease(datarecord);
    return ret;
}

/*
 *
 */

int
HeimODAddServerAlias(ODNodeRef node, ODRecordRef record, CFStringRef alias, CFErrorRef *error)
{
    CFErrorRef localerror = NULL;
    bool r = false;
    CFStringRef key;
    ODRecordRef datarecord;

    datarecord = copyDataRecord(node, record, 
				kHeimODKerberosServerName, 
				kHeimLDAPKerberosServerName, &key,
				NULL, error);

    if (datarecord == NULL)
	goto out;

    r = ODRecordAddValue(datarecord, key, alias, &localerror);
    if (!r) {
	/* check for duplicate errors and ignore them */
	if (localerror && CFErrorGetCode(localerror) == kODErrorRecordAttributeValueSchemaError) {
	    CFRelease(localerror);
	    r = true;
	} else if (error)
	    *error = localerror;
	else if (localerror)
	    CFRelease(localerror);

	goto out;
    }
    
    if (!ODRecordSynchronize(datarecord, error))
	r = false;
out:
    if (datarecord)
	CFRelease(datarecord);
    return r ? 0 : 1;
}

/*
 *
 */

int
HeimODRemoveServerAlias(ODNodeRef node, ODRecordRef record, CFStringRef alias, CFErrorRef *error)
{
    bool r = false;
    CFStringRef key;
    ODRecordRef datarecord;

    datarecord = copyDataRecord(node, record, 
				kHeimODKerberosServerName, 
				kHeimLDAPKerberosServerName, &key,
				NULL, error);

    if (datarecord == NULL)
	goto out;

    r = ODRecordRemoveValue(datarecord, key, alias, error);
    if (!r)
	goto out;
    
    if (!ODRecordSynchronize(datarecord, error))
	r = false;
out:
    if (datarecord)
	CFRelease(datarecord);
    return r ? 0 : 1;
}

/*
 *
 */

CFArrayRef
HeimODCopyServerAliases(ODNodeRef node, ODRecordRef record, CFErrorRef *error)
{
    ODRecordRef datarecord;
    CFStringRef key;
    CFArrayRef res = NULL;

    datarecord = copyDataRecord(node, record, 
				kHeimODKerberosServerName, 
				kHeimLDAPKerberosServerName, &key,
				NULL, error);
    if (datarecord == NULL)
	goto out;

    res = ODRecordCopyValues(datarecord, key, error);
 out:
    if (datarecord)
	CFRelease(datarecord);
    return res;
}

int
HeimODSetKerberosMaxLife(ODNodeRef node, ODRecordRef record, time_t t, CFErrorRef *error)
{
    if (error)
	*error = NULL;
    return 0;
}

/*
 *
 */

time_t
HeimODGetKerberosMaxLife(ODNodeRef node, ODRecordRef record, CFErrorRef *error)
{
    if (error)
	*error = NULL;
    return 0;
}

/*
 *
 */

int
HeimODSetKerberosMaxRenewable(ODNodeRef node, ODRecordRef record, time_t t, CFErrorRef *error)
{
    if (error)
	*error = NULL;
    return 0;
}

/*
 *
 */

time_t
HeimODGetKerberosMaxRenewable(ODNodeRef node, ODRecordRef record, CFErrorRef *error)
{
    abort();
    if (error)
	*error = NULL;
    return 0;
}

static CFStringRef
getkeykey(ODRecordRef record)
{
    if (is_record_server(record))
	return kHeimLDAPKerberosKeys;
    return kHeimODKerberosKeys;
}

static unsigned
last_kvno_array(CFArrayRef array, krb5_context context, krb5_principal principal)
{
    unsigned kvno = 0;
    CFIndex i;
    
    for (i = 0; i < CFArrayGetCount(array); i++) {
	CFDataRef key = CFArrayGetValueAtIndex(array, i);
	hdb_keyset keyset;
	int ret;
	
	if (key == NULL || CFGetTypeID(key) != CFDataGetTypeID())
	    continue;
	
	ret = decode_hdb_keyset(CFDataGetBytePtr(key), CFDataGetLength(key), &keyset, NULL);
	if (ret)
	    continue;
	
	if (principal) {
	    if (keyset.principal == NULL || krb5_principal_compare(context, keyset.principal, principal) == 0) {
		free_hdb_keyset(&keyset);
		continue;
	    }
	} else {
	    if (keyset.principal) {
		free_hdb_keyset(&keyset);
		continue;
	    }
	}
	
	if (kvno < keyset.kvno)
	    kvno = keyset.kvno;
	
	free_hdb_keyset(&keyset);
    }
    return kvno;
}

static unsigned
last_kvno_record(ODRecordRef datarecord, krb5_context context, krb5_principal principal)
{
    CFArrayRef array;
    unsigned kvno = 0;

    array = ODRecordCopyValues(datarecord, getkeykey(datarecord), NULL);
    if (array == NULL)
	return 0;

    kvno = last_kvno_array(array, context, principal);
 
    CFRelease(array);

    return kvno;
}

static krb5_enctype
find_enctype(krb5_context context, CFStringRef string)
{
    krb5_enctype ret;
    krb5_enctype e;
    char *str = cfstring2cstring(string);

    ret = krb5_string_to_enctype(context, str, &e);
    free(str);
    if (ret)
	return ENCTYPE_NULL;
    return e;
}

static void
applier(const void *value, void *context)
{
    void (^block)(const void *value) = context;
    block(value);
}

static void
arrayapplyblock(CFArrayRef theArray, CFRange range, void (^block)(const void *value))
{
    CFArrayApplyFunction(theArray, range, applier, block);
}

static char *
get_lkdc_realm(void)
{
    ODNodeRef localRef = NULL;
    ODRecordRef kdcConfRef = NULL;
    CFArrayRef data = NULL;
    char *LKDCRealm = NULL;

    localRef = ODNodeCreateWithName(kCFAllocatorDefault, kODSessionDefault,
				    CFSTR("/Local/Default"), NULL);
    if (localRef == NULL)
	goto out;

    kdcConfRef = ODNodeCopyRecord(localRef, kODRecordTypeConfiguration,
				  CFSTR("KerberosKDC"), NULL, NULL);
    if (kdcConfRef == NULL)
	goto out;
    
    data = ODRecordCopyValues(kdcConfRef, kRealName, NULL);
    if (data == NULL)
	goto out;
	
    if (CFArrayGetCount(data) != 1)
	goto out;

    LKDCRealm = cfstring2cstring((CFStringRef)CFArrayGetValueAtIndex(data, 0));

 out:
    if (data)
	CFRelease(data);
    if (kdcConfRef)
	CFRelease(kdcConfRef);
    if (localRef)
	CFRelease(localRef);

    return LKDCRealm;
}

/*
 *
 */

static CFDataRef
update_keys(krb5_context context,
	    CFArrayRef enctypes,
	    unsigned kvno,
	    krb5_principal princ,
	    int include_principal,
	    const char *password,
	    CFErrorRef *error)
{
    heim_octet_string data;
    __block hdb_keyset key;
    __block krb5_error_code ret;
    CFArrayRef defaultenctypes = NULL;
    CFDataRef element = NULL;
    size_t size;

    memset(&key, 0, sizeof(key));
    
    if (error)
	*error = NULL;
    

    if (enctypes == NULL) {
	enctypes = defaultenctypes = HeimODCopyDefaultEnctypes(error);
	if (enctypes == NULL)
	    return NULL;
    }

    key.kvno = kvno;
    
    if (include_principal) {
	ret = krb5_copy_principal(context, princ, &key.principal);
	if (ret) {
	    createError(NULL, error, ret, CFSTR("out of memory"));
	    goto out;
	}
    }
    

    arrayapplyblock(enctypes, CFRangeMake(0, CFArrayGetCount(enctypes)), ^(const void *value) {
	void *ptr;
	
	if (CFGetTypeID(value) != CFStringGetTypeID())
	    return;
	
	krb5_enctype e = find_enctype(context, (CFStringRef)value);
	if (e == ENCTYPE_NULL)
	    return;
	
	ptr = realloc(key.keys.val, (key.keys.len + 1) * sizeof(key.keys.val[0]));
	if (ptr == NULL)
	    return;
	key.keys.val = ptr;
	
	/* clear new entry */
	memset(&key.keys.val[key.keys.len], 0, sizeof(key.keys.val[0]));
	
	if (password) {
	    krb5_data pw, opaque;
	    krb5_salt salt;
	    
	    krb5_get_pw_salt(context, princ, &salt);
	    
	    pw.data = (void *)password;
	    pw.length = strlen(password);
	    
	    krb5_data_zero(&opaque);
	    
	    ret = krb5_string_to_key_data_salt_opaque (context,
						       e,
						       pw,
						       salt,
						       opaque,
						       &key.keys.val[key.keys.len].key);
	    
	    /* Now set salt */
	    key.keys.val[key.keys.len].salt = calloc(1, sizeof(*key.keys.val[key.keys.len].salt));
	    if (key.keys.val[key.keys.len].salt == NULL)
		abort();
	    
	    key.keys.val[key.keys.len].salt->type = salt.salttype;
	    krb5_data_zero(&key.keys.val[key.keys.len].salt->salt);
	    
	    ret = krb5_data_copy(&key.keys.val[key.keys.len].salt->salt,
				 salt.saltvalue.data,
				 salt.saltvalue.length);
	    if (ret)
		abort();
	    
	    krb5_free_salt(context, salt);
	    
	} else {
	    ret = krb5_generate_random_keyblock(context, e, &key.keys.val[key.keys.len].key);
	}
	if (ret)
	    abort();
	
	key.keys.len++;
	
    });
    if (key.keys.len == 0) {
	ret = 1;
	createError(NULL, error, ret, CFSTR("no Kerberos enctypes found"));
	goto out;
    }
    
#if 0 /* XXX */
    /* seal using master key */
    for (i = 0; i < key.keys.len; i++) {
	ret = hdb_seal_key_mkey(context, &key.keys.val[i], mkey);
	if (ret) abort();
    }
#endif

    ASN1_MALLOC_ENCODE(hdb_keyset, data.data, data.length, &key, &size, ret);
    if (ret) {
	createError(NULL, error, ret, CFSTR("Failed to encode keyset"));
	goto out;
    }
    if (data.length != size)
	krb5_abortx(context, "internal asn.1 encoder error");
    
    element = CFDataCreate(kCFAllocatorDefault, data.data, data.length);
    free(data.data);
    if (element == NULL) {
	ret = ENOMEM;
	goto out;
    }

out:
    if (defaultenctypes)
	CFRelease(defaultenctypes);

    free_hdb_keyset(&key);

    return element;
}

int
HeimODSetKeys(ODNodeRef node, ODRecordRef record, CFStringRef principal, CFArrayRef enctypes, CFStringRef password, unsigned long flags, CFErrorRef *error)
{
    krb5_context context;
    CFDataRef element = NULL;
    __block krb5_error_code ret;
    char *princstr = NULL, *passwordstr = NULL;
    krb5_principal princ = NULL;
    ODRecordRef datarecord = NULL;
    bool is_server;
    unsigned kvno;

    ret = krb5_init_context(&context);
    if (ret) {
	createError(NULL, error, 1, CFSTR("can't create kerberos context"));
	return 1;
    }

    datarecord = copyDataRecord(node, record, NULL, NULL, NULL, &is_server, error);
    if (datarecord == NULL)
	goto out;

    if (principal) {
	princstr = cfstring2cstring(principal);
    } else if (is_server == 0) {
	CFStringRef name = ODRecordGetRecordName(record);
	char *str;

	str = cfstring2cstring(name);
	if (str == NULL) {
	    ret = ENOENT;
	    goto out;
	}
	asprintf(&princstr, "%s@%s", str, KRB5_LKDC_REALM_NAME);
	free(str);

    } else {
	CFArrayRef array;
	CFStringRef user;

	array = ODRecordCopyValues(datarecord, kHeimLDAPKerberosUserName, error);
	if (array == NULL) {
	    ret = 1;
	    goto out;
	}

	if (CFArrayGetCount(array) < 1) {
	    CFRelease(array);
	    ret = 1;
	    createError(NULL, error, ret, CFSTR("can't find user principal name"));
	    goto out;
	}

	user = CFArrayGetValueAtIndex(array, 0);
	if (user == NULL || CFGetTypeID(user) != CFStringGetTypeID()) {
	    CFRelease(array);
	    ret = 1;
	    createError(NULL, error, ret, CFSTR("user principal name not a string"));
	    goto out;
	}

	princstr = cfstring2cstring(user);

	CFRelease(array);
    }
    if (princstr == NULL) {
	createError(NULL, error, ENOMEM, CFSTR("out of memory"));
	ret = ENOMEM;
	goto out;
    }

    ret = krb5_parse_name(context, princstr, &princ);
    if (ret) {
	createError(NULL, error, ret, CFSTR("failed to parse principal"));
	goto out;
    }

    /*
     * Covert if we have a password, otherwise we'll us a random key.
     */

    if (password) {
	if (CFDataGetTypeID() == CFGetTypeID(password)) {
	    CFDataRef data = (CFDataRef)password;
	    passwordstr = malloc(CFDataGetLength(data) + 1);
	    if (passwordstr == NULL) {
		createError(NULL, error, ENOMEM, CFSTR("out of memory"));
		ret = 1;
		goto out;
	    }

	    memcpy(passwordstr, CFDataGetBytePtr(data), CFDataGetLength(data));
	    passwordstr[CFDataGetLength(data)] = '\0';

	} else if (CFStringGetTypeID() == CFGetTypeID(password)) {
	    passwordstr = cfstring2cstring(password);
	    if (passwordstr == NULL) {
		createError(NULL, error, ENOMEM, CFSTR("out of memory"));
		ret = 1;
		goto out;
	    }
	} else {
	    createError(NULL, error, EINVAL, CFSTR("invalid string type"));
	    ret = 1;
	    goto out;
	}
    }

    kvno = last_kvno_record(datarecord, context, principal ? princ : NULL) + 1;

    /*
     * If it the LKDC domain, gets go and get the realm from the
     * local realm database to make sure the we don't use the same
     * salting for every principal out there.
     */

    if (krb5_principal_is_lkdc(context, princ)) {
	char *realm = get_lkdc_realm();
	krb5_principal_set_realm(context, princ, realm);
	free(realm);
    }
    
    /* build the new keyset */
    element = update_keys(context, enctypes, kvno, princ, principal != 0, passwordstr, error);
    if (element == NULL)
	goto out;

    if ((flags & kHeimODAdminSetKeysAppendKey) == 0) {
	CFMutableArrayRef array;

	array = CFArrayCreateMutable(kCFAllocatorDefault, 1,
				     &kCFTypeArrayCallBacks);
	if (array == NULL) {
	    ret = ENOMEM;
	    goto out;
	}
	CFArrayAppendValue(array, element);
	
	bool r = ODRecordSetValue(datarecord, getkeykey(datarecord), array, error); 
	CFRelease(array);
	if (!r) {
	    ret = EINVAL;
	    goto out;
	}
    } else {
	bool r = ODRecordAddValue(datarecord, getkeykey(datarecord), element, error); 
	if (!r) {
	    ret = EINVAL;
	    goto out;
	}
    }

    if (!ODRecordSynchronize(datarecord, error))
	ret = EINVAL;

 out:
    if (princ)
	krb5_free_principal(context, princ);
    if (princstr)
	free(princstr);
    if (passwordstr) {
	memset(passwordstr, 0, strlen(passwordstr));
	free(passwordstr);
    }
    if (datarecord)
	CFRelease(datarecord);
    if (context)
	krb5_free_context(context);
    if (element)
	CFRelease(element);

    return ret;
}

static void
delete_enctypes(krb5_context context, CFArrayRef enctypes, CFMutableArrayRef keyset)
{
    CFIndex n, m, o, count;
    krb5_enctype *etlist;

    count = CFArrayGetCount(enctypes);
    if (count == 0)
	return;
    
    etlist = calloc(count + 1, sizeof(*etlist));
    if (etlist == NULL)
	return;

    for (n = m = 0; n < count; n++) {
	CFStringRef str = CFArrayGetValueAtIndex(enctypes, n);
	if (CFGetTypeID(str) != CFStringGetTypeID())
	    continue;
	etlist[m] = find_enctype(context, str);
	if (etlist[m] != ETYPE_NULL)
	    m++;
    }
    if (m == 0) {
	free(etlist);
	return;
    }
    
    count = CFArrayGetCount(keyset);
    
    for (n = 0; n < count; n++) {
	krb5_error_code ret;
	hdb_keyset key;
	size_t sz;
	int found = 0;

	CFDataRef el = CFArrayGetValueAtIndex(keyset, n);
	if (CFGetTypeID(el) != CFDataGetTypeID())
	    continue;
	
	ret = decode_hdb_keyset(CFDataGetBytePtr(el), CFDataGetLength(el), &key, &sz);
	if (ret)
	    continue;
	
	for (m = 0; etlist[m] != ETYPE_NULL; m++) {
	    for (o = 0; o < key.keys.len; o++) {
		if (key.keys.val[o].key.keytype == etlist[m]) {
		    /* delete key and shift down keys */
		    free_Key(&key.keys.val[o]);
		    if (o + 1 != key.keys.len)
			memmove(&key.keys.val[o], &key.keys.val[o + 1], sizeof(key.keys.val[o]) * (key.keys.len - o - 1));
		    found = 1;
		    key.keys.len--;
		    o--;
		    if (key.keys.len == 0)
			free(key.keys.val);
		}
	    }
	}
	if (found && key.keys.len == 0) {
	    CFArrayRemoveValueAtIndex(keyset, n);
	    count--;
	    n--;
	} else if (found) {
	    /* found a key, re-encode and put back in the array */
	    size_t length;
	    void *data;

	    ASN1_MALLOC_ENCODE(hdb_keyset, data, length, &key, &sz, ret);
	    if (ret)
		return;
	    if (length != sz)
		krb5_abortx(context, "internal asn1 error");

	    CFDataRef d = CFDataCreate(NULL, data, length);
	    if (d == NULL)
		abort();
	    
	    CFArraySetValueAtIndex(keyset, n, d);
	    CFRelease(d);
	}
	free_hdb_keyset(&key);
    }
    
    free(etlist);
    return;
}

CFArrayRef
HeimODModifyKeys(CFArrayRef prevKeyset,
		 CFStringRef principal,
		 CFArrayRef enctypes,
		 CFStringRef password,
		 unsigned long flags,
		 CFErrorRef *error)
{
    CFMutableArrayRef keyset = NULL;
    krb5_context context = NULL;
    char *princstr = NULL, *passwordstr = NULL;
    krb5_principal princ = NULL;
    CFDataRef element = NULL;
    krb5_error_code ret;
    int32_t kvno;
    
    if (error)
	*error = NULL;

    ret = krb5_init_context(&context);
    if (ret) {
	createError(NULL, error, 1, CFSTR("can't create kerberos context"));
	return NULL;
    }
    
    /* check if we are deleting the password */
   
    if (flags & kHeimODAdminDeleteEnctypes) {
	if (prevKeyset == NULL)
	    goto out;
	
	keyset = CFArrayCreateMutableCopy(NULL, 0, prevKeyset);
	if (keyset == NULL)
	    goto out;
	
	delete_enctypes(context, enctypes, keyset);

    } else {

	princstr = cfstring2cstring(principal);
	if (princstr == NULL) {
	    createError(NULL, error, ENOMEM, CFSTR("out of memory"));
	    goto out;
	}
	
	ret = krb5_parse_name(context, princstr, &princ);
	if (ret) {
	    createError(NULL, error, ret, CFSTR("failed to parse principal"));
	    goto out;
	}
	
	passwordstr = cfstring2cstring(password);
	if (passwordstr == NULL)
	    goto out;
	
	if (prevKeyset) {
	    kvno = last_kvno_array(prevKeyset, context, NULL);
	    kvno++;
	} else {
	    kvno = 1;
	}
    
	element = update_keys(context, enctypes, kvno, princ, 0, passwordstr, error);
	if (element == NULL)
	    goto out;
    
	if (prevKeyset && (flags & kHeimODAdminSetKeysAppendKey) != 0)
	    keyset = CFArrayCreateMutableCopy(NULL, 0, prevKeyset);
	else
	    keyset = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
	if (keyset == NULL)
	    goto out;
    
	CFArrayAppendValue(keyset, element);
    }
    
out:
    if (princstr)
	free(princstr);
    if (passwordstr) {
	memset(passwordstr, 0, strlen(passwordstr));
	free(passwordstr);
    }
    if (element)
	CFRelease(element);
    if (princ)
	krb5_free_principal(context, princ);
    if (context)
	krb5_free_context(context);
    
    return keyset;
}

CFStringRef
HeimODKeysetToString(CFDataRef element, CFErrorRef *error)
{
    krb5_context context;
    krb5_error_code ret;
    CFStringRef str, str2;
    hdb_keyset key;
    size_t sz;
    
    ret = krb5_init_context(&context);
    if (ret) {
	createError(NULL, error, ret, CFSTR("failed to create context"));
	return NULL;
    }
    
    ret = decode_hdb_keyset(CFDataGetBytePtr(element),
			    CFDataGetLength(element),
			    &key, &sz);
    if (ret)
	return NULL;
    
    str = CFStringCreateWithFormat(NULL, NULL, CFSTR("key kvno %d"), key.kvno);
    if (str == NULL)
	goto out;
    
    if (key.principal) {
	char *p = NULL;
	(void)krb5_unparse_name(context, key.principal, &p);
	str2 = CFStringCreateWithFormat(NULL, NULL, CFSTR("%@ principal: %s"), str, p);
	free(p);
	CFRelease(str);
	str = str2;
	if (str == NULL) {
	    createError(NULL, error, ENOMEM, CFSTR("out of memory"));
	    goto out;
	}
    }

out:
    if (context)
	krb5_free_context(context);
    
    free_hdb_keyset(&key);
    
    return str;
}

/*
 *
 */

CFArrayRef
HeimODCopyDefaultEnctypes(CFErrorRef *error)
{
    CFMutableArrayRef enctypes;

    enctypes = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
    if (enctypes == NULL) {
	createError(NULL, error, ENOMEM, CFSTR("Can't create enctype array ref"));
	return NULL;
    }

    CFArrayAppendValue(enctypes, CFSTR("aes256-cts-hmac-sha1-96"));
    CFArrayAppendValue(enctypes, CFSTR("aes128-cts-hmac-sha1-96"));
    CFArrayAppendValue(enctypes, CFSTR("des3-cbc-sha1"));

    return enctypes;
}


/*
 *
 */

int
HeimODAddSubjectAltCertName(ODNodeRef node, ODRecordRef record, CFStringRef subject, CFStringRef issuer, CFErrorRef *error)
{
    abort();
    *error = NULL;
    return 0;
}


#ifdef PKINIT
static SecCertificateRef
find_ta(SecCertificateRef incert)
{
    SecTrustResultType resultType = kSecTrustResultDeny;
    SecPolicyRef policy = NULL;
    SecTrustRef trust = NULL;
    SecCertificateRef cert = NULL;
    CFMutableArrayRef inArray = NULL;
    OSStatus ret;

    policy = SecPolicyCreateBasicX509();

    inArray = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);

    CFArrayAppendValue(inArray, incert);

    ret = SecTrustCreateWithCertificates(inArray, policy, &trust);
    if (ret)
	goto out;
	
    ret = SecTrustEvaluate(trust, &resultType);
    if (ret)
	goto out;

    if (resultType != kSecTrustResultUnspecified && resultType != kSecTrustResultProceed)
	goto out;

    CFIndex certCount = SecTrustGetCertificateCount(trust);
    if (certCount <= 0)
	goto out;

    cert = SecTrustGetCertificateAtIndex(trust, certCount - 1);

    CFRetain(cert);

 out:
    if (policy)
	CFRelease(policy);
    if (trust)
	CFRelease(trust);
    if (inArray)
	CFRelease(inArray);

    return cert;
}
#endif

/*
 *
 */

int
HeimODAddCertificate(ODNodeRef node, ODRecordRef record, SecCertificateRef ref, CFErrorRef *error)
{
#ifdef PKINIT
    CFStringRef leaf = NULL, anchor = NULL;
    SecCertificateRef ta = NULL;
    hx509_name subject = NULL, taname = NULL;
    char *subjectstr = NULL, *tastr = NULL;
    hx509_context context = NULL;
    CFDataRef data = NULL;
    hx509_cert cert = NULL;
    CFStringRef ace = NULL;
    int ret;

    ta = find_ta(ref);
    if (ta == NULL) {
	ret = EINVAL;
	createError(NULL, error, ret, CFSTR("Failed to find trust anchor"));
	goto out;
    }

    ret = hx509_context_init(&context);
    if (ret) {
	ret = EINVAL;
	createError(NULL, error, ret, CFSTR("Failed to create hx509 context"));
	goto out;
    }

    data = SecCertificateCopyData(ref);
    if (data == NULL) {
	ret = ENOMEM;
	createError(NULL, error, ret, CFSTR("out of memory"));
	goto out;
    }

    ret = hx509_cert_init_data(context, CFDataGetBytePtr(data), CFDataGetLength(data), &cert); 
    CFRelease(data);
    if (ret) {
	createError(NULL, error, ret, CFSTR("failed to decode certificate"));
	goto out;
    }

    ret = hx509_cert_get_subject(cert, &subject);
    hx509_cert_free(cert);
    if (ret) {
	createError(NULL, error, ret, CFSTR("out of memory"));
	goto out;
    }

    data = SecCertificateCopyData(ta);
    if (data == NULL) {
	ret = ENOMEM;
	createError(NULL, error, ret, CFSTR("out of memory"));
	goto out;
    }

    ret = hx509_cert_init_data(context, CFDataGetBytePtr(data), CFDataGetLength(data), &cert); 
    CFRelease(data);
    if (ret) {
	createError(NULL, error, ret, CFSTR("failed to decode certificate"));
	goto out;
    }

    ret = hx509_cert_get_subject(cert, &taname);
    hx509_cert_free(cert);
    if (ret) {
	createError(NULL, error, ret, CFSTR("out of memory"));
	goto out;
    }

    ret = hx509_name_to_string(subject, &subjectstr);
    if (ret) {
	createError(NULL, error, ret, CFSTR("out of memory"));
	goto out;
    }
    ret = hx509_name_to_string(taname, &tastr);
    if (ret) {
	createError(NULL, error, ret, CFSTR("out of memory"));
	goto out;
    }

    leaf = CFStringCreateWithCString(kCFAllocatorDefault, subjectstr, kCFStringEncodingUTF8);
    if (leaf == NULL) {
	ret = ENOMEM;
	createError(NULL, error, ret, CFSTR("out of memory"));
	goto out;
    }

    anchor = CFStringCreateWithCString(kCFAllocatorDefault, tastr, kCFStringEncodingUTF8);
    if (anchor == NULL) {
	ret = ENOMEM;
	createError(NULL, error, ret, CFSTR("out of memory"));
	goto out;
    }

    if (!HeimODAddCertificateSubjectAndTrustAnchor(node, record, leaf, anchor, error))
	ret = EINVAL;

 out:
    if (leaf)
	CFRelease(leaf);
    if (anchor)
	CFRelease(anchor);
    if (taname)
	hx509_name_free(&taname);
    if (subject)
	hx509_name_free(&subject);
    if (tastr)
	free(tastr);
    if (subjectstr)
	free(subjectstr);
    if (ace)
	CFRelease(ace);
    if (ta)
	CFRelease(ta);
    if (context)
	hx509_context_free(&context);
    return ret;
#else
    return EINVAL;
#endif
}

/*
 *
 */

int
HeimODAddSubjectAltCertSHA1Digest(ODNodeRef node, ODRecordRef record, CFDataRef hash, CFErrorRef *error)
{
    abort();
    *error = NULL;
    return 0;
}

/*
 *
 */

CFArrayRef
HeimODCopySubjectAltNames(ODNodeRef node, ODRecordRef record, CFErrorRef *error)
{
    abort();
    *error = NULL;
    return 0;
}

/*
 *
 */

int
HeimODRemoveSubjectAltElement(ODNodeRef node, ODRecordRef record, CFTypeRef element, CFErrorRef *error)
{
    abort();
    *error = NULL;
    return 0;
}

int
HeimODAddCertificateSubjectAndTrustAnchor(ODNodeRef node, ODRecordRef record, CFStringRef leafSubject, CFStringRef trustAnchorSubject, CFErrorRef *error)
{
    CFArrayRef array;
    CFStringRef ace;

    if (error)
	*error = NULL;

    ace = CFStringCreateWithFormat(NULL, 0, CFSTR("X509:<T>%@<S>%@"), trustAnchorSubject, leafSubject);
    if (ace == NULL) {
	createError(NULL, error, ENOMEM, CFSTR("out of memory"));
	return 1;
    }	

    /* filter out dups */
    array = ODRecordCopyValues(record, kHeimASI, NULL);
    if (array) {
	__block int found_match = 0;
	arrayapplyblock(array, CFRangeMake(0, CFArrayGetCount(array)), ^(const void *value) {
	    if (CFGetTypeID(value) != CFStringGetTypeID())
		return;

	    if (CFStringCompare(value, ace, 0) == kCFCompareEqualTo)
		found_match = 1;
	    });
	CFRelease(array);
	if (found_match) {
	    CFRelease(ace);
	    return 0;
	}
    }

    bool r = ODRecordAddValue(record, kHeimASI, ace, error); 
    CFRelease(ace);
    if (!r)
	return 1;

    if (!ODRecordSynchronize(record, error))
	return 1;

    return 0;
}

int
HeimODRemoveCertificateSubjectAndTrustAnchor(ODNodeRef node, ODRecordRef record, CFStringRef leafSubject, CFStringRef trustAnchorSubject, CFErrorRef *error)
{
    CFStringRef ace;
    CFErrorRef e = NULL;

    if (error)
	*error = NULL;

    ace = CFStringCreateWithFormat(NULL, 0, CFSTR("X509:<T>%@<S>%@"), trustAnchorSubject, leafSubject);
    if (ace == NULL) {
	createError(NULL, error, ENOMEM, CFSTR("out of memory"));
	return 1;
    }	

    bool r = ODRecordRemoveValue(record, kHeimASI, ace, &e);
    CFRelease(ace);
    if (!r && e == NULL)
	return 0; /* entry didn't exists */
    else if (!r) {
	if (error)
	    *error = e;
	else
	    CFRelease(e);
	return 1;
    }

    if (!ODRecordSynchronize(record, error))
	return 1;

    return 0;
}

int
HeimODAddAppleIDAlias(ODNodeRef node, ODRecordRef record, CFStringRef alias, CFErrorRef *error)
{
    CFArrayRef array;

    if (error)
	*error = NULL;

    if (is_record_server(record)) {
	createError(NULL, error, EINVAL, CFSTR("AppleID alias not supported for Server Mode"));
	return 1;
    }	

    array = ODRecordCopyValues(record, kODAttributeTypeRecordName, NULL);
    if (array) {
	__block int found_match = 0;
	arrayapplyblock(array, CFRangeMake(0, CFArrayGetCount(array)), ^(const void *value) {
	    if (CFGetTypeID(value) != CFStringGetTypeID())
		return;

	    if (CFStringCompare(value, alias, 0) == kCFCompareEqualTo)
		found_match = 1;
	    });
	CFRelease(array);
	if (found_match)
	    return 0;
    }

    bool r = ODRecordAddValue(record, kODAttributeTypeRecordName, alias, error); 
    if (!r)
	return 1;

    if (!ODRecordSynchronize(record, error))
	return 1;

    return 0;
}

int
HeimODRemoveAppleIDAlias(ODNodeRef node, ODRecordRef record, CFStringRef alias, CFErrorRef *error)
{
    CFErrorRef e = NULL;

    if (error)
	*error = NULL;

    if (is_record_server(record)) {
	createError(NULL, error, EINVAL, CFSTR("AppleID alias not supported for Server Mode"));
	return 1;
    }	

    bool r = ODRecordRemoveValue(record, kODAttributeTypeRecordName, alias, &e);
    if (!r && e == NULL)
	return 0; /* entry didn't exists */
    else if (!r) {
	if (error)
	    *error = e;
	else
	    CFRelease(e);
	return 1;
    }

    if (!ODRecordSynchronize(record, error))
	return 1;

    return 0;
}

struct dumploadkeys {
    CFStringRef dumpkey;
    CFStringRef clientkey;
    CFStringRef serverkey;
    unsigned long flags;
#define LOAD_SERVER		1
#define LOAD_CLIENT		2
#define LOAD_SERVER_APPEND	4
#define DUMP			8
    bool (*load)(krb5_context, ODRecordRef, CFDictionaryRef, CFStringRef, CFTypeRef, unsigned long flags, CFErrorRef *);
    CFArrayRef (*dump_record)(ODRecordRef, CFArrayRef, CFErrorRef *);
    CFArrayRef (*dump_entry)(krb5_context, hdb_entry *, CFErrorRef *);
};

static bool
load_simple(krb5_context context, ODRecordRef record, CFDictionaryRef dict, CFStringRef key, CFTypeRef data, unsigned long flags, CFErrorRef *error)
{
    bool ret = false;

    if (flags & LOAD_SERVER_APPEND) {
	CFIndex n, num = CFArrayGetCount(data);
	for (n = 0; n < num; n++) {
	    ret = ODRecordAddValue(record, key, CFArrayGetValueAtIndex(data, n), error);
	    if (!ret)
		return false;
	}
    } else {
	ret = ODRecordSetValue(record, key, data, error);
    }
    if (!ret && *error == NULL)
	createError(NULL, error, 1, CFSTR("Failed to load the key %@ with value %@"), key, data);
    return ret;
}


static CFArrayRef
dump_simple(ODRecordRef record, CFArrayRef val, CFErrorRef *error)
{
    return CFRetain(val);
}

static CFArrayRef
edump_principal(krb5_context context, hdb_entry *entry, CFErrorRef *error)
{
    krb5_error_code ret;
    CFArrayRef array;
    CFStringRef value;
    char *name;

    ret = krb5_unparse_name(context, entry->principal, &name);
    if (ret)
	return NULL;
    
    value = CFStringCreateWithCString(kCFAllocatorDefault, name, kCFStringEncodingUTF8);
    krb5_xfree(name);
    if (value == NULL)
	return NULL;
    array = CFArrayCreate(kCFAllocatorDefault, (const void **)&value, 1, &kCFTypeArrayCallBacks);
    CFRelease(value);

    return array;
}

static CFArrayRef
edump_flags(krb5_context context, hdb_entry *entry, CFErrorRef *error)
{
    CFArrayRef array = NULL;
    CFStringRef value;

    value = CFStringCreateWithFormat(kCFAllocatorDefault, NULL, CFSTR("%d"), HDBFlags2int(entry->flags));
    if (value == NULL)
	return NULL;

    array = CFArrayCreate(kCFAllocatorDefault, (const void **)&value, 1, &kCFTypeArrayCallBacks);
    CFRelease(value);

    return array;
}

static bool
load_keys(krb5_context context, ODRecordRef record, CFDictionaryRef dict, CFStringRef key, CFTypeRef data, unsigned long flags, CFErrorRef *error)
{
    CFMutableArrayRef newdata = NULL;
    krb5_principal p = NULL;
    int ret;
    bool b;
    
    /* need to make sure that keys are propperly mapped */
    if (flags & LOAD_SERVER_APPEND) {
	CFIndex n, num;
	hdb_keyset keyset;
	
	memset(&keyset, 0, sizeof(keyset));
	
	newdata = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
	if (newdata == NULL)
	    return false;
	
	CFArrayRef aprinc = CFDictionaryGetValue(dict, CFSTR("KerberosPrincipal"));
	if (aprinc == NULL || CFGetTypeID(aprinc) != CFArrayGetTypeID()) {
	    createError(NULL, error, 1, CFSTR("Failed to find principal in load dictionary"));
	    b = false;
	    goto out;
	}
	CFStringRef princ = CFArrayGetValueAtIndex(aprinc, 0);
	if (princ == NULL || CFGetTypeID(princ) != CFStringGetTypeID()) {
	    createError(NULL, error, 1, CFSTR("Failed to find principal in load dictionary"));
	    b = false;
	    goto out;
	}
	char *str = cfstring2cstring(princ);
	if (str == NULL) {
	    createError(NULL, error, ENOMEM, CFSTR("out of memory"));
	    b = false;
	    goto out;
	}

	ret = krb5_parse_name(context, str, &p);
	free(str);
	if (ret) {
	    createError(NULL, error, ret, CFSTR("failed to parse name"));
	    b = false;
	    goto out;
	}
	

	if (CFGetTypeID(data) != CFArrayGetTypeID())
	    abort();
	
	num = CFArrayGetCount(data);
	
	for (n = 0; n < num; n++) {
	    CFDataRef d = CFArrayGetValueAtIndex(data, n);
	    
	    ret = decode_hdb_keyset(CFDataGetBytePtr(d), CFDataGetLength(d), &keyset, NULL);
	    if (ret) {
		createError(NULL, error, 1, CFSTR("failed to decode hdb_keyset"));
		b = false;
		goto out;
	    }
	    
	    if (keyset.principal == NULL) {
		size_t len, size;
		void *to;
		
		(void)krb5_copy_principal(context, p, &keyset.principal);
		
		ASN1_MALLOC_ENCODE(hdb_keyset, to, size, &keyset, &len, ret);
		if (ret) {
		    free_hdb_keyset(&keyset);
		    createError(NULL, error, ret, CFSTR("failed to parse name"));
		    b = false;
		    goto out;
		}
		if (len != size)
		    abort();
		
		CFDataRef key = CFDataCreate(NULL, to, len);
		free(to);
		free_hdb_keyset(&keyset);
		if (key == NULL) {
		    createError(NULL, error, ENOMEM, CFSTR("out of memory"));
		    b = false;
		    goto out;
		}
		CFArrayAppendValue(newdata, key);
		CFRelease(key);
	    } else {
		free_hdb_keyset(&keyset);
		CFArrayAppendValue(newdata, d);
	    }
	}
	data = newdata;
    }
    
    b = load_simple(context, record, dict, key, data, flags, error);
out:
    if (p)
	krb5_free_principal(context, p);
    if (newdata)
	CFRelease(newdata);
    return b;
}


static CFArrayRef
edump_keys(krb5_context context, hdb_entry *entry, CFErrorRef *error)
{
    CFArrayRef array = NULL;
    krb5_error_code ret;
    krb5_data data;
    CFDataRef value;
    hdb_keyset key;
    size_t size;

    key.kvno = entry->kvno;
    key.keys.len = entry->keys.len;
    key.keys.val = entry->keys.val;
    key.principal = NULL;

    ASN1_MALLOC_ENCODE(hdb_keyset, data.data, data.length, &key, &size, ret);
    if (ret)
	return NULL;
    if (data.length != size)
	krb5_abortx(context, "internal asn.1 encoder error");

    value = CFDataCreate(kCFAllocatorDefault, data.data, data.length);
    free(data.data);
    if (value == NULL)
	return NULL;

    array = CFArrayCreate(kCFAllocatorDefault, (const void **)&value, 1, &kCFTypeArrayCallBacks);
    CFRelease(value);

    return array;
}


struct dumploadkeys dlkeys[] = {
    { 
	CFSTR("KerberosPrincipal"),
	kHeimODKerberosUserName,
	kHeimLDAPKerberosUserName,
	LOAD_SERVER | DUMP,
	load_simple,
	dump_simple,
	edump_principal
    },
    { 
	CFSTR("KerberosPrincipal"),
	kHeimODKerberosUserName,
	kHeimLDAPKerberosServerName,
	LOAD_SERVER | LOAD_SERVER_APPEND,
	load_simple,
	dump_simple,
	edump_principal
    },
    { 
	CFSTR("KerberosServerPrincipal"),
	kHeimODKerberosServerName,
	kHeimLDAPKerberosServerName,
	LOAD_SERVER | LOAD_CLIENT | DUMP,
	load_simple,
	dump_simple,
	edump_principal
    },
    { 
	CFSTR("KerberosFlags"),
	kHeimODKerberosFlags,
	kHeimLDAPKerberosFlags,
	LOAD_CLIENT | LOAD_SERVER | DUMP,
	load_simple,
	dump_simple,
	edump_flags
    },
    { 
	CFSTR("KerberosKeys"),
	kHeimODKerberosKeys,
	kHeimLDAPKerberosKeys,
	LOAD_CLIENT | LOAD_SERVER | LOAD_SERVER_APPEND | DUMP,
	load_keys,
	dump_simple,
	edump_keys
    }
};


CFDictionaryRef
HeimODDumpRecord(ODNodeRef node, ODRecordRef record, CFStringRef principal, CFErrorRef *error)
{
    ODRecordRef datarecord;
    CFMutableDictionaryRef dict;
    bool is_server;
    size_t n;

    if (error == NULL)
	abort();

    dict = CFDictionaryCreateMutable(NULL, 0,
				     &kCFTypeDictionaryKeyCallBacks,
				     &kCFTypeDictionaryValueCallBacks);
    if (dict == NULL) {
	createError(NULL, error, ENOMEM, CFSTR("out of memory"));
	return NULL;
    }

    CFDictionaryAddValue(dict, CFSTR("version"), CFSTR("1"));

    datarecord = copyDataRecord(node, record, NULL, NULL, NULL, &is_server, error);
    if (datarecord == NULL) {
	CFRelease(dict); dict = NULL;
	goto out;
    }

    for (n = 0; n < sizeof(dlkeys) / sizeof(dlkeys[0]); n++) {
	CFStringRef key = is_server ? dlkeys[n].serverkey : dlkeys[n].clientkey;
	CFArrayRef values;

	if ((dlkeys[n].flags & DUMP) == 0)
	    continue;

	values = ODRecordCopyValues(datarecord, key, NULL);
	if (values == NULL)
	    continue;

	CFArrayRef v = dlkeys[n].dump_record(datarecord, values, error);
	if (v) {
	    CFDictionaryAddValue(dict, dlkeys[n].dumpkey, v);
	    CFRelease(v);
	}

	CFRelease(values);
	if (*error) {
	    CFRelease(dict); dict = NULL;
	    break;
	}
    }

 out:
    if (datarecord)
	CFRelease(datarecord);

    return dict;
}

bool
HeimODLoadRecord(ODNodeRef node, ODRecordRef record, CFDictionaryRef dict, 
		 unsigned long flags, CFErrorRef *error)
{
    unsigned long loadflags = 0;
    ODRecordRef datarecord;
    bool is_server;
    bool ret = false;
    size_t n;
    krb5_error_code status;
    krb5_context context;

    if (error == NULL)
	abort();

    status = krb5_init_context(&context);
    if (status) {
	createError(NULL, error, status, CFSTR("can't create kerberos context"));
	return false;
    }

    datarecord = copyDataRecord(node, record, NULL, NULL, NULL, &is_server, error);
    if (datarecord == NULL)
	goto out;

    loadflags |= is_server ? LOAD_SERVER : LOAD_CLIENT;
    if (flags & kHeimODAdminLoadAsAppend)
	loadflags |= LOAD_SERVER_APPEND;

    for (n = 0; n < sizeof(dlkeys) / sizeof(dlkeys[0]); n++) {
	CFStringRef key = is_server ? dlkeys[n].serverkey : dlkeys[n].clientkey;
	CFTypeRef values;

	if ((dlkeys[n].flags & loadflags) != loadflags)
	    continue;

	values = CFDictionaryGetValue(dict, dlkeys[n].dumpkey);
	if (values == NULL)
	    continue;

	ret = dlkeys[n].load(context, datarecord, dict, key, values, loadflags, error);
	if (!ret)
	    goto out;
    }

    ret = ODRecordSynchronize(record, error);
    if (!ret) {
	if (*error == NULL)
	    createError(NULL, error, 1, CFSTR("Failed to syncronize the record"));
	goto out;
    }

    ret = true;

 out:
    if (context)
	krb5_free_context(context);
    if (datarecord)
	CFRelease(datarecord);

    return ret;
}

CFDictionaryRef
HeimODDumpHdbEntry(struct hdb_entry *entry, CFErrorRef *error)
{
    CFMutableDictionaryRef dict = NULL;
    krb5_context context;
    krb5_error_code ret;
    size_t n;

    if (error == NULL)
	abort();

    ret = krb5_init_context(&context);
    if (ret) {
	createError(NULL, error, 1, CFSTR("can't create kerberos context"));
	goto out;
    }

    dict = CFDictionaryCreateMutable(NULL, 0,
				     &kCFTypeDictionaryKeyCallBacks,
				     &kCFTypeDictionaryValueCallBacks);
    if (dict == NULL) {
	createError(NULL, error, ENOMEM, CFSTR("out of memory"));
	goto out;
    }

    CFDictionaryAddValue(dict, CFSTR("version"), CFSTR("1"));

    for (n = 0; n < sizeof(dlkeys) / sizeof(dlkeys[0]); n++) {

	if ((dlkeys[n].flags & DUMP) == 0)
	    continue;

	CFArrayRef v = dlkeys[n].dump_entry(context, entry, error);
	if (v) {
	    CFDictionaryAddValue(dict, dlkeys[n].dumpkey, v);
	    CFRelease(v);
	}

	if (*error) {
	    CFRelease(dict); dict = NULL;
	    break;
	}
    }

 out:
    krb5_free_context(context);

    return dict;
}
