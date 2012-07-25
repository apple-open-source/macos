/*
 * Copyright (c) 2006-2010 Apple Inc. All Rights Reserved.
 * 
 * @APPLE_LICENSE_HEADER_START@
 * 
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */

/* 
 * SecItemServer.c - CoreFoundation-based constants and functions for
    access to Security items (certificates, keys, identities, and
    passwords.)
 */

#include <securityd/SecItemServer.h>

#include <Security/SecItem.h>
#include <Security/SecItemPriv.h>
#include <Security/SecItemInternal.h>
#include <Security/SecKey.h>
#include <Security/SecKeyPriv.h>
#include <Security/SecCertificateInternal.h>
#include <Security/SecIdentity.h>
#include <Security/SecIdentityPriv.h>
#include <Security/SecFramework.h>
#include <Security/SecRandom.h>
#include <Security/SecBasePriv.h>
#include <errno.h>
#include <limits.h>
#include <pthread.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <Security/SecBase.h>
#include <CoreFoundation/CFData.h>
#include <CoreFoundation/CFDate.h>
#include <CoreFoundation/CFDictionary.h>
#include <CoreFoundation/CFNumber.h>
#include <CoreFoundation/CFString.h>
#include <CoreFoundation/CFURL.h>
#include <CommonCrypto/CommonDigest.h>
#include <CommonCrypto/CommonCryptor.h>
#include <CommonCrypto/CommonCryptorSPI.h>
#include <libkern/OSByteOrder.h>
#include <security_utilities/debugging.h>
#include <assert.h>
#include <Security/SecInternal.h>
#include <TargetConditionals.h>
#include "securityd_client.h"
#include "securityd_server.h"
#include "sqlutils.h"
#include <AssertMacros.h>
#include <asl.h>
#include <inttypes.h>

#if TARGET_OS_EMBEDDED && !TARGET_IPHONE_SIMULATOR
#define USE_HWAES  1
#define USE_KEYSTORE  1
#else /* no hardware aes */
#define USE_HWAES 0
#define USE_KEYSTORE  0
#endif /* hardware aes */

#if USE_HWAES
#include <IOKit/IOKitLib.h>
#include <Kernel/IOKit/crypto/IOAESTypes.h>
#endif /* USE_HWAES */

#if USE_KEYSTORE
#include <Kernel/IOKit/crypto/AppleKeyStoreDefs.h>
#include <MobileKeyBag/MobileKeyBag.h>
typedef int32_t keyclass_t;
#else
/* TODO: this needs to be available in the sim! */
#define kAppleKeyStoreKeyWrap 0
#define kAppleKeyStoreKeyUnwrap 1
typedef int32_t keyclass_t;
typedef int32_t key_handle_t;
typedef int32_t keybag_handle_t;
enum key_classes {
    key_class_ak = 6,
    key_class_ck,
    key_class_dk,
    key_class_aku,
    key_class_cku,
    key_class_dku
};
#endif /* USE_KEYSTORE */

/* KEYBAG_LEGACY and KEYBAG_NONE are private to security and have special meaning.
   They should not collide with AppleKeyStore constants, but are only referenced
   in here.
 */
enum {
    KEYBAG_LEGACY = -3, /* Set q_keybag to KEYBAG_LEGACY to use legacy decrypt. */
    KEYBAG_BACKUP = -2, /* -2 == backup_keybag_handle, constant dictated by AKS */
    KEYBAG_NONE =   -1, /* Set q_keybag to KEYBAG_NONE to obtain cleartext data. */
    KEYBAG_DEVICE = 0, /* 0 == device_keybag_handle, constant dictated by AKS */
};

#if 0
#include <CoreFoundation/CFPriv.h>
#else
/* Pass NULL for the current user's home directory */
CF_EXPORT
CFURLRef CFCopyHomeDirectoryURLForUser(CFStringRef uName);
#endif

/* label when certificate data is joined with key data */
#define CERTIFICATE_DATA_COLUMN_LABEL "certdata" 

#define CURRENT_DB_VERSION 5

#define CLOSE_DB  0

static bool isArray(CFTypeRef cfType) {
    return cfType && CFGetTypeID(cfType) == CFArrayGetTypeID();
}

static bool isData(CFTypeRef cfType) {
    return cfType && CFGetTypeID(cfType) == CFDataGetTypeID();
}

static bool isDictionary(CFTypeRef cfType) {
    return cfType && CFGetTypeID(cfType) == CFDictionaryGetTypeID();
}

static bool isNumber(CFTypeRef cfType) {
    return cfType && CFGetTypeID(cfType) == CFNumberGetTypeID();
}

static bool isNumberOfType(CFTypeRef cfType, CFNumberType number) {
    return isNumber(cfType) && CFNumberGetType(cfType) == number;
}

static bool isString(CFTypeRef cfType) {
    return cfType && CFGetTypeID(cfType) == CFStringGetTypeID();
}

typedef struct s3dl_db
{
	char *db_name;
	pthread_key_t key;
	pthread_mutex_t mutex;
	/* Linked list of s3dl_db_thread * */
	struct s3dl_db_thread *dbt_head;
	/* True iff crypto facilities are available and keychain key is usable. */
	bool use_hwaes;
} s3dl_db;

typedef s3dl_db *db_handle;

typedef struct s3dl_db_thread
{
	struct s3dl_db_thread *dbt_next;
	s3dl_db *db;
	sqlite3 *s3_handle;
	bool autocommit;
	bool in_transaction;
} s3dl_db_thread;

typedef struct s3dl_results_handle
{
    uint32_t recordid;
    sqlite3_stmt *stmt;
} s3dl_results_handle;

/* Mapping from class name to kc_class pointer. */
static CFDictionaryRef gClasses;

/* Forward declaration of import export SPIs. */
enum SecItemFilter {
    kSecNoItemFilter,
    kSecSysBoundItemFilter,
    kSecBackupableItemFilter,
};

static CFDictionaryRef SecServerExportKeychainPlist(s3dl_db_thread *dbt,
    keybag_handle_t src_keybag, keybag_handle_t dest_keybag,
    enum SecItemFilter filter, int version, OSStatus *error);
static OSStatus SecServerImportKeychainInPlist(s3dl_db_thread *dbt,
    keybag_handle_t src_keybag, keybag_handle_t dest_keybag,
    CFDictionaryRef keychain, enum SecItemFilter filter);

#if USE_HWAES || USE_KEYSTORE
/*
 * Encryption support.
 */
static pthread_once_t hwcrypto_init_once = PTHREAD_ONCE_INIT;
static io_connect_t hwaes_codec = MACH_PORT_NULL;
static io_connect_t keystore = MACH_PORT_NULL;

static void service_matching_callback(void *refcon, io_iterator_t iterator)
{
	io_object_t obj = IO_OBJECT_NULL;

	while ((obj = IOIteratorNext(iterator))) {
        kern_return_t ret = IOServiceOpen(obj, mach_task_self(), 0,
            (io_connect_t*)refcon);
        if (ret) {
            asl_log(NULL, NULL, ASL_LEVEL_ERR,
                "IOServiceOpen() failed: %x", ret);
        }
        IOObjectRelease(obj);
	}
}

static void connect_to_service(const char *className, io_connect_t *connect)
{
	kern_return_t kernResult;
	io_iterator_t iterator = MACH_PORT_NULL;
    IONotificationPortRef port = MACH_PORT_NULL;
	CFDictionaryRef	classToMatch;

	if ((classToMatch = IOServiceMatching(className)) == NULL) {
		asl_log(NULL, NULL, ASL_LEVEL_ERR,
            "IOServiceMatching failed for '%s'", className);
		return;
	}

    /* consumed by IOServiceGetMatchingServices, we need it if that fails. */
    CFRetain(classToMatch);
    kernResult = IOServiceGetMatchingServices(kIOMasterPortDefault,
        classToMatch, &iterator);

    if (kernResult == KERN_SUCCESS) {
        CFRelease(classToMatch);
    } else {
        asl_log(NULL, NULL, ASL_LEVEL_WARNING,
            "IOServiceGetMatchingServices() failed %x using notifiction",
            kernResult);

        port = IONotificationPortCreate(kIOMasterPortDefault);
        if (!port) {
            asl_log(NULL, NULL, ASL_LEVEL_ERR,
                "IONotificationPortCreate() failed");
            return;
        }

        kernResult = IOServiceAddMatchingNotification(port,
            kIOFirstMatchNotification, classToMatch, service_matching_callback,
            connect, &iterator);
        if (kernResult) {
            asl_log(NULL, NULL, ASL_LEVEL_ERR,
                "IOServiceAddMatchingNotification() failed: %x", kernResult);
            return;
        }
    }

    /* Check whether it was already there before we registered for the
       notification. */
    service_matching_callback(connect, iterator);

    if (port) {
        /* We'll get set up to wait for it to appear */
        if (*connect == MACH_PORT_NULL) {
            asl_log(NULL, NULL, ASL_LEVEL_ERR,
                "Waiting for %s to show up.", className);
            CFStringRef mode = CFSTR("WaitForCryptoService");
            CFRunLoopAddSource(CFRunLoopGetCurrent(),
                IONotificationPortGetRunLoopSource(port), mode);
            CFRunLoopRunInMode(mode, 30.0, true);
            if (hwaes_codec == MACH_PORT_NULL)
                asl_log(NULL, NULL, ASL_LEVEL_ERR, "Cannot find AES driver");
        }
        IONotificationPortDestroy(port);
    }

    IOObjectRelease(iterator);

    if (*connect) {
        asl_log(NULL, NULL, ASL_LEVEL_INFO, "Obtained connection %d for %s",
            *connect, className);
    }
	return;
}

static void hwcrypto_init(void)
{
    connect_to_service(kIOAESAcceleratorClass, &hwaes_codec);
    connect_to_service(kAppleKeyStoreServiceName, &keystore);

    if (keystore != MACH_PORT_NULL) {
        IOReturn kernResult = IOConnectCallMethod(keystore,
            kAppleKeyStoreUserClientOpen, NULL, 0, NULL, 0, NULL, NULL,
            NULL, NULL);
        if (kernResult) {
            asl_log(NULL, NULL, ASL_LEVEL_ERR,
                "Failed to open AppleKeyStore: %x", kernResult);
        } else {
            asl_log(NULL, NULL, ASL_LEVEL_INFO, "Opened AppleKeyStore");
        }
    }
    /* TODO: Remove this once the kext runs the daemon on demand if
       there is no system keybag. */
    int kb_state = MKBGetDeviceLockState(NULL);
    asl_log(NULL, NULL, ASL_LEVEL_INFO, "AppleKeyStore lock state: %d",
        kb_state);
}

static bool hwaes_crypt(IOAESOperation operation, UInt32 keyHandle,
	UInt32 keySizeInBits, const UInt8 *keyBits, const UInt8 *iv,
	UInt32 textLength, UInt8 *plainText, UInt8 *cipherText)
{
	struct IOAESAcceleratorRequest aesRequest;
	kern_return_t kernResult;
	IOByteCount outSize;

    if (!hwaes_codec) {
		asl_log(NULL, NULL, ASL_LEVEL_ERR, "aes codec not initialized");
        return false;
    }

	aesRequest.plainText = plainText;
	aesRequest.cipherText = cipherText;
	aesRequest.textLength = textLength;
	memcpy(aesRequest.iv.ivBytes, iv, 16);
	aesRequest.operation = operation;
	aesRequest.keyData.key.keyLength = keySizeInBits;
    aesRequest.keyData.keyHandle = keyHandle;
	if (keyBits) {
		memcpy(aesRequest.keyData.key.keyBytes, keyBits, keySizeInBits / 8);
	} else {
		bzero(aesRequest.keyData.key.keyBytes, keySizeInBits / 8);
	}

	outSize = sizeof(aesRequest);
	kernResult = IOConnectCallStructMethod(hwaes_codec, 
		kIOAESAcceleratorPerformAES, &aesRequest, outSize, 
		&aesRequest, &outSize);

	if (kernResult != KERN_SUCCESS) {
		asl_log(NULL, NULL, ASL_LEVEL_ERR, "kIOAESAcceleratorPerformAES: %x",
            kernResult);
		return false;
	}

    asl_log(NULL, NULL, ASL_LEVEL_INFO,
        "kIOAESAcceleratorPerformAES processed: %lu bytes", textLength);

	return true;
}

static bool hwaes_key_available(void)
{
    /* The AES driver needs to have a 16byte aligned address */
    UInt8 buf[32] = {};
    UInt8 *bufp = (UInt8*)(((intptr_t)&buf[15]) & ~15);

    pthread_once(&hwcrypto_init_once, hwcrypto_init);
    return hwaes_crypt(IOAESOperationEncrypt,
		kIOAESAcceleratorKeyHandleKeychain, 128, NULL, bufp, 16, bufp, bufp);
}


#else /* !USE_HWAES */

static bool hwaes_key_available(void)
{
	return false;
}

#endif /* !USE_HWAES */

static int s3dl_create_path(const char *path)
{
	char pathbuf[PATH_MAX];
	size_t pos, len = strlen(path);
	if (len == 0 || len > PATH_MAX)
		return SQLITE_CANTOPEN;
	memcpy(pathbuf, path, len);
	for (pos = len; --pos > 0;)
	{
		/* Search backwards for trailing '/'. */
		if (pathbuf[pos] == '/')
		{
			pathbuf[pos] = '\0';
			/* Attempt to create parent directories of the database. */
			if (!mkdir(pathbuf, 0777))
				break;
			else 
			{
				int err = errno;
				if (err == EEXIST)
					return 0;
				if (err == ENOTDIR)
					return SQLITE_CANTOPEN;
				if (err == EROFS)
					return SQLITE_READONLY;
				if (err == EACCES)
					return SQLITE_PERM;
				if (err == ENOSPC || err == EDQUOT)
					return SQLITE_FULL;
				if (err == EIO)
					return SQLITE_IOERR;

				/* EFAULT || ELOOP | ENAMETOOLONG || something else */
				return SQLITE_INTERNAL;
			}
		}
	}
	return 0;
}

/* Start an exclusive transaction if we don't have one yet. */
static int s3dl_begin_transaction(s3dl_db_thread *dbt)
{
	if (dbt->in_transaction)
		return dbt->autocommit ? SQLITE_INTERNAL : SQLITE_OK;

	int s3e = sqlite3_exec(dbt->s3_handle, "BEGIN EXCLUSIVE TRANSACTION",
		NULL, NULL, NULL);
	if (s3e == SQLITE_OK)
		dbt->in_transaction = true;

	return s3e;
}

/* Commit the current transaction if we have one. */
static int s3dl_commit_transaction(s3dl_db_thread *dbt)
{
	if (!dbt->in_transaction)
		return SQLITE_OK;

	int s3e = sqlite3_exec(dbt->s3_handle, "COMMIT TRANSACTION",
		NULL, NULL, NULL);
	if (s3e == SQLITE_OK)
		dbt->in_transaction = false;

	return s3e;
}

/* Rollback the current transaction if we have one. */
static int s3dl_rollback_transaction(s3dl_db_thread *dbt)
{
	if (!dbt->in_transaction)
		return SQLITE_OK;

	int s3e = sqlite3_exec(dbt->s3_handle, "ROLLBACK TRANSACTION",
		NULL, NULL, NULL);
	if (s3e == SQLITE_OK)
		dbt->in_transaction = false;

	return s3e;
}

/* If we are in a transaction and autocommt is on, commit the transaction
   if s3e == SQLITE_OK, otherwise rollback the transaction. */
static int s3dl_end_transaction(s3dl_db_thread *dbt, int s3e)
{
	if (dbt->autocommit)
	{
		if (s3e == SQLITE_OK)
			return s3dl_commit_transaction(dbt);
		else
			s3dl_rollback_transaction(dbt);
	}

	return s3e;
}

static int s3dl_close_dbt(s3dl_db_thread *dbt)
{
	int s3e = sqlite3_close(dbt->s3_handle);
	free(dbt);
	return s3e;
}

typedef enum {
    kc_blob_attr,  // CFString or CFData, preserves caller provided type.
    kc_data_attr,
    kc_string_attr,
    kc_number_attr,
    kc_date_attr,
    kc_creation_date_attr,
    kc_modification_date_attr
} kc_attr_kind;

enum {
    kc_constrain_not_null = (1 << 0),       // attr value can't be null
    kc_constrain_default_0 = (1 << 1),      // default attr value is 0
    kc_constrain_default_empty = (1 << 2),  // default attr value is ""
    kc_digest_attr = (1 << 3),              // col in db is sha1 of attr value
};

typedef struct kc_attr_desc {
    CFStringRef name;
    kc_attr_kind kind;
    CFOptionFlags flags;
} kc_attr_desc;

typedef struct kc_class {
    CFStringRef name;
    CFIndex n_attrs;
    const kc_attr_desc *attrs;
} kc_class;

#if 0
typedef struct kc_item {
    kc_class *c;
    CFMutableDictionaryRef a;
} kc_item;

typedef CFIndex kc_attr_id;
#endif

#define KC_ATTR(name, kind, flags) { CFSTR(name), kc_ ## kind ## _attr, flags }

static const kc_attr_desc genp_attrs[] = {
    KC_ATTR("pdmn", string, 0),
    KC_ATTR("agrp", string, 0),
    KC_ATTR("cdat", creation_date, 0),
    KC_ATTR("mdat", modification_date, 0),
    KC_ATTR("desc", blob, kc_digest_attr),
    KC_ATTR("icmt", blob, kc_digest_attr),
    KC_ATTR("crtr", number, 0),
    KC_ATTR("type", number, 0),
    KC_ATTR("scrp", number, 0),
    KC_ATTR("labl", blob, kc_digest_attr),
    KC_ATTR("alis", blob, kc_digest_attr),
    KC_ATTR("invi", number, 0),
    KC_ATTR("nega", number, 0),
    KC_ATTR("cusi", number, 0),
    KC_ATTR("prot", blob, kc_digest_attr),
    KC_ATTR("acct", blob, kc_constrain_not_null | kc_constrain_default_empty | kc_digest_attr),
    KC_ATTR("svce", blob, kc_constrain_not_null | kc_constrain_default_empty | kc_digest_attr),
    KC_ATTR("gena", blob, kc_digest_attr),
};
#if 0
static const kc_attr_id genp_unique[] = {
    // acct, svce, agrp
    15, 16, 1
};
static kc_class_constraint genp_constraints[] = {
    { kc_unique_constraint, sizeof(genp_unique) / sizeof(*genp_unique), genp_unique },
};
#endif
static const kc_class genp_class = {
    .name = CFSTR("genp"),
    .n_attrs = sizeof(genp_attrs) / sizeof(*genp_attrs),
    .attrs = genp_attrs,
};

static const kc_attr_desc inet_attrs[] = {
    KC_ATTR("pdmn", string, 0),
    KC_ATTR("agrp", string, 0),
    KC_ATTR("cdat", creation_date, 0),
    KC_ATTR("mdat", modification_date, 0),
    KC_ATTR("desc", blob, kc_digest_attr),
    KC_ATTR("icmt", blob, kc_digest_attr),
    KC_ATTR("crtr", number, 0),
    KC_ATTR("type", number, 0),
    KC_ATTR("scrp", number, 0),
    KC_ATTR("labl", blob, kc_digest_attr),
    KC_ATTR("alis", blob, kc_digest_attr),
    KC_ATTR("invi", number, 0),
    KC_ATTR("nega", number, 0),
    KC_ATTR("cusi", number, 0),
    KC_ATTR("prot", blob, kc_digest_attr),
    KC_ATTR("acct", blob, kc_constrain_not_null | kc_constrain_default_empty | kc_digest_attr),
    KC_ATTR("sdmn", blob, kc_constrain_not_null | kc_constrain_default_empty | kc_digest_attr),
    KC_ATTR("srvr", blob, kc_constrain_not_null | kc_constrain_default_empty | kc_digest_attr),
    KC_ATTR("ptcl", number, kc_constrain_not_null | kc_constrain_default_0),
    KC_ATTR("atyp", blob, kc_digest_attr),
    KC_ATTR("port", number, kc_constrain_not_null | kc_constrain_default_0),
    KC_ATTR("path", blob, kc_constrain_not_null | kc_constrain_default_empty | kc_digest_attr),
};
#if 0
static const kc_attr_id inet_unique[] = {
    // acct, sdmn, srvr, ptcl, atyp, port, path, agrp
    15, 16, 17, 18, 19, 20, 21, 1
};
static kc_class_constraint inet_constraints[] = {
    { kc_unique_constraint, sizeof(inet_unique) / sizeof(*inet_unique), inet_unique },
};
#endif
static const kc_class inet_class = {
    .name = CFSTR("inet"),
    .n_attrs = sizeof(inet_attrs) / sizeof(*inet_attrs),
    .attrs = inet_attrs,
};

static const kc_attr_desc cert_attrs[] = {
    KC_ATTR("pdmn", string, 0),
    KC_ATTR("agrp", string, 0),
    KC_ATTR("cdat", creation_date, 0),
    KC_ATTR("mdat", modification_date, 0),
    KC_ATTR("ctyp", number, kc_constrain_not_null | kc_constrain_default_0),
    KC_ATTR("cenc", number, 0),
    KC_ATTR("labl", blob, kc_digest_attr),
    KC_ATTR("alis", blob, kc_digest_attr),
    KC_ATTR("subj", data, kc_digest_attr),
    KC_ATTR("issr", data, kc_constrain_not_null | kc_constrain_default_empty | kc_digest_attr),
    KC_ATTR("slnr", data, kc_constrain_not_null | kc_constrain_default_empty | kc_digest_attr),
    KC_ATTR("skid", data, kc_digest_attr),
    KC_ATTR("pkhh", data, 0),
};
#if 0
static const kc_attr_id cert_unique[] = {
    //ctyp, issr, slnr, agrp
    2, 7, 8, 1
};
static kc_class_constraint cert_constraints[] = {
    { kc_unique_constraint, sizeof(cert_unique) / sizeof(*cert_unique), cert_unique, },
};
#endif
static const kc_class cert_class = {
    .name = CFSTR("cert"),
    .n_attrs = sizeof(cert_attrs) / sizeof(*cert_attrs),
    .attrs = cert_attrs,
};

static const kc_attr_desc keys_attrs[] = {
    KC_ATTR("pdmn", string, 0),
    KC_ATTR("agrp", string, 0),
    KC_ATTR("cdat", creation_date, 0),
    KC_ATTR("mdat", modification_date, 0),
    KC_ATTR("kcls", number, kc_constrain_not_null | kc_constrain_default_0),
    KC_ATTR("labl", blob, kc_digest_attr),
    KC_ATTR("alis", blob, kc_digest_attr),
    KC_ATTR("perm", number, 0),
    KC_ATTR("priv", number, 0),
    KC_ATTR("modi", number, 0),
    KC_ATTR("klbl", data, kc_constrain_not_null | kc_constrain_default_empty),
    KC_ATTR("atag", blob, kc_constrain_not_null | kc_constrain_default_empty | kc_digest_attr),
    KC_ATTR("crtr", number, kc_constrain_not_null | kc_constrain_default_0),
    KC_ATTR("type", number, kc_constrain_not_null | kc_constrain_default_0),
    KC_ATTR("bsiz", number, kc_constrain_not_null | kc_constrain_default_0),
    KC_ATTR("esiz", number, kc_constrain_not_null | kc_constrain_default_0),
    KC_ATTR("sdat", date, kc_constrain_not_null | kc_constrain_default_0),
    KC_ATTR("edat", date, kc_constrain_not_null | kc_constrain_default_0),
    KC_ATTR("sens", number, 0),
    KC_ATTR("asen", number, 0),
    KC_ATTR("extr", number, 0),
    KC_ATTR("next", number, 0),
    KC_ATTR("encr", number, 0),
    KC_ATTR("decr", number, 0),
    KC_ATTR("drve", number, 0),
    KC_ATTR("sign", number, 0),
    KC_ATTR("vrfy", number, 0),
    KC_ATTR("snrc", number, 0),
    KC_ATTR("vyrc", number, 0),
    KC_ATTR("wrap", number, 0),
    KC_ATTR("unwp", number, 0),
};
#if 0
static const kc_attr_id keys_unique[] = {
    // kcls, klbl, atag, crtr, type, bsiz, esiz, sdat, edat, agrp
    2, 8, 9, 10, 11, 12, 13, 14, 15, 1
};
static kc_class_constraint keys_constraints[] = {
    { kc_unique_constraint, sizeof(keys_unique) / sizeof(*keys_unique), keys_unique, },
};
#endif
static const kc_class keys_class = {
    .name = CFSTR("keys"),
    .n_attrs = sizeof(keys_attrs) / sizeof(*keys_attrs),
    .attrs = keys_attrs,
};

/* An identity which is really a cert + a key, so all cert and keys attrs are
 allowed. */
static const kc_class identity_class = {
    .name = CFSTR("idnt"),
    .n_attrs = 0,
    .attrs = NULL,
};

static const kc_attr_desc *kc_attr_desc_with_key(const kc_class *c,
                                                 CFTypeRef key,
                                                 OSStatus *error) {
    /* Special case: identites can have all attributes of either cert
       or keys. */
    if (c == &identity_class) {
        const kc_attr_desc *desc;
        if (!(desc = kc_attr_desc_with_key(&cert_class, key, 0)))
            desc = kc_attr_desc_with_key(&keys_class, key, error);
        return desc;
    }

    if (isString(key)) {
        CFIndex ix;
        for (ix = 0; ix < c->n_attrs; ++ix) {
            if (CFEqual(c->attrs[ix].name, key)) {
                return &c->attrs[ix];
            }
        }

        /* TODO: Remove this hack since it's violating this function's contract to always set an error when it returns NULL. */
        if (CFEqual(key, kSecAttrSynchronizable))
            return NULL;

    }

    if (error && !*error)
        *error = errSecNoSuchAttr;

    return NULL;
}

static const char * const s3dl_upgrade_sql[] = {
    /* 0 */
    /* Upgrade from version 0 -- empty db a.k.a. current schema. */
    "CREATE TABLE genp("
    "rowid INTEGER PRIMARY KEY AUTOINCREMENT,"
    "cdat REAL,"
    "mdat REAL,"
    "desc BLOB,"
    "icmt BLOB,"
    "crtr INTEGER,"
    "type INTEGER,"
    "scrp INTEGER,"
    "labl BLOB,"
    "alis BLOB,"
    "invi INTEGER,"
    "nega INTEGER,"
    "cusi INTEGER,"
    "prot BLOB,"
    "acct BLOB NOT NULL DEFAULT '',"
    "svce BLOB NOT NULL DEFAULT '',"
    "gena BLOB,"
    "data BLOB,"
    "agrp TEXT,"
    "pdmn TEXT,"
    "UNIQUE("
    "acct,svce,agrp"
    "));"
    "CREATE TABLE inet("
    "rowid INTEGER PRIMARY KEY AUTOINCREMENT,"
    "cdat REAL,"
    "mdat REAL,"
    "desc BLOB,"
    "icmt BLOB,"
    "crtr INTEGER,"
    "type INTEGER,"
    "scrp INTEGER,"
    "labl BLOB,"
    "alis BLOB,"
    "invi INTEGER,"
    "nega INTEGER,"
    "cusi INTEGER,"
    "prot BLOB,"
    "acct BLOB NOT NULL DEFAULT '',"
    "sdmn BLOB NOT NULL DEFAULT '',"
    "srvr BLOB NOT NULL DEFAULT '',"
    "ptcl INTEGER NOT NULL DEFAULT 0,"
    "atyp BLOB NOT NULL DEFAULT '',"
    "port INTEGER NOT NULL DEFAULT 0,"
    "path BLOB NOT NULL DEFAULT '',"
    "data BLOB,"
    "agrp TEXT,"
    "pdmn TEXT,"
    "UNIQUE("
    "acct,sdmn,srvr,ptcl,atyp,port,path,agrp"
    "));"
    "CREATE TABLE cert("
    "rowid INTEGER PRIMARY KEY AUTOINCREMENT,"
    "cdat REAL,"
    "mdat REAL,"
    "ctyp INTEGER NOT NULL DEFAULT 0,"
    "cenc INTEGER,"
    "labl BLOB,"
    "alis BLOB,"
    "subj BLOB,"
    "issr BLOB NOT NULL DEFAULT '',"
    "slnr BLOB NOT NULL DEFAULT '',"
    "skid BLOB,"
    "pkhh BLOB,"
    "data BLOB,"
    "agrp TEXT,"
    "pdmn TEXT,"
    "UNIQUE("
    "ctyp,issr,slnr,agrp"
    "));"
    "CREATE TABLE keys("
    "rowid INTEGER PRIMARY KEY AUTOINCREMENT,"
    "cdat REAL,"
    "mdat REAL,"
    "kcls INTEGER NOT NULL DEFAULT 0,"
    "labl BLOB,"
    "alis BLOB,"
    "perm INTEGER,"
    "priv INTEGER,"
    "modi INTEGER,"
    "klbl BLOB NOT NULL DEFAULT '',"
    "atag BLOB NOT NULL DEFAULT '',"
    "crtr INTEGER NOT NULL DEFAULT 0,"
    "type INTEGER NOT NULL DEFAULT 0,"
    "bsiz INTEGER NOT NULL DEFAULT 0,"
    "esiz INTEGER NOT NULL DEFAULT 0,"
    "sdat REAL NOT NULL DEFAULT 0,"
    "edat REAL NOT NULL DEFAULT 0,"
    "sens INTEGER,"
    "asen INTEGER,"
    "extr INTEGER,"
    "next INTEGER,"
    "encr INTEGER,"
    "decr INTEGER,"
    "drve INTEGER,"
    "sign INTEGER,"
    "vrfy INTEGER,"
    "snrc INTEGER,"
    "vyrc INTEGER,"
    "wrap INTEGER,"
    "unwp INTEGER,"
    "data BLOB,"
    "agrp TEXT,"
    "pdmn TEXT,"
    "UNIQUE("
    "kcls,klbl,atag,crtr,type,bsiz,esiz,sdat,edat,agrp"
    "));"
    "CREATE TABLE tversion(version INTEGER);"
    "INSERT INTO tversion(version) VALUES(5);",

    /* 1 */
    /* Create indices. */
    "CREATE INDEX ialis ON cert(alis);"
    "CREATE INDEX isubj ON cert(subj);"
    "CREATE INDEX iskid ON cert(skid);"
    "CREATE INDEX ipkhh ON cert(pkhh);"
    "CREATE INDEX ikcls ON keys(kcls);"
    "CREATE INDEX iklbl ON keys(klbl);"
    "CREATE INDEX iencr ON keys(encr);"
    "CREATE INDEX idecr ON keys(decr);"
    "CREATE INDEX idrve ON keys(drve);"
    "CREATE INDEX isign ON keys(sign);"
    "CREATE INDEX ivrfy ON keys(vrfy);"
    "CREATE INDEX iwrap ON keys(wrap);"
    "CREATE INDEX iunwp ON keys(unwp);",

    /* 2 */
    /* Rename version 1 tables. */
    "ALTER TABLE genp RENAME TO ogenp;"
    "ALTER TABLE inet RENAME TO oinet;"
    "ALTER TABLE cert RENAME TO ocert;"
    "ALTER TABLE keys RENAME TO okeys;",

    /* 3 */
    /* Rename version 2 or version 3 tables and drop version table since
       step 0 creates it. */
    "ALTER TABLE genp RENAME TO ogenp;"
    "ALTER TABLE inet RENAME TO oinet;"
    "ALTER TABLE cert RENAME TO ocert;"
    "ALTER TABLE keys RENAME TO okeys;"
    "DROP TABLE tversion;",

    /* 4 */
    /* Move data from version 1 or version 2 tables to new ones and drop old
       ones. */
    /* Set the agrp on all (apple internal) items to apple. */
    "INSERT INTO genp (rowid,cdat,mdat,desc,icmt,crtr,type,scrp,labl,alis,invi,nega,cusi,prot,acct,svce,gena,data) SELECT rowid,cdat,mdat,desc,icmt,crtr,type,scrp,labl,alis,invi,nega,cusi,prot,acct,svce,gena,data from ogenp;"
    "INSERT INTO inet (rowid,cdat,mdat,desc,icmt,crtr,type,scrp,labl,alis,invi,nega,cusi,prot,acct,sdmn,srvr,ptcl,atyp,port,path,data) SELECT rowid,cdat,mdat,desc,icmt,crtr,type,scrp,labl,alis,invi,nega,cusi,prot,acct,sdmn,srvr,ptcl,atyp,port,path,data from oinet;"
    "INSERT INTO cert (rowid,ctyp,cenc,labl,alis,subj,issr,slnr,skid,pkhh,data) SELECT rowid,ctyp,cenc,labl,alis,subj,issr,slnr,skid,pkhh,data from ocert;"
    "INSERT INTO keys (rowid,kcls,labl,alis,perm,priv,modi,klbl,atag,crtr,type,bsiz,esiz,sdat,edat,sens,asen,extr,next,encr,decr,drve,sign,vrfy,snrc,vyrc,wrap,unwp,data) SELECT rowid,kcls,labl,alis,perm,priv,modi,klbl,atag,crtr,type,bsiz,esiz,sdat,edat,sens,asen,extr,next,encr,decr,drve,sign,vrfy,snrc,vyrc,wrap,unwp,data from okeys;"
    "UPDATE genp SET agrp='apple';"
    "UPDATE inet SET agrp='apple';"
    "UPDATE cert SET agrp='apple';"
    "UPDATE keys SET agrp='apple';"
    "DROP TABLE ogenp;"
    "DROP TABLE oinet;"
    "DROP TABLE ocert;"
    "DROP TABLE okeys;",

    /* 5 */
    /* Move data from version 3 tables to new ones and drop old ones. */
    "INSERT INTO genp (rowid,cdat,mdat,desc,icmt,crtr,type,scrp,labl,alis,invi,nega,cusi,prot,acct,svce,gena,data,agrp) SELECT rowid,cdat,mdat,desc,icmt,crtr,type,scrp,labl,alis,invi,nega,cusi,prot,acct,svce,gena,data,agrp from ogenp;"
    "INSERT INTO inet (rowid,cdat,mdat,desc,icmt,crtr,type,scrp,labl,alis,invi,nega,cusi,prot,acct,sdmn,srvr,ptcl,atyp,port,path,data,agrp) SELECT rowid,cdat,mdat,desc,icmt,crtr,type,scrp,labl,alis,invi,nega,cusi,prot,acct,sdmn,srvr,ptcl,atyp,port,path,data,agrp from oinet;"
    "INSERT INTO cert (rowid,ctyp,cenc,labl,alis,subj,issr,slnr,skid,pkhh,data,agrp) SELECT rowid,ctyp,cenc,labl,alis,subj,issr,slnr,skid,pkhh,data,agrp from ocert;"
    "INSERT INTO keys (rowid,kcls,labl,alis,perm,priv,modi,klbl,atag,crtr,type,bsiz,esiz,sdat,edat,sens,asen,extr,next,encr,decr,drve,sign,vrfy,snrc,vyrc,wrap,unwp,data,agrp) SELECT rowid,kcls,labl,alis,perm,priv,modi,klbl,atag,crtr,type,bsiz,esiz,sdat,edat,sens,asen,extr,next,encr,decr,drve,sign,vrfy,snrc,vyrc,wrap,unwp,data,agrp from okeys;"
    "DROP TABLE ogenp;"
    "DROP TABLE oinet;"
    "DROP TABLE ocert;"
    "DROP TABLE okeys;",

    /* 6 */
    /* Move data from version 4 tables to new ones and drop old ones. */
    "INSERT INTO genp (rowid,cdat,mdat,desc,icmt,crtr,type,scrp,labl,alis,invi,nega,cusi,prot,acct,svce,gena,data,agrp,pdmn) SELECT rowid,cdat,mdat,desc,icmt,crtr,type,scrp,labl,alis,invi,nega,cusi,prot,acct,svce,gena,data,agrp,pdmn from ogenp;"
    "INSERT INTO inet (rowid,cdat,mdat,desc,icmt,crtr,type,scrp,labl,alis,invi,nega,cusi,prot,acct,sdmn,srvr,ptcl,atyp,port,path,data,agrp,pdmn) SELECT rowid,cdat,mdat,desc,icmt,crtr,type,scrp,labl,alis,invi,nega,cusi,prot,acct,sdmn,srvr,ptcl,atyp,port,path,data,agrp,pdmn from oinet;"
    "INSERT INTO cert (rowid,ctyp,cenc,labl,alis,subj,issr,slnr,skid,pkhh,data,agrp,pdmn) SELECT rowid,ctyp,cenc,labl,alis,subj,issr,slnr,skid,pkhh,data,agrp,pdmn from ocert;"
    "INSERT INTO keys (rowid,kcls,labl,alis,perm,priv,modi,klbl,atag,crtr,type,bsiz,esiz,sdat,edat,sens,asen,extr,next,encr,decr,drve,sign,vrfy,snrc,vyrc,wrap,unwp,data,agrp,pdmn) SELECT rowid,kcls,labl,alis,perm,priv,modi,klbl,atag,crtr,type,bsiz,esiz,sdat,edat,sens,asen,extr,next,encr,decr,drve,sign,vrfy,snrc,vyrc,wrap,unwp,data,agrp,pdmn from okeys;"
    "DROP TABLE ogenp;"
    "DROP TABLE oinet;"
    "DROP TABLE ocert;"
    "DROP TABLE okeys;",
};

struct sql_stages {
    int pre;
    int main;
    int post;
    bool init_pdmn;
};

static struct sql_stages s3dl_upgrade_script[] = {
    { -1, 0, 1, false },  /* Create version 5 database. */
    { 2, 0, 4, true },    /* Upgrade to version 5 from version 1 (LittleBear). */
    { 3, 0, 4, true },    /* Upgrade to version 5 from version 2 (BigBearBeta). */
    { 3, 0, 5, true },    /* Upgrade to version 5 from version 3 (Apex). */
    { 3, 0, 6, true },    /* Upgrade to version 5 from version 4 (Telluride). */
};

static int sql_run_script(s3dl_db_thread *dbt, int number)
{
    int s3e;

    /* Script -1 == skip this step. */
    if (number < 0)
        return SQLITE_OK;

    /* If we are attempting to run a script we don't have, fail. */
    if ((size_t)number >= sizeof(s3dl_upgrade_sql) / sizeof(*s3dl_upgrade_sql))
        return SQLITE_CORRUPT;

    char *errmsg = NULL;
    s3e = sqlite3_exec(dbt->s3_handle, s3dl_upgrade_sql[number],
        NULL, NULL, &errmsg);
    if (errmsg) {
        secwarning("script %d: %s", number, errmsg);
        sqlite3_free(errmsg);
    }

    return s3e;
}


static int s3dl_dbt_upgrade_from_version(s3dl_db_thread *dbt, int version)
{
    /* We need to go from db version to CURRENT_DB_VERSION, let's do so. */
    int s3e;

    /* If we are attempting to upgrade to a version greater than what we have
       an upgrade script for, fail. */
    if (version < 0 ||
        (size_t)version >= sizeof(s3dl_upgrade_script) / sizeof(*s3dl_upgrade_script))
        return SQLITE_CORRUPT;

    struct sql_stages *script = &s3dl_upgrade_script[version];
    s3e = sql_run_script(dbt, script->pre);
    if (s3e == SQLITE_OK)
        s3e = sql_run_script(dbt, script->main);
    if (s3e == SQLITE_OK)
        s3e = sql_run_script(dbt, script->post);
    if (script->init_pdmn) {
        OSStatus status = s3e;
        /* version 3 and earlier used legacy blob. */
        CFDictionaryRef backup = SecServerExportKeychainPlist(dbt,
            version < 4 ? KEYBAG_LEGACY : KEYBAG_DEVICE,
            KEYBAG_NONE, kSecNoItemFilter, version, &status);
        if (backup) {
            if (status) {
                secerror("Ignoring export error: %d during upgrade", status);
            }
            status = SecServerImportKeychainInPlist(dbt, KEYBAG_NONE,
                KEYBAG_DEVICE, backup, kSecNoItemFilter);
            CFRelease(backup);
        } else if (status == errSecInteractionNotAllowed){
            status = errSecUpgradePending;
        }
        s3e = status;
    }

    return s3e;
}

static int s3dl_dbt_create_or_upgrade(s3dl_db_thread *dbt)
{
    sqlite3_stmt *stmt = NULL;
	int s3e;

    /* Find out if we need to upgrade from version 0 (empty db) or version 1
       -- the db schema before we had a tversion table. */
    s3e = sqlite3_prepare(dbt->s3_handle, "SELECT cdat FROM genp", -1, &stmt, NULL);
    if (stmt)
        sqlite3_finalize(stmt);

    return s3dl_dbt_upgrade_from_version(dbt, s3e ? 0 : 1);
}

/* Return the current database version in *version.  Returns a
   SQLITE error. */
static int s3dl_dbt_get_version(s3dl_db_thread *dbt, int *version)
{
	sqlite3 *s3h = dbt->s3_handle;
	int s3e;

    sqlite3_stmt *stmt = NULL;
    static const char sql[] = "SELECT version FROM tversion LIMIT 1;";
    s3e = sqlite3_prepare(s3h, sql, sizeof(sql) - 1, &stmt, NULL);
    if (s3e)
        goto errOut;

    s3e = sqlite3_step(stmt);
    if (s3e == SQLITE_ROW) {
        *version = sqlite3_column_int(stmt, 0);
        s3e = SQLITE_OK;
    } else if (s3e) {
        secwarning("SELECT version step: %s", sqlite3_errmsg(s3h));
    } else {
        /* We have a VERSION table but we didn't find a version
           value now what?   I suppose we pretend the db is corrupted
           since this isn't supposed to ever happen. */
        s3e = SQLITE_CORRUPT;
    } 

errOut:
    if (stmt) {
        /* We ignore this error since this function may return SQLITE_ERROR,
        SQLITE_IOERR_READ or SQLITE_ABORT if the stmt itself failed, but
        that's something we would have handeled already. */
		sqlite3_finalize(stmt);
    }

	return s3e;
}

/* This function is called if the db doesn't have the proper version.  We
   start an exclusive transaction and recheck the version, and then perform
   the upgrade within that transaction. */
static int s3dl_dbt_upgrade(s3dl_db_thread *dbt)
{
    int version;
    int s3e;

    require_noerr(s3e = s3dl_begin_transaction(dbt), errOut);
    s3e = s3dl_dbt_get_version(dbt, &version);
    if (!s3e) {
        s3e = s3dl_dbt_upgrade_from_version(dbt, version);
    } else {
        /* We have no version table yet so we need to create a new db or
           upgrade from version 1 (the db schema without a tversion table). */
        require_noerr(s3e = s3dl_dbt_create_or_upgrade(dbt), errOut);
    }

errOut:

    return s3dl_end_transaction(dbt, s3e);
}

static int s3dl_create_dbt(s3dl_db *db, s3dl_db_thread **pdbt, int create)
{
	sqlite3 *s3h;
	int retries, s3e;
    for (retries = 0; retries < 2; ++retries) {
        s3e = sqlite3_open(db->db_name, &s3h);
        if (s3e == SQLITE_CANTOPEN && create)
        {
            /* Make sure the path to db->db_name exists and is writable, then
               try again. */
            s3dl_create_path(db->db_name);
            s3e = sqlite3_open(db->db_name, &s3h);
        }

        if (!s3e) {
            s3dl_db_thread *dbt = (s3dl_db_thread *)malloc(sizeof(s3dl_db_thread));
            dbt->dbt_next = NULL;
            dbt->db = db;
            dbt->s3_handle = s3h;
            dbt->autocommit = true;
            dbt->in_transaction = false;

            int version;
            s3e = s3dl_dbt_get_version(dbt, &version);
            if (s3e == SQLITE_EMPTY || s3e == SQLITE_ERROR || (!s3e && version < CURRENT_DB_VERSION)) {
                s3e = s3dl_dbt_upgrade(dbt);
                if (s3e) {
                    asl_log(NULL, NULL, ASL_LEVEL_CRIT,
                        "failed to upgrade keychain %s: %d", db->db_name, s3e);
                    if (s3e != errSecUpgradePending) {
                        s3e = SQLITE_CORRUPT;
                    }
                }
            } else if (s3e) {
                asl_log(NULL, NULL, ASL_LEVEL_ERR,
                    "failed to obtain database version for %s: %d",
                    db->db_name, s3e);
            } else if (version > CURRENT_DB_VERSION) {
                /* We can't downgrade so we treat a too new db as corrupted. */
                asl_log(NULL, NULL, ASL_LEVEL_ERR,
                    "found keychain %s with version: %d which is newer than %d marking as corrupted",
                    db->db_name, version, CURRENT_DB_VERSION);
                s3e = SQLITE_CORRUPT;
            }

            if (s3e) {
                s3dl_close_dbt(dbt);
            } else {
                *pdbt = dbt;
                break;
            }
        }

        if (s3e == SQLITE_CORRUPT || s3e == SQLITE_NOTADB ||
            s3e == SQLITE_CANTOPEN || s3e == SQLITE_PERM ||
            s3e == SQLITE_CONSTRAINT) {
            size_t len = strlen(db->db_name);
            char *old_db_name = malloc(len + 9);
            memcpy(old_db_name, db->db_name, len);
            strcpy(old_db_name + len, ".corrupt");
            if (rename(db->db_name, old_db_name)) {
                asl_log(NULL, NULL, ASL_LEVEL_CRIT,
                    "unable to rename corrupt keychain %s -> %s: %s",
                    db->db_name, old_db_name, strerror(errno));
                free(old_db_name);
                break;
            } else {
                asl_log(NULL, NULL, ASL_LEVEL_ERR,
                    "renamed corrupt keychain %s -> %s (%d)",
                    db->db_name, old_db_name, s3e);
            }
            free(old_db_name);
        } else if (s3e) {
            asl_log(NULL, NULL, ASL_LEVEL_CRIT,
                "failed to open keychain %s: %d", db->db_name, s3e);
            break;
        }
    }

	return s3e;
}

/* Called when a thread that was using this db goes away. */
static void s3dl_dbt_destructor(void *data)
{
	s3dl_db_thread *dbt = (s3dl_db_thread *)data;
	int found = 0;

	/* Remove the passed in dbt from the linked list. */
	/* TODO: Log pthread errors. */
	pthread_mutex_lock(&dbt->db->mutex);
	s3dl_db_thread **pdbt = &dbt->db->dbt_head;
	for (;*pdbt; pdbt = &(*pdbt)->dbt_next)
	{
		if (*pdbt == dbt)
		{
			*pdbt = dbt->dbt_next;
			found = 1;
			break;
		}
	}
	/* TODO: Log pthread errors. */
	pthread_mutex_unlock(&dbt->db->mutex);

	/* Don't hold dbt->db->mutex while cleaning up the dbt. */
	if (found)
		s3dl_close_dbt(dbt);
}

/* Agressivly write to pdbHandle since we want to be able to call internal SPI
   function during initialization. */
static int s3dl_create_db_handle(const char *db_name, db_handle *pdbHandle,
	s3dl_db_thread **pdbt, bool autocommit, bool create, bool use_hwaes)
{
	void *mem = malloc(sizeof(s3dl_db) + strlen(db_name) + 1);
	s3dl_db *db = (s3dl_db *)mem;
	db->db_name = ((char *)mem) + sizeof(*db);
	strcpy(db->db_name, db_name);
    /* Make sure we set this before calling s3dl_create_dbt, since that might
       trigger a db upgrade which needs to decrypt stuff. */
    db->use_hwaes = use_hwaes;

	s3dl_db_thread *dbt;
	int s3e = s3dl_create_dbt(db, &dbt, create);
	if (s3e != SQLITE_OK)
	{
        if (s3e == errSecUpgradePending) {
            secerror("Device locked during initial open + upgrade attempt");
            dbt = NULL;
        } else {
            free(mem);
            return s3e;
        }
	} else {
        dbt->autocommit = autocommit;
    }
	db->dbt_head = dbt;

	int err = pthread_key_create(&db->key, s3dl_dbt_destructor);
	if (!err)
		err = pthread_mutex_init(&db->mutex, NULL);
	if (!err && dbt)
		err = pthread_setspecific(db->key, dbt);
	if (err)
	{
		/* TODO: Log err (which is an errno) somehow. */
        if (dbt)
            s3e = s3dl_close_dbt(dbt);
		if (s3e == SQLITE_OK)
			s3e = SQLITE_INTERNAL;

		free(mem);
	} else {
		if (pdbt)
			*pdbt = dbt;

		*pdbHandle = (db_handle)db;
	}

    return s3e;
}

static int s3dl_close_db_handle(db_handle dbHandle)
{
	s3dl_db *db = (s3dl_db *)dbHandle;
	int s3e = SQLITE_OK;

	/* Walk the list of dbt's and close them all. */
	s3dl_db_thread *next_dbt = db->dbt_head;
	while (next_dbt)
	{
		s3dl_db_thread *dbt = next_dbt;
		next_dbt = next_dbt->dbt_next;
		int s3e2 = s3dl_close_dbt(dbt);
		if (s3e2 != SQLITE_OK && s3e == SQLITE_OK)
			s3e = s3e2;
	}

	pthread_key_delete(db->key);
	free(db);

	return s3e;
}

static int s3dl_get_dbt(db_handle dbHandle, s3dl_db_thread **pdbt)
{
	if (!dbHandle)
        return SQLITE_ERROR;

	s3dl_db *db = (s3dl_db *)dbHandle;
	int s3e = SQLITE_OK;
	s3dl_db_thread *dbt = pthread_getspecific(db->key);
	if (!dbt)
	{
		/* We had no dbt yet, so create a new one, but don't create the
		   database. */
		s3e = s3dl_create_dbt(db, &dbt, false);
		if (s3e == SQLITE_OK)
		{
			/* Lock the mutex, insert the new entry at the head of the
			   linked list and release the lock. */
			int err = pthread_mutex_lock(&db->mutex);
			if (!err)
			{
				dbt->dbt_next = db->dbt_head;
				db->dbt_head = dbt;
				err = pthread_mutex_unlock(&db->mutex);
			}

			/* Set the dbt as this threads dbt for db. */
			if (!err)
				err = pthread_setspecific(db->key, dbt);
			if (err)
			{
				/* TODO: Log err (which is an errno) somehow. */
				s3e = s3dl_close_dbt(dbt);
				if (s3e == SQLITE_OK)
					s3e = SQLITE_INTERNAL;
			}
		}
	}
	*pdbt = dbt;
	return s3e;
}

/* Return an OSStatus for a sqlite3 error code. */
static OSStatus osstatus_for_s3e(int s3e)
{
	if (s3e > 0 && s3e <= SQLITE_DONE) switch (s3e)
	{
	case SQLITE_OK:
		return 0;
	case SQLITE_ERROR:
		return errSecNotAvailable; /* errSecDuplicateItem; */
	case SQLITE_FULL: /* Happens if we run out of uniqueids */
		return errSecNotAvailable; /* TODO: Replace with a better error code. */
	case SQLITE_PERM:
	case SQLITE_READONLY:
		return errSecNotAvailable;
	case SQLITE_CANTOPEN:
		return errSecNotAvailable;
	case SQLITE_EMPTY:
		return errSecNotAvailable;
	case SQLITE_CONSTRAINT:
        return errSecDuplicateItem;
	case SQLITE_ABORT:
		return -1;
	case SQLITE_MISMATCH:
		return errSecNoSuchAttr;
	case SQLITE_AUTH:
		return errSecNotAvailable;
	case SQLITE_NOMEM:
		return -2; /* TODO: Replace with a real error code. */
	case SQLITE_INTERNAL:
	default:
		return errSecNotAvailable; /* TODO: Replace with a real error code. */
	}
    return s3e;
}

const uint32_t v0KeyWrapOverHead = 8;

/* Wrap takes a 128 - 256 bit key as input and returns output of
   inputsize + 64 bits.
   In bytes this means that a
   16 byte (128 bit) key returns a 24 byte wrapped key
   24 byte (192 bit) key returns a 32 byte wrapped key
   32 byte (256 bit) key returns a 40 byte wrapped key  */
static int ks_crypt(uint32_t selector, keybag_handle_t keybag,
    keyclass_t keyclass, uint32_t textLength, const uint8_t *source, uint8_t *dest, size_t *dest_len) {
#if USE_KEYSTORE
	kern_return_t kernResult;

    if (keystore == MACH_PORT_NULL) {
		asl_log(NULL, NULL, ASL_LEVEL_ERR, "No AppleKeyStore connection");
        return errSecNotAvailable;
    }

    uint64_t inputs[] = { keybag, keyclass };
    uint32_t num_inputs = sizeof(inputs)/sizeof(*inputs);
    kernResult = IOConnectCallMethod(keystore, selector, inputs,
        num_inputs, source, textLength, NULL, NULL, dest, dest_len);

	if (kernResult != KERN_SUCCESS) {
        if (kernResult == kIOReturnNotPermitted) {
            /* Access to item attempted while keychain is locked. */
            asl_log(NULL, NULL, ASL_LEVEL_INFO,
                "%s sel: %d bag: %d cls: %d src: %p len: %"PRIu32" err: kIOReturnNotPermitted",
                (selector == kAppleKeyStoreKeyWrap ? "kAppleKeyStoreKeyWrap"
                 : "kAppleKeyStoreKeyUnwrap"),
                selector, keybag, keyclass, source, textLength);
            return errSecInteractionNotAllowed;
        } else if (kernResult == kIOReturnError) {
            /* Item can't be decrypted on this device, ever, so drop the item. */
            secerror("%s sel: %d bag: %d cls: %d src: %p len: %lu err: kIOReturnError",
                (selector == kAppleKeyStoreKeyWrap ? "kAppleKeyStoreKeyWrap"
                 : "kAppleKeyStoreKeyUnwrap"),
                selector, keybag, keyclass, source, textLength);
            return errSecDecode;
        } else {
            secerror("%s sel: %d bag: %d cls: %d src: %p len: %lu err: %x",
                (selector == kAppleKeyStoreKeyWrap ? "kAppleKeyStoreKeyWrap"
                 : "kAppleKeyStoreKeyUnwrap"),
                selector, keybag, keyclass, source, textLength, kernResult);
            return errSecNotAvailable;
        }
	}
	return errSecSuccess;
#else
    if (selector == kAppleKeyStoreKeyWrap) {
        /* The no encryption case. */
        if (*dest_len >= textLength + 8) {
            memcpy(dest, source, textLength);
            memset(dest + textLength, 8, 8);
            *dest_len = textLength + 8;
        } else
            return errSecNotAvailable;
    } else if (selector == kAppleKeyStoreKeyUnwrap) {
        if (*dest_len + 8 >= textLength) {
            memcpy(dest, source, textLength - 8);
            *dest_len = textLength - 8;
        } else
            return errSecNotAvailable;
    }
    return errSecSuccess;
#endif
}

#if 0

typedef struct kc_item {
    CFMutableDictionaryRef item;
    CFIndex n_attrs;
    CFStringRef *attrs;
    void *values[];
} kc_item;

#define kc_item_size(n) sizeof(kc_item) + 2 * sizeof(void *)
#define kc_item_init(i, n) do { \
    kc_item *_kc_item_a = (i); \
    _kc_item_a->item = NULL; \
    _kc_item_a->n_attrs = (n); \
    _kc_item_a->attrs = (CFStringRef *)&_kc_item_a->values[_kc_item_a->n_attrs]; \
} while(0)

static kc_item *kc_item_create(CFIndex n_attrs) {
    kc_item *item = malloc(kc_item_size(n_attrs));
    kc_item_init(item, n_attrs);
    return item;
}

static void kc_item_destroy(kc_item *item) {
    CFReleaseSafe(item->item);
    free(item);
}

static kc_item *kc_item_init_with_data() {

}

/* Encodes an item. */
static CFDataRef kc_item_encode(const kc_item *item) {
    CFDictionaryRef attrs = CFDictionaryCreate(0, (const void **)item->attrs,
                                               (const void **)item->values,
                                               item->n_attrs, 0, 0);
    CFDataRef encoded = CFPropertyListCreateData(0, attrs,
        kCFPropertyListBinaryFormat_v1_0, 0, 0);
    CFRelease(attrs);
    return encoded;
}

struct kc_item_set_attr {
    CFIndex ix;
    kc_item *item;
    OSStatus error;
};

static void kc_item_set_attr(CFStringRef key, void *value,
                             void *context) {
    struct kc_item_set_attr *c = context;
    if (CFGetTypeID(key) != CFStringGetTypeID()) {
        c->error = 1;
        return;
    }
    c->item->attrs[c->ix] = key;
    c->item->values[c->ix++] = value;
}

/* Returns a malloced item. */
static CFDictionaryRef kc_item_decode(CFDataRef encoded_item) {
    CFPropertyListFormat format;
    CFPropertyListRef attrs;
    attrs = CFPropertyListCreateWithData(0, encoded_item,
                                         kCFPropertyListImmutable, &format, 0);
    if (!attrs)
        return NULL;

    kc_item *item = NULL;
    if (CFGetTypeID(attrs) != CFDictionaryGetTypeID())
        goto errOut;

    item = kc_item_create(CFDictionaryGetCount(attrs));
    int failed = 0;
    CFDictionaryApplyFunction(attrs,
                              (CFDictionaryApplierFunction)kc_item_set_attr,
                              &failed);

errOut:
#if 0
    CFRelease(attrs);
    return item;
#else
    kc_item_destroy(item);
    return attrs;
#endif
}

#endif

/* Given plainText create and return a CFDataRef containing:
   BULK_KEY = RandomKey()
   version || keyclass || KeyStore_WRAP(keyclass, BULK_KEY) ||
    AES(BULK_KEY, NULL_IV, plainText || padding)
 */
static int ks_encrypt_data(keybag_handle_t keybag,
    keyclass_t keyclass, CFDataRef plainText, CFDataRef *pBlob) {
    CFMutableDataRef blob = NULL;
    //check(keybag >= 0);

    /* Precalculate output blob length. */
    const uint32_t bulkKeySize = 32; /* Use 256 bit AES key for bulkKey. */
    const uint32_t maxKeyWrapOverHead = 8 + 32;
    uint8_t bulkKey[bulkKeySize];
    uint8_t bulkKeyWrapped[bulkKeySize + maxKeyWrapOverHead];
    size_t bulkKeyWrappedSize = sizeof(bulkKeyWrapped);
    uint32_t key_wrapped_size;

	/* TODO: We should return a better error here. */
	int s3e = errSecAllocate;
    if (!plainText || CFGetTypeID(plainText) != CFDataGetTypeID()
        || keyclass == 0) {
        s3e = errSecParam;
        goto out;
    }

    size_t ptLen = CFDataGetLength(plainText);
    size_t ctLen = ptLen;
    size_t tagLen = 16;
    uint32_t version = 2;

    if (SecRandomCopyBytes(kSecRandomDefault, bulkKeySize, bulkKey))
        goto out;

    /* Now that we're done using the bulkKey, in place encrypt it. */
    require_noerr_quiet(s3e = ks_crypt(kAppleKeyStoreKeyWrap, keybag, keyclass,
                                       bulkKeySize, bulkKey, bulkKeyWrapped,
                                       &bulkKeyWrappedSize), out);
    key_wrapped_size = (uint32_t)bulkKeyWrappedSize;

    size_t blobLen = sizeof(version) + sizeof(keyclass) +
        sizeof(key_wrapped_size) + key_wrapped_size + ctLen + tagLen;

	require_quiet(blob = CFDataCreateMutable(NULL, blobLen), out);
    CFDataSetLength(blob, blobLen);
	UInt8 *cursor = CFDataGetMutableBytePtr(blob);

    *((uint32_t *)cursor) = version;
    cursor += sizeof(version);

    *((keyclass_t *)cursor) = keyclass;
    cursor += sizeof(keyclass);

    *((uint32_t *)cursor) = key_wrapped_size;
    cursor += sizeof(key_wrapped_size);

    memcpy(cursor, bulkKeyWrapped, key_wrapped_size);
    cursor += key_wrapped_size;

    /* Encrypt the plainText with the bulkKey. */
    CCCryptorStatus ccerr = CCCryptorGCM(kCCEncrypt, kCCAlgorithmAES128,
                                         bulkKey, bulkKeySize,
                                         NULL, 0,  /* iv */
                                         NULL, 0,  /* auth data */
                                         CFDataGetBytePtr(plainText), ptLen,
                                         cursor,
                                         cursor + ctLen, &tagLen);
    if (ccerr) {
        asl_log(NULL, NULL, ASL_LEVEL_ERR, "CCCryptorGCM failed: %d", ccerr);
        s3e = errSecInternal;
        goto out;
    }
    if (tagLen != 16) {
        asl_log(NULL, NULL, ASL_LEVEL_ERR,
                "CCCryptorGCM expected: 16 got: %ld byte tag", tagLen);
        s3e = errSecInternal;
        goto out;
    }

out:
    memset(bulkKey, 0, sizeof(bulkKey));
	if (s3e) {
		CFReleaseSafe(blob);
	} else {
		*pBlob = blob;
	}
	return s3e;
}

/* Given cipherText containing:
   version || keyclass || KeyStore_WRAP(keyclass, BULK_KEY) ||
    AES(BULK_KEY, NULL_IV, plainText || padding)
   return the plainText. */
static int ks_decrypt_data(keybag_handle_t keybag,
    keyclass_t *pkeyclass, CFDataRef blob, CFDataRef *pPlainText,
    uint32_t *version_p) {
    const uint32_t bulkKeySize = 32; /* Use 256 bit AES key for bulkKey. */
    uint8_t bulkKey[bulkKeySize];
    size_t bulkKeyCapacity = sizeof(bulkKey);

    CFMutableDataRef plainText = NULL;
    check(keybag >= 0);

	int s3e = errSecDecode;
    if (!blob) {
        /* TODO: We should return a better error here. */
        s3e = errSecParam;
        goto out;
    }

    size_t blobLen = CFDataGetLength(blob);
    const uint8_t *cursor = CFDataGetBytePtr(blob);
    uint32_t version;
    keyclass_t keyclass;
    uint32_t wrapped_key_size;

    /* Check for underflow, ensuring we have at least one full AES block left. */
    if (blobLen < sizeof(version) + sizeof(keyclass) +
        bulkKeySize + v0KeyWrapOverHead + 16)
        goto out;

    version = *((uint32_t *)cursor);
    cursor += sizeof(version);

    keyclass = *((keyclass_t *)cursor);
    if (pkeyclass)
        *pkeyclass = keyclass;
    cursor += sizeof(keyclass);

    size_t minimum_blob_len = sizeof(version) + sizeof(keyclass) + 16;
    size_t ctLen = blobLen - sizeof(version) - sizeof(keyclass);
    size_t tagLen = 0;
    switch (version) {
        case 0:
            wrapped_key_size = bulkKeySize + v0KeyWrapOverHead;
            break;
        case 2:
            tagLen = 16;
            minimum_blob_len -= 16; // Remove PKCS7 padding block requirement
            ctLen -= tagLen;        // Remove tagLen from ctLen
            /* DROPTHROUGH */
        case 1:
            wrapped_key_size = *((uint32_t *)cursor);
            cursor += sizeof(wrapped_key_size);
            minimum_blob_len += sizeof(wrapped_key_size);
            ctLen -= sizeof(wrapped_key_size);
            break;
        default:
            goto out;
    }

    /* Validate key wrap length against total length */
    require(blobLen - minimum_blob_len - tagLen >= wrapped_key_size, out);
    ctLen -= wrapped_key_size;
    if (version < 2 && (ctLen & 0xF) != 0)
        goto out;

    /* Now unwrap the bulk key using a key in the keybag. */
    require_noerr_quiet(s3e = ks_crypt(kAppleKeyStoreKeyUnwrap, keybag,
        keyclass, wrapped_key_size, cursor, bulkKey, &bulkKeyCapacity), out);
    cursor += wrapped_key_size;

    plainText = CFDataCreateMutable(NULL, ctLen);
    if (!plainText)
        goto out;
    CFDataSetLength(plainText, ctLen);

    /* Decrypt the cipherText with the bulkKey. */
    CCCryptorStatus ccerr;
    if (tagLen) {
        uint8_t tag[tagLen];
        ccerr = CCCryptorGCM(kCCDecrypt, kCCAlgorithmAES128,
                             bulkKey, bulkKeySize,
                             NULL, 0,  /* iv */
                             NULL, 0,  /* auth data */
                             cursor, ctLen,
                             CFDataGetMutableBytePtr(plainText),
                             tag, &tagLen);
        if (ccerr) {
            secerror("CCCryptorGCM failed: %d", ccerr);
            /* TODO: Should this be errSecDecode once AppleKeyStore correctly
             identifies uuid unwrap failures? */
            s3e = errSecDecode; /* errSecInteractionNotAllowed; */
            goto out;
        }
        if (tagLen != 16) {
            secerror("CCCryptorGCM expected: 16 got: %ld byte tag", tagLen);
            s3e = errSecInternal;
            goto out;
        }
        cursor += ctLen;
        if (memcmp(tag, cursor, tagLen)) {
            secerror("CCCryptorGCM computed tag not same as tag in blob");
            s3e = errSecDecode;
            goto out;
        }
    } else {
        size_t ptLen;
        ccerr = CCCrypt(kCCDecrypt, kCCAlgorithmAES128, kCCOptionPKCS7Padding,
                        bulkKey, bulkKeySize, NULL, cursor, ctLen,
                        CFDataGetMutableBytePtr(plainText), ctLen, &ptLen);
        if (ccerr) {
            secerror("CCCrypt failed: %d", ccerr);
            /* TODO: Should this be errSecDecode once AppleKeyStore correctly
               identifies uuid unwrap failures? */
            s3e = errSecDecode; /* errSecInteractionNotAllowed; */
            goto out;
        }
        CFDataSetLength(plainText, ptLen);
    }
	s3e = errSecSuccess;
    if (version_p) *version_p = version;
out:
    memset(bulkKey, 0, bulkKeySize);
	if (s3e) {
		CFReleaseSafe(plainText);
	} else {
		*pPlainText = plainText;
	}
    return s3e;
}

/* Iff dir_encrypt is true dir_encrypt source -> dest, otherwise decrypt
   source -> dest.  In both cases iv is used asa the Initialization Vector and
   textLength bytes are encrypted or decrypted.   TextLength must be a multiple
   of 16, since this function does not do any padding. */
static bool kc_aes_crypt(s3dl_db_thread *dbt, bool dir_encrypt, const UInt8 *iv,
	UInt32 textLength, const UInt8 *source, UInt8 *dest) {
#if USE_HWAES
	if (dbt->db->use_hwaes) {
		IOAESOperation operation;
		UInt8 *plainText;
		UInt8 *cipherText;

		if (dir_encrypt) {
			operation = IOAESOperationEncrypt;
			plainText = (UInt8 *)source;
			cipherText = dest;
		} else {
			operation = IOAESOperationDecrypt;
			plainText = dest;
			cipherText = (UInt8 *)source;
		}

		return hwaes_crypt(operation,
			kIOAESAcceleratorKeyHandleKeychain, 128, NULL, iv, textLength,
			plainText, cipherText);
	}
	else
#endif /* USE_HWAES */
	{
		/* The no encryption case. */
		memcpy(dest, source, textLength);
		return true;
	}
}

#if 0
/* Pre 4.0 blob encryption code, unused when keystore support is enabled. */

/* Given plainText create and return a CFDataRef containing:
   IV || AES(KC_KEY, IV, plainText || SHA1(plainText) || padding)
 */
static int kc_encrypt_data(s3dl_db_thread *dbt, CFDataRef plainText,
	CFDataRef *pCipherText) {
    CFMutableDataRef cipherText = NULL;
	/* TODO: We should return a better error here. */
	int s3e = SQLITE_AUTH;
    if (!plainText || CFGetTypeID(plainText) != CFDataGetTypeID()) {
        s3e = SQLITE_MISMATCH;
        goto out;
    }

    CFIndex ptLen = CFDataGetLength(plainText);
	CFIndex ctLen = ptLen + CC_SHA1_DIGEST_LENGTH;
	CFIndex padLen = 16 - (ctLen & 15);
	/* Pad output buffer capacity to nearest multiple of 32 bytes for cache
	   coherency. */
    CFIndex paddedTotalLength = (16 + ctLen + padLen + 0x1F) & ~0x1F;
	cipherText = CFDataCreateMutable(NULL, paddedTotalLength);
    if (!cipherText)
        goto out;
    CFDataSetLength(cipherText, 16 + ctLen + padLen);
	UInt8 *iv = CFDataGetMutableBytePtr(cipherText);
    if (SecRandomCopyBytes(kSecRandomDefault, 16, iv))
        goto out;

	UInt8 *ct = iv + 16;
    const UInt8 *pt = CFDataGetBytePtr(plainText);
	memcpy(ct, pt, ptLen);
	CC_SHA1(pt, ptLen, ct + ptLen);
	memset(ct + ctLen, padLen, padLen);

	/* Encrypt the data in place. */
    if (!kc_aes_crypt(dbt, true, iv, ctLen + padLen, ct, ct)) {
        goto out;
    }

	s3e = SQLITE_OK;
out:
	if (s3e) {
		CFReleaseSafe(cipherText);
	} else {
		*pCipherText = cipherText;
	}
	return s3e;
}
#endif

/* Given cipherText containing:
   IV || AES(KC_KEY, IV, plainText || SHA1(plainText) || padding)
   return the plainText. */
static int kc_decrypt_data(s3dl_db_thread *dbt, CFDataRef cipherText,
	CFDataRef *pPlainText) {
    CFMutableDataRef plainText = NULL;
	/* TODO: We should return a better error here. */
	int s3e = SQLITE_AUTH;
    if (!cipherText)
        goto out;

    CFIndex ctLen = CFDataGetLength(cipherText);
    if (ctLen < 48 || (ctLen & 0xF) != 0)
        goto out;

    const UInt8 *iv = CFDataGetBytePtr(cipherText);
    const UInt8 *ct = iv + 16;
    CFIndex ptLen = ctLen - 16;

    /* Cast: debug check for overflow before casting to uint32_t later */
    assert((unsigned long)ptLen<UINT32_MAX); /* correct as long as CFIndex is signed long */

	/* Pad output buffer capacity to nearest multiple of 32 bytes for cache
	   coherency. */
    CFIndex paddedLength = (ptLen + 0x1F) & ~0x1F;
    plainText = CFDataCreateMutable(NULL, paddedLength);
    if (!plainText)
        goto out;
    CFDataSetLength(plainText, ptLen);
    UInt8 *pt = CFDataGetMutableBytePtr(plainText);

    /* 64 bits case: Worst case here is we dont decrypt the full data. No security issue */
    if (!kc_aes_crypt(dbt, false, iv, (uint32_t)ptLen, ct, pt)) {
        goto out;
    }

	/* Now check and remove the padding. */
	UInt8 pad = pt[ptLen - 1];
	if (pad < 1 || pad > 16) {
		asl_log(NULL, NULL, ASL_LEVEL_ERR,
            "kc_decrypt_data: bad padding bytecount: 0x%02X", pad);
		goto out;
	}
	CFIndex ix;
	ptLen -= pad;
	for (ix = 0; ix < pad - 1; ++ix) {
		if (pt[ptLen + ix] != pad) {
            asl_log(NULL, NULL, ASL_LEVEL_ERR,
                "kc_decrypt_data: bad padding byte: %lu: 0x%02X",
				ix, pt[ptLen + ix]);
			goto out;
		}
	}

	UInt8 sha1[CC_SHA1_DIGEST_LENGTH];
	ptLen -= CC_SHA1_DIGEST_LENGTH;
    /* 64 bits cast: worst case here is we dont hash the full data and the decrypt fail or
       suceed when it should not have. No security issue. */
	CC_SHA1(pt, (CC_LONG)ptLen, sha1);
	if (memcmp(sha1, pt + ptLen, CC_SHA1_DIGEST_LENGTH)) {
		asl_log(NULL, NULL, ASL_LEVEL_ERR, "kc_decrypt_data: digest mismatch");
		goto out;
	}

	CFDataSetLength(plainText, ptLen);

	s3e = SQLITE_OK;
out:
	if (s3e) {
		CFReleaseSafe(plainText);
	} else {
		*pPlainText = plainText;
	}
    return s3e;
}

/* AUDIT[securityd](done):
   value (ok) is a caller provided, non NULL CFTypeRef.
 */
static int kc_bind_paramter(sqlite3_stmt *stmt, int param, CFTypeRef value)
{
    CFTypeID valueId;
    int s3e;

	/* TODO: Can we use SQLITE_STATIC below everwhere we currently use
	   SQLITE_TRANSIENT since we finalize the statement before the value
	   goes out of scope? */
    if (!value || (valueId = CFGetTypeID(value)) == CFNullGetTypeID()) {
        /* Skip bindings for NULL values.  sqlite3 will interpret unbound
           params as NULL which is exactly what we want. */
#if 1
		s3e = SQLITE_OK;
#else
		s3e = sqlite3_bind_null(stmt, param);
#endif
		secdebug("bind", "bind_null: %d", s3e);
    } else if (valueId == CFStringGetTypeID()) {
        const char *cstr = CFStringGetCStringPtr(value, kCFStringEncodingUTF8);
        if (cstr) {
            s3e = sqlite3_bind_text_wrapper(stmt, param, cstr, strlen(cstr),
                SQLITE_TRANSIENT);
			secdebug("bind", "quick bind_text: %s: %d", cstr, s3e);
        } else {
            CFIndex len = 0;
            CFRange range = { 0, CFStringGetLength(value) };
            CFStringGetBytes(value, range, kCFStringEncodingUTF8,
                0, FALSE, NULL, 0, &len);
            {
                CFIndex usedlen = 0;
                char buf[len];
                CFStringGetBytes(value, range, kCFStringEncodingUTF8,
                    0, FALSE, (UInt8 *)buf, len, &usedlen);
                s3e = sqlite3_bind_text_wrapper(stmt, param, buf, usedlen,
                    SQLITE_TRANSIENT);
				secdebug("bind", "slow bind_text: %.*s: %d", usedlen, buf, s3e);
            }
        }
    } else if (valueId == CFDataGetTypeID()) {
        CFIndex len = CFDataGetLength(value);
        if (len) {
            s3e = sqlite3_bind_blob_wrapper(stmt, param, CFDataGetBytePtr(value),
                len, SQLITE_TRANSIENT);
            secdebug("bind", "bind_blob: %.*s: %d",
                CFDataGetLength(value), CFDataGetBytePtr(value), s3e);
        } else {
            s3e = sqlite3_bind_text(stmt, param, "", 0, SQLITE_TRANSIENT);
        }
    } else if (valueId == CFDateGetTypeID()) {
        CFAbsoluteTime abs_time = CFDateGetAbsoluteTime(value);
        s3e = sqlite3_bind_double(stmt, param, abs_time);
		secdebug("bind", "bind_double: %f: %d", abs_time, s3e);
    } else if (valueId == CFBooleanGetTypeID()) {
        int bval = CFBooleanGetValue(value);
        s3e = sqlite3_bind_int(stmt, param, bval);
        secdebug("bind", "bind_int: %d: %d", bval, s3e);
    } else if (valueId == CFNumberGetTypeID()) {
        Boolean convertOk;
        if (CFNumberIsFloatType(value)) {
            double nval;
            convertOk = CFNumberGetValue(value, kCFNumberDoubleType, &nval);
            s3e = sqlite3_bind_double(stmt, param, nval);
			secdebug("bind", "bind_double: %f: %d", nval, s3e);
        } else {
            CFIndex len = CFNumberGetByteSize(value);
            /* TODO: should sizeof int be 4?  sqlite seems to think so. */
            if (len <= (CFIndex)sizeof(int)) {
                int nval;
                convertOk = CFNumberGetValue(value, kCFNumberIntType, &nval);
                s3e = sqlite3_bind_int(stmt, param, nval);
				secdebug("bind", "bind_int: %d: %d", nval, s3e);
            } else {
                sqlite_int64 nval;
                convertOk = CFNumberGetValue(value, kCFNumberLongLongType,
                    &nval);
                s3e = sqlite3_bind_int64(stmt, param, nval);
				secdebug("bind", "bind_int64: %lld: %d", nval, s3e);
            }
        }
        if (!convertOk) {
            /* TODO: CFNumberGetValue failed somehow. */
            s3e = SQLITE_INTERNAL;
        }
    } else {
        /* Unsupported CF type used. */
        s3e = SQLITE_MISMATCH;
    }

	return s3e;
}

/* Compile the statement in sql and return it as stmt. */
static int kc_prepare_statement(sqlite3 *s3h, CFStringRef sql,
    sqlite3_stmt **stmt)
{
    int s3e;
    const char *cstr = CFStringGetCStringPtr(sql, kCFStringEncodingUTF8);
    if (cstr) {
        secdebug("sql", "quick prepare: %s", cstr);
        s3e = sqlite3_prepare_wrapper(s3h, cstr, strlen(cstr), stmt, NULL);
    } else {
        CFIndex len = 0;
        CFRange range = { 0, CFStringGetLength(sql) };
        CFStringGetBytes(sql, range, kCFStringEncodingUTF8,
            0, FALSE, NULL, 0, &len);
        {
            CFIndex usedlen = 0;
            char buf[len];
            CFStringGetBytes(sql, range, kCFStringEncodingUTF8,
                0, FALSE, (UInt8 *)buf, len, &usedlen);
            secdebug("sql", "slow prepare: %.*s", usedlen, buf);
            s3e = sqlite3_prepare_wrapper(s3h, buf, usedlen, stmt, NULL);
        }
    }

    /* sqlite3_prepare returns SQLITE_ERROR if the table doesn't exist or one
       of the attributes the caller passed in doesn't exist.  */
    if (s3e == SQLITE_ERROR) {
        secdebug("sql", "sqlite3_prepare: %s", sqlite3_errmsg(s3h));
        s3e = errSecParam;
    }

    return s3e;
}

/* Return types. */
typedef uint32_t ReturnTypeMask;
enum
{
    kSecReturnDataMask = 1 << 0,
    kSecReturnAttributesMask = 1 << 1,
    kSecReturnRefMask = 1 << 2,
    kSecReturnPersistentRefMask = 1 << 3,
};

/* Constant indicating there is no limit to the number of results to return. */
enum
{
    kSecMatchUnlimited = kCFNotFound
};

/* Upper limit for number of keys in a QUERY dictionary. */
#ifdef NO_SERVER
#define QUERY_KEY_LIMIT  (31 + 53)
#else
#define QUERY_KEY_LIMIT  (53)
#endif

typedef struct Pair
{
    const void *key;
    const void *value;
} Pair;

/* Nothing in this struct is retained since all the
   values below are extracted from the dictionary passed in by the
   caller. */
typedef struct Query
{
    /* Class of this query. */
    const kc_class *q_class;

    /* Dictionary with all attributes and values in clear (to be encrypted). */
    CFMutableDictionaryRef q_item;

    /* q_pairs is an array of Pair structs.  Elements with indices
     [0, q_attr_end) contain attribute key value pairs.  Elements with
     indices [q_match_begin, q_match_end) contain match key value pairs.
     Thus q_attr_end is the number of attrs in q_pairs and
     q_match_begin - q_match_end is the number of matches in q_pairs.  */
    CFIndex q_match_begin;
    CFIndex q_match_end;
    CFIndex q_attr_end;

    OSStatus q_error;
    ReturnTypeMask q_return_type;

    CFDataRef q_data;
    CFTypeRef q_ref;
    sqlite_int64 q_row_id;

    CFArrayRef q_use_item_list;
#if defined(MULTIPLE_KEYCHAINS)
    CFArrayRef q_use_keychain;
    CFArrayRef q_use_keychain_list;
#endif /* !defined(MULTIPLE_KEYCHAINS) */

    /* Value of kSecMatchLimit key if present. */
    CFIndex q_limit;

    /* Keybag handle to use for this item. */
    keybag_handle_t q_keybag;
    keyclass_t q_keyclass;
    //CFStringRef q_keyclass_s;

    Pair q_pairs[];
} Query;

/* Inline accessors to attr and match values in a query. */
static inline CFIndex query_attr_count(const Query *q)
{
    return q->q_attr_end;
}

static inline Pair query_attr_at(const Query *q, CFIndex ix)
{
    return q->q_pairs[ix];
}

static inline CFIndex query_match_count(const Query *q)
{
    return q->q_match_end - q->q_match_begin;
}

static inline Pair query_match_at(const Query *q, CFIndex ix)
{
    return q->q_pairs[q->q_match_begin + ix];
}

/* Private routines used to parse a query. */

/* Sets q_keyclass based on value. */
static void query_parse_keyclass(const void *value, Query *q) {
    if (!isString(value)) {
        q->q_error = errSecParam;
        return;
    } else if (CFEqual(value, kSecAttrAccessibleWhenUnlocked)) {
        q->q_keyclass = key_class_ak;
    } else if (CFEqual(value, kSecAttrAccessibleAfterFirstUnlock)) {
        q->q_keyclass = key_class_ck;
    } else if (CFEqual(value, kSecAttrAccessibleAlways)) {
        q->q_keyclass = key_class_dk;
    } else if (CFEqual(value, kSecAttrAccessibleWhenUnlockedThisDeviceOnly)) {
        q->q_keyclass = key_class_aku;
    } else if (CFEqual(value, kSecAttrAccessibleAfterFirstUnlockThisDeviceOnly)) {
        q->q_keyclass = key_class_cku;
    } else if (CFEqual(value, kSecAttrAccessibleAlwaysThisDeviceOnly)) {
        q->q_keyclass = key_class_dku;
    } else {
        q->q_error = errSecParam;
        return;
    }
    //q->q_keyclass_s = value;
}

static CFStringRef copyString(CFTypeRef obj) {
    CFTypeID tid = CFGetTypeID(obj);
    if (tid == CFStringGetTypeID())
        return CFStringCreateCopy(0, obj);
    else if (tid == CFDataGetTypeID())
        return CFStringCreateFromExternalRepresentation(0, obj, kCFStringEncodingUTF8);
    else
        return NULL;
}

static CFDataRef copyData(CFTypeRef obj) {
    CFTypeID tid = CFGetTypeID(obj);
    if (tid == CFDataGetTypeID()) {
        return CFDataCreateCopy(0, obj);
    } else if (tid == CFStringGetTypeID()) {
        return CFStringCreateExternalRepresentation(0, obj, kCFStringEncodingUTF8, 0);
    } else if (tid == CFNumberGetTypeID()) {
        SInt32 value;
        CFNumberGetValue(obj, kCFNumberSInt32Type, &value);
        return CFDataCreate(0, (const UInt8 *)&value, sizeof(value));
    } else {
        return NULL;
    }
}

static CFTypeRef copyBlob(CFTypeRef obj) {
    CFTypeID tid = CFGetTypeID(obj);
    if (tid == CFDataGetTypeID()) {
        return CFDataCreateCopy(0, obj);
    } else if (tid == CFStringGetTypeID()) {
        return CFStringCreateCopy(0, obj);
    } else if (tid == CFNumberGetTypeID()) {
        CFRetain(obj);
        return obj;
    } else {
        return NULL;
    }
}

static CFTypeRef copyNumber(CFTypeRef obj) {
    CFTypeID tid = CFGetTypeID(obj);
    if (tid == CFNumberGetTypeID()) {
        CFRetain(obj);
        return obj;
    } else if (tid == CFBooleanGetTypeID()) {
        SInt32 value = CFBooleanGetValue(obj);
        return CFNumberCreate(0, kCFNumberSInt32Type, &value);
    } else if (tid == CFStringGetTypeID()) {
        SInt32 value = CFStringGetIntValue(obj);
        CFStringRef t = CFStringCreateWithFormat(0, 0, CFSTR("%ld"), value);
        /* If a string converted to an int isn't equal to the int printed as
           a string, return a CFStringRef instead. */
        if (!CFEqual(t, obj)) {
            CFRelease(t);
            return CFStringCreateCopy(0, obj);
        }
        CFRelease(t);
        return CFNumberCreate(0, kCFNumberSInt32Type, &value);
    } else
        return NULL;
}

static CFDateRef copyDate(CFTypeRef obj) {
    CFTypeID tid = CFGetTypeID(obj);
    if (tid == CFDateGetTypeID()) {
        CFRetain(obj);
        return obj;
    } else
        return NULL;
}

/* AUDIT[securityd](done):
   key (ok) is a caller provided, string or number of length 4.
   value (ok) is a caller provided, non NULL CFTypeRef.
 */
static void query_add_attribute(const void *key, const void *value, Query *q)
{
    const kc_attr_desc *desc;
    if (!(desc = kc_attr_desc_with_key(q->q_class, key, &q->q_error)))
        return;

    CFTypeRef attr = NULL;
    switch (desc->kind) {
        case kc_data_attr:
            attr = copyData(value);
            break;
        case kc_blob_attr:
            attr = copyBlob(value);
            break;
        case kc_date_attr:
        case kc_creation_date_attr:
        case kc_modification_date_attr:
            attr = copyDate(value);
            break;
        case kc_number_attr:
            attr = copyNumber(value);
            break;
        case kc_string_attr:
            attr = copyString(value);
            break;
    }

    if (!attr) {
        q->q_error = errSecItemInvalidValue;
        return;
    }

    /* Store plaintext attr data in q_item. */
    if (q->q_item) {
        CFDictionarySetValue(q->q_item, desc->name, attr);
    }

    if (CFEqual(desc->name, kSecAttrAccessible)) {
        query_parse_keyclass(attr, q);
    }

    /* Convert attr to (sha1) digest if requested. */
    if (desc->flags & kc_digest_attr) {
        CFDataRef data = copyData(attr);
        CFRelease(attr);
        if (!data) {
            q->q_error = errSecInternal;
            return;
        }

        CFMutableDataRef digest = CFDataCreateMutable(0, CC_SHA1_DIGEST_LENGTH);
        CFDataSetLength(digest, CC_SHA1_DIGEST_LENGTH);
        /* 64 bits cast: worst case is we generate the wrong hash */
        assert((unsigned long)CFDataGetLength(data)<UINT32_MAX); /* Debug check. Correct as long as CFIndex is long */
        CC_SHA1(CFDataGetBytePtr(data), (CC_LONG)CFDataGetLength(data),
                CFDataGetMutableBytePtr(digest));
        CFRelease(data);
        attr = digest;
    }

    /* Record the new attr key, value in q_pairs. */
    q->q_pairs[q->q_attr_end].key = desc->name;
    q->q_pairs[q->q_attr_end++].value = attr;
}

/* First remove key from q->q_pairs if it's present, then add the attribute again. */
static void query_set_attribute(const void *key, const void *value, Query *q) {
    if (CFDictionaryContainsKey(q->q_item, key)) {
        CFIndex ix;
        for (ix = 0; ix < q->q_attr_end; ++ix) {
            if (CFEqual(key, q->q_pairs[ix].key)) {
                CFReleaseSafe(q->q_pairs[ix].value);
                --q->q_attr_end;
                for (; ix < q->q_attr_end; ++ix) {
                    q->q_pairs[ix] = q->q_pairs[ix + 1];
                }
                CFDictionaryRemoveValue(q->q_item, key);
                break;
            }
        }
    }
    query_add_attribute(key, value, q);
}

/* AUDIT[securityd](done):
   key (ok) is a caller provided, string starting with 'm'.
   value (ok) is a caller provided, non NULL CFTypeRef.
 */
static void query_add_match(const void *key, const void *value, Query *q)
{
    /* Record the match key, value in q_pairs. */
    --(q->q_match_begin);
    q->q_pairs[q->q_match_begin].key = key;
    q->q_pairs[q->q_match_begin].value = value;

    if (CFEqual(kSecMatchLimit, key)) {
        /* Figure out what the value for kSecMatchLimit is if specified. */
        if (CFGetTypeID(value) == CFNumberGetTypeID()) {
            if (!CFNumberGetValue(value, kCFNumberCFIndexType, &q->q_limit))
                q->q_error = errSecItemInvalidValue;
        } else if (CFEqual(kSecMatchLimitAll, value)) {
            q->q_limit = kSecMatchUnlimited;
        } else if (CFEqual(kSecMatchLimitOne, value)) {
            q->q_limit = 1;
        } else {
            q->q_error = errSecItemInvalidValue;
        }
    }
}

static bool query_set_class(Query *q, CFStringRef c_name, OSStatus *error) {
    const void *value;
    if (c_name && CFGetTypeID(c_name) == CFStringGetTypeID() &&
        (value = CFDictionaryGetValue(gClasses, c_name)) &&
        (q->q_class == 0 || q->q_class == value)) {
        q->q_class = value;
        return true;
    }

    if (error && !*error)
        *error = c_name ? errSecNoSuchClass : errSecItemClassMissing;

    return false;
}

static const kc_class *query_get_class(CFDictionaryRef query, OSStatus *error) {
    CFStringRef c_name = NULL;
    const void *value = CFDictionaryGetValue(query, kSecClass);
    if (isString(value)) {
        c_name = value;
    } else {
        value = CFDictionaryGetValue(query, kSecValuePersistentRef);
        if (isData(value)) {
            CFDataRef pref = value;
            _SecItemParsePersistentRef(pref, &c_name, 0);
        }
    }

    if (c_name && (value = CFDictionaryGetValue(gClasses, c_name))) {
        return value;
    } else {
        if (error && !*error)
            *error = c_name ? errSecNoSuchClass : errSecItemClassMissing;
        return false;
    }
}

/* AUDIT[securityd](done):
   key (ok) is a caller provided, string starting with 'c'.
   value (ok) is a caller provided, non NULL CFTypeRef.
 */
static void query_add_class(const void *key, const void *value, Query *q)
{
    if (CFEqual(key, kSecClass)) {
        query_set_class(q, value, &q->q_error);
    } else {
        q->q_error = errSecItemInvalidKey;
    }
}

/* AUDIT[securityd](done):
   key (ok) is a caller provided, string starting with 'r'.
   value (ok) is a caller provided, non NULL CFTypeRef.
 */
static void query_add_return(const void *key, const void *value, Query *q)
{
    ReturnTypeMask mask;
    if (CFGetTypeID(value) != CFBooleanGetTypeID()) {
        q->q_error = errSecItemInvalidValue;
        return;
    }

    int set_it = CFEqual(value, kCFBooleanTrue);

    if (CFEqual(key, kSecReturnData))
        mask = kSecReturnDataMask;
    else if (CFEqual(key, kSecReturnAttributes))
        mask = kSecReturnAttributesMask;
    else if (CFEqual(key, kSecReturnRef))
        mask = kSecReturnRefMask;
    else if (CFEqual(key, kSecReturnPersistentRef))
        mask = kSecReturnPersistentRefMask;
    else {
        q->q_error = errSecItemInvalidKey;
        return;
    }

    if ((q->q_return_type & mask) && !set_it) {
        /* Clear out this bit (it's set so xor with the mask will clear it). */
        q->q_return_type ^= mask;
    } else if (!(q->q_return_type & mask) && set_it) {
        /* Set this bit. */
        q->q_return_type |= mask;
    }
}

/* AUDIT[securityd](done):
   key (ok) is a caller provided, string starting with 'u'.
   value (ok since q_use_item_list is unused) is a caller provided, non
       NULL CFTypeRef.
 */
static void query_add_use(const void *key, const void *value, Query *q)
{
    if (CFEqual(key, kSecUseItemList)) {
        /* TODO: Add sanity checking when we start using this. */
        q->q_use_item_list = value;
    }
#if defined(MULTIPLE_KEYCHAINS)
    else if (CFEqual(key, kSecUseKeychain))
        q->q_use_keychain = value;
    else if (CFEqual(key, kSecUseKeychainList))
        q->q_use_keychain_list = value;
#endif /* !defined(MULTIPLE_KEYCHAINS) */
    else {
        q->q_error = errSecItemInvalidKey;
        return;
    }
}

static void query_set_data(const void *value, Query *q) {
    if (!isData(value)) {
        q->q_error = errSecItemInvalidValue;
    } else {
        q->q_data = value;
        if (q->q_item)
            CFDictionarySetValue(q->q_item, kSecValueData, value);
    }
}

/* AUDIT[securityd](done):
   key (ok) is a caller provided, string starting with 'u'.
   value (ok) is a caller provided, non NULL CFTypeRef.
 */
static void query_add_value(const void *key, const void *value, Query *q)
{
    if (CFEqual(key, kSecValueData)) {
        query_set_data(value, q);
#if NO_SERVER
    } else if (CFEqual(key, kSecValueRef)) {
        q->q_ref = value;
        /* TODO: Add value type sanity checking. */
#endif
    } else if (CFEqual(key, kSecValuePersistentRef)) {
        CFStringRef c_name;
        if (_SecItemParsePersistentRef(value, &c_name, &q->q_row_id))
            query_set_class(q, c_name, &q->q_error);
        else
            q->q_error = errSecItemInvalidValue;
    } else {
        q->q_error = errSecItemInvalidKey;
        return;
    }
}

/* AUDIT[securityd](done):
   key (ok) is a caller provided, unchecked.
   value (ok) is a caller provided, unchecked.
 */
static void query_update_applier(const void *key, const void *value,
    void *context)
{
    Query *q = (Query *)context;
    /* If something went wrong there is no point processing any more args. */
    if (q->q_error)
        return;

    /* Make sure we have a string key. */
    if (!isString(key)) {
        q->q_error = errSecItemInvalidKeyType;
        return;
    }

    if (!value) {
        q->q_error = errSecItemInvalidValue;
        return;
    }

    if (CFEqual(key, kSecValueData)) {
        query_set_data(value, q);
    } else {
        /* Make sure we have a value. */
        if (!value) {
            q->q_error = errSecItemInvalidValue;
            return;
        }

        query_add_attribute(key, value, q);
    }
}

/* AUDIT[securityd](done):
   key (ok) is a caller provided, unchecked.
   value (ok) is a caller provided, unchecked.
 */
static void query_applier(const void *key, const void *value, void *context)
{
    Query *q = (Query *)context;
    /* If something went wrong there is no point processing any more args. */
    if (q->q_error)
        return;

    /* Make sure we have a key. */
    if (!key) {
        q->q_error = errSecItemInvalidKeyType;
        return;
    }

    /* Make sure we have a value. */
    if (!value) {
        q->q_error = errSecItemInvalidValue;
        return;
    }

    /* Figure out what type of key we are dealing with. */
    CFTypeID key_id = CFGetTypeID(key);
    if (key_id == CFStringGetTypeID()) {
        CFIndex key_len = CFStringGetLength(key);
        /* String keys can be different things.  The subtype is determined by:
           length 4 strings are all attributes.  Otherwise the first char
           determines the type:
           c: class must be kSecClass
           m: match like kSecMatchPolicy
           r: return like kSecReturnData
           u: use keys
           v: value
         */
        if (key_len == 4) {
            /* attributes */
            query_add_attribute(key, value, q);
        } else if (key_len > 1) {
            UniChar k_first_char = CFStringGetCharacterAtIndex(key, 0);
            switch (k_first_char)
            {
            case 'c': /* class */
                query_add_class(key, value, q);
                break;
            case 'm': /* match */
                query_add_match(key, value, q);
                break;
            case 'r': /* return */
                query_add_return(key, value, q);
                break;
            case 'u': /* use */
                query_add_use(key, value, q);
                break;
            case 'v': /* value */
                query_add_value(key, value, q);
                break;
            default:
                q->q_error = errSecItemInvalidKey;
                break;
            }
        } else {
            q->q_error = errSecItemInvalidKey;
        }
    } else if (key_id == CFNumberGetTypeID()) {
        /* Numeric keys are always (extended) attributes. */
        /* TODO: Why is this here? query_add_attribute() doesn't take numbers. */
        query_add_attribute(key, value, q);
    } else {
        /* We only support string and number type keys. */
        q->q_error = errSecItemInvalidKeyType;
    }
}

static CFStringRef query_infer_keyclass(Query *q, CFStringRef agrp) {
    /* apsd and lockdown are always dku. */
    if (CFEqual(agrp, CFSTR("com.apple.apsd"))
        || CFEqual(agrp, CFSTR("lockdown-identities"))) {
        return kSecAttrAccessibleAlwaysThisDeviceOnly;
    }
    /* All other certs or in the apple agrp is dk. */
    if (q->q_class == &cert_class) {
        /* third party certs are always dk. */
        return kSecAttrAccessibleAlways;
    }
    /* The rest defaults to ak. */
    return kSecAttrAccessibleWhenUnlocked;
}

static void query_ensure_keyclass(Query *q, CFStringRef agrp) {
    if (q->q_keyclass == 0) {
        CFStringRef accessible = query_infer_keyclass(q, agrp);
        query_add_attribute(kSecAttrAccessible, accessible, q);
    }
}

static void query_destroy(Query *q, OSStatus *status)
{
    if (status && *status == 0 && q->q_error)
        *status = q->q_error;

    CFIndex ix, attr_count = query_attr_count(q);
    for (ix = 0; ix < attr_count; ++ix) {
        CFReleaseSafe(query_attr_at(q, ix).value);
    }
    CFReleaseSafe(q->q_item);
    free(q);
}

/* Allocate and initialize a Query object for query. */
static Query *query_create(const kc_class *qclass, CFDictionaryRef query,
                           OSStatus *error)
{
    if (!qclass) {
        if (error && !*error)
            *error = errSecItemClassMissing;
        return NULL;
    }

    /* Number of pairs we need is the number of attributes in this class
       plus the number of keys in the dictionary, minus one for each key in
       the dictionary that is a regular attribute. */
    CFIndex key_count = qclass->n_attrs;
    if (query) {
        key_count += CFDictionaryGetCount(query);
        CFIndex ix;
        for (ix = 0; ix < qclass->n_attrs; ++ix) {
            if (CFDictionaryContainsKey(query, qclass->attrs[ix].name))
                --key_count;
        }
    }

    if (key_count > QUERY_KEY_LIMIT) {
        if (error && !*error)
            *error = errSecItemIllegalQuery;
        return NULL;
    }

    Query *q = calloc(1, sizeof(Query) + sizeof(Pair) * key_count);
    if (q == NULL) {
        if (error && !*error)
            *error = errSecAllocate;
        return NULL;
    }

    q->q_class = qclass;
    q->q_match_begin = q->q_match_end = key_count;

    return q;
}

/* Parse query for a Query object q. */
static bool query_parse_with_applier(Query *q, CFDictionaryRef query,
                                     CFDictionaryApplierFunction applier,
                                     OSStatus *error) {
    if (q->q_item == NULL) {
        q->q_item = CFDictionaryCreateMutable(0, 0,
                                              &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    }
    CFDictionaryApplyFunction(query, applier, q);
    if (q->q_error) {
        if (error && !*error)
            *error = q->q_error;
        return false;
    }
    return true;
}

/* Parse query for a Query object q. */
static bool query_parse(Query *q, CFDictionaryRef query,
                               OSStatus *error) {
    return query_parse_with_applier(q, query, query_applier, error);
}

/* Parse query for a Query object q. */
static bool query_update_parse(Query *q, CFDictionaryRef update,
                        OSStatus *error) {
    return query_parse_with_applier(q, update, query_update_applier, error);
}

static Query *query_create_with_limit(CFDictionaryRef query, CFIndex limit,
                                      OSStatus *error) {
    Query *q;
    q = query_create(query_get_class(query, error), query, error);
    if (q) {
        q->q_limit = limit;
        if (!query_parse(q, query, error)) {
            query_destroy(q, error);
            return NULL;
        }
    }
    return q;
}

/* Make sure all attributes that are marked as not_null have a value.  If
   force_date is false, only set mdat and cdat if they aren't already set. */
static void
query_pre_add(Query *q, bool force_date) {
    CFDateRef now = CFDateCreate(0, CFAbsoluteTimeGetCurrent());
    CFIndex ix;
    for (ix = 0; ix < q->q_class->n_attrs; ++ix) {
        const kc_attr_desc *desc = &q->q_class->attrs[ix];
        if (desc->kind == kc_creation_date_attr ||
            desc->kind == kc_modification_date_attr) {
            if (force_date) {
                query_set_attribute(desc->name, now, q);
            } else if (!CFDictionaryContainsKey(q->q_item, desc->name)) {
                query_add_attribute(desc->name, now, q);
            }
        } else if ((desc->flags & kc_constrain_not_null) &&
                   !CFDictionaryContainsKey(q->q_item, desc->name)) {
            CFTypeRef value = NULL;
            if (desc->flags & kc_constrain_default_0) {
                if (desc->kind == kc_date_attr)
                    value = CFDateCreate(kCFAllocatorDefault, 0.0);
                else {
                    SInt32 vzero = 0;
                    value = CFNumberCreate(0, kCFNumberSInt32Type, &vzero);
                }
            } else if (desc->flags & kc_constrain_default_empty) {
                if (desc->kind == kc_data_attr)
                    value = CFDataCreate(kCFAllocatorDefault, NULL, 0);
                else {
                    value = CFSTR("");
                    CFRetain(value);
                }
            }
            if (value) {
                /* Safe to use query_add_attribute here since the attr wasn't
                 set yet. */
                query_add_attribute(desc->name, value, q);
                CFRelease(value);
            }
        }
    }
    CFReleaseSafe(now);
}

/* Update modification_date if needed. */
static void
query_pre_update(Query *q) {
    CFIndex ix;
    for (ix = 0; ix < q->q_class->n_attrs; ++ix) {
        const kc_attr_desc *desc = &q->q_class->attrs[ix];
        if (desc->kind == kc_modification_date_attr) {
            CFDateRef now = CFDateCreate(0, CFAbsoluteTimeGetCurrent());
            query_set_attribute(desc->name, now, q);
            CFReleaseSafe(now);
        }
    }
}

/* AUDIT[securityd](done):
   accessGroup (ok) is a caller provided, non NULL CFTypeRef.

   Return true iff accessGroup is allowable according to accessGroups.
 */
static bool accessGroupsAllows(CFArrayRef accessGroups,
    CFStringRef accessGroup) {
    /* NULL accessGroups is wildcard. */
    if (!accessGroups)
        return true;
    /* Make sure we have a string. */
    if (!isString(accessGroup))
        return false;

    /* Having the special accessGroup "*" allows access to all accessGroups. */
    CFRange range = { 0, CFArrayGetCount(accessGroups) };
    if (range.length &&
        (CFArrayContainsValue(accessGroups, range, accessGroup) ||
         CFArrayContainsValue(accessGroups, range, CFSTR("*"))))
        return true;

    return false;
}

static bool itemInAccessGroup(CFDictionaryRef item, CFArrayRef accessGroups) {
    return accessGroupsAllows(accessGroups,
                              CFDictionaryGetValue(item, kSecAttrAccessGroup));
}

static void s3dl_merge_into_dict(const void *key, const void *value, void *context) {
    CFDictionarySetValue(context, key, value);
}

/* Return whatever the caller requested based on the value of q->q_return_type.
   keys and values must be 3 larger than attr_count in size to accomadate the
   optional data, class and persistant ref results.  This is so we can use
   the CFDictionaryCreate() api here rather than appending to a
   mutable dictionary. */
static CFTypeRef handle_result(Query *q, CFMutableDictionaryRef item,
                               sqlite_int64 rowid) {
    CFTypeRef a_result;
    CFDataRef data;
    data = CFDictionaryGetValue(item, kSecValueData);
	if (q->q_return_type == 0) {
		/* Caller isn't interested in any results at all. */
		a_result = kCFNull;
	} else if (q->q_return_type == kSecReturnDataMask) {
        if (data) {
            a_result = data;
            CFRetain(a_result);
        } else {
            a_result = CFDataCreate(kCFAllocatorDefault, NULL, 0);
        }
	} else if (q->q_return_type == kSecReturnPersistentRefMask) {
		a_result = _SecItemMakePersistentRef(q->q_class->name, rowid);
	} else {
		/* We need to return more than one value. */
        if (q->q_return_type & kSecReturnRefMask) {
            CFDictionarySetValue(item, kSecClass, q->q_class->name);
        } else if ((q->q_return_type & kSecReturnAttributesMask)) {
            if (!(q->q_return_type & kSecReturnDataMask)) {
                CFDictionaryRemoveValue(item, kSecValueData);
            }
        } else {
            if (data)
                CFRetain(data);
            CFDictionaryRemoveAllValues(item);
            if ((q->q_return_type & kSecReturnDataMask) && data) {
                CFDictionarySetValue(item, kSecValueData, data);
                CFRelease(data);
            }
        }
		if (q->q_return_type & kSecReturnPersistentRefMask) {
            CFDataRef pref = _SecItemMakePersistentRef(q->q_class->name, rowid);
			CFDictionarySetValue(item, kSecValuePersistentRef, pref);
            CFRelease(pref);
		}

		a_result = item;
        CFRetain(item);
	}

	return a_result;
}

static CFStringRef s3dl_insert_sql(Query *q) {
    /* We always have at least one attribute, the agrp. */
    CFMutableStringRef sql = CFStringCreateMutable(NULL, 0);
    CFStringAppendFormat(sql, NULL, CFSTR("INSERT INTO %@(data"), q->q_class->name);

    CFIndex ix, attr_count = query_attr_count(q);
    for (ix = 0; ix < attr_count; ++ix) {
        CFStringAppendFormat(sql, NULL, CFSTR(",%@"),
                             query_attr_at(q, ix).key);
    }
    if (q->q_row_id) {
        CFStringAppendFormat(sql, NULL, CFSTR(",rowid"));
    }

    CFStringAppend(sql, CFSTR(")VALUES(?"));

    for (ix = 0; ix < attr_count; ++ix) {
        CFStringAppend(sql, CFSTR(",?"));
	}
    if (q->q_row_id) {
        CFStringAppendFormat(sql, NULL, CFSTR(",%qd"), q->q_row_id);
    }
    CFStringAppend(sql, CFSTR(");"));
    return sql;
}

static CFDataRef s3dl_encode_item(keybag_handle_t keybag, keyclass_t keyclass,
                                  CFDictionaryRef item, OSStatus *error) {
    /* Encode to be encrypted item. */
    CFDataRef plain = CFPropertyListCreateData(0, item,
        kCFPropertyListBinaryFormat_v1_0, 0, 0);
    CFDataRef edata = NULL;
	if (plain) {
        int s3e = ks_encrypt_data(keybag, keyclass, plain, &edata);
        if (s3e) {
            asl_log(NULL, NULL, ASL_LEVEL_CRIT,
                    "ks_encrypt_data: failed: %d", s3e);
            if (error && !*error)
                *error = s3e;
        }
        CFRelease(plain);
    } else {
        if (error && !*error)
            *error = errSecAllocate;
    }
    return edata;
}

/* Bind the parameters to the INSERT statement. */
static int s3dl_insert_bind(Query *q, sqlite3_stmt *stmt) {
    int s3e = SQLITE_OK;
	int param = 1;
    OSStatus error = 0;
    CFDataRef edata = s3dl_encode_item(q->q_keybag, q->q_keyclass, q->q_item, &error);
	if (edata) {
        s3e = sqlite3_bind_blob_wrapper(stmt, param, CFDataGetBytePtr(edata),
                                CFDataGetLength(edata), SQLITE_TRANSIENT);
        secdebug("bind", "bind_blob: %.*s: %d",
                 CFDataGetLength(edata), CFDataGetBytePtr(edata), s3e);
        CFRelease(edata);
	} else {
        s3e = error;
    }
	param++;

    CFIndex ix, attr_count = query_attr_count(q);
    for (ix = 0; s3e == SQLITE_OK && ix < attr_count; ++ix) {
        s3e = kc_bind_paramter(stmt, param++, query_attr_at(q, ix).value);
	}

    return s3e;
}

/* AUDIT[securityd](done):
   attributes (ok) is a caller provided dictionary, only its cf type has
       been checked.
 */
static OSStatus
s3dl_query_add(s3dl_db_thread *dbt, Query *q, CFTypeRef *result)
{
    int s3e;

    if (query_match_count(q) != 0)
        return errSecItemMatchUnsupported;

    /* Add requires a class to be specified unless we are adding a ref. */
    if (q->q_use_item_list)
        return errSecUseItemListUnsupported;

	/* Actual work here. */
	sqlite3 *s3h = dbt->s3_handle;

    CFStringRef sql = s3dl_insert_sql(q);
	sqlite3_stmt *stmt = NULL;
    s3e = kc_prepare_statement(s3h, sql, &stmt);
	CFRelease(sql);

	if (s3e == SQLITE_OK)
        s3e = s3dl_insert_bind(q, stmt);

	/* Now execute the INSERT statement (step). */
	if (s3e == SQLITE_OK) {
		s3e = sqlite3_step(stmt);
		if (s3e == SQLITE_DONE) {
			s3e = SQLITE_OK;
            if (q->q_return_type) {
                *result = handle_result(q, q->q_item, sqlite3_last_insert_rowid(s3h));
            }
		} else if (s3e == SQLITE_ERROR) {
			secdebug("sql", "insert: %s", sqlite3_errmsg(s3h));
            /* The object already existed. */
			s3e = errSecDuplicateItem;
        }
	}

	/* Free the stmt. */
	if (stmt) {
		int s3e2 = sqlite3_finalize(stmt);
		if (s3e2 != SQLITE_OK && s3e == SQLITE_OK)
			s3e = s3e2;
	}

	return s3e == SQLITE_OK ? 0 : osstatus_for_s3e(s3e);
}

typedef void (*s3dl_handle_row)(sqlite3_stmt *stmt, void *context);

static CFMutableDictionaryRef
s3dl_item_from_col(sqlite3_stmt *stmt, Query *q, int col,
                   CFArrayRef accessGroups, keyclass_t *keyclass) {
    CFMutableDictionaryRef item = NULL;
    CFErrorRef error = NULL;
    CFDataRef edata = NULL;
    CFDataRef plain =  NULL;

    require_action(edata = CFDataCreateWithBytesNoCopy(0, sqlite3_column_blob(stmt, col),
                                                       sqlite3_column_bytes(stmt, col),
                                                       kCFAllocatorNull),
                   out, q->q_error = errSecDecode);

    /* Decrypt and decode the item and check the decoded attributes against the query. */
    uint32_t version;
    require_noerr((q->q_error = ks_decrypt_data(q->q_keybag, keyclass, edata, &plain, &version)), out);
    if (version < 2) {
        goto out;
    }

    CFPropertyListFormat format;
    item = (CFMutableDictionaryRef)CFPropertyListCreateWithData(0, plain,
        kCFPropertyListMutableContainers, &format, &error);
    if (!item) {
        secerror("decode failed: %@ [item: %@]", error, plain);
        q->q_error = (OSStatus)CFErrorGetCode(error); /* possibly truncated error codes: whatever */
        CFRelease(error);
    } else if (!isDictionary(item)) {
        CFRelease(item);
        item = NULL;
    } else if (!itemInAccessGroup(item, accessGroups)) {
        secerror("items accessGroup %@ not in %@",
                 CFDictionaryGetValue(item, kSecAttrAccessGroup),
                 accessGroups);
        CFRelease(item);
        item = NULL;
    }
    /* TODO: Validate keyclass attribute. */

out:
    CFReleaseSafe(edata);
    CFReleaseSafe(plain);
    return item;
}

struct s3dl_query_ctx {
    Query *q;
    CFArrayRef accessGroups;
    CFTypeRef result;
    int found;
};

static void s3dl_query_row(sqlite3_stmt *stmt, void *context) {
    struct s3dl_query_ctx *c = context;
    Query *q = c->q;

    CFMutableDictionaryRef item = s3dl_item_from_col(stmt, q, 1,
                                                     c->accessGroups, NULL);
    if (!item)
        return;

    if (q->q_class == &identity_class) {
        // TODO: Use col 2 for key rowid and use both rowids in persistant ref.
        CFMutableDictionaryRef key = s3dl_item_from_col(stmt, q, 3,
                                                        c->accessGroups, NULL);
        if (!key)
            goto out;

        CFDataRef certData = CFDictionaryGetValue(item, kSecValueData);
        if (certData) {
            CFDictionarySetValue(key, CFSTR(CERTIFICATE_DATA_COLUMN_LABEL),
                                 certData);
            CFDictionaryRemoveValue(item, kSecValueData);
        }
        CFDictionaryApplyFunction(item, s3dl_merge_into_dict, key);
        CFRelease(item);
        item = key;
    }

    sqlite_int64 rowid = sqlite3_column_int64(stmt, 0);
    CFTypeRef a_result = handle_result(q, item, rowid);
    if (a_result) {
        if (a_result == kCFNull) {
            /* Caller wasn't interested in a result, but we still
             count this row as found. */
        } else if (q->q_limit == 1) {
            c->result = a_result;
        } else {
            CFArrayAppendValue((CFMutableArrayRef)c->result, a_result);
            CFRelease(a_result);
        }
        c->found++;
    }

out:
    CFRelease(item);
}

struct s3dl_update_row_ctx {
    struct s3dl_query_ctx qc;
    Query *u;
    sqlite3_stmt *update_stmt;
};


static void s3dl_update_row(sqlite3_stmt *stmt, void *context)
{
    struct s3dl_update_row_ctx *c = context;
    Query *q = c->qc.q;

    int s3e = SQLITE_OK;
    CFDataRef edata = NULL;
    keyclass_t keyclass;
    sqlite_int64 rowid = sqlite3_column_int64(stmt, 0);
    CFMutableDictionaryRef item;
    require(item = s3dl_item_from_col(stmt, q, 1, c->qc.accessGroups,
                                      &keyclass), out);

    /* Update modified attributes in item and reencrypt. */
    Query *u = c->u;
    CFDictionaryApplyFunction(u->q_item, s3dl_merge_into_dict, item);
    if (u->q_keyclass) {
        keyclass = u->q_keyclass;
    }

    require(edata = s3dl_encode_item(q->q_keybag, keyclass, item, &q->q_error),
            out);

    /* Bind rowid and data to UPDATE statement and step. */
    /* Skip over already bound attribute values being updated, since we don't
       change them for each item. */
    CFIndex count = 1 + query_attr_count(u);
    /* 64 bits cast: worst case is if you try to have more than 2^32 attributes, we will drop some */
    assert(count < INT_MAX); /* Debug check */
    int param = (int)count;
    s3e = sqlite3_bind_blob_wrapper(c->update_stmt, param++,
                            CFDataGetBytePtr(edata), CFDataGetLength(edata),
                            SQLITE_TRANSIENT);
    if (s3e == SQLITE_OK)
        s3e = sqlite3_bind_int64(c->update_stmt, param++, rowid);

    /* Now execute the UPDATE statement (step). */
	if (s3e == SQLITE_OK) {
		s3e = sqlite3_step(c->update_stmt);
		if (s3e == SQLITE_DONE) {
            s3e = SQLITE_OK;
            c->qc.found++;
            secdebug("sql", "updated row: %llu", rowid);
		} else if (s3e == SQLITE_ERROR) {
            /* sqlite3_reset() below will return the real error. */
            s3e = SQLITE_OK;
        }
	}

out:
    if (s3e) q->q_error = osstatus_for_s3e(s3e);
    s3e = sqlite3_reset(c->update_stmt);    /* Reset state, but not bindings. */
    if (s3e && !q->q_error) q->q_error = osstatus_for_s3e(s3e);
    if (q->q_error) { secdebug("sql", "update failed: %d", q->q_error); }

	CFReleaseSafe(item);
	CFReleaseSafe(edata);
}

/* Append AND is needWhere is NULL or *needWhere is false.  Append WHERE
   otherwise.  Upon return *needWhere will be false.  */
static void
sqlAppendWhereOrAnd(CFMutableStringRef sql, bool *needWhere) {
    if (!needWhere || !*needWhere) {
        CFStringAppend(sql, CFSTR(" AND "));
    } else {
        CFStringAppend(sql, CFSTR(" WHERE "));
        *needWhere = false;
    }
}

static void
sqlAppendWhereBind(CFMutableStringRef sql, CFStringRef col, bool *needWhere) {
    sqlAppendWhereOrAnd(sql, needWhere);
    CFStringAppend(sql, col);
    CFStringAppend(sql, CFSTR("=?"));
}

static void
sqlAppendWhereROWID(CFMutableStringRef sql,
                    CFStringRef col, sqlite_int64 row_id,
                    bool *needWhere) {
    if (row_id > 0) {
        sqlAppendWhereOrAnd(sql, needWhere);
        CFStringAppendFormat(sql, NULL, CFSTR("%@=%lld"), col, row_id);
    }
}

static void
sqlAppendWhereAttrs(CFMutableStringRef sql, const Query *q, bool *needWhere) {
    CFIndex ix, attr_count = query_attr_count(q);
    for (ix = 0; ix < attr_count; ++ix) {
        sqlAppendWhereBind(sql, query_attr_at(q, ix).key, needWhere);
    }
}

static void
sqlAppendWhereAccessGroups(CFMutableStringRef sql,
                           CFStringRef col,
                           CFArrayRef accessGroups,
                           bool *needWhere) {
    CFIndex ix, ag_count;
    if (!accessGroups || 0 == (ag_count = CFArrayGetCount(accessGroups))) {
        return;
    }

    sqlAppendWhereOrAnd(sql, needWhere);
#if 1
    CFStringAppend(sql, col);
    CFStringAppend(sql, CFSTR(" IN (?"));
    for (ix = 1; ix < ag_count; ++ix) {
        CFStringAppend(sql, CFSTR(",?"));
    }
    CFStringAppend(sql, CFSTR(")"));
#else
    CFStringAppendFormat(sql, 0, CFSTR("(%@=?"), col);
    for (ix = 1; ix < ag_count; ++ix) {
        CFStringAppendFormat(sql, 0, CFSTR(" OR %@=?"), col);
    }
    CFStringAppend(sql, CFSTR(")"));
#endif
}

static void sqlAppendWhereClause(CFMutableStringRef sql, const Query *q,
    CFArrayRef accessGroups) {
    bool needWhere = true;
    sqlAppendWhereROWID(sql, CFSTR("ROWID"), q->q_row_id, &needWhere);
    sqlAppendWhereAttrs(sql, q, &needWhere);
    sqlAppendWhereAccessGroups(sql, CFSTR("agrp"), accessGroups, &needWhere);
}

static void sqlAppendLimit(CFMutableStringRef sql, CFIndex limit) {
    if (limit != kSecMatchUnlimited)
        CFStringAppendFormat(sql, NULL, CFSTR(" LIMIT %d;"), limit);
    else
        CFStringAppend(sql, CFSTR(";"));
}

static CFStringRef s3dl_select_sql(Query *q, int version, CFArrayRef accessGroups) {
    CFMutableStringRef sql = CFStringCreateMutable(NULL, 0);
	if (q->q_class == &identity_class) {
        CFStringAppendFormat(sql, NULL, CFSTR("SELECT crowid, "
            CERTIFICATE_DATA_COLUMN_LABEL ", rowid, data FROM "
            "(SELECT cert.rowid AS crowid, cert.labl AS labl,"
            " cert.issr AS issr, cert.slnr AS slnr, cert.skid AS skid,"
            " keys.*, cert.data AS " CERTIFICATE_DATA_COLUMN_LABEL
            " FROM keys, cert"
            " WHERE keys.priv == 1 AND cert.pkhh == keys.klbl"));
        sqlAppendWhereAccessGroups(sql, CFSTR("cert.agrp"), accessGroups, 0);
        /* The next 3 sqlAppendWhere calls are in the same order as in
           sqlAppendWhereClause().  This makes sqlBindWhereClause() work,
           as long as we do an extra sqlBindAccessGroups first. */
        sqlAppendWhereROWID(sql, CFSTR("crowid"), q->q_row_id, 0);
        CFStringAppend(sql, CFSTR(")"));
        bool needWhere = true;
        sqlAppendWhereAttrs(sql, q, &needWhere);
        sqlAppendWhereAccessGroups(sql, CFSTR("agrp"), accessGroups, &needWhere);
	} else {
        CFStringAppend(sql, (version < 5 ? CFSTR("SELECT * FROM ") :
                             CFSTR("SELECT rowid, data FROM ")));
		CFStringAppend(sql, q->q_class->name);
        sqlAppendWhereClause(sql, q, accessGroups);
    }
    sqlAppendLimit(sql, q->q_limit);

    return sql;
}

static int sqlBindAccessGroups(sqlite3_stmt *stmt, CFArrayRef accessGroups,
                               int *pParam) {
    int s3e = SQLITE_OK;
    int param = *pParam;
    CFIndex ix, count = accessGroups ? CFArrayGetCount(accessGroups) : 0;
    for (ix = 0; ix < count; ++ix) {
        s3e = kc_bind_paramter(stmt, param++,
                               CFArrayGetValueAtIndex(accessGroups, ix));
        if (s3e)
            break;
    }
    *pParam = param;
    return s3e;
}

static int sqlBindWhereClause(sqlite3_stmt *stmt, const Query *q,
    CFArrayRef accessGroups, int *pParam) {
    int s3e = SQLITE_OK;
    int param = *pParam;
    CFIndex ix, attr_count = query_attr_count(q);
    for (ix = 0; ix < attr_count; ++ix) {
        s3e = kc_bind_paramter(stmt, param++, query_attr_at(q, ix).value);
        if (s3e)
            break;
	}

    /* Bind the access group to the sql. */
    if (s3e == SQLITE_OK) {
        s3e = sqlBindAccessGroups(stmt, accessGroups, &param);
    }

    *pParam = param;
    return s3e;
}

static OSStatus
s3dl_query(s3dl_db_thread *dbt, s3dl_handle_row handle_row, int version,
           void *context)
{
    struct s3dl_query_ctx *c = context;
    Query *q = c->q;
    CFArrayRef accessGroups = c->accessGroups;
    int s3e;

    /* Sanity check the query. */
    if (q->q_ref)
        return errSecValueRefUnsupported;
    if (q->q_row_id && query_attr_count(q))
        return errSecItemIllegalQuery;

	/* Actual work here. */
	sqlite3 *s3h = dbt->s3_handle;

    CFStringRef sql = s3dl_select_sql(q, version, accessGroups);
	sqlite3_stmt *stmt = NULL;
    s3e = kc_prepare_statement(s3h, sql, &stmt);
	CFRelease(sql);

	/* Bind the values being searched for to the SELECT statement. */
	if (s3e == SQLITE_OK) {
        int param = 1;
        if (q->q_class == &identity_class) {
            /* Bind the access groups to cert.agrp. */
            s3e = sqlBindAccessGroups(stmt, accessGroups, &param);
        }
        if (s3e == SQLITE_OK)
            s3e = sqlBindWhereClause(stmt, q, accessGroups, &param);
    }

	/* Now execute the SELECT statement (step). */
    if (q->q_limit == 1) {
        c->result = NULL;
    } else {
        c->result = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
    }
	while (s3e == SQLITE_OK &&
        (q->q_limit == kSecMatchUnlimited || c->found < q->q_limit)) {
		s3e = sqlite3_step(stmt);
		if (s3e == SQLITE_ROW) {
			handle_row(stmt, context);
            /* Extract the error returned by handle_row. */
            s3e = q->q_error;
            if (s3e == errSecDecode) {
                secerror("Ignoring undecryptable %@ item: ", q->q_class->name);
                /* Ignore decode errors, since that means the item in question
                   is unreadable forever, this allows export to skip these
                   items and a backup/restore cycle to filter broken items
                   from your keychain.
                   Ideally we should mark the current rowid for removal since
                   it's corrupt.  The tricky bit is at this level we no longer
                   know if the key or the cert failed to decode when dealing
                   with identities. */
                s3e = SQLITE_OK;
            }
		} else if (s3e == SQLITE_DONE) {
            if (c->found == 0) {
                /* We ran out of rows and didn't get any matches yet. */
                s3e = errSecItemNotFound;
            } else {
				/* We got at least one match, so set the error status to ok. */
				s3e = SQLITE_OK;
			}
            break;
		} else if (s3e != SQLITE_OK) {
			secdebug("sql", "select: %d: %s", s3e, sqlite3_errmsg(s3h));
        }
	}

	/* Free the stmt. */
	if (stmt) {
		int s3e2 = sqlite3_finalize(stmt);
		if (s3e2 != SQLITE_OK && s3e == SQLITE_OK)
			s3e = s3e2;
	}

    OSStatus status;
	if (s3e) status = osstatus_for_s3e(s3e);
    else if (q->q_error) status = q->q_error;
    else return 0;

    CFReleaseNull(c->result);
    return status;
}

static OSStatus
s3dl_copy_matching(s3dl_db_thread *dbt, Query *q, CFTypeRef *result,
                   CFArrayRef accessGroups)
{
    struct s3dl_query_ctx ctx = {
        .q = q, .accessGroups = accessGroups,
    };
    OSStatus status = s3dl_query(dbt, s3dl_query_row, CURRENT_DB_VERSION, &ctx);
    if (result)
        *result = ctx.result;
    else
        CFReleaseSafe(ctx.result);
    return status;
}

static CFStringRef s3dl_update_sql(Query *q) {
    CFMutableStringRef sql = CFStringCreateMutable(NULL, 0);
    CFStringAppendFormat(sql, NULL, CFSTR("UPDATE %@ SET"), q->q_class->name);

    CFIndex ix, attr_count = query_attr_count(q);
    for (ix = 0; ix < attr_count; ++ix) {
        CFStringAppendFormat(sql, NULL, CFSTR(" %@=?,"),
                             query_attr_at(q, ix).key);
    }
    CFStringAppend(sql, CFSTR(" data=? WHERE ROWID=?;"));

    return sql;
}

/* Bind the parameters to the UPDATE statement; data=? and ROWID=? are left
   unbound, since they are bound in s3dl_update_row when the update statement
   is (re)used. */
static int s3dl_update_bind(Query *q, sqlite3_stmt *stmt) {
	/* Bind the values being updated to the UPDATE statement. */
    int s3e = SQLITE_OK;
	int param = 1;
    CFIndex ix, attr_count = query_attr_count(q);
    for (ix = 0; s3e == SQLITE_OK && ix < attr_count; ++ix) {
        s3e = kc_bind_paramter(stmt, param++, query_attr_at(q, ix).value);
	}
    return s3e;
}

/* AUDIT[securityd](done):
   attributesToUpdate (ok) is a caller provided dictionary,
       only its cf types have been checked.
 */
static OSStatus
s3dl_query_update(s3dl_db_thread *dbt, Query *q,
    CFDictionaryRef attributesToUpdate, CFArrayRef accessGroups)
{
    /* Sanity check the query. */
    if (query_match_count(q) != 0)
        return errSecItemMatchUnsupported;
    if (q->q_ref)
        return errSecValueRefUnsupported;
    if (q->q_row_id && query_attr_count(q))
        return errSecItemIllegalQuery;

    int s3e = SQLITE_OK;

    Query *u = query_create(q->q_class, attributesToUpdate, &q->q_error);
    if (u == NULL) return q->q_error;
    if (!query_update_parse(u, attributesToUpdate, &q->q_error))
        goto errOut;
    query_pre_update(u);

	/* Actual work here. */
	sqlite3 *s3h = dbt->s3_handle;

    CFStringRef sql = s3dl_update_sql(u);
	sqlite3_stmt *stmt = NULL;
    s3e = kc_prepare_statement(s3h, sql, &stmt);
	CFRelease(sql);

    if (s3e == SQLITE_OK)
        s3e = s3dl_update_bind(u, stmt);

    if (s3e == SQLITE_OK) {
        s3e = s3dl_begin_transaction(dbt);
        q->q_return_type = 0;
        struct s3dl_update_row_ctx ctx = {
            .qc = {
                .q = q, .accessGroups = accessGroups,
            },
            .u = u,
            .update_stmt = stmt
        };
        u->q_error = s3dl_query(dbt, s3dl_update_row, CURRENT_DB_VERSION, &ctx);
        s3e = s3dl_end_transaction(dbt, s3e);
    }

	/* Free the stmt. */
	if (stmt) {
		int s3e2 = sqlite3_finalize(stmt);
		if (s3e2 != SQLITE_OK && s3e == SQLITE_OK)
			s3e = s3e2;
	}

errOut:
    query_destroy(u, &q->q_error);

	if (s3e) return osstatus_for_s3e(s3e);
    if (q->q_error) return q->q_error;
    return 0;
}

static OSStatus
s3dl_query_delete(s3dl_db_thread *dbt, Query *q, CFArrayRef accessGroups)
{
	sqlite3 *s3h = dbt->s3_handle;
	sqlite3_stmt *stmt = NULL;
    CFMutableStringRef sql;
    int s3e;

    sql = CFStringCreateMutable(NULL, 0);
    CFStringAppendFormat(sql, NULL, CFSTR("DELETE FROM %@"), q->q_class->name);
    sqlAppendWhereClause(sql, q, accessGroups);
    CFStringAppend(sql, CFSTR(";"));
    s3e = kc_prepare_statement(s3h, sql, &stmt);
	CFRelease(sql);

	/* Bind the parameters to the DELETE statement. */
	if (s3e == SQLITE_OK) {
        int param = 1;
        s3e = sqlBindWhereClause(stmt, q, accessGroups, &param);
    }

	/* Now execute the DELETE statement (step). */
	if (s3e == SQLITE_OK) {
		s3e = sqlite3_step(stmt);
		if (s3e == SQLITE_DONE) {
            int changes = sqlite3_changes(s3h);
			/* When doing a delete without a where clause sqlite reports 0
			   changes since it drops and recreates the table rather than
			   deleting all the records in it.  */
            if (changes == 0 && query_attr_count(q) > 0) {
                s3e = errSecItemNotFound;
            } else {
                s3e = SQLITE_OK;
				secdebug("sql", "deleted: %d records", changes);
			}
		} else if (s3e != SQLITE_OK) {
			secdebug("sql", "delete: %d: %s", s3e, sqlite3_errmsg(s3h));
        }
	}

	/* Free the stmt. */
	if (stmt) {
		int s3e2 = sqlite3_finalize(stmt);
		if (s3e2 != SQLITE_OK && s3e == SQLITE_OK)
			s3e = s3e2;
	}

    return s3e;
}

/* Return true iff the item in question should not be backed up, nor restored,
   but when restoring a backup the original version of the item should be
   added back to the keychain again after the restore completes. */
static bool SecItemIsSystemBound(CFDictionaryRef item, const kc_class *class) {
    CFStringRef agrp = CFDictionaryGetValue(item, kSecAttrAccessGroup);
    if (!isString(agrp))
        return false;

    if (CFEqual(agrp, CFSTR("lockdown-identities"))) {
        secdebug("backup", "found sys_bound item: %@", item);
        return true;
    }

    if (CFEqual(agrp, CFSTR("apple")) && class == &genp_class) {
        CFStringRef service = CFDictionaryGetValue(item, kSecAttrService);
        CFStringRef account = CFDictionaryGetValue(item, kSecAttrAccount);
        if (isString(service) && isString(account) &&
            CFEqual(service, CFSTR("com.apple.managedconfiguration")) &&
            (CFEqual(account, CFSTR("Public")) ||
                CFEqual(account, CFSTR("Private")))) {
            secdebug("backup", "found sys_bound item: %@", item);
            return true;
        }
    }
    secdebug("backup", "found non sys_bound item: %@", item);
    return false;
}

/* Delete all items from the current keychain.  If this is not an in
   place upgrade we don't delete items in the 'lockdown-identities'
   access group, this ensures that an import or restore of a backup
   will never overwrite an existing activation record. */
static OSStatus SecServerDeleteAll(s3dl_db_thread *dbt) {
    int s3e = sqlite3_exec(dbt->s3_handle,
        "DELETE from genp;"
        "DELETE from inet;"
        "DELETE FROM cert;"
        "DELETE FROM keys;",
        NULL, NULL, NULL);
    return s3e == SQLITE_OK ? 0 : osstatus_for_s3e(s3e);
}

struct s3dl_export_row_ctx {
    struct s3dl_query_ctx qc;
    keybag_handle_t dest_keybag;
    enum SecItemFilter filter;
    int version;
    s3dl_db_thread *dbt;
};

/* Return NULL if the current row isn't a match.  Return kCFNull if the
 current row is a match and q->q_return_type == 0.  Otherwise return
 whatever the caller requested based on the value of q->q_return_type. */
static CFMutableDictionaryRef
s3dl_item_from_pre_v5(sqlite3_stmt *stmt, Query *q, s3dl_db_thread *dbt,
                      keyclass_t *keyclass, sqlite_int64 *rowid) {
	int cix = 0, cc = sqlite3_column_count(stmt);

    CFMutableDictionaryRef item = CFDictionaryCreateMutable(0, 0,
        &kCFTypeDictionaryKeyCallBacks, & kCFTypeDictionaryValueCallBacks);
	for (cix = 0; cix < cc; ++cix) {
        const char *cname = sqlite3_column_name(stmt, cix);
        CFStringRef key = NULL;
        CFTypeRef value = NULL;
		int ctype = sqlite3_column_type(stmt, cix);
		switch (ctype) {
            case SQLITE_INTEGER:
            {
                sqlite_int64 i64Value = sqlite3_column_int64(stmt, cix);
                if (!strcmp(cname, "rowid")) {
                    *rowid = i64Value;
                    continue;
                } else if (i64Value > INT_MAX || i64Value < INT_MIN) {
                    value = CFNumberCreate(0, kCFNumberLongLongType, &i64Value);
                } else {
                    int iValue = (int)i64Value;
                    value = CFNumberCreate(0, kCFNumberIntType, &iValue);
                }
                break;
            }
            case SQLITE_FLOAT:
                value = CFDateCreate(0, sqlite3_column_double(stmt, cix));
                break;
            case SQLITE_TEXT:
                value = CFStringCreateWithCString(0,
                                                  (const char *)sqlite3_column_text(stmt, cix),
                                                  kCFStringEncodingUTF8);
                break;
            case SQLITE_BLOB:
                value = CFDataCreate(0, sqlite3_column_blob(stmt, cix),
                                     sqlite3_column_bytes(stmt, cix));
                if (value && !strcmp(cname, "data")) {
                    CFDataRef plain;
                    if (q->q_keybag == KEYBAG_LEGACY) {
                        q->q_error = kc_decrypt_data(dbt, value, &plain);
                    } else {
                        q->q_error = ks_decrypt_data(q->q_keybag, keyclass,
                                                     value, &plain, 0);
                    }
                    CFRelease(value);
                    if (q->q_error) {
                        secerror("failed to decrypt data: %d", q->q_error);
                        goto out;
                    }
                    value = plain;
                    key = kSecValueData;
                }
                break;
            default:
                secwarning("Unsupported column type: %d", ctype);
                /*DROPTHROUGH*/
            case SQLITE_NULL:
                /* Don't return NULL valued attributes to the caller. */
                continue;
		}

        if (!value)
            continue;

        if (key) {
            CFDictionarySetValue(item, key, value);
        } else {
            key = CFStringCreateWithCString(0, cname, kCFStringEncodingUTF8);
            if (key) {
                CFDictionarySetValue(item, key, value);
                CFRelease(key);
            }
        }
        CFRelease(value);
	}

    return item;
out:
    CFReleaseSafe(item);
	return NULL;
}

static void s3dl_export_row(sqlite3_stmt *stmt, void *context) {
    struct s3dl_export_row_ctx *c = context;
    Query *q = c->qc.q;
    keyclass_t keyclass = 0;

    CFMutableDictionaryRef item;
    sqlite_int64 rowid = -1;
    if (c->version < 5) {
        item = s3dl_item_from_pre_v5(stmt, q, c->dbt, &keyclass, &rowid);
    } else {
        item = s3dl_item_from_col(stmt, q, 1, c->qc.accessGroups, &keyclass);
        rowid = sqlite3_column_int64(stmt, 0);
    }

    if (item) {
        /* Only export sysbound items is do_sys_bound is true, only export non sysbound items otherwise. */
        bool do_sys_bound = c->filter == kSecSysBoundItemFilter;
        if (c->filter == kSecNoItemFilter ||
            SecItemIsSystemBound(item, q->q_class) == do_sys_bound) {
            /* Re-encode the item. */
            secdebug("item", "export rowid %llu item: %@", rowid, item);
            /* The code below could be moved into handle_row. */
            CFDataRef pref = _SecItemMakePersistentRef(q->q_class->name, rowid);
            if (pref) {
                if (c->dest_keybag != KEYBAG_NONE) {
                    /* Encode and encrypt the item to the specified keybag. */
                    CFDataRef plain = CFPropertyListCreateData(0, item, kCFPropertyListBinaryFormat_v1_0, 0, 0);
                    CFDictionaryRemoveAllValues(item);
                    if (plain) {
                        CFDataRef edata = NULL;
                        int s3e = ks_encrypt_data(c->dest_keybag, keyclass, plain, &edata);
                        if (s3e)
                            q->q_error = osstatus_for_s3e(s3e);
                        if (edata) {
                            CFDictionarySetValue(item, kSecValueData, edata);
                            CFRelease(edata);
                        }
                        CFRelease(plain);
                    }
                }
                if (CFDictionaryGetCount(item)) {
                    CFDictionarySetValue(item, kSecValuePersistentRef, pref);
                    CFArrayAppendValue((CFMutableArrayRef)c->qc.result, item);
                    c->qc.found++;
                }
                CFRelease(pref);
            }
        }
        CFRelease(item);
    }
}

static CFDictionaryRef SecServerExportKeychainPlist(s3dl_db_thread *dbt,
    keybag_handle_t src_keybag, keybag_handle_t dest_keybag,
    enum SecItemFilter filter, int version, OSStatus *error) {
    CFMutableDictionaryRef keychain;
    keychain = CFDictionaryCreateMutable(kCFAllocatorDefault, 0,
        &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    if (!keychain) {
        if (error && !*error)
            *error = errSecAllocate;
        goto errOut;
    }
    unsigned class_ix;
    Query q = { .q_keybag = src_keybag };
    q.q_return_type = kSecReturnDataMask | kSecReturnAttributesMask | \
        kSecReturnPersistentRefMask;
    q.q_limit = kSecMatchUnlimited;

    /* Get rid of this duplicate. */
    const kc_class *kc_classes[] = {
        &genp_class,
        &inet_class,
        &cert_class,
        &keys_class
    };

    for (class_ix = 0; class_ix < sizeof(kc_classes) / sizeof(*kc_classes);
        ++class_ix) {
        q.q_class = kc_classes[class_ix];
        struct s3dl_export_row_ctx ctx = {
            .qc = { .q = &q, },
            .dest_keybag = dest_keybag, .filter = filter, .version = version,
            .dbt = dbt,
        };
        ctx.qc.result = CFArrayCreateMutable(kCFAllocatorDefault, 0,
                                             &kCFTypeArrayCallBacks);
        if (ctx.qc.result) {
            OSStatus status = s3dl_query(dbt, s3dl_export_row, version, &ctx);
            if (status == noErr) {
                if (CFArrayGetCount(ctx.qc.result))
                    CFDictionaryAddValue(keychain, q.q_class->name, ctx.qc.result);
                CFRelease(ctx.qc.result);
            } else if (status != errSecItemNotFound) {
                if (error && !*error)
                    *error = status;
                if (status == errSecInteractionNotAllowed) {
                    secerror("Device locked during attempted keychain upgrade");
                    CFReleaseNull(keychain);
                    break;
                }
            }
        }
    }
errOut:
    return keychain;
}

static OSStatus SecServerExportKeychain(s3dl_db_thread *dbt,
    keybag_handle_t src_keybag, keybag_handle_t dest_keybag,
    CFDataRef *data_out) {
    OSStatus status = noErr;
    /* Export everything except the items for which SecItemIsSystemBound()
       returns true. */
    CFDictionaryRef keychain = SecServerExportKeychainPlist(dbt,
        src_keybag, dest_keybag, kSecBackupableItemFilter, CURRENT_DB_VERSION,
        &status);
    if (keychain) {
        CFErrorRef error = NULL;
        *data_out = CFPropertyListCreateData(kCFAllocatorDefault, keychain,
                                             kCFPropertyListBinaryFormat_v1_0,
                                             0, &error);
        CFRelease(keychain);
        if (error) {
            secerror("Error encoding keychain: %@", error);
            status = (OSStatus)CFErrorGetCode(error); /* possibly truncated error code, whatever */
            CFRelease(error);
        }
    }

    return status;
}

struct SecServerImportClassState {
	s3dl_db_thread *dbt;
    OSStatus status;
    keybag_handle_t src_keybag;
    keybag_handle_t dest_keybag;
    enum SecItemFilter filter;
};

struct SecServerImportItemState {
    const kc_class *class;
	struct SecServerImportClassState *s;
};

/* Infer a keyclass for 3.x items being imported from a backup.  Return NULL
   to leave keyclass unchanged. */
static void SecItemImportInferAccessible(Query *q) {
    CFStringRef agrp = CFDictionaryGetValue(q->q_item, kSecAttrAccessGroup);
    CFStringRef accessible = NULL;
    if (isString(agrp)) {
        if (CFEqual(agrp, CFSTR("apple"))) {
            if (q->q_class == &cert_class) {
                /* apple certs are always dk. */
                accessible = kSecAttrAccessibleAlways;
            } else if (q->q_class == &genp_class) {
                CFStringRef svce = CFDictionaryGetValue(q->q_item, kSecAttrService);
                if (isString(svce)) {
                    if (CFEqual(svce, CFSTR("iTools"))) {
                        /* iTools password is dk for now. */
                        accessible = kSecAttrAccessibleAlways;
                    } else if (CFEqual(svce, CFSTR("BackupAgent"))) {
                        /* We assume that acct == BackupPassword use aku. */
                        accessible = kSecAttrAccessibleWhenUnlockedThisDeviceOnly;
                    } else if (CFEqual(svce, CFSTR("MobileBluetooth"))) {
                        /* MobileBlueTooh uses cku. */
                        accessible = kSecAttrAccessibleAfterFirstUnlockThisDeviceOnly;
                    }
                }
            } else if (q->q_class == &inet_class) {
                CFStringRef ptcl = CFDictionaryGetValue(q->q_item, kSecAttrProtocol);
                if (isString(ptcl)) {
                    /* LDAP is never needed without UI so ak. */
                    if (CFEqual(ptcl, kSecAttrProtocolLDAP) ||
                        CFEqual(ptcl, kSecAttrProtocolLDAPS)) {
                        accessible = kSecAttrAccessibleWhenUnlocked;
                    }
                }
            }
            if (accessible == NULL) {
                /* Everything not covered by a special case in the apple
                   access group (including keys) ends up in class Ck. */
                accessible = kSecAttrAccessibleAfterFirstUnlock;
            }
        } else if (CFEqual(agrp, CFSTR("com.apple.apsd"))
                   || CFEqual(agrp, CFSTR("lockdown-identities"))) {
            /* apsd and lockdown are always dku. */
            accessible = kSecAttrAccessibleAlwaysThisDeviceOnly;
        }
    }
    if (accessible == NULL) {
        if (q->q_class == &cert_class) {
            /* third party certs are always dk. */
            accessible = kSecAttrAccessibleAlways;
        } else {
            /* The rest defaults to ak. */
            accessible = kSecAttrAccessibleWhenUnlocked;
        }
    }
    query_add_attribute(kSecAttrAccessible, accessible, q);
}

/* Infer accessibility and access group for pre-v2 (iOS4.x and earlier) items
   being imported from a backup.  */
static void SecItemImportMigrate(Query *q) {
    CFStringRef agrp = CFDictionaryGetValue(q->q_item, kSecAttrAccessGroup);
    CFStringRef accessible = CFDictionaryGetValue(q->q_item, kSecAttrAccessible);

    if (!isString(agrp) || !isString(accessible))
        return;
    if (q->q_class == &genp_class && CFEqual(accessible, kSecAttrAccessibleAlways)) {
        CFStringRef svce = CFDictionaryGetValue(q->q_item, kSecAttrService);
        if (!isString(svce)) return;
        if (CFEqual(agrp, CFSTR("apple"))) {
            if (CFEqual(svce, CFSTR("AirPort"))) {
                query_set_attribute(kSecAttrAccessible, kSecAttrAccessibleAfterFirstUnlock, q);
            } else if (CFEqual(svce, CFSTR("com.apple.airplay.password"))) {
                query_set_attribute(kSecAttrAccessible, kSecAttrAccessibleWhenUnlocked, q);
            } else if (CFEqual(svce, CFSTR("YouTube"))) {
                query_set_attribute(kSecAttrAccessible, kSecAttrAccessibleWhenUnlocked, q);
                query_set_attribute(kSecAttrAccessGroup, CFSTR("com.apple.youtube.credentials"), q);
            } else {
                CFStringRef desc = CFDictionaryGetValue(q->q_item, kSecAttrDescription);
                if (!isString(desc)) return;
                if (CFEqual(desc, CFSTR("IPSec Shared Secret")) || CFEqual(desc, CFSTR("PPP Password"))) {
                    query_set_attribute(kSecAttrAccessible, kSecAttrAccessibleAfterFirstUnlock, q);
                }
            }
        }
    } else if (q->q_class == &inet_class && CFEqual(accessible, kSecAttrAccessibleAlways)) {
        if (CFEqual(agrp, CFSTR("PrintKitAccessGroup"))) {
            query_set_attribute(kSecAttrAccessible, kSecAttrAccessibleWhenUnlocked, q);
        } else if (CFEqual(agrp, CFSTR("apple"))) {
            CFTypeRef ptcl = CFDictionaryGetValue(q->q_item, kSecAttrProtocol);
            bool is_proxy = false;
            if (isNumber(ptcl)) {
                SInt32 iptcl;
                CFNumberGetValue(ptcl, kCFNumberSInt32Type, &iptcl);
                is_proxy = (iptcl == FOUR_CHAR_CODE('htpx') ||
                            iptcl == FOUR_CHAR_CODE('htsx') ||
                            iptcl == FOUR_CHAR_CODE('ftpx') ||
                            iptcl == FOUR_CHAR_CODE('rtsx') ||
                            iptcl == FOUR_CHAR_CODE('xpth') ||
                            iptcl == FOUR_CHAR_CODE('xsth') ||
                            iptcl == FOUR_CHAR_CODE('xptf') ||
                            iptcl == FOUR_CHAR_CODE('xstr'));
            } else if (isString(ptcl)) {
                is_proxy = (CFEqual(ptcl, kSecAttrProtocolHTTPProxy) ||
                            CFEqual(ptcl, kSecAttrProtocolHTTPSProxy) ||
                            CFEqual(ptcl, kSecAttrProtocolRTSPProxy) ||
                            CFEqual(ptcl, kSecAttrProtocolFTPProxy));
            }
            if (is_proxy)
                query_set_attribute(kSecAttrAccessible, kSecAttrAccessibleWhenUnlocked, q);
        }
    }
}

static void SecServerImportItem(const void *value, void *context) {
    struct SecServerImportItemState *state =
        (struct SecServerImportItemState *)context;
    if (state->s->status)
        return;
    if (!isDictionary(value)) {
        state->s->status = errSecParam;
        return;
    }

    CFDictionaryRef item = (CFDictionaryRef)value;

    /* We don't filter non sys_bound items during import since we know we
       will never have any in this case, we use the kSecSysBoundItemFilter
       to indicate that we don't preserve rowid's during import instead. */
    if (state->s->filter == kSecBackupableItemFilter &&
        SecItemIsSystemBound(item, state->class))
        return;

    Query *q = query_create(state->class, item, &state->s->status);
    if (!q)
        return;

    /* TODO: We'd like to use query_update_applier here instead of
       query_parse(), since only attrs, kSecValueData and kSecValuePersistentRef. */
    query_parse(q, item, &state->s->status);

    CFDataRef pdata = NULL;
    if (state->s->src_keybag == KEYBAG_NONE) {
        /* Not strictly needed if we are restoring from cleartext db that is
           version 5 or later, but currently that never happens.  Also the
           migrate code should be specific enough that it's still safe even
           in that case, it's just slower. */
        SecItemImportMigrate(q);
    } else {
        if (q->q_data) {
            keyclass_t keyclass;
            /* Decrypt the data using state->s->dest_keybag. */
            uint32_t version;
            q->q_error = ks_decrypt_data(state->s->src_keybag,
                &keyclass, q->q_data, &pdata, &version);
            if (q->q_error) {
                /* keyclass attribute doesn't match decoded value, fail. */
                q->q_error = errSecDecode;
                asl_log(NULL, NULL, ASL_LEVEL_ERR,
                    "keyclass attribute %d doesn't match keyclass in blob %d",
                    q->q_keyclass, keyclass);
            }
            if (version < 2) {
                /* Old V4 style keychain backup being imported. */
                /* Make sure the keyclass in the dictionary matched what we got
                   back from decoding the data blob. */
                if (q->q_keyclass && q->q_keyclass != keyclass) {
                    q->q_error = errSecDecode;
                    asl_log(NULL, NULL, ASL_LEVEL_ERR,
                            "keyclass attribute %d doesn't match keyclass in blob %d",
                            q->q_keyclass, keyclass);
                } else {
                    query_set_data(pdata, q);
                    SecItemImportMigrate(q);
                }
            } else {
                /* version 2 or later backup. */
                CFErrorRef error = NULL;
                CFPropertyListFormat format;
                item = CFPropertyListCreateWithData(0, pdata, kCFPropertyListImmutable, &format, &error);
                if (item) {
                    query_update_parse(q, item, &state->s->status);
                    secdebug("item", "importing status: %ld %@",
                             state->s->status, item);
                    CFRelease(item);
                } else if (error) {
                    secerror("failed to decode v%d item data: %@",
                             version, error);
                    CFRelease(error);
                }
            }
        }
    }

    if (q->q_keyclass == 0)
        SecItemImportInferAccessible(q);

    if (q->q_error) {
        state->s->status = q->q_error;
    } else {
        if (q->q_keyclass == 0) {
            state->s->status = errSecParam;
        } else {
            if (state->s->filter == kSecSysBoundItemFilter) {
                /* We don't set the rowid of sys_bound items we reimport
                   after an import since that might fail if an item in the
                   restored keychain already used that rowid. */
                q->q_row_id = 0;
            }
            query_pre_add(q, false);
            state->s->status = s3dl_query_add(state->s->dbt, q, NULL);
        }
    }
    CFReleaseSafe(pdata);
    query_destroy(q, &state->s->status);

    /* Reset error if we had one, since we just skip the current item
       and continue importing what we can. */
    if (state->s->status) {
        secerror("Failed to import %@ item %ld ignoring error",
                 state->class->name, state->s->status);
        state->s->status = 0;
    }
}

static void SecServerImportClass(const void *key, const void *value,
    void *context) {
    struct SecServerImportClassState *state =
        (struct SecServerImportClassState *)context;
    if (state->status)
        return;
    if (!isString(key)) {
        state->status = errSecParam;
        return;
    }
    const void *desc = CFDictionaryGetValue(gClasses, key);
    const kc_class *class = desc;
    if (!class || class == &identity_class) {
        state->status = errSecParam;
        return;
    }
    struct SecServerImportItemState item_state = {
        .class = class, .s = state
    };
    if (isArray(value)) {
        CFArrayRef items = (CFArrayRef)value;
        CFArrayApplyFunction(items, CFRangeMake(0, CFArrayGetCount(items)),
            SecServerImportItem, &item_state);
    } else {
        CFDictionaryRef item = (CFDictionaryRef)value;
        SecServerImportItem(item, &item_state);
    }
}

static OSStatus SecServerImportKeychainInPlist(s3dl_db_thread *dbt,
    keybag_handle_t src_keybag, keybag_handle_t dest_keybag,
    CFDictionaryRef keychain, enum SecItemFilter filter) {
    OSStatus status = errSecSuccess;

    CFDictionaryRef sys_bound = NULL;
    if (filter == kSecBackupableItemFilter) {
        /* Grab a copy of all the items for which SecItemIsSystemBound()
           returns true. */
        require(sys_bound = SecServerExportKeychainPlist(dbt, KEYBAG_DEVICE,
            KEYBAG_NONE, kSecSysBoundItemFilter, CURRENT_DB_VERSION,
            &status), errOut);
    }

    /* Delete everything in the keychain. */
    require_noerr(status = SecServerDeleteAll(dbt), errOut);

    struct SecServerImportClassState state = {
        .dbt = dbt,
        .src_keybag = src_keybag,
        .dest_keybag = dest_keybag,
        .filter = filter
    };
    /* Import the provided items, preserving rowids. */
    CFDictionaryApplyFunction(keychain, SecServerImportClass, &state);

    if (sys_bound) {
        state.src_keybag = KEYBAG_NONE;
        /* Import the items we preserved with random rowids. */
        state.filter = kSecSysBoundItemFilter;
        CFDictionaryApplyFunction(sys_bound, SecServerImportClass, &state);
        CFRelease(sys_bound);
    }
    status = state.status;

errOut:
    return status;
}

static OSStatus SecServerImportKeychain(s3dl_db_thread *dbt,
    keybag_handle_t src_keybag,
    keybag_handle_t dest_keybag, CFDataRef data) {
    int s3e = s3dl_begin_transaction(dbt);
    OSStatus status = errSecSuccess;
    if (s3e != SQLITE_OK) {
        status = osstatus_for_s3e(s3e);
    } else {
        CFDictionaryRef keychain;
        CFPropertyListFormat format;
        CFErrorRef error = NULL;
        keychain = CFPropertyListCreateWithData(kCFAllocatorDefault, data,
                                             kCFPropertyListImmutable, &format,
                                             &error);
        if (keychain) {
            if (isDictionary(keychain)) {
                status = SecServerImportKeychainInPlist(dbt, src_keybag,
                                                        dest_keybag, keychain,
                                                        kSecBackupableItemFilter);
            } else {
                status = errSecParam;
            }
            CFRelease(keychain);
        } else {
            secerror("Error decoding keychain: %@", error);
            status = (OSStatus)CFErrorGetCode(error); /* possibly truncated error code, whatever */
            CFRelease(error);
        }
    }

    s3e = s3dl_end_transaction(dbt, status);
    return status ? status : osstatus_for_s3e(s3e);
}

static OSStatus
SecServerMigrateKeychain(s3dl_db_thread *dbt,
    int32_t handle_in, CFDataRef data_in,
    int32_t *handle_out, CFDataRef *data_out) {
    OSStatus status;

    if (handle_in == kSecMigrateKeychainImport) {
        if (data_in == NULL) {
            return errSecParam;
        }
        /* Import data_in. */
        status = SecServerImportKeychain(dbt, KEYBAG_NONE, KEYBAG_DEVICE, data_in);
        *data_out = NULL;
        *handle_out = 0;
    } else if (handle_in == kSecMigrateKeychainExport) {
        if (data_in != NULL) {
            return errSecParam;
        }
        /* Export the keychain and return the result in data_out. */
        status = SecServerExportKeychain(dbt, KEYBAG_DEVICE, KEYBAG_NONE, data_out);
        *handle_out = 0;
    } else {
        status = errSecParam;
    }

    return status;
}

static keybag_handle_t ks_open_keybag(CFDataRef keybag, CFDataRef password) {
#if USE_KEYSTORE
    uint64_t outputs[] = { KEYBAG_NONE };
    uint32_t num_outputs = sizeof(outputs) / sizeof(*outputs);
    IOReturn kernResult;

    kernResult = IOConnectCallMethod(keystore,
        kAppleKeyStoreKeyBagCreateWithData, NULL, 0, CFDataGetBytePtr(keybag),
        CFDataGetLength(keybag), outputs, &num_outputs, NULL, 0);
    if (kernResult) {
        asl_log(NULL, NULL, ASL_LEVEL_ERR,
            "kAppleKeyStoreKeyBagCreateWithData: %x", kernResult);
        goto errOut;
    }
    if (password) {
        kernResult = IOConnectCallMethod(keystore, kAppleKeyStoreKeyBagUnlock,
            outputs, 1, CFDataGetBytePtr(password), CFDataGetLength(password),
            NULL, 0, NULL, NULL);
        if (kernResult) {
            asl_log(NULL, NULL, ASL_LEVEL_ERR,
                "kAppleKeyStoreKeyBagCreateWithData: %x", kernResult);
            goto errOut;
        }
    }
    return (keybag_handle_t)outputs[0];
errOut:
    return -3;
#else
    return KEYBAG_NONE;
#endif
}

static void ks_close_keybag(keybag_handle_t keybag) {
#if USE_KEYSTORE
    uint64_t inputs[] = { keybag };
	IOReturn kernResult = IOConnectCallMethod(keystore,
        kAppleKeyStoreKeyBagRelease, inputs, 1, NULL, 0, NULL, NULL, NULL, 0);
    if (kernResult) {
        asl_log(NULL, NULL, ASL_LEVEL_ERR,
            "kAppleKeyStoreKeyBagRelease: %d: %x", keybag, kernResult);
    }
#endif
}

static OSStatus SecServerKeychainBackup(s3dl_db_thread *dbt, CFDataRef keybag,
    CFDataRef password, CFDataRef *backup) {
    OSStatus status;
    keybag_handle_t backup_keybag = ks_open_keybag(keybag, password);
    /* Export from system keybag to backup keybag. */
    status = SecServerExportKeychain(dbt, KEYBAG_DEVICE, backup_keybag,
        backup);
    ks_close_keybag(backup_keybag);
    return status;
}

static OSStatus SecServerKeychainRestore(s3dl_db_thread *dbt, CFDataRef backup,
    CFDataRef keybag, CFDataRef password) {
    OSStatus status;
    keybag_handle_t backup_keybag = ks_open_keybag(keybag, password);
    /* Import from backup keybag to system keybag. */
    status = SecServerImportKeychain(dbt, backup_keybag, KEYBAG_DEVICE,
        backup);
    ks_close_keybag(backup_keybag);
    return status;
}

/* External SPI support code. */

/* Pthread_once protecting the kc_dbhandle and the singleton kc_dbhandle. */
static pthread_once_t kc_dbhandle_init_once = PTHREAD_ONCE_INIT;
static db_handle kc_dbhandle = NULL;

/* This function is called only once and should initialize kc_dbhandle. */
static void kc_dbhandle_init(void)
{
#if 0
    CFTypeRef kc_attributes[] = {
        kSecAttrAccessible,
        kSecAttrAccessGroup,
        kSecAttrCreationDate,
        kSecAttrModificationDate,
        kSecAttrDescription,
        kSecAttrComment,
        kSecAttrCreator,
        kSecAttrType,
        kSecAttrLabel,
        kSecAttrIsInvisible,
        kSecAttrIsNegative,
        kSecAttrAccount,
        kSecAttrService,
        kSecAttrGeneric,
        kSecAttrSecurityDomain,
        kSecAttrServer,
        kSecAttrProtocol,
        kSecAttrAuthenticationType,
        kSecAttrPort,
        kSecAttrPath,
        kSecAttrSubject,
        kSecAttrIssuer,
        kSecAttrSerialNumber,
        kSecAttrSubjectKeyID,
        kSecAttrPublicKeyHash,
        kSecAttrCertificateType,
        kSecAttrCertificateEncoding,
        kSecAttrKeyClass,
        kSecAttrApplicationLabel,
        kSecAttrIsPermanent,
        kSecAttrApplicationTag,
        kSecAttrKeyType,
        kSecAttrKeySizeInBits,
        kSecAttrEffectiveKeySize,
        kSecAttrCanEncrypt,
        kSecAttrCanDecrypt,
        kSecAttrCanDerive,
        kSecAttrCanSign,
        kSecAttrCanVerify,
        kSecAttrCanWrap,
        kSecAttrCanUnwrap,
        kSecAttrScriptCode,
        kSecAttrAlias,
        kSecAttrHasCustomIcon,
        kSecAttrVolume,
        kSecAttrAddress,
        kSecAttrAFPServerSignature,
        kSecAttrCRLType,
        kSecAttrCRLEncoding,
        kSecAttrKeyCreator,
        kSecAttrIsPrivate,
        kSecAttrIsModifiable,
        kSecAttrStartDate,
        kSecAttrEndDate,
        kSecAttrIsSensitive,
        kSecAttrWasAlwaysSensitive,
        kSecAttrIsExtractable,
        kSecAttrWasNeverExtractable,
        kSecAttrCanSignRecover,
        kSecAttrCanVerifyRecover
    };
#endif
    CFTypeRef kc_class_names[] = {
        kSecClassGenericPassword,
        kSecClassInternetPassword,
        kSecClassCertificate,
        kSecClassKey,
        kSecClassIdentity
    };
    const void *kc_classes[] = {
        &genp_class,
        &inet_class,
        &cert_class,
        &keys_class,
        &identity_class
    };

#if 0
    gAttributes = CFSetCreate(kCFAllocatorDefault, kc_attributes,
        sizeof(kc_attributes) / sizeof(*kc_attributes), &kCFTypeSetCallBacks);
#endif
    gClasses = CFDictionaryCreate(kCFAllocatorDefault, kc_class_names,
                                  kc_classes,
                                  sizeof(kc_classes) / sizeof(*kc_classes),
                                  &kCFTypeDictionaryKeyCallBacks, 0);

	const char *kcRelPath;

	bool use_hwaes = hwaes_key_available();
	if (use_hwaes) {
		asl_log(NULL, NULL, ASL_LEVEL_INFO, "using hwaes key");
		kcRelPath = "/Library/Keychains/keychain-2.db";
	} else {
		asl_log(NULL, NULL, ASL_LEVEL_ERR, "unable to access hwaes key");
		kcRelPath = "/Library/Keychains/keychain-2-debug.db";
	}

	bool autocommit = true;
	bool create = true;

#if NO_SERVER
        /* Added this block of code back to keep the tests happy for now. */
	const char *home = getenv("HOME");
	char path[PATH_MAX];
	size_t homeLen = strlen(home);
	size_t kcRelPathLen = strlen(kcRelPath);
        if (homeLen + kcRelPathLen > sizeof(path))
            return;
        strlcpy(path, home, sizeof(path));
        strlcat(path, kcRelPath, sizeof(path));
        kcRelPath = path;
#endif
	s3dl_create_db_handle(kcRelPath, &kc_dbhandle, NULL /* dbt */, autocommit,
		create, use_hwaes);
}

#if NO_SERVER
void kc_dbhandle_reset(void);
void kc_dbhandle_reset(void)
{
    s3dl_close_db_handle(kc_dbhandle);
    kc_dbhandle_init();
}
#endif


/* Return a per thread dbt handle for the keychain.  If create is true create
   the database if it does not yet exist.  If it is false, just return an
   error if it fails to auto-create. */
static int kc_get_dbt(s3dl_db_thread **dbt, bool create)
{
    return s3dl_get_dbt(kc_dbhandle, dbt);
}

static int kc_release_dbt(s3dl_db_thread *dbt)
{
#if CLOSE_DB
	s3dl_dbt_destructor(dbt);
	pthread_setspecific(dbt->db->key, NULL);
	//int s3e = s3dl_close_db_handle(dbt->db);
	//return s3e;
#endif
	return SQLITE_OK;
}


/****************************************************************************
 **************** Beginning of Externally Callable Interface ****************
 ****************************************************************************/


/* AUDIT[securityd](done):
   query (ok) is a caller provided dictionary, only its cf type has been checked.
 */
OSStatus
_SecItemCopyMatching(CFDictionaryRef query, CFTypeRef *result,
    CFArrayRef accessGroups)
{
	pthread_once(&kc_dbhandle_init_once, kc_dbhandle_init);
    CFIndex ag_count;
    if (!accessGroups || 0 == (ag_count = CFArrayGetCount(accessGroups)))
        return errSecMissingEntitlement;

    /* Having the special accessGroup "*" allows access to all accessGroups. */
    if (CFArrayContainsValue(accessGroups, CFRangeMake(0, ag_count), CFSTR("*")))
        accessGroups = NULL;

    OSStatus error = 0;
    Query *q = query_create_with_limit(query, 1, &error);
    if (q) {
        CFStringRef agrp = CFDictionaryGetValue(q->q_item, kSecAttrAccessGroup);
        if (agrp && accessGroupsAllows(accessGroups, agrp)) {
            const void *val = agrp;
            accessGroups = CFArrayCreate(0, &val, 1, &kCFTypeArrayCallBacks);
        } else {
            CFRetainSafe(accessGroups);
        }

        /* Sanity check the query. */
        if (q->q_use_item_list) {
            error = errSecUseItemListUnsupported;
#if defined(MULTIPLE_KEYCHAINS)
        } else if (q->q_use_keychain) {
            error = errSecUseKeychainUnsupported;
#endif
        } else if (q->q_return_type != 0 && result == NULL) {
            error = errSecReturnMissingPointer;
        } else if (!q->q_error) {
            s3dl_db_thread *dbt;
            int s3e = s3dl_get_dbt(kc_dbhandle, &dbt);
            if (s3e == SQLITE_OK) {
                s3e = s3dl_copy_matching(dbt, q, result, accessGroups);
                /* TODO: Check error of this function if s3e is noErr. */
                kc_release_dbt(dbt);
            }
            if (s3e)
                error = osstatus_for_s3e(s3e);
        }

        CFReleaseSafe(accessGroups);
        query_destroy(q, &error);
    }

	return error;
}

/* AUDIT[securityd](done):
   attributes (ok) is a caller provided dictionary, only its cf type has
       been checked.
 */
OSStatus
_SecItemAdd(CFDictionaryRef attributes, CFTypeRef *result,
    CFArrayRef accessGroups)
{
	pthread_once(&kc_dbhandle_init_once, kc_dbhandle_init);
    CFIndex ag_count;
    if (!accessGroups || 0 == (ag_count = CFArrayGetCount(accessGroups)))
        return errSecMissingEntitlement;

    OSStatus error = 0;
    Query *q = query_create_with_limit(attributes, 0, &error);
    if (q) {
        /* Access group sanity checking. */
        CFStringRef agrp = (CFStringRef)CFDictionaryGetValue(attributes,
            kSecAttrAccessGroup);

        CFArrayRef ag = accessGroups;
        /* Having the special accessGroup "*" allows access to all accessGroups. */
        if (CFArrayContainsValue(accessGroups, CFRangeMake(0, ag_count), CFSTR("*")))
            accessGroups = NULL;

        if (agrp) {
            /* The user specified an explicit access group, validate it. */
            if (!accessGroupsAllows(accessGroups, agrp))
                return errSecNoAccessForItem;
        } else {
            agrp = (CFStringRef)CFArrayGetValueAtIndex(ag, 0);

            /* We are using an implicit access group, add it as if the user
               specified it as an attribute. */
            query_add_attribute(kSecAttrAccessGroup, agrp, q);
        }

        query_ensure_keyclass(q, agrp);

        if (q->q_row_id)
            error = errSecValuePersistentRefUnsupported;
    #if defined(MULTIPLE_KEYCHAINS)
        else if (q->q_use_keychain_list)
            error = errSecUseKeychainListUnsupported;
    #endif
        else if (!q->q_error) {
            s3dl_db_thread *dbt;
            int s3e = s3dl_get_dbt(kc_dbhandle, &dbt);
            if (s3e == SQLITE_OK) {
                s3e = s3dl_begin_transaction(dbt);
                if (s3e == SQLITE_OK) {
                    query_pre_add(q, true);
                    s3e = s3dl_query_add(dbt, q, result);
                }
                s3e = s3dl_end_transaction(dbt, s3e);
            }

            /* TODO: Check error on this function if s3e is 0. */
            kc_release_dbt(dbt);

            if (s3e)
                error = osstatus_for_s3e(s3e);
        }
        query_destroy(q, &error);
    }
    return error;
}

/* AUDIT[securityd](done):
   query (ok) and attributesToUpdate (ok) are a caller provided dictionaries,
       only their cf types have been checked.
 */
OSStatus
_SecItemUpdate(CFDictionaryRef query,
    CFDictionaryRef attributesToUpdate, CFArrayRef accessGroups)
{
	pthread_once(&kc_dbhandle_init_once, kc_dbhandle_init);
    CFIndex ag_count;
    if (!accessGroups || 0 == (ag_count = CFArrayGetCount(accessGroups)))
        return errSecMissingEntitlement;

    /* Having the special accessGroup "*" allows access to all accessGroups. */
    if (CFArrayContainsValue(accessGroups, CFRangeMake(0, ag_count), CFSTR("*")))
        accessGroups = NULL;

    OSStatus error = 0;
    Query *q = query_create_with_limit(query, kSecMatchUnlimited, &error);
    if (q) {
        /* Sanity check the query. */
        if (q->q_use_item_list) {
            error = errSecUseItemListUnsupported;
        } else if (q->q_return_type & kSecReturnDataMask) {
            /* Update doesn't return anything so don't ask for it. */
            error = errSecReturnDataUnsupported;
        } else if (q->q_return_type & kSecReturnAttributesMask) {
            error = errSecReturnAttributesUnsupported;
        } else if (q->q_return_type & kSecReturnRefMask) {
            error = errSecReturnRefUnsupported;
        } else {
            /* Access group sanity checking. */
            CFStringRef agrp = (CFStringRef)CFDictionaryGetValue(attributesToUpdate,
                kSecAttrAccessGroup);
            if (agrp) {
                /* The user is attempting to modify the access group column,
                   validate it to make sure the new value is allowable. */
                if (!accessGroupsAllows(accessGroups, agrp))
                    error = errSecNoAccessForItem;
            }

            if (!error) {
                s3dl_db_thread *dbt;
                int s3e = s3dl_get_dbt(kc_dbhandle, &dbt);
                if (s3e == SQLITE_OK) {
                    s3e = s3dl_query_update(dbt, q, attributesToUpdate, accessGroups);

                    /* TODO: Check error on this function if s3e is 0. */
                    kc_release_dbt(dbt);
                }

                if (s3e)
                    error = osstatus_for_s3e(s3e);
            }
        }
        query_destroy(q, &error);
    }
    return error;
}

/* AUDIT[securityd](done):
   query (ok) is a caller provided dictionary, only its cf type has been checked.
 */
OSStatus
_SecItemDelete(CFDictionaryRef query, CFArrayRef accessGroups)
{
	pthread_once(&kc_dbhandle_init_once, kc_dbhandle_init);
    CFIndex ag_count;
    if (!accessGroups || 0 == (ag_count = CFArrayGetCount(accessGroups)))
        return errSecMissingEntitlement;

    /* Having the special accessGroup "*" allows access to all accessGroups. */
    if (CFArrayContainsValue(accessGroups, CFRangeMake(0, ag_count), CFSTR("*")))
        accessGroups = NULL;

    OSStatus error = 0;
    Query *q = query_create_with_limit(query, kSecMatchUnlimited, &error);
    if (q) {
        /* Sanity check the query. */
        if (q->q_limit != kSecMatchUnlimited)
            error = errSecMatchLimitUnsupported;
        else if (query_match_count(q) != 0)
            error = errSecItemMatchUnsupported;
        else if (q->q_ref)
            error = errSecValueRefUnsupported;
        else if (q->q_row_id && query_attr_count(q))
            error = errSecItemIllegalQuery;
        else {
            s3dl_db_thread *dbt;
            int s3e = s3dl_get_dbt(kc_dbhandle, &dbt);
            if (s3e == SQLITE_OK) {
                s3e = s3dl_query_delete(dbt, q, accessGroups);

                /* TODO: Check error on this function if s3e is 0. */
                kc_release_dbt(dbt);
            }
            if (s3e)
                error = osstatus_for_s3e(s3e);
        }
        query_destroy(q, &error);
    }
    return error;
}

/* AUDIT[securityd](done):
   No caller provided inputs.
 */
bool
_SecItemDeleteAll(void)
{
	pthread_once(&kc_dbhandle_init_once, kc_dbhandle_init);
    static const char deleteAllSQL[] = "BEGIN EXCLUSIVE TRANSACTION; "
            "DELETE from inet; DELETE from cert; DELETE from keys; DELETE from genp; "
            "COMMIT TRANSACTION; VACUUM;";

	s3dl_db_thread *dbt;
	int s3e = kc_get_dbt(&dbt, true);
    if (s3e == SQLITE_OK) {
        s3e = sqlite3_exec(dbt->s3_handle, deleteAllSQL, NULL, NULL, NULL);
        kc_release_dbt(dbt);
    }
    return (s3e == SQLITE_OK);
}

/* TODO: Move to a location shared between securityd and Security framework. */
static const char *restore_keychain_location = "/Library/Keychains/keychain.restoring";

/* AUDIT[securityd](done):
   No caller provided inputs.
 */
OSStatus
_SecServerRestoreKeychain(void)
{
    static db_handle restore_dbhandle = NULL;
	s3dl_db_thread *restore_dbt = NULL, *dbt = NULL;
    CFDataRef backup = NULL;
    OSStatus status = errSecSuccess;
    int s3e;

	pthread_once(&kc_dbhandle_init_once, kc_dbhandle_init);
	require_noerr(s3e = s3dl_get_dbt(kc_dbhandle, &dbt), errOut);

    /* Export everything from the keychain we are restoring, this upgrades it
       to whatever version is current first if needed. */
	bool use_hwaes = hwaes_key_available();
	require_noerr(s3e = s3dl_create_db_handle(restore_keychain_location,
        &restore_dbhandle, &restore_dbt, true, false, use_hwaes), errOut);
    require_noerr(status = SecServerExportKeychain(restore_dbt, KEYBAG_DEVICE,
        KEYBAG_NONE, &backup), errOut);
    require_noerr(status = SecServerImportKeychain(dbt, KEYBAG_NONE,
        KEYBAG_DEVICE, backup), errOut);

errOut:
    s3e = s3dl_close_db_handle(restore_dbhandle);
    CFReleaseSafe(backup);
    kc_release_dbt(restore_dbt);
    kc_release_dbt(dbt);

	if (s3e != SQLITE_OK)
		return osstatus_for_s3e(s3e);
    return status;
}

/* AUDIT[securityd](done):
   args_in (ok) is a caller provided, CFArrayRef.
 */
OSStatus
_SecServerMigrateKeychain(CFArrayRef args_in, CFTypeRef *args_out)
{
	pthread_once(&kc_dbhandle_init_once, kc_dbhandle_init);
    CFMutableArrayRef args = NULL;
    CFNumberRef hin = NULL, hout = NULL;
    int32_t handle_in, handle_out = 0;
    CFDataRef data_in, data_out = NULL;
    OSStatus status = errSecParam;
    CFIndex argc = CFArrayGetCount(args_in);

	s3dl_db_thread *dbt;
	int s3e = s3dl_get_dbt(kc_dbhandle, &dbt);
	if (s3e != SQLITE_OK)
		return osstatus_for_s3e(s3e);

    require_quiet(argc == 1 || argc == 2, errOut);
    hin = (CFNumberRef)CFArrayGetValueAtIndex(args_in, 0);
    require_quiet(isNumberOfType(hin, kCFNumberSInt32Type), errOut);
    require_quiet(CFNumberGetValue(hin, kCFNumberSInt32Type, &handle_in), errOut);
    if (argc > 1) {
        data_in = (CFDataRef)CFArrayGetValueAtIndex(args_in, 1);
        require_quiet(data_in, errOut);
        require_quiet(CFGetTypeID(data_in) == CFDataGetTypeID(), errOut);
    } else {
        data_in = NULL;
    }

    secdebug("migrate", "migrate: %d %d", handle_in, data_in);

    status = SecServerMigrateKeychain(dbt, handle_in, data_in, &handle_out, &data_out);

    require_quiet(args = CFArrayCreateMutable(kCFAllocatorDefault, 2, &kCFTypeArrayCallBacks), errOut);
    require_quiet(hout = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &handle_out), errOut);
    CFArrayAppendValue(args, hout);
    if (data_out)
        CFArrayAppendValue(args, data_out);
    *args_out = args;
    args = NULL;

errOut:
    kc_release_dbt(dbt);
    CFReleaseSafe(args);
    CFReleaseSafe(hout);
    CFReleaseSafe(data_out);
	return status;
}

OSStatus
_SecServerKeychainBackup(CFArrayRef args_in, CFTypeRef *args_out) {
	pthread_once(&kc_dbhandle_init_once, kc_dbhandle_init);
    OSStatus status = errSecParam;
    CFIndex argc = args_in ? CFArrayGetCount(args_in) : 0;
    CFDataRef backup = NULL;

	s3dl_db_thread *dbt;
	int s3e = s3dl_get_dbt(kc_dbhandle, &dbt);
	if (s3e != SQLITE_OK)
		return osstatus_for_s3e(s3e);

    require_quiet(args_out != NULL, errOut);
    if (argc == 0) {
#if USE_KEYSTORE
        require_noerr_quiet(status = SecServerExportKeychain(dbt, KEYBAG_DEVICE, backup_keybag_handle, &backup), errOut);
#else
        goto errOut;
#endif
    }
    else if (argc == 1 || argc == 2) {
        CFDataRef keybag = (CFDataRef)CFArrayGetValueAtIndex(args_in, 0);
        require_quiet(isData(keybag), errOut);
        CFDataRef password;
        if (argc > 1) {
            password = (CFDataRef)CFArrayGetValueAtIndex(args_in, 1);
            require_quiet(isData(password), errOut);
        } else {
            password = NULL;
        }
        require_noerr_quiet(status = SecServerKeychainBackup(dbt, keybag, password, &backup), errOut);
    }
    *args_out = backup;

errOut:
    kc_release_dbt(dbt);
    return status;
}

OSStatus
_SecServerKeychainRestore(CFArrayRef args_in, CFTypeRef *dummy) {
	pthread_once(&kc_dbhandle_init_once, kc_dbhandle_init);
    OSStatus status = errSecParam;
    CFIndex argc = CFArrayGetCount(args_in);

	s3dl_db_thread *dbt;
	int s3e = s3dl_get_dbt(kc_dbhandle, &dbt);
	if (s3e != SQLITE_OK)
		return osstatus_for_s3e(s3e);

    require_quiet(argc == 2 || argc == 3, errOut);
    CFDataRef backup = (CFDataRef)CFArrayGetValueAtIndex(args_in, 0);
    require_quiet(isData(backup), errOut);
    CFDataRef keybag = (CFDataRef)CFArrayGetValueAtIndex(args_in, 1);
    require_quiet(isData(keybag), errOut);
    CFDataRef password;
    if (argc > 2) {
        password = (CFDataRef)CFArrayGetValueAtIndex(args_in, 2);
        require_quiet(isData(password), errOut);
    } else {
        password = NULL;
    }

    status = SecServerKeychainRestore(dbt, backup, keybag, password);
    if (!backup) {
    }
    if (dummy) {
        *dummy = NULL;
    }

    status = errSecSuccess;
errOut:
    kc_release_dbt(dbt);
    return status;
}
