/*
 * Copyright (c) 2011 Kungliga Tekniska Högskolan
 * (Royal Institute of Technology, Stockholm, Sweden).
 * All rights reserved.
 *
 * Portions Copyright (c) 2011 Apple Inc. All rights reserved.
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
 * 3. Neither the name of KTH nor the names of its contributors may be
 *    used to endorse or promote products derived from this software without
 *    specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY KTH AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL KTH OR ITS CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */


#define HEIMDAL_PRINTF_ATTRIBUTE(x)

#include <GSSItem.h>
#include <gssapi.h>
#include <gssapi_spi.h>
#include <gssapi_mech.h>
#include <heimbase.h>
#include <krb5.h>
#include <roken.h>
#include <kcm.h>



#include <notify.h>

#include <Security/Security.h>

#define GSS_CONST_TYPE(t,k) const t k = (t)(CFSTR(#k));

GSS_CONST_TYPE(CFTypeRef,   kGSSAttrClass);
GSS_CONST_TYPE(CFStringRef, kGSSAttrClassKerberos);
GSS_CONST_TYPE(CFStringRef, kGSSAttrClassNTLM);
GSS_CONST_TYPE(CFStringRef, kGSSAttrClassIAKerb);

GSS_CONST_TYPE(CFTypeRef,   kGSSAttrSupportGSSCredential);
GSS_CONST_TYPE(CFTypeRef,   kGSSAttrNameType);
GSS_CONST_TYPE(CFTypeRef,   kGSSAttrNameTypeGSSExportedName);
GSS_CONST_TYPE(CFTypeRef,   kGSSAttrNameTypeGSSUsername);
GSS_CONST_TYPE(CFTypeRef,   kGSSAttrNameTypeGSSHostBasedService);
GSS_CONST_TYPE(CFTypeRef,   kGSSAttrName);
GSS_CONST_TYPE(CFTypeRef,   kGSSAttrNameDisplay);
GSS_CONST_TYPE(CFTypeRef,   kGSSAttrUUID);
GSS_CONST_TYPE(CFTypeRef,   kGSSAttrTransientExpire);
GSS_CONST_TYPE(CFTypeRef,   kGSSAttrTransientDefaultInClass);
GSS_CONST_TYPE(CFTypeRef,   kGSSAttrCredentialPassword);
GSS_CONST_TYPE(CFTypeRef,   kGSSAttrCredentialStore);
GSS_CONST_TYPE(CFTypeRef,   kGSSAttrCredentialSecIdentity);
GSS_CONST_TYPE(CFTypeRef,   kGSSAttrCredentialExists);
GSS_CONST_TYPE(CFTypeRef,   kGSSAttrStatusPersistant);
GSS_CONST_TYPE(CFTypeRef,   kGSSAttrStatusAutoAcquire);
GSS_CONST_TYPE(CFTypeRef,   kGSSAttrStatusTransient);

GSS_CONST_TYPE(CFTypeRef,   kGSSAttrStatusAutoAcquireStatus);


GSS_CONST_TYPE(CFTypeRef,   kGSSOperationChangePasswordOldPassword);
GSS_CONST_TYPE(CFTypeRef,   kGSSOperationChangePasswordNewPassword);

#undef GSS_CONST_TYPE

static CFNumberRef kGSSSecPasswordType = NULL;

static gss_cred_id_t itemToGSSCred(GSSItemRef, OM_uint32 *, CFErrorRef *);
static void updateTransientValues(GSSItemRef);


static CFStringRef kGSSConfKeys = CFSTR("kGSSConfKeys");


struct GSSItem {
    CFRuntimeBase base;
    CFMutableDictionaryRef keys;
    CFUUIDRef gssCredential;
};

static void
_gssitem_release(struct GSSItem *item)
{
    if (item->keys) {
	CFRelease(item->keys);
	item->keys = NULL;
    }
    if (item->gssCredential) {
	CFRelease(item->gssCredential);
	item->gssCredential = NULL;
    }
}

static CFDictionaryRef valid_set_types;
static CFDictionaryRef transient_types;
static int notify_token;
static dispatch_queue_t bgq;
static CFTypeID gssitemid = _kCFRuntimeNotATypeID;


static void
create_tables(void * ctx __attribute__((__unused__)))
{
    CFMutableDictionaryRef top, cl;

    bgq = dispatch_queue_create("org.h5l.gss.item", DISPATCH_QUEUE_CONCURRENT);
    heim_assert(bgq != NULL, "no breakground queue");

    top = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    heim_assert(top != NULL, "out of memory");
    cl = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    heim_assert(top != NULL, "out of memory");

    CFDictionarySetValue(cl, kGSSAttrClass, kCFBooleanTrue);
    CFDictionarySetValue(cl, kGSSAttrNameType, kCFBooleanTrue);
    CFDictionarySetValue(cl, kGSSAttrName, kCFBooleanTrue);
    CFDictionarySetValue(cl, kGSSAttrUUID, kCFBooleanTrue);
    CFDictionarySetValue(cl, kGSSAttrCredentialPassword, kCFBooleanFalse);
    CFDictionarySetValue(cl, kGSSAttrCredentialSecIdentity, kCFBooleanFalse);
    CFDictionarySetValue(cl, kGSSAttrStatusPersistant, kCFBooleanFalse);
    CFDictionarySetValue(cl, kGSSAttrStatusAutoAcquire, kCFBooleanFalse);
    CFDictionarySetValue(cl, kGSSAttrStatusTransient, kCFBooleanFalse);

    CFDictionarySetValue(top, kGSSAttrClassKerberos, cl);
    CFRelease(cl);

    cl = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);

    CFDictionarySetValue(cl, kGSSAttrClass, kCFBooleanTrue);
    CFDictionarySetValue(cl, kGSSAttrNameType, kCFBooleanTrue);
    CFDictionarySetValue(cl, kGSSAttrName, kCFBooleanTrue);
    CFDictionarySetValue(cl, kGSSAttrUUID, kCFBooleanTrue);
    CFDictionarySetValue(cl, kGSSAttrCredentialPassword, kCFBooleanFalse);
    CFDictionarySetValue(cl, kGSSAttrStatusPersistant, kCFBooleanFalse);
    CFDictionarySetValue(cl, kGSSAttrStatusAutoAcquire, kCFBooleanFalse);
    CFDictionarySetValue(cl, kGSSAttrStatusTransient, kCFBooleanFalse);

    CFDictionarySetValue(top, kGSSAttrClassNTLM, cl);
    CFRelease(cl);

    valid_set_types = top;

    cl = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);

    CFDictionarySetValue(cl, kGSSAttrTransientExpire, kCFBooleanTrue);
    transient_types = cl;

    const int32_t gssnum = 'GSSP';
    kGSSSecPasswordType = CFNumberCreate(NULL, kCFNumberSInt32Type, &gssnum);

    (void)notify_register_check(KRB5_KCM_NOTIFY_CACHE_CHANGED, &notify_token);

    static const CFRuntimeClass gssItemClass = {
		0,
		"GSSItem",
		NULL,
		NULL,
		(void(*)(CFTypeRef))_gssitem_release,
		NULL,
		NULL,
		NULL,
		NULL
	    };
    gssitemid = _CFRuntimeRegisterClass(&gssItemClass);
}

static void
gss_init(void)
{
    static dispatch_once_t once;
    dispatch_once_f(&once, NULL, create_tables);
}

#pragma mark Item

CFTypeID
GSSItemGetTypeID(void)
{
    gss_init();
    return gssitemid;
}

static GSSItemRef
GSSCreateItem(CFAllocatorRef alloc, CFDictionaryRef keys)
{
    GSSItemRef item;

    item = (GSSItemRef)_CFRuntimeCreateInstance(alloc, gssitemid, sizeof(struct GSSItem) - sizeof(CFRuntimeBase), NULL);
    if (item == NULL)
	return NULL;

    if (keys)
	item->keys = CFDictionaryCreateMutableCopy(alloc, 0, keys);
    else
	item->keys = CFDictionaryCreateMutable(alloc, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    item->gssCredential = NULL;

    return item;
}

#pragma mark Configuration


static CFStringRef
CreateNewUUID(GSSItemRef item)
{
    CFUUIDRef uuid = CFUUIDCreate(NULL);
    if (uuid == NULL)
	return NULL;
    
    /* uuid is not a property list type, make it a string */
    CFStringRef uuidstr = CFUUIDCreateString(NULL, uuid);
    CFRelease(uuid);
    if (uuidstr == NULL)
	return NULL;

    CFDictionarySetValue(item->keys, kGSSAttrUUID, uuidstr);
    return uuidstr;
}

static CFURLRef
copyConfigurationURL(void)
{
    CFURLRef file, home = CFCopyHomeDirectoryURLForUser(NULL);
    if (home == NULL)
	return NULL;

    file = CFURLCreateCopyAppendingPathComponent(NULL, home, CFSTR("Library/Preferences/com.apple.GSS.items.plist"), false);
    CFRelease(home);

    return file;
}

static CFStringRef
CopyTransientUUID(gss_cred_id_t cred)
{
    CFUUIDRef uuid = GSSCredentialCopyUUID(cred);
    CFStringRef uuidstr = NULL;

    if (uuid) {
	uuidstr = CFUUIDCreateString(NULL, uuid);
	CFRelease(uuid);
    }

    return uuidstr;
}

struct CreateContext {
    CFMutableDictionaryRef c;
    CFMutableDictionaryRef transitentUUIDs;
};

static void
createItem(const void *key, const void *value, void *contextValue)
{
    struct CreateContext *context = contextValue;

    GSSItemRef item = GSSCreateItem(NULL, value);
    if (item) {
	gss_cred_id_t cred = itemToGSSCred(item, NULL, NULL);

	if (cred) {
	    CFStringRef uuid = CopyTransientUUID(cred);
	    if (uuid) {
		CFDictionarySetValue(context->transitentUUIDs, uuid, kCFBooleanTrue);
		CFRelease(uuid);
	    }
	}

	CFDictionarySetValue(context->c, key, item);
	CFRelease(item);
    }
}

static void
addTransientKeys(struct CreateContext *createContext)
{
    OM_uint32 min_stat;

    gss_iter_creds (&min_stat, 0, GSS_KRB5_MECHANISM, ^(gss_iter_OID oid, gss_cred_id_t cred) {
	    OM_uint32 maj_stat, junk;
	    gss_name_t name;

	    CFStringRef uuid = CopyTransientUUID(cred);
	    if (uuid == NULL)
		return;
	    
	    if (CFDictionaryGetValue(createContext->transitentUUIDs, uuid)) {
		CFRelease(uuid);
		return;
	    }

	    GSSItemRef item = GSSCreateItem(NULL, NULL);
	    if (item == NULL) {
		CFRelease(uuid);
		return;
	    }

	    CFDictionarySetValue(item->keys, kGSSAttrUUID, uuid);

	    CFDictionarySetValue(item->keys, kGSSAttrClass, kGSSAttrClassKerberos);
	    CFDictionarySetValue(item->keys, kGSSAttrNameType, kGSSAttrNameTypeGSSExportedName);

	    name = _gss_cred_copy_name(&junk, cred, NULL);
	    if (name == NULL) {
		CFRelease(uuid);
		CFRelease(item);
		return;
	    }

	    gss_buffer_desc buffer = { 0, NULL };
	    maj_stat = gss_export_name(&junk, name, &buffer);
	    gss_release_name(&junk, &name);
	    if (maj_stat) {
		CFRelease(uuid);
		CFRelease(item);
		return;
	    }

	    CFDataRef data = CFDataCreate(NULL, buffer.value, buffer.length);
	    CFDictionarySetValue(item->keys, kGSSAttrName, data);
	    CFRelease(data);

	    updateTransientValues(item);
	    CFDictionarySetValue(item->keys, kGSSAttrStatusTransient, kCFBooleanTrue);

	    CFDictionarySetValue(createContext->c, uuid, item);

	    item->gssCredential = GSSCredentialCopyUUID(cred);

	    CFRelease(item);
	    CFRelease(uuid);
	});
}

static void
initCreateContext(struct CreateContext *createContext)
{
    heim_assert(createContext->c == NULL, "init more then once");

    createContext->c = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    heim_assert(createContext->c != NULL, "out of memory");

    createContext->transitentUUIDs = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    heim_assert(createContext->transitentUUIDs != NULL, "out of memory");
}

static CFMutableDictionaryRef
copyConfiguration(bool create, CFErrorRef *error)
{
    struct CreateContext createContext = { NULL, NULL };
    CFReadStreamRef s;
    CFDictionaryRef d = NULL, keys;
    CFURLRef url;

    url = copyConfigurationURL();
    if (url == NULL)
	return NULL;

    s = CFReadStreamCreateWithFile(kCFAllocatorDefault, url);
    CFRelease(url);
    if (s == NULL)
	goto out;

    if (!CFReadStreamOpen(s)) {
	CFRelease(s);
	goto out;
    }

    d = (CFDictionaryRef)CFPropertyListCreateWithStream(kCFAllocatorDefault, s, 0, kCFPropertyListImmutable, NULL, error);
    CFRelease(s);
    if (d == NULL)
	goto out;

    if (CFGetTypeID(d) != CFDictionaryGetTypeID())
	goto out;

    initCreateContext(&createContext);

    keys = CFDictionaryGetValue(d, kGSSConfKeys);
    if (keys == NULL) {
	CFRelease(createContext.c);
	createContext.c = NULL;
	goto out;
    }

    CFDictionaryApplyFunction(keys, createItem, &createContext);

 out:
    if (create && createContext.c == NULL)
	initCreateContext(&createContext);

    if (createContext.c)
	addTransientKeys(&createContext);

    if (createContext.transitentUUIDs)
	CFRelease(createContext.transitentUUIDs);
    if (d)
	CFRelease(d);

    return createContext.c;
}

static void
storeItem(const void *key, const void *value, void *context)
{
    GSSItemRef item = (GSSItemRef)value;

    heim_assert(CFGetTypeID(item) == GSSItemGetTypeID(), "trying to store a non GSSItem");

    /* if the credential is a transit credential, dont store in store */
    if (CFDictionaryGetValue(item->keys, kGSSAttrStatusTransient))
	return;
    CFDictionarySetValue(context, key, item->keys);
}

static void
storeConfiguration(CFDictionaryRef conf)
{
    CFMutableDictionaryRef c = NULL, keys = NULL;
    CFWriteStreamRef s;
    CFURLRef url;

    url = copyConfigurationURL();
    if (url == NULL)
	return;

    s = CFWriteStreamCreateWithFile(NULL, url);
    CFRelease(url);
    if (s == NULL)
	return;

    keys = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    if (keys == NULL) {
	CFRelease(s);
	return;
    }
    CFDictionaryApplyFunction(conf, storeItem, keys);

    c = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    if (c == NULL) {
	CFRelease(keys);
	CFRelease(s);
	return;
    }

    CFDictionarySetValue(c, kGSSConfKeys, keys);
    CFRelease(keys);

    if (CFWriteStreamOpen(s)) {
	CFPropertyListWrite(c, s, kCFPropertyListBinaryFormat_v1_0, 0, NULL);
	CFWriteStreamClose(s);
    }

    CFRelease(s);
    CFRelease(c);
}

#pragma mark ToGSS conversion

static gss_name_t
itemCopyGSSName(GSSItemRef item, CFErrorRef *error)
{
    gss_name_t gssname = GSS_C_NO_NAME;
    CFTypeRef name, type;
    CFTypeID nametype;
    gss_buffer_desc buffer;
    gss_const_OID nameoid;
    OM_uint32 junk;

    type = CFDictionaryGetValue(item->keys, kGSSAttrNameType);
    if (type == NULL)
	return GSS_C_NO_NAME;

    if (CFEqual(type, kGSSAttrNameTypeGSSUsername))
	nameoid = GSS_C_NT_USER_NAME;
    else if (CFEqual(type, kGSSAttrNameTypeGSSHostBasedService))
	nameoid = GSS_C_NT_HOSTBASED_SERVICE;
    else if (CFEqual(type, kGSSAttrNameTypeGSSExportedName))
	nameoid = GSS_C_NT_EXPORT_NAME;
    else
	return GSS_C_NO_NAME;

    name = CFDictionaryGetValue(item->keys, kGSSAttrName);
    if (name == NULL)
	return GSS_C_NO_NAME;

    nametype = CFGetTypeID(name);
    if (nametype == CFStringGetTypeID()) {
	buffer.value = rk_cfstring2cstring(name);
	if (buffer.value == NULL)
	    return GSS_C_NO_NAME;
	buffer.length = strlen((char *)buffer.value);
    } else if (nametype == CFDataGetTypeID()) {
	buffer.value = malloc(CFDataGetLength(name));
	if (buffer.value == NULL)
	    return GSS_C_NO_NAME;
	memcpy(buffer.value, CFDataGetBytePtr(name), CFDataGetLength(name));
	buffer.length = CFDataGetLength(name);
    } else
	return GSS_C_NO_NAME;
	
    (void)gss_import_name(&junk, &buffer, nameoid, &gssname);

    return gssname;
}

static gss_const_OID
itemToMechOID(GSSItemRef item, CFErrorRef *error)
{
    CFTypeRef type;

    type = CFDictionaryGetValue(item->keys, kGSSAttrClass);
    if (type == NULL)
	return GSS_C_NO_OID;

    if (CFEqual(type, kGSSAttrClassKerberos))
	return GSS_KRB5_MECHANISM;
    else if (CFEqual(type, kGSSAttrClassNTLM))
	return GSS_NTLM_MECHANISM;
    else if (CFEqual(type, kGSSAttrClassIAKerb))
	return GSS_IAKERB_MECHANISM;

    return GSS_C_NO_OID;
}

static gss_cred_id_t
itemToGSSCred(GSSItemRef item, OM_uint32 *lifetime, CFErrorRef *error)
{
    gss_OID_set mechs = GSS_C_NO_OID_SET;
    OM_uint32 maj_stat, min_stat;
    gss_cred_id_t gsscred;
    gss_const_OID mechoid;
    gss_name_t name;

    if (item->gssCredential) {
	gsscred = GSSCreateCredentialFromUUID(item->gssCredential);
	if (gsscred && lifetime)
	    (void)gss_inquire_cred(&min_stat, gsscred, NULL, lifetime, NULL, NULL);
	return gsscred;
    }

    mechoid = itemToMechOID(item, error);
    if (mechoid == NULL)
	return GSS_C_NO_CREDENTIAL;

    name = itemCopyGSSName(item, error);
    if (name == NULL)
	return GSS_C_NO_CREDENTIAL;

    maj_stat = gss_create_empty_oid_set(&min_stat, &mechs);
    if (maj_stat != GSS_S_COMPLETE) {
	if (error) *error = _gss_mg_create_cferror(maj_stat, min_stat, NULL);
	gss_release_name(&min_stat, &name);
	return GSS_C_NO_CREDENTIAL;
    }

    maj_stat = gss_add_oid_set_member(&min_stat, mechoid, &mechs);
    if (maj_stat != GSS_S_COMPLETE) {
	if (error) *error = _gss_mg_create_cferror(maj_stat, min_stat, NULL);
	gss_release_oid_set(&min_stat, &mechs);
	gss_release_name(&min_stat, &name);
	return GSS_C_NO_CREDENTIAL;
    }
    
    maj_stat = gss_acquire_cred(&min_stat, name, GSS_C_INDEFINITE, mechs,
				GSS_C_INITIATE, &gsscred, NULL, lifetime);
    gss_release_oid_set(&min_stat, &mechs);
    gss_release_name(&min_stat, &name);
    if (maj_stat) {
	if (error) *error = _gss_mg_create_cferror(maj_stat, min_stat, mechoid);
	return GSS_C_NO_CREDENTIAL;
    }

    item->gssCredential = GSSCredentialCopyUUID(gsscred);

    return gsscred;
}

#pragma mark Keychain

static CFTypeRef
extractCopyPassword(GSSItemRef item, bool getAttributes)
{
#ifndef __APPLE_TARGET_EMBEDDED__
    OM_uint32 maj_stat, junk;
    gss_buffer_desc buffer;
    CFDataRef password = NULL;
    char *name, *realm;
    gss_name_t gssname;
    CFStringRef val;
    OSStatus osret;
    UInt32 length;
    void *buf;
    void *ptr;
    
    /* First look for UUID version of the credential */
    
    val = CFDictionaryGetValue((CFDictionaryRef)item->keys, kGSSAttrUUID);
    if (val) {
	CFTypeRef result = NULL;

	name = rk_cfstring2cstring(val);
	if (name == NULL)
	    return NULL;

	CFDataRef dataoid = CFDataCreate(NULL, (void *)name, strlen(name));
	free(name);

	CFMutableDictionaryRef query = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
	if (query == NULL) {
	    CFRelease(dataoid);
	    return NULL;
	}

	CFDictionaryAddValue(query, kSecClass, kSecClassGenericPassword);
	CFDictionaryAddValue(query, kSecAttrType, kGSSSecPasswordType);
	CFDictionaryAddValue(query, kSecAttrGeneric, dataoid);
	CFDictionaryAddValue(query, kSecAttrService, CFSTR("GSS"));
	CFRelease(dataoid);
	if (getAttributes)
	    CFDictionaryAddValue(query, kSecReturnAttributes, kCFBooleanTrue);
	else
	    CFDictionaryAddValue(query, kSecReturnData, kCFBooleanTrue);

	osret = SecItemCopyMatching(query, &result);
	CFRelease(query);
	if (osret == noErr) {
	    CFTypeID expectedType = getAttributes ? CFDictionaryGetTypeID() : CFDataGetTypeID();
	    if (CFGetTypeID(result) != expectedType) {
		CFRelease(result);
		return NULL;
	    }
	    return result;
	}
    }

    if (getAttributes)
	return NULL;
    
    /* check for legacy kerberos version */
    
    gssname = itemCopyGSSName(item, NULL);
    if (gssname == NULL)
	return NULL;
    
    maj_stat = gss_display_name(&junk, gssname, &buffer, NULL);
    gss_release_name(&junk, &gssname);
    if (maj_stat)
	return NULL;
    
    ptr = memchr(buffer.value, '@', buffer.length);
    if (ptr == NULL) {
	asprintf(&name, "%.*s", (int)buffer.length, buffer.value);
	realm = strdup("");
    } else {
	size_t len = (int)((char *)ptr - (char *)buffer.value);
	asprintf(&name, "%.*s", (int)len, (char *)buffer.value);
	asprintf(&realm, "%.*s", (int)(buffer.length - len - 1), (char *)buffer.value + len + 1);
    }
    gss_release_buffer(&junk, &buffer);
    
    osret = SecKeychainFindGenericPassword(NULL, (UInt32)strlen(realm), realm,
					   (UInt32)strlen(name), name,
					   &length, &buf, NULL);
    free(name);
    free(realm);
    if (osret != noErr)
	return NULL;

    password = CFDataCreate(NULL, buf, length);
    SecKeychainItemFreeContent(NULL, buf);
    
    return password;
#else /* __APPLE_TARGET_EMBEDDED__ */
    return NULL;
#endif /* !__APPLE_TARGET_EMBEDDED__ */
}

static Boolean
storePassword(GSSItemRef item, CFDictionaryRef attributes, CFStringRef password, CFErrorRef *error)
{
#ifndef __APPLE_TARGET_EMBEDDED__
    CFTypeRef itemRef = NULL;
    OSStatus osret;
    CFStringRef uuidname;
    char *name, *pw;
    
    uuidname = CFDictionaryGetValue((CFDictionaryRef)item->keys, kGSSAttrUUID);
    if (uuidname == NULL && CFGetTypeID(uuidname) != CFStringGetTypeID())
	return false;

    name = rk_cfstring2cstring(uuidname);
    if (name == NULL)
	return false;

    CFDataRef dataoid = CFDataCreate(NULL, (void *)name, strlen(name));
    free(name);
    if (dataoid == NULL)
	return false;

    CFMutableDictionaryRef itemAttr = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    if (itemAttr == NULL) {
	CFRelease(dataoid);
	return false;
    }

    CFDictionaryAddValue(itemAttr, kSecAttrGeneric, dataoid);
    CFRelease(dataoid);

    CFStringRef gssclass = CFDictionaryGetValue(item->keys, kGSSAttrClass);
    if (gssclass == NULL)
	gssclass = CFSTR("unknown mech");
    CFStringRef displayname = CFDictionaryGetValue(item->keys, kGSSAttrNameDisplay);
    if (displayname == NULL)
	displayname = uuidname;

    CFStringRef description = CFStringCreateWithFormat(NULL, NULL, CFSTR("GSS %@ password for %@"), gssclass, displayname);
    if (description == NULL) {
	CFRelease(itemAttr);
	return false;
    }	
    CFDictionaryAddValue(itemAttr, kSecAttrDescription, description);
    CFDictionaryAddValue(itemAttr, kSecAttrLabel, description);
    CFRelease(description);
    
    CFDictionaryAddValue(itemAttr, kSecClass, kSecClassGenericPassword);
    CFDictionaryAddValue(itemAttr, kSecAttrType, kGSSSecPasswordType);
    CFDictionaryAddValue(itemAttr, kSecAttrAccount, uuidname);
    CFDictionaryAddValue(itemAttr, kSecAttrService, CFSTR("GSS"));

    pw = rk_cfstring2cstring(password);
    if (pw == NULL) {
	CFRelease(itemAttr);
	return false;
    }
    CFDataRef datapw = CFDataCreate(NULL, (void *)pw, strlen(pw));
    memset(pw, 0, strlen(pw));
    free(pw);
    if (datapw == NULL) {
	CFRelease(itemAttr);
	return false;
    }
    CFDictionaryAddValue(itemAttr, kSecValueData, datapw);
    CFRelease(datapw);

    CFTypeRef access = (void *)CFDictionaryGetValue(attributes, kSecAttrAccessGroup);
    if (access)
	CFDictionaryAddValue(itemAttr, kSecAttrAccessGroup, access);

    osret = SecItemAdd(itemAttr, &itemRef);
    CFRelease(itemAttr);
    if (osret)
	return false;

    CFRelease(itemRef);

    return true;
#else
    return false;
#endif /* !__APPLE_TARGET_EMBEDDED__ */
}

#pragma mark Matching

struct iterateAttr {
    GSSItemRef item;
    CFDictionaryRef attrs;
    CFErrorRef error;
    bool match;
};

static bool
applyClassItems(GSSItemRef item, CFDictionaryRef attributes,
		CFDictionaryApplierFunction applier, CFErrorRef *error)
{
    struct iterateAttr mattrs;
    CFDictionaryRef classattrs;
    CFTypeRef itemclass;

    if (error)
	*error = NULL;

    itemclass = CFDictionaryGetValue(attributes, kGSSAttrClass);
    if (itemclass == NULL) {
	itemclass = CFDictionaryGetValue(item->keys, kGSSAttrClass);
	if (itemclass == NULL)
	    return false;
    }

    classattrs = CFDictionaryGetValue(valid_set_types, itemclass);
    if (classattrs == NULL)
	return false;

    mattrs.attrs = attributes;
    mattrs.item = item;
    mattrs.error = NULL;
    mattrs.match = true;

    CFDictionaryApplyFunction(classattrs, applier, &mattrs);

    if (mattrs.error) {
	if (error)
	    *error = mattrs.error;
	else
	    CFRelease(mattrs.error);
	return false;
    }
    return mattrs.match;
}

static void
matchAttr(const void *key, const void *value, void *context)
{
    struct iterateAttr *mattrs = context;

    if (!mattrs->match)
	return;

    CFTypeRef attrvalue = CFDictionaryGetValue(mattrs->attrs, key);
    CFTypeRef itemvalue = CFDictionaryGetValue(mattrs->item->keys, key);

    if (attrvalue && itemvalue && !CFEqual(attrvalue, itemvalue))
	mattrs->match = false;
}

static bool
matchAttrs(GSSItemRef item, CFDictionaryRef attrs)
{
    return applyClassItems(item, attrs, matchAttr, NULL);
}

struct search {
    CFDictionaryRef attrs;
    CFMutableArrayRef result;
};

static void
searchFunction(const void *key, const void *value, void *context)
{
    struct search *search = context;
    GSSItemRef item = (GSSItemRef)value;

    if (matchAttrs(item, search->attrs))
	CFArrayAppendValue(search->result, item);
}


static CFMutableArrayRef
searchCopyResult(CFMutableDictionaryRef conf, CFDictionaryRef attrs)
{
    CFMutableArrayRef result;
    struct search search;

    result = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
    if (result == NULL)
	return NULL;

    search.attrs = attrs;
    search.result = result;

    CFDictionaryApplyFunction(conf, searchFunction, &search);

    if (CFArrayGetCount(result) == 0) {
	CFRelease(result);
	return NULL;
    }

    return result;
}

static void
modifyAttributes(const void *key, const void *value, void *context)
{
    struct iterateAttr *mattrs = context;
    CFTypeRef newValue = CFDictionaryGetValue(mattrs->attrs, key);

    /* check if this valid should be stored in the permanent store */
    if (!CFBooleanGetValue(value))
	return;

    if (newValue)
	CFDictionarySetValue(mattrs->item->keys, key, newValue);

}

static bool
modifyItem(GSSItemRef item, CFDictionaryRef attributes, CFErrorRef *error)
{
    bool res = applyClassItems(item, attributes, modifyAttributes, error);
    if (!res)
	return res;
    
    CFTypeRef cred = CFDictionaryGetValue(attributes, kGSSAttrCredentialPassword);

    if (cred && CFGetTypeID(cred) == CFStringGetTypeID()) {
	bool shouldStore = true, mustStore = false;
	CFBooleanRef store;

	store = CFDictionaryGetValue(attributes, kGSSAttrCredentialStore);
	if (store)
	    shouldStore = mustStore = (CFGetTypeID(store) == CFBooleanGetTypeID() && CFBooleanGetValue(store));
    
	if (shouldStore) {
	    res = storePassword(item, attributes, cred, error);
	    if (res && mustStore)
		return res;
	}
    }
    return true;
}


static void
validateAttributes(const void *key, const void *value, void *context)
{
    struct iterateAttr *mattrs = context;

    if (mattrs->error != NULL)
	return;

    Boolean b = CFBooleanGetValue(value);
    if (b) {
	CFTypeRef status = CFDictionaryGetValue(mattrs->attrs, key);
	if (status == NULL)
	    mattrs->error = CFErrorCreate(NULL, CFSTR("com.apple.GSS"), EINVAL, NULL);
    }
}


static bool
validateItem(GSSItemRef item, CFErrorRef *error)
{
    return applyClassItems(item, item->keys, validateAttributes, error);
}

/*
 *
 */

static void
updateTransientValues(GSSItemRef item)
{
    OM_uint32 maj_stat, junk, lifetime = 0;
    gss_cred_id_t gsscred = itemToGSSCred(item, &lifetime, NULL);
    gss_buffer_desc buffer;

    CFDictionaryRef attributes = extractCopyPassword(item, 1);
    if (attributes) {
	CFDictionarySetValue(item->keys, kGSSAttrCredentialPassword, kCFBooleanTrue);
	CFRelease(attributes);
    }

    if (gsscred) {

	CFDictionarySetValue(item->keys, kGSSAttrCredentialExists, kCFBooleanTrue);

	maj_stat = gss_cred_label_get(&junk, gsscred, "kcm-status", &buffer);
	if (maj_stat == GSS_S_COMPLETE) {
	    CFDataRef data = CFDataCreate(NULL, buffer.value, buffer.length);

	    if (data) {
		CFDictionarySetValue(item->keys, kGSSAttrStatusAutoAcquireStatus, data);
		CFRelease(data);
	    }
	    gss_release_buffer(&junk, &buffer);
	}

#define TimeIntervalSince1970  978307200.0
	CFDateRef date;
	if (lifetime)
	    date = CFDateCreate(NULL, time(NULL) - TimeIntervalSince1970 + lifetime);
	else
	    date = CFDateCreate(NULL, 0.0); /* We don't know when expired, so make it expired */

	CFDictionarySetValue(item->keys, kGSSAttrTransientExpire, date);
	CFRelease(date);

	{
	    gss_buffer_set_t buffers = NULL;

	    maj_stat = gss_inquire_cred_by_oid(&junk, gsscred, GSS_C_CRED_GET_DEFAULT, &buffers);
	    gss_release_buffer_set(&junk, &buffers);
	    if (maj_stat == GSS_S_COMPLETE)
		CFDictionarySetValue(item->keys, kGSSAttrTransientDefaultInClass, kCFBooleanTrue);
	}

	CFRelease(gsscred);

    } else {
	CFDictionaryRemoveValue(item->keys, kGSSAttrTransientExpire);
    }

    gss_name_t gssname = itemCopyGSSName(item, NULL);
    if (gssname) {

	maj_stat = gss_display_name(&junk, gssname, &buffer, NULL);
	if (maj_stat == GSS_S_COMPLETE) {
	    CFStringRef name;
	    name = CFStringCreateWithFormat(NULL, NULL, CFSTR("%.*s"), (int)buffer.length, buffer.value);
	    gss_release_buffer(&junk, &buffer);
	    if (name) {
		CFDictionarySetValue(item->keys, kGSSAttrNameDisplay, name);
		CFRelease(name);
	    }
	}
	gss_release_name(&junk, &gssname);
    }
}

#pragma mark API

GSSItemRef
GSSItemAdd(CFDictionaryRef attributes, CFErrorRef *error)
{
    CFMutableDictionaryRef conf;
    GSSItemRef item;
    CFStringRef uuidstr = NULL;
    CFMutableArrayRef items;

    gss_init();

    heim_assert(attributes != NULL, "no attributes passed to GSSItemAdd");

    if (error)
	*error = NULL;

    conf = copyConfiguration(true, error);
    if (conf == NULL)
	return NULL;

    /* refuse to add dups */
    items = searchCopyResult(conf, attributes);
    if (items) {
	CFRelease(items);
	CFRelease(conf);
	return NULL;
    }

    item = GSSCreateItem(NULL, NULL);
    if (item == NULL)
	goto out;

    uuidstr = CreateNewUUID(item);
    if (uuidstr == NULL) {
	CFRelease(item);
	item = NULL;
	goto out;
    }
    
    if (!modifyItem(item, attributes, error)) {
	CFRelease(item);
	item = NULL;
	goto out;
    }

    if (!validateItem(item, error)) {
	CFRelease(item);
	item = NULL;
	goto out;
    }
    
    updateTransientValues(item);

    CFDictionarySetValue(conf, uuidstr, item);

    storeConfiguration(conf);

out:
    if (uuidstr)
	CFRelease(uuidstr);
    if (conf)
	CFRelease(conf);

    return item;
}

Boolean
GSSItemUpdate(CFDictionaryRef query, CFDictionaryRef attributesToUpdate, CFErrorRef *error)
{
    CFMutableDictionaryRef conf;
    CFMutableArrayRef items;

    gss_init();

    heim_assert(query != NULL, "no attributes passed to GSSItemAdd");

    if (error)
	*error = NULL;

    conf = copyConfiguration(true, error);
    if (conf == NULL)
	return false;

    items = searchCopyResult(conf, query);
    if (items == NULL) {
	CFRelease(conf);
	return false;
    }

    CFIndex n, count = CFArrayGetCount(items);
    Boolean res = true;
    for (n = 0; n < count; n++) {
	GSSItemRef item = (GSSItemRef)CFArrayGetValueAtIndex(items, n);

	res = modifyItem(item, attributesToUpdate, error);
	if (res)
	    break;
    }
    CFRelease(items);

    if (res)
	storeConfiguration(conf);

    CFRelease(conf);

    return res;
}

static Boolean
ItemDeleteItem(CFMutableDictionaryRef conf, GSSItemRef item, CFErrorRef *error)
{
    CFTypeRef uuidstr = CFDictionaryGetValue(item->keys, kGSSAttrUUID);

    if (uuidstr == NULL)
	return false;

    gss_cred_id_t cred = itemToGSSCred(item, NULL, NULL);
    if (cred) {
	OM_uint32 junk;
	gss_destroy_cred(&junk, &cred);
    }
    CFDictionaryRemoveValue(conf, uuidstr);

    return true;
}

Boolean
GSSItemDeleteItem(GSSItemRef item, CFErrorRef *error)
{
    CFMutableDictionaryRef conf;
    Boolean res = false;

    conf = copyConfiguration(false, error);
    if (conf == NULL)
	return false;

    res = ItemDeleteItem(conf, item, error);
    if (res)
	storeConfiguration(conf);

    CFRelease(conf);

    return res;
}

Boolean
GSSItemDelete(CFDictionaryRef query, CFErrorRef *error)
{
    CFMutableDictionaryRef conf;
    CFMutableArrayRef items;

    gss_init();

    heim_assert(query != NULL, "no attributes passed to GSSItemDelete");

    if (error)
	*error = NULL;

    conf = copyConfiguration(false, error);
    if (conf == NULL)
	return false;

    items = searchCopyResult(conf, query);
    if (items == NULL) {
	CFRelease(conf);
	return false;
    }

    CFIndex n, count = CFArrayGetCount(items);
    Boolean res = false;

    for (n = 0; n < count; n++) {
	GSSItemRef item = (GSSItemRef)CFArrayGetValueAtIndex(items, n);

	if (ItemDeleteItem(conf, item, error))
	    res = true;
    }
    CFRelease(items);

    if (res)
	storeConfiguration(conf);

    CFRelease(conf);

    return res;
}

CFArrayRef
GSSItemCopyMatching(CFDictionaryRef query, CFErrorRef *error)
{
    CFMutableDictionaryRef conf;
    CFMutableArrayRef items;

    gss_init();

    if (error)
	*error = NULL;

    conf = copyConfiguration(true, error);
    if (conf == NULL)
	return NULL;

    items = searchCopyResult(conf, query);
    CFRelease(conf);
    if (items == NULL)
	return NULL;

    /*
     * Update the transient info
     */

    CFIndex n, count = CFArrayGetCount(items);
    for (n = 0; n < count; n++) {
	GSSItemRef item = (GSSItemRef)CFArrayGetValueAtIndex(items, n);

	updateTransientValues(item);
    }

    return items;
}

/*
 *
 */

struct __GSSOperationType {
    void (*func)(GSSItemRef, CFDictionaryRef, dispatch_queue_t, GSSItemOperationCallbackBlock);
};

static void
itemAcquire(GSSItemRef item, CFDictionaryRef options, dispatch_queue_t q, GSSItemOperationCallbackBlock callback)
{
    gss_cred_id_t gsscred = GSS_C_NO_CREDENTIAL;
    CFMutableDictionaryRef gssattrs = NULL;
    gss_name_t name = GSS_C_NO_NAME;
    CFErrorRef error = NULL;
    gss_const_OID mech;
    CFTypeRef cred = NULL;
    OM_uint32 junk;

    name = itemCopyGSSName(item, &error);
    if (name == NULL)
	goto out;

    mech = itemToMechOID(item, &error);
    if (mech == GSS_C_NO_OID)
	goto out;

    gssattrs = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    if (gssattrs == NULL)
	goto out;
    
    if (options) {
	cred = CFDictionaryGetValue(options, kGSSAttrCredentialPassword);
	if (cred)
	    CFDictionarySetValue(gssattrs, kGSSICPassword, cred);
	else {
	    cred = CFDictionaryGetValue(options, kGSSAttrCredentialSecIdentity);
	    if (cred)
		CFDictionarySetValue(gssattrs, kGSSICCertificate, cred);
	}
    }

    /*
     * Force Kerberos/kcm/gsscred layer to use the the same UUID
     */
    CFTypeRef uuidstr = CFDictionaryGetValue(item->keys, kGSSAttrUUID);
    if (uuidstr) {
	CFStringRef cacheName = CFStringCreateWithFormat(NULL, NULL, CFSTR("API:%@"), uuidstr);
	if (cacheName == NULL)
	    goto out;

	CFDictionarySetValue(gssattrs, kGSSICKerberosCacheName, cacheName);
	CFRelease(cacheName);
    }

    if (cred == NULL) {

	if ((cred = extractCopyPassword(item, 0)) != NULL) {
	    CFDictionarySetValue(gssattrs, kGSSICPassword, cred);
	    CFRelease(cred);
	    cred = NULL;
	} else if ((cred = NULL /* XXX extract certfigcate */) != NULL) {
	    /* CFDictionarySetValue(gssattrs, kGSSAttrCredentialSecIdentity, cred); */
	}
    }

    (void)gss_aapl_initial_cred(name, mech, gssattrs, &gsscred, &error);

    if (item->gssCredential) {
	CFRelease(item->gssCredential);
	item->gssCredential = NULL;
    }

    if (gsscred)
	item->gssCredential = GSSCredentialCopyUUID(gsscred);

    updateTransientValues(item);

 out:
    if (gssattrs)
	CFRelease(gssattrs);
    if (name)
	gss_release_name(&junk, &name);
    
    dispatch_async(q, ^{
	    callback(gsscred, error);
	    if (error)
		CFRelease(error);
	    if (gsscred)
		CFRelease(gsscred);
    });
}

const struct __GSSOperationType __kGSSOperationAcquire = {
    itemAcquire
};

static void
itemDestroyTransient(GSSItemRef item, CFDictionaryRef options, dispatch_queue_t q, GSSItemOperationCallbackBlock callback)
{
    CFErrorRef error = NULL;
    gss_cred_id_t gsscred;
    OM_uint32 junk;
    CFBooleanRef result = kCFBooleanFalse;

    gsscred = itemToGSSCred(item, NULL, &error);
    if (gsscred) {
	(void)gss_destroy_cred(&junk, &gsscred);
	result = kCFBooleanTrue;
    }

    dispatch_async(q, ^{
	callback(result, error);
	if (error)
	    CFRelease(error);
    });
}

const struct __GSSOperationType __kGSSOperationDestoryTransient = {
    itemDestroyTransient
};
const struct __GSSOperationType __kGSSOperationDestroyTransient = {
    itemDestroyTransient
};


static void
itemGetGSSCredential(GSSItemRef item, CFDictionaryRef options, dispatch_queue_t q, GSSItemOperationCallbackBlock callback)
{
    gss_cred_id_t gsscred;
    CFErrorRef error = NULL;

    gsscred = itemToGSSCred(item, NULL, &error);

    dispatch_async(q, ^{
	callback(gsscred, error);
	if (error)
	    CFRelease(error);
    });
}

const struct __GSSOperationType __kGSSOperationGetGSSCredential = {
    itemGetGSSCredential
};

static void
itemCredentialDiagnostics(GSSItemRef item, CFDictionaryRef options, dispatch_queue_t q, GSSItemOperationCallbackBlock callback)
{
    gss_cred_id_t gsscred;
    CFErrorRef error = NULL;
    CFMutableArrayRef array = NULL;
    OM_uint32 junk;
    
    gsscred = itemToGSSCred(item, NULL, &error);
    if (gsscred) {
	gss_buffer_set_t data_set = GSS_C_NO_BUFFER_SET;

	if (gss_inquire_cred_by_oid(&junk, gsscred, GSS_C_CRED_DIAG, &data_set) != GSS_S_COMPLETE || data_set->count < 1)
	    goto out;

	array = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
	if (array == NULL)
	    goto out;

	OM_uint32 n;

	for (n = 0; n < data_set->count; n++) {
	    CFDataRef data = NULL;

	    data = CFDataCreate(NULL, data_set->elements[n].value, data_set->elements[n].length);
	    if (data) {
		CFArrayAppendValue(array, data);
		CFRelease(data);
	    }
	}
	gss_release_buffer_set(&junk, &data_set);
	CFRelease(gsscred);
    }
    
 out:

    dispatch_async(q, ^{
	callback(array, error);
	if (array)
	    CFRelease(array);
	if (error)
	    CFRelease(error);
    });
}

const struct __GSSOperationType __kGSSOperationCredentialDiagnostics = {
    itemCredentialDiagnostics
};

static void
itemChangePassword(GSSItemRef item, CFDictionaryRef options, dispatch_queue_t q, GSSItemOperationCallbackBlock callback)
{
    CFMutableDictionaryRef chpwdopts = NULL;
    gss_name_t gssname = NULL;
    CFErrorRef error = NULL;
    gss_const_OID mech;
    
    CFStringRef oldpw = CFDictionaryGetValue(options, kGSSOperationChangePasswordOldPassword);
    CFStringRef newpw = CFDictionaryGetValue(options, kGSSOperationChangePasswordNewPassword);

    if (oldpw == NULL || newpw == NULL)
	goto out;

    mech = itemToMechOID(item, &error);
    if (mech == GSS_C_NO_OID)
	goto out;

    chpwdopts = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    if (chpwdopts == NULL)
	goto out;

    gssname = itemCopyGSSName(item, &error);
    if (gssname) {

	CFDictionaryAddValue(chpwdopts, kGSSChangePasswordOldPassword, oldpw);
	CFDictionaryAddValue(chpwdopts, kGSSChangePasswordNewPassword, newpw);

	(void)gss_aapl_change_password(gssname, mech, chpwdopts, &error);
    }    
  out:
    if (gssname)
	CFRelease(gssname);

    dispatch_async(q, ^{
	callback(NULL, error);
	if (error)
	    CFRelease(error);
    });
}

const struct __GSSOperationType __kGSSOperationChangePassword = {
    itemChangePassword
};

/*
 *
 */

static void
itemSetDefault(GSSItemRef item, CFDictionaryRef options, dispatch_queue_t q, GSSItemOperationCallbackBlock callback)
{
    CFErrorRef error = NULL;
    gss_cred_id_t gsscred;

    gsscred = itemToGSSCred(item, NULL, &error);
    if (gsscred) {
	gss_buffer_set_t data_set = GSS_C_NO_BUFFER_SET;
	OM_uint32 maj_stat, min_stat, junk;

	maj_stat = gss_inquire_cred_by_oid(&min_stat, gsscred, GSS_C_CRED_SET_DEFAULT, &data_set);
	gss_release_buffer_set(&junk, &data_set);
	CFRelease(gsscred);
	if (maj_stat != GSS_S_COMPLETE)
	    error = _gss_mg_create_cferror(maj_stat, min_stat, NULL);
    }

    dispatch_async(q, ^{
	    callback(NULL, error);
	    if (error)
		CFRelease(error);
	});
}

const struct __GSSOperationType __kGSSOperationSetDefault = {
    itemSetDefault
};


/*
 *
 */

static void
itemRenewCredential(GSSItemRef item, CFDictionaryRef options, dispatch_queue_t q, GSSItemOperationCallbackBlock callback)
{
    CFErrorRef error = NULL;
    gss_cred_id_t gsscred;
    OM_uint32 maj_stat, min_stat;

    gsscred = itemToGSSCred(item, NULL, &error);
    if (gsscred) {
	gss_buffer_set_t data_set = GSS_C_NO_BUFFER_SET;

	maj_stat = gss_inquire_cred_by_oid(&min_stat, gsscred, GSS_C_CRED_RENEW, &data_set);
	gss_release_buffer_set(&min_stat, &data_set);
	CFRelease(gsscred);
	if (maj_stat != GSS_S_COMPLETE)
	    error = _gss_mg_create_cferror(maj_stat, min_stat, NULL);
    }

    dispatch_async(q, ^{
	    callback(NULL, error);
	    if (error)
		CFRelease(error);
	});
}

const struct __GSSOperationType __kGSSOperationRenewCredential = {
    itemRenewCredential
};

/*
 *
 */

static void
itemRemoveBackingCredential(GSSItemRef item, CFDictionaryRef options, dispatch_queue_t q, GSSItemOperationCallbackBlock callback)
{
    CFErrorRef error = NULL;
    CFStringRef uuidname;
    
    uuidname = CFDictionaryGetValue((CFDictionaryRef)item->keys, kGSSAttrUUID);
    if (uuidname == NULL || CFGetTypeID(uuidname) != CFStringGetTypeID())
	goto out;

    CFMutableDictionaryRef itemAttr = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    if (itemAttr == NULL) {
	goto out;
    }

    CFDictionaryAddValue(itemAttr, kSecClass, kSecClassGenericPassword);
    CFDictionaryAddValue(itemAttr, kSecAttrType, kGSSSecPasswordType);
    CFDictionaryAddValue(itemAttr, kSecAttrAccount, uuidname);
    CFDictionaryAddValue(itemAttr, kSecAttrService, CFSTR("GSS"));

    (void)SecItemDelete(itemAttr);
    CFRelease(itemAttr);

 out:
    dispatch_async(q, ^{
	    callback(NULL, error);
	    if (error)
		CFRelease(error);
	});
}


const struct __GSSOperationType __kGSSOperationRemoveBackingCredential = {
    itemRemoveBackingCredential
};


/*
 *
 */

Boolean
GSSItemOperation(GSSItemRef item, GSSOperation op, CFDictionaryRef options,
		 dispatch_queue_t q, GSSItemOperationCallbackBlock func)
{
    GSSItemOperationCallbackBlock callback;

    gss_init();

    callback = (GSSItemOperationCallbackBlock)Block_copy(func);

    CFRetain(item);
    if (options)
	CFRetain(options);

    dispatch_async(bgq, ^{
	    op->func(item, options, q, callback);
	    Block_release(callback);
	    CFRelease(item);
	    if (options)
		CFRelease(options);
	});

    return true;
}

/*
 *
 */

CFTypeRef
GSSItemGetValue(GSSItemRef item, CFStringRef key)
{
    int checkNeeded = 0;

    if (CFDictionaryGetValue(transient_types, key)
	&& notify_check(notify_token, &checkNeeded) == NOTIFY_STATUS_OK
	&& checkNeeded)
	updateTransientValues(item);

    return CFDictionaryGetValue(item->keys, key);
}
