/*
 * Copyright (c) 2006-2013 Apple Inc. All Rights Reserved.
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
#include <securityd/SecDbItem.h>

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
#include <utilities/SecIOFormat.h>
#include <utilities/SecCFWrappers.h>
#include <utilities/SecCFError.h>
#include <utilities/der_plist.h>
#include <limits.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <Security/SecBase.h>
#include <CoreFoundation/CFData.h>
#include <CoreFoundation/CFDate.h>
#include <CoreFoundation/CFArray.h>
#include <CoreFoundation/CFDictionary.h>
#include <CoreFoundation/CFNumber.h>
#include <CoreFoundation/CFString.h>
#include <CoreFoundation/CFURL.h>
#include <CommonCrypto/CommonDigest.h>
#include <CommonCrypto/CommonDigestSPI.h>
#include <CommonCrypto/CommonCryptor.h>
#include <CommonCrypto/CommonCryptorSPI.h>
#include <libkern/OSByteOrder.h>
#include <utilities/debugging.h>
#include <assert.h>
#include <Security/SecInternal.h>
#include "securityd_client.h"
#include "utilities/sqlutils.h"
#include "utilities/SecIOFormat.h"
#include "utilities/SecFileLocations.h"
#include <utilities/iCloudKeychainTrace.h>
#include <AssertMacros.h>
#include <asl.h>
#include <inttypes.h>
#include <utilities/array_size.h>
#include <utilities/SecDb.h>
#include <securityd/SOSCloudCircleServer.h>
#include <notify.h>
#include "OTATrustUtilities.h"

#if USE_KEYSTORE
#include <IOKit/IOKitLib.h>
#include <libaks.h>
#if TARGET_OS_EMBEDDED
#include <MobileKeyBag/MobileKeyBag.h>
#endif
#endif /* USE_KEYSTORE */


/* g_keychain_handle is the keybag handle used for encrypting item in the keychain.
   For testing purposes, it can be set to something other than the default, with SecItemServerSetKeychainKeybag */
#if USE_KEYSTORE
#if TARGET_OS_MAC && !TARGET_OS_EMBEDDED
static keybag_handle_t g_keychain_keybag = session_keybag_handle;
#else
static keybag_handle_t g_keychain_keybag = device_keybag_handle;
#endif
#else /* !USE_KEYSTORE */
static int32_t g_keychain_keybag = 0; /* 0 == device_keybag_handle, constant dictated by AKS */
#endif /* USE_KEYSTORE */

void SecItemServerSetKeychainKeybag(int32_t keybag)
{
    g_keychain_keybag=keybag;
}

void SecItemServerResetKeychainKeybag(void)
{
#if USE_KEYSTORE
#if TARGET_OS_MAC && !TARGET_OS_EMBEDDED
    g_keychain_keybag = session_keybag_handle;
#else
    g_keychain_keybag = device_keybag_handle;
#endif
#else /* !USE_KEYSTORE */
    g_keychain_keybag = 0; /* 0 == device_keybag_handle, constant dictated by AKS */
#endif /* USE_KEYSTORE */
}

/* KEYBAG_NONE is private to security and have special meaning.
   They should not collide with AppleKeyStore constants, but are only referenced
   in here.
 */
#define KEYBAG_NONE (-1)   /* Set q_keybag to KEYBAG_NONE to obtain cleartext data. */
#define KEYBAG_DEVICE (g_keychain_keybag) /* actual keybag used to encrypt items */

/* Changed the name of the keychain changed notification, for testing */
static const char *g_keychain_changed_notification = kSecServerKeychainChangedNotification;

void SecItemServerSetKeychainChangedNotification(const char *notification_name)
{
    g_keychain_changed_notification = notification_name;
}

/* label when certificate data is joined with key data */
#define CERTIFICATE_DATA_COLUMN_LABEL "certdata"

#define CURRENT_DB_VERSION 6

#define CLOSE_DB  0

/* Forward declaration of import export SPIs. */
enum SecItemFilter {
    kSecNoItemFilter,
    kSecSysBoundItemFilter,
    kSecBackupableItemFilter,
};

static CF_RETURNS_RETAINED CFDictionaryRef SecServerExportKeychainPlist(SecDbConnectionRef dbt,
    keybag_handle_t src_keybag, keybag_handle_t dest_keybag,
    enum SecItemFilter filter, CFErrorRef *error);
static bool SecServerImportKeychainInPlist(SecDbConnectionRef dbt,
    keybag_handle_t src_keybag, keybag_handle_t dest_keybag,
    CFDictionaryRef keychain, enum SecItemFilter filter, CFErrorRef *error);

#if USE_KEYSTORE

static bool hwaes_key_available(void)
{
    keybag_handle_t handle = bad_keybag_handle;
    keybag_handle_t special_handle = bad_keybag_handle;
#if TARGET_OS_MAC && !TARGET_OS_EMBEDDED
    special_handle = session_keybag_handle;
#elif TARGET_OS_EMBEDDED
    special_handle = device_keybag_handle;
#endif
    kern_return_t kr = aks_get_system(special_handle, &handle);
    if (kr != kIOReturnSuccess) {
#if TARGET_OS_EMBEDDED
        /* TODO: Remove this once the kext runs the daemon on demand if
         there is no system keybag. */
        int kb_state = MKBGetDeviceLockState(NULL);
        asl_log(NULL, NULL, ASL_LEVEL_INFO, "AppleKeyStore lock state: %d", kb_state);
#endif
    }
    return true;
}

#else /* !USE_KEYSTORE */

static bool hwaes_key_available(void)
{
	return false;
}

#endif /* USE_KEYSTORE */

// MARK -
// MARK Keychain version 6 schema

#define __FLAGS(ARG, ...) SECDBFLAGS(__VA_ARGS__)
#define SECDBFLAGS(ARG, ...) __FLAGS_##ARG | __FLAGS(__VA_ARGS__)

#define SecDbFlags(P,L,I,S,A,D,R,C,H,B,Z,E,N) (__FLAGS_##P|__FLAGS_##L|__FLAGS_##I|__FLAGS_##S|__FLAGS_##A|__FLAGS_##D|__FLAGS_##R|__FLAGS_##C|__FLAGS_##H|__FLAGS_##B|__FLAGS_##Z|__FLAGS_##E|__FLAGS_##N)

#define __FLAGS_   0
#define __FLAGS_P  kSecDbPrimaryKeyFlag
#define __FLAGS_L  kSecDbInFlag
#define __FLAGS_I  kSecDbIndexFlag
#define __FLAGS_S  kSecDbSHA1ValueInFlag
#define __FLAGS_A  kSecDbReturnAttrFlag
#define __FLAGS_D  kSecDbReturnDataFlag
#define __FLAGS_R  kSecDbReturnRefFlag
#define __FLAGS_C  kSecDbInCryptoDataFlag
#define __FLAGS_H  kSecDbInHashFlag
#define __FLAGS_B  kSecDbInBackupFlag
#define __FLAGS_Z  kSecDbDefault0Flag
#define __FLAGS_E  kSecDbDefaultEmptyFlag
#define __FLAGS_N  kSecDbNotNullFlag

//                                                                   ,------------- P : Part of primary key
//                                                                  / ,------------ L : Stored in local database
//                                                                 / / ,----------- I : Attribute wants an index in the database
//                                                                / / / ,---------- S : SHA1 hashed attribute value in database (implies L)
//                                                               / / / / ,--------- A : Returned to client as attribute in queries
//                                                              / / / / / ,-------- D : Returned to client as data in queries
//                                                             / / / / / / ,------- R : Returned to client as ref/persistant ref in queries
//                                                            / / / / / / / ,------ C : Part of encrypted blob
//                                                           / / / / / / / / ,----- H : Attribute is part of item SHA1 hash (Implied by C)
//                                                          / / / / / / / / / ,---- B : Attribute is part of iTunes/iCloud backup bag
//                                                         / / / / / / / / / / ,--- Z : Attribute has a default value of 0
//                                                        / / / / / / / / / / / ,-- E : Attribute has a default value of "" or empty data
//                                                       / / / / / / / / / / / / ,- N : Attribute must have a value
//                                                      / / / / / / / / / / / / /
//                                                     / / / / / / / / / / / / /
//                                                    | | | | | | | | | | | | |
// common to all                                      | | | | | | | | | | | | |
SECDB_ATTR(v6rowid, "rowid", RowId,        SecDbFlags( ,L, , , , ,R, , ,B, , , ));
SECDB_ATTR(v6cdat, "cdat", CreationDate,   SecDbFlags( ,L, , ,A, , ,C,H, , , , ));
SECDB_ATTR(v6mdat, "mdat",ModificationDate,SecDbFlags( ,L, , ,A, , ,C,H, , , , ));
SECDB_ATTR(v6labl, "labl", Blob,           SecDbFlags( ,L, ,S,A, , ,C,H, , , , ));
SECDB_ATTR(v6data, "data", EncryptedData,  SecDbFlags( ,L, , , , , , , ,B, , , ));
SECDB_ATTR(v6agrp, "agrp", String,         SecDbFlags(P,L, , ,A, , ,C,H, , , , ));
SECDB_ATTR(v6pdmn, "pdmn", Access,         SecDbFlags( ,L, , ,A, , ,C,H, , , , ));
SECDB_ATTR(v6sync, "sync", Sync,           SecDbFlags(P,L,I, ,A, , ,C,H, ,Z, ,N));
SECDB_ATTR(v6tomb, "tomb", Tomb,           SecDbFlags( ,L, , , , , ,C,H, ,Z, ,N));
SECDB_ATTR(v6sha1, "sha1", SHA1,           SecDbFlags( ,L,I, ,A, ,R, , , , , , ));
SECDB_ATTR(v6v_Data, "v_Data", Data,       SecDbFlags( , , , , ,D, ,C,H, , , , ));
SECDB_ATTR(v6v_pk, "v_pk", PrimaryKey,     SecDbFlags( , , , , , , , , , , , , ));
// genp and inet and keys                             | | | | | | | | | | | | |
SECDB_ATTR(v6crtr, "crtr", Number,         SecDbFlags( ,L, , ,A, , ,C,H, , , , ));
SECDB_ATTR(v6alis, "alis", Blob,           SecDbFlags( ,L, ,S,A, , ,C,H, , , , ));
// genp and inet                                      | | | | | | | | | | | | |
SECDB_ATTR(v6desc, "desc", Blob,           SecDbFlags( ,L, ,S,A, , ,C,H, , , , ));
SECDB_ATTR(v6icmt, "icmt", Blob,           SecDbFlags( ,L, ,S,A, , ,C,H, , , , ));
SECDB_ATTR(v6type, "type", Number,         SecDbFlags( ,L, , ,A, , ,C,H, , , , ));
SECDB_ATTR(v6invi, "invi", Number,         SecDbFlags( ,L, , ,A, , ,C,H, , , , ));
SECDB_ATTR(v6nega, "nega", Number,         SecDbFlags( ,L, , ,A, , ,C,H, , , , ));
SECDB_ATTR(v6cusi, "cusi", Number,         SecDbFlags( ,L, , ,A, , ,C,H, , , , ));
SECDB_ATTR(v6prot, "prot", Blob,           SecDbFlags( ,L, ,S,A, , ,C,H, , , , ));
SECDB_ATTR(v6scrp, "scrp", Number,         SecDbFlags( ,L, , ,A, , ,C,H, , , , ));
SECDB_ATTR(v6acct, "acct", Blob,           SecDbFlags(P,L, ,S,A, , ,C,H, , ,E,N));
// genp only                                          | | | | | | | | | | | | |
SECDB_ATTR(v6svce, "svce", Blob,           SecDbFlags(P,L, ,S,A, , ,C,H, , ,E,N));
SECDB_ATTR(v6gena, "gena", Blob,           SecDbFlags( ,L, ,S,A, , ,C,H, , , , ));
// inet only                                          | | | | | | | | | | | | |
SECDB_ATTR(v6sdmn, "sdmn", Blob,           SecDbFlags(P,L, ,S,A, , ,C,H, , ,E,N));
SECDB_ATTR(v6srvr, "srvr", Blob,           SecDbFlags(P,L, ,S,A, , ,C,H, , ,E,N));
SECDB_ATTR(v6ptcl, "ptcl", Number,         SecDbFlags(P,L, , ,A, , ,C,H, ,Z, ,N));
SECDB_ATTR(v6atyp, "atyp", Blob,           SecDbFlags(P,L, ,S,A, , ,C,H, , ,E,N));
SECDB_ATTR(v6port, "port", Number,         SecDbFlags(P,L, , ,A, , ,C,H, ,Z, ,N));
SECDB_ATTR(v6path, "path", Blob,           SecDbFlags(P,L, ,S,A, , ,C,H, , ,E,N));
// cert only                                          | | | | | | | | | | | | |
SECDB_ATTR(v6ctyp, "ctyp", Number,         SecDbFlags(P,L, , ,A, , ,C,H, ,Z, ,N));
SECDB_ATTR(v6cenc, "cenc", Number,         SecDbFlags( ,L, , ,A, , ,C,H, , , , ));
SECDB_ATTR(v6subj, "subj", Data,           SecDbFlags( ,L,I,S,A, , ,C,H, , , , ));
SECDB_ATTR(v6issr, "issr", Data,           SecDbFlags(P,L, ,S,A, , ,C,H, , ,E,N));
SECDB_ATTR(v6slnr, "slnr", Data,           SecDbFlags(P,L, ,S,A, , ,C,H, , ,E,N));
SECDB_ATTR(v6skid, "skid", Data,           SecDbFlags( ,L,I,S,A, , ,C,H, , , , ));
SECDB_ATTR(v6pkhh, "pkhh", Data,           SecDbFlags( ,L,I, ,A, , ,C,H, , , , ));
// cert attributes that share names with common ones but have different flags
SECDB_ATTR(v6certalis, "alis", Blob,       SecDbFlags( ,L,I,S,A, , ,C,H, , , , ));
// keys only                                          | | | | | | | | | | | | |
SECDB_ATTR(v6kcls, "kcls", Number,         SecDbFlags(P,L,I,S,A, , ,C,H, ,Z, ,N));
SECDB_ATTR(v6perm, "perm", Number,         SecDbFlags( ,L, , ,A, , ,C,H, , , , ));
SECDB_ATTR(v6priv, "priv", Number,         SecDbFlags( ,L, , ,A, , ,C,H, , , , ));
SECDB_ATTR(v6modi, "modi", Number,         SecDbFlags( ,L, , ,A, , ,C,H, , , , ));
SECDB_ATTR(v6klbl, "klbl", Data,           SecDbFlags(P,L,I, ,A, , ,C,H, , ,E,N));
SECDB_ATTR(v6atag, "atag", Blob,           SecDbFlags(P,L, ,S,A, , ,C,H, , ,E,N));
SECDB_ATTR(v6bsiz, "bsiz", Number,         SecDbFlags(P,L, , ,A, , ,C,H, ,Z, ,N));
SECDB_ATTR(v6esiz, "esiz", Number,         SecDbFlags(P,L, , ,A, , ,C,H, ,Z, ,N));
SECDB_ATTR(v6sdat, "sdat", Date,           SecDbFlags(P,L, , ,A, , ,C,H, ,Z, ,N));
SECDB_ATTR(v6edat, "edat", Date,           SecDbFlags(P,L, , ,A, , ,C,H, ,Z, ,N));
SECDB_ATTR(v6sens, "sens", Number,         SecDbFlags( ,L, , ,A, , ,C,H, , , , ));
SECDB_ATTR(v6asen, "asen", Number,         SecDbFlags( ,L, , ,A, , ,C,H, , , , ));
SECDB_ATTR(v6extr, "extr", Number,         SecDbFlags( ,L, , ,A, , ,C,H, , , , ));
SECDB_ATTR(v6next, "next", Number,         SecDbFlags( ,L, , ,A, , ,C,H, , , , ));
SECDB_ATTR(v6encr, "encr", Number,         SecDbFlags( ,L,I, ,A, , ,C,H, , , , ));
SECDB_ATTR(v6decr, "decr", Number,         SecDbFlags( ,L,I, ,A, , ,C,H, , , , ));
SECDB_ATTR(v6drve, "drve", Number,         SecDbFlags( ,L,I, ,A, , ,C,H, , , , ));
SECDB_ATTR(v6sign, "sign", Number,         SecDbFlags( ,L,I, ,A, , ,C,H, , , , ));
SECDB_ATTR(v6vrfy, "vrfy", Number,         SecDbFlags( ,L,I, ,A, , ,C,H, , , , ));
SECDB_ATTR(v6snrc, "snrc", Number,         SecDbFlags( ,L, , ,A, , ,C,H, , , , ));
SECDB_ATTR(v6vyrc, "vyrc", Number,         SecDbFlags( ,L, , ,A, , ,C,H, , , , ));
SECDB_ATTR(v6wrap, "wrap", Number,         SecDbFlags( ,L,I, ,A, , ,C,H, , , , ));
SECDB_ATTR(v6unwp, "unwp", Number,         SecDbFlags( ,L,I, ,A, , ,C,H, , , , ));
// keys attributes that share names with common ones but have different flags
SECDB_ATTR(v6keytype, "type", Number,      SecDbFlags(P,L, , ,A, , ,C,H, ,Z, ,N));
SECDB_ATTR(v6keycrtr, "crtr", Number,      SecDbFlags(P,L, , ,A, , ,C,H, ,Z, ,N));

static const SecDbClass genp_class = {
    .name = CFSTR("genp"),
    .attrs = {
        &v6rowid,
        &v6cdat,
        &v6mdat,
        &v6desc,
        &v6icmt,
        &v6crtr,
        &v6type,
        &v6scrp,
        &v6labl,
        &v6alis,
        &v6invi,
        &v6nega,
        &v6cusi,
        &v6prot,
        &v6acct,
        &v6svce,
        &v6gena,
        &v6data,
        &v6agrp,
        &v6pdmn,
        &v6sync,
        &v6tomb,
        &v6sha1,
        &v6v_Data,
        &v6v_pk,
        NULL
    },
};

static const SecDbClass inet_class = {
    .name = CFSTR("inet"),
    .attrs = {
        &v6rowid,
        &v6cdat,
        &v6mdat,
        &v6desc,
        &v6icmt,
        &v6crtr,
        &v6type,
        &v6scrp,
        &v6labl,
        &v6alis,
        &v6invi,
        &v6nega,
        &v6cusi,
        &v6prot,
        &v6acct,
        &v6sdmn,
        &v6srvr,
        &v6ptcl,
        &v6atyp,
        &v6port,
        &v6path,
        &v6data,
        &v6agrp,
        &v6pdmn,
        &v6sync,
        &v6tomb,
        &v6sha1,
        &v6v_Data,
        &v6v_pk,
        0
    },
};

static const SecDbClass cert_class = {
    .name = CFSTR("cert"),
    .attrs = {
        &v6rowid,
        &v6cdat,
        &v6mdat,
        &v6ctyp,
        &v6cenc,
        &v6labl,
        &v6certalis,
        &v6subj,
        &v6issr,
        &v6slnr,
        &v6skid,
        &v6pkhh,
        &v6data,
        &v6agrp,
        &v6pdmn,
        &v6sync,
        &v6tomb,
        &v6sha1,
        &v6v_Data,
        &v6v_pk,
        0
    },
};

static const SecDbClass keys_class = {
    .name = CFSTR("keys"),
    .attrs = {
        &v6rowid,
        &v6cdat,
        &v6mdat,
        &v6kcls,
        &v6labl,
        &v6alis,
        &v6perm,
        &v6priv,
        &v6modi,
        &v6klbl,
        &v6atag,
        &v6keycrtr,
        &v6keytype,
        &v6bsiz,
        &v6esiz,
        &v6sdat,
        &v6edat,
        &v6sens,
        &v6asen,
        &v6extr,
        &v6next,
        &v6encr,
        &v6decr,
        &v6drve,
        &v6sign,
        &v6vrfy,
        &v6snrc,
        &v6vyrc,
        &v6wrap,
        &v6unwp,
        &v6data,
        &v6agrp,
        &v6pdmn,
        &v6sync,
        &v6tomb,
        &v6sha1,
        &v6v_Data,
        &v6v_pk,
        0
    }
};

/* An identity which is really a cert + a key, so all cert and keys attrs are
 allowed. */
static const SecDbClass identity_class = {
    .name = CFSTR("idnt"),
    .attrs = {
        0
    },
};

static const SecDbAttr *SecDbAttrWithKey(const SecDbClass *c,
                                         CFTypeRef key,
                                         CFErrorRef *error) {
    /* Special case: identites can have all attributes of either cert
       or keys. */
    if (c == &identity_class) {
        const SecDbAttr *desc;
        if (!(desc = SecDbAttrWithKey(&cert_class, key, 0)))
            desc = SecDbAttrWithKey(&keys_class, key, error);
        return desc;
    }

    if (isString(key)) {
        SecDbForEachAttr(c, a) {
            if (CFEqual(a->name, key))
                return a;
        }
    }

    SecError(errSecNoSuchAttr, error, CFSTR("attribute %@ not found in class %@"), key, c->name);

    return NULL;
}

static bool kc_transaction(SecDbConnectionRef dbt, CFErrorRef *error, bool(^perform)()) {
    __block bool ok = true;
    return ok && SecDbTransaction(dbt, kSecDbExclusiveTransactionType, error, ^(bool *commit) {
        ok = *commit = perform();
    });
}

static CFStringRef SecDbGetKindSQL(SecDbAttrKind kind) {
    switch (kind) {
        case kSecDbBlobAttr:
        case kSecDbDataAttr:
        case kSecDbSHA1Attr:
        case kSecDbPrimaryKeyAttr:
        case kSecDbEncryptedDataAttr:
            return CFSTR("BLOB");
        case kSecDbAccessAttr:
        case kSecDbStringAttr:
            return CFSTR("TEXT");
        case kSecDbNumberAttr:
        case kSecDbSyncAttr:
        case kSecDbTombAttr:
            return CFSTR("INTEGER");
        case kSecDbDateAttr:
        case kSecDbCreationDateAttr:
        case kSecDbModificationDateAttr:
            return CFSTR("REAL");
        case kSecDbRowIdAttr:
            return CFSTR("INTEGER PRIMARY KEY AUTOINCREMENT");
    }
}

static void SecDbAppendUnqiue(CFMutableStringRef sql, CFStringRef value, bool *haveUnique) {
    assert(haveUnique);
    if (!*haveUnique)
        CFStringAppend(sql, CFSTR("UNIQUE("));

    SecDbAppendElement(sql, value, haveUnique);
}

static void SecDbAppendCreateTableWithClass(CFMutableStringRef sql, const SecDbClass *c) {
    CFStringAppendFormat(sql, 0, CFSTR("CREATE TABLE %@("), c->name);
    SecDbForEachAttrWithMask(c,desc,kSecDbInFlag) {
        CFStringAppendFormat(sql, 0, CFSTR("%@ %@"), desc->name, SecDbGetKindSQL(desc->kind));
        if (desc->flags & kSecDbNotNullFlag)
            CFStringAppend(sql, CFSTR(" NOT NULL"));
        if (desc->flags & kSecDbDefault0Flag)
            CFStringAppend(sql, CFSTR(" DEFAULT 0"));
        if (desc->flags & kSecDbDefaultEmptyFlag)
            CFStringAppend(sql, CFSTR(" DEFAULT ''"));
        CFStringAppend(sql, CFSTR(","));
    }

    bool haveUnique = false;
    SecDbForEachAttrWithMask(c,desc,kSecDbPrimaryKeyFlag | kSecDbInFlag) {
        SecDbAppendUnqiue(sql, desc->name, &haveUnique);
    }
    if (haveUnique)
        CFStringAppend(sql, CFSTR(")"));

    CFStringAppend(sql, CFSTR(");"));
}

static const char * const s3dl_upgrade_sql[] = {
    /* 0 */
    "",

    /* 1 */
    /* Create indices. */
    "CREATE INDEX igsha ON genp(sha1);"
    "CREATE INDEX iisha ON inet(sha1);"
    "CREATE INDEX icsha ON cert(sha1);"
    "CREATE INDEX iksha ON keys(sha1);"
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
    "",

    /* 3 */
    /* Rename version 2 or version 3 tables and drop version table since
       step 0 creates it. */
    "ALTER TABLE genp RENAME TO ogenp;"
    "ALTER TABLE inet RENAME TO oinet;"
    "ALTER TABLE cert RENAME TO ocert;"
    "ALTER TABLE keys RENAME TO okeys;"
    "DROP TABLE tversion;",

    /* 4 */
    "",

    /* 5 */
    "",

    /* 6 */
    "",

    /* 7 */
    /* Move data from version 5 tables to new ones and drop old ones. */
    "INSERT INTO genp (rowid,cdat,mdat,desc,icmt,crtr,type,scrp,labl,alis,invi,nega,cusi,prot,acct,svce,gena,data,agrp,pdmn) SELECT rowid,cdat,mdat,desc,icmt,crtr,type,scrp,labl,alis,invi,nega,cusi,prot,acct,svce,gena,data,agrp,pdmn from ogenp;"
    "INSERT INTO inet (rowid,cdat,mdat,desc,icmt,crtr,type,scrp,labl,alis,invi,nega,cusi,prot,acct,sdmn,srvr,ptcl,atyp,port,path,data,agrp,pdmn) SELECT rowid,cdat,mdat,desc,icmt,crtr,type,scrp,labl,alis,invi,nega,cusi,prot,acct,sdmn,srvr,ptcl,atyp,port,path,data,agrp,pdmn from oinet;"
    "INSERT INTO cert (rowid,cdat,mdat,ctyp,cenc,labl,alis,subj,issr,slnr,skid,pkhh,data,agrp,pdmn) SELECT rowid,cdat,mdat,ctyp,cenc,labl,alis,subj,issr,slnr,skid,pkhh,data,agrp,pdmn from ocert;"
    "INSERT INTO keys (rowid,cdat,mdat,kcls,labl,alis,perm,priv,modi,klbl,atag,crtr,type,bsiz,esiz,sdat,edat,sens,asen,extr,next,encr,decr,drve,sign,vrfy,snrc,vyrc,wrap,unwp,data,agrp,pdmn) SELECT rowid,cdat,mdat,kcls,labl,alis,perm,priv,modi,klbl,atag,crtr,type,bsiz,esiz,sdat,edat,sens,asen,extr,next,encr,decr,drve,sign,vrfy,snrc,vyrc,wrap,unwp,data,agrp,pdmn from okeys;"
    "DROP TABLE ogenp;"
    "DROP TABLE oinet;"
    "DROP TABLE ocert;"
    "DROP TABLE okeys;"
    "CREATE INDEX igsha ON genp(sha1);"
    "CREATE INDEX iisha ON inet(sha1);"
    "CREATE INDEX icsha ON cert(sha1);"
    "CREATE INDEX iksha ON keys(sha1);",
};

struct sql_stages {
    int pre;
    int main;
    int post;
    bool init_pdmn; // If true do a full export followed by an import of the entire database so all items are re-encoded.
};

/* On disk database format version upgrade scripts.
   If pre is 0, version is unsupported and db is considered corrupt for having that version.
   First entry creates the current db, each susequent entry upgrade to current from the version
   represented by the index of the slot.  Each script is either -1 (disabled) of the number of
   the script in the main table.
    {pre,main,post, reencode} */
static struct sql_stages s3dl_upgrade_script[] = {
    { -1, 0, 1, false },/* 0->current: Create version 6 (Innsbruck) database. */
    {},                 /* 1->current: Upgrade to version 6 from version 1 (LittleBear) -- Unsupported. */
    {},                 /* 2->current: Upgrade to version 6 from version 2 (BigBearBeta) -- Unsupported */
    {},                 /* 3->current: Upgrade to version 6 from version 3 (Apex) -- Unsupported */
    {},                 /* 4->current: Upgrade to version 6 from version 4 (Telluride) -- Unsupported */
    { 3, 0, 7, true },  /* 5->current: Upgrade to version 6 from version 5 (TellurideGM). */
};

static bool sql_run_script(SecDbConnectionRef dbt, int number, CFErrorRef *error)
{
    /* Script -1 == skip this step. */
    if (number < 0)
        return true;

    /* If we are attempting to run a script we don't have, fail. */
    if ((size_t)number >= array_size(s3dl_upgrade_sql))
        return SecDbError(SQLITE_CORRUPT, error, CFSTR("script %d exceeds maximum %d"),
                                number, (int)(array_size(s3dl_upgrade_sql)));
    __block bool ok = true;
    if (number == 0) {
        CFMutableStringRef sql = CFStringCreateMutable(0, 0);
        SecDbAppendCreateTableWithClass(sql, &genp_class);
        SecDbAppendCreateTableWithClass(sql, &inet_class);
        SecDbAppendCreateTableWithClass(sql, &cert_class);
        SecDbAppendCreateTableWithClass(sql, &keys_class);
        CFStringAppend(sql, CFSTR("CREATE TABLE tversion(version INTEGER);INSERT INTO tversion(version) VALUES(6);"));
        CFStringPerformWithCString(sql, ^(const char *sql_string) {
            ok = SecDbErrorWithDb(sqlite3_exec(SecDbHandle(dbt), sql_string, NULL, NULL, NULL),
                                     SecDbHandle(dbt), error, CFSTR("sqlite3_exec: %s"), sql_string);
        });
        CFReleaseSafe(sql);
    } else {
        ok = SecDbErrorWithDb(sqlite3_exec(SecDbHandle(dbt), s3dl_upgrade_sql[number], NULL, NULL, NULL),
                                 SecDbHandle(dbt), error, CFSTR("sqlite3_exec: %s"), s3dl_upgrade_sql[number]);
    }
    return ok;
}

/* Return the current database version in *version.  Returns a
 SQLITE error. */
static bool s3dl_dbt_get_version(SecDbConnectionRef dbt, int *version, CFErrorRef *error)
{
    CFStringRef sql = CFSTR("SELECT version FROM tversion LIMIT 1");
    return SecDbWithSQL(dbt, sql, error, ^(sqlite3_stmt *stmt) {
        __block bool found_version = false;
        bool step_ok = SecDbForEach(stmt, error, ^(int row_index __unused) {
            if (!found_version) {
                *version = sqlite3_column_int(stmt, 0);
                found_version = true;
            }
            return found_version;
        });
        if (!found_version) {
            /* We have a tversion table but we didn't find a single version
             value, now what? I suppose we pretend the db is corrupted
             since this isn't supposed to ever happen. */
            step_ok = SecDbError(SQLITE_CORRUPT, error, CFSTR("Failed to read version: database corrupt"));
            secwarning("SELECT version step: %@", error ? *error : NULL);
        }
        return step_ok;
    });
}

static bool s3dl_dbt_upgrade_from_version(SecDbConnectionRef dbt, int version, CFErrorRef *error)
{
    /* We need to go from db version to CURRENT_DB_VERSION, let's do so. */
    __block bool ok = true;
    /* O, guess we're done already. */
    if (version == CURRENT_DB_VERSION)
        return ok;

    if (ok && version < 6) {
        // Pre v6 keychains need to have WAL enabled, since SecDb only
        // does this at db creation time.
        // NOTE: This has to be run outside of a transaction.
        ok = (SecDbExec(dbt, CFSTR("PRAGMA auto_vacuum = FULL"), error) &&
              SecDbExec(dbt, CFSTR("PRAGMA journal_mode = WAL"), error));
    }

    // Start a transaction to do the upgrade within
    if (ok) { ok = SecDbTransaction(dbt, kSecDbExclusiveTransactionType, error, ^(bool *commit) {
        // Be conservative and get the version again once we start a transaction.
        int cur_version = version;
        s3dl_dbt_get_version(dbt, &cur_version, NULL);

        /* If we are attempting to upgrade to a version greater than what we have
         an upgrade script for, fail. */
        if (ok && (cur_version < 0 ||
            (size_t)cur_version >= array_size(s3dl_upgrade_script))) {
            ok = SecDbError(SQLITE_CORRUPT, error, CFSTR("no upgrade script for version: %d"), cur_version);
            secerror("no upgrade script for version %d", cur_version);
        }

        struct sql_stages *script;
        if (ok) {
            script = &s3dl_upgrade_script[cur_version];
            if (script->pre == 0)
                ok = SecDbError(SQLITE_CORRUPT, error, CFSTR("unsupported db version %d"), cur_version);
        }
        if (ok)
            ok = sql_run_script(dbt, script->pre, error);
        if (ok)
            ok = sql_run_script(dbt, script->main, error);
        if (ok)
            ok = sql_run_script(dbt, script->post, error);
        if (ok && script->init_pdmn) {
            CFErrorRef localError = NULL;
            CFDictionaryRef backup = SecServerExportKeychainPlist(dbt,
                                                                  KEYBAG_DEVICE, KEYBAG_NONE, kSecNoItemFilter, &localError);
            if (backup) {
                if (localError) {
                    secerror("Ignoring export error: %@ during upgrade export", localError);
                    CFReleaseNull(localError);
                }
                ok = SecServerImportKeychainInPlist(dbt, KEYBAG_NONE,
                                                    KEYBAG_DEVICE, backup, kSecNoItemFilter, &localError);
                CFRelease(backup);
            } else {
                ok = false;

                if (localError && CFErrorGetCode(localError) == errSecInteractionNotAllowed) {
                    SecError(errSecUpgradePending, error,
                         CFSTR("unable to complete upgrade due to device lock state"));
                    secerror("unable to complete upgrade due to device lock state");
                } else {
                    secerror("unable to complete upgrade for unknown reason, marking DB as corrupt: %@", localError);
                    SecDbCorrupt(dbt);
                }
            }

            if (localError) {
                if (error && !*error)
                    *error = localError;
                else
                    CFRelease(localError);
            }
        } else if (!ok) {
            secerror("unable to complete upgrade scripts, marking DB as corrupt: %@", error ? *error : NULL);
            SecDbCorrupt(dbt);
        }
        *commit = ok;
    }); } else {
        secerror("unable to complete upgrade scripts, marking DB as corrupt: %@", error ? *error : NULL);
        SecDbCorrupt(dbt);
    }

    return ok;
}


/* This function is called if the db doesn't have the proper version.  We
   start an exclusive transaction and recheck the version, and then perform
   the upgrade within that transaction. */
static bool s3dl_dbt_upgrade(SecDbConnectionRef dbt, CFErrorRef *error)
{
    // Already in a transaction
    //return kc_transaction(dbt, error, ^{
        int version = 0; // Upgrade from version 0 == create new db
        s3dl_dbt_get_version(dbt, &version, NULL);
        return s3dl_dbt_upgrade_from_version(dbt, version, error);
    //});
}

const uint32_t v0KeyWrapOverHead = 8;

/* Wrap takes a 128 - 256 bit key as input and returns output of
   inputsize + 64 bits.
   In bytes this means that a
   16 byte (128 bit) key returns a 24 byte wrapped key
   24 byte (192 bit) key returns a 32 byte wrapped key
   32 byte (256 bit) key returns a 40 byte wrapped key  */
static bool ks_crypt(uint32_t selector, keybag_handle_t keybag,
    keyclass_t keyclass, uint32_t textLength, const uint8_t *source, uint8_t *dest, size_t *dest_len, CFErrorRef *error) {
#if USE_KEYSTORE
	kern_return_t kernResult = kIOReturnBadArgument;

    if (selector == kAppleKeyStoreKeyWrap) {
        kernResult = aks_wrap_key(source, textLength, keyclass, keybag, dest, (int*)dest_len);
    } else if (selector == kAppleKeyStoreKeyUnwrap) {
        kernResult = aks_unwrap_key(source, textLength, keyclass, keybag, dest, (int*)dest_len);
    }

	if (kernResult != KERN_SUCCESS) {
        if ((kernResult == kIOReturnNotPermitted) || (kernResult == kIOReturnNotPrivileged)) {
            /* Access to item attempted while keychain is locked. */
            return SecError(errSecInteractionNotAllowed, error, CFSTR("ks_crypt: %x failed to %s item (class %"PRId32", bag: %"PRId32") Access to item attempted while keychain is locked."),
                     kernResult, (selector == kAppleKeyStoreKeyWrap ? "wrap" : "unwrap"), keyclass, keybag);
        } else if (kernResult == kIOReturnError) {
            /* Item can't be decrypted on this device, ever, so drop the item. */
            return SecError(errSecDecode, error, CFSTR("ks_crypt: %x failed to %s item (class %"PRId32", bag: %"PRId32") Item can't be decrypted on this device, ever, so drop the item."),
                     kernResult, (selector == kAppleKeyStoreKeyWrap ? "wrap" : "unwrap"), keyclass, keybag);
        } else {
            return SecError(errSecNotAvailable, error, CFSTR("ks_crypt: %x failed to %s item (class %"PRId32", bag: %"PRId32")"),
                     kernResult, (selector == kAppleKeyStoreKeyWrap ? "wrap" : "unwrap"), keyclass, keybag);
        }
	}
	return true;
#else /* !USE_KEYSTORE */
    if (selector == kAppleKeyStoreKeyWrap) {
        /* The no encryption case. */
        if (*dest_len >= textLength + 8) {
            memcpy(dest, source, textLength);
            memset(dest + textLength, 8, 8);
            *dest_len = textLength + 8;
        } else
            return SecError(errSecNotAvailable, error, CFSTR("ks_crypt: failed to wrap item (class %"PRId32")"), keyclass);
    } else if (selector == kAppleKeyStoreKeyUnwrap) {
        if (*dest_len + 8 >= textLength) {
            memcpy(dest, source, textLength - 8);
            *dest_len = textLength - 8;
        } else
            return SecError(errSecNotAvailable, error, CFSTR("ks_crypt: failed to unwrap item (class %"PRId32")"), keyclass);
    }
    return true;
#endif /* USE_KEYSTORE */
}


CFDataRef kc_plist_copy_der(CFPropertyListRef plist, CFErrorRef *error) {
    size_t len = der_sizeof_plist(plist, error);
    CFMutableDataRef encoded = CFDataCreateMutable(0, len);
    CFDataSetLength(encoded, len);
    uint8_t *der_end = CFDataGetMutableBytePtr(encoded);
    const uint8_t *der = der_end;
    der_end += len;
    der_end = der_encode_plist(plist, error, der, der_end);
    if (!der_end) {
        CFReleaseNull(encoded);
    } else {
        assert(!der_end || der_end == der);
    }
    return encoded;
}

static CFDataRef kc_copy_digest(const struct ccdigest_info *di, size_t len,
                                const void *data, CFErrorRef *error) {
    CFMutableDataRef digest = CFDataCreateMutable(0, di->output_size);
    CFDataSetLength(digest, di->output_size);
    ccdigest(di, len, data, CFDataGetMutableBytePtr(digest));
    return digest;
}

CFDataRef kc_copy_sha1(size_t len, const void *data, CFErrorRef *error) {
    return kc_copy_digest(ccsha1_di(), len, data, error);
}

CFDataRef kc_copy_plist_sha1(CFPropertyListRef plist, CFErrorRef *error) {
    CFDataRef der = kc_plist_copy_der(plist, error);
    CFDataRef digest = NULL;
    if (der) {
        digest = kc_copy_sha1(CFDataGetLength(der), CFDataGetBytePtr(der), error);
        CFRelease(der);
    }
    return digest;
}

/* Given plainText create and return a CFDataRef containing:
   BULK_KEY = RandomKey()
   version || keyclass || KeyStore_WRAP(keyclass, BULK_KEY) ||
    AES(BULK_KEY, NULL_IV, plainText || padding)
 */
bool ks_encrypt_data(keybag_handle_t keybag,
    keyclass_t keyclass, CFDataRef plainText, CFDataRef *pBlob, CFErrorRef *error) {
    CFMutableDataRef blob = NULL;
    bool ok = true;
    //check(keybag >= 0);

    /* Precalculate output blob length. */
    const uint32_t bulkKeySize = 32; /* Use 256 bit AES key for bulkKey. */
    const uint32_t maxKeyWrapOverHead = 8 + 32;
    uint8_t bulkKey[bulkKeySize];
    uint8_t bulkKeyWrapped[bulkKeySize + maxKeyWrapOverHead];
    size_t bulkKeyWrappedSize = sizeof(bulkKeyWrapped);
    uint32_t key_wrapped_size;

    if (!plainText || CFGetTypeID(plainText) != CFDataGetTypeID()
        || keyclass == 0) {
        ok = SecError(errSecParam, error, CFSTR("ks_encrypt_data: invalid plain text"));
        goto out;
    }

    size_t ptLen = CFDataGetLength(plainText);
    size_t ctLen = ptLen;
    size_t tagLen = 16;
    uint32_t version = 3;

    if (SecRandomCopyBytes(kSecRandomDefault, bulkKeySize, bulkKey)) {
        ok = SecError(errSecAllocate, error, CFSTR("ks_encrypt_data: SecRandomCopyBytes failed"));
        goto out;
    }

    /* Now that we're done using the bulkKey, in place encrypt it. */
    require_quiet(ok = ks_crypt(kAppleKeyStoreKeyWrap, keybag, keyclass,
                                       bulkKeySize, bulkKey, bulkKeyWrapped,
                                       &bulkKeyWrappedSize, error), out);
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
        ok = SecError(errSecInternal, error, CFSTR("ks_encrypt_data: CCCryptorGCM failed: %d"), ccerr);
        goto out;
    }
    if (tagLen != 16) {
        ok = SecError(errSecInternal, error, CFSTR("ks_encrypt_data: CCCryptorGCM expected: 16 got: %ld byte tag"), tagLen);
        goto out;
    }

out:
    memset(bulkKey, 0, sizeof(bulkKey));
	if (!ok) {
		CFReleaseSafe(blob);
	} else {
		*pBlob = blob;
	}
    return ok;
}

/* Given cipherText containing:
   version || keyclass || KeyStore_WRAP(keyclass, BULK_KEY) ||
    AES(BULK_KEY, NULL_IV, plainText || padding)
   return the plainText. */
bool ks_decrypt_data(keybag_handle_t keybag,
    keyclass_t *pkeyclass, CFDataRef blob, CFDataRef *pPlainText,
    uint32_t *version_p, CFErrorRef *error) {
    const uint32_t bulkKeySize = 32; /* Use 256 bit AES key for bulkKey. */
    uint8_t bulkKey[bulkKeySize];
    size_t bulkKeyCapacity = sizeof(bulkKey);
    bool ok = true;

    CFMutableDataRef plainText = NULL;
#if USE_KEYSTORE
#if TARGET_OS_IPHONE
    check(keybag >= 0);
#else
    check((keybag >= 0) || (keybag == session_keybag_handle));
#endif
#endif

    if (!blob) {
        ok = SecError(errSecParam, error, CFSTR("ks_decrypt_data: invalid blob"));
        goto out;
    }

    size_t blobLen = CFDataGetLength(blob);
    const uint8_t *cursor = CFDataGetBytePtr(blob);
    uint32_t version;
    keyclass_t keyclass;
    uint32_t wrapped_key_size;

    /* Check for underflow, ensuring we have at least one full AES block left. */
    if (blobLen < sizeof(version) + sizeof(keyclass) +
        bulkKeySize + v0KeyWrapOverHead + 16) {
        ok = SecError(errSecDecode, error, CFSTR("ks_decrypt_data: Check for underflow"));
        goto out;
    }

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
            /* DROPTHROUGH */
            /* v2 and v3 have the same crypto, just different dictionary encodings. */
        case 3:
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
            ok = SecError(errSecDecode, error, CFSTR("ks_decrypt_data: invalid version %d"), version);
            goto out;
    }

    /* Validate key wrap length against total length */
    require(blobLen - minimum_blob_len - tagLen >= wrapped_key_size, out);
    ctLen -= wrapped_key_size;
    if (version < 2 && (ctLen & 0xF) != 0) {
        ok = SecError(errSecDecode, error, CFSTR("ks_decrypt_data: invalid version"));
        goto out;
    }

    /* Now unwrap the bulk key using a key in the keybag. */
    require_quiet(ok = ks_crypt(kAppleKeyStoreKeyUnwrap, keybag,
        keyclass, wrapped_key_size, cursor, bulkKey, &bulkKeyCapacity, error), out);
    cursor += wrapped_key_size;

    plainText = CFDataCreateMutable(NULL, ctLen);
    if (!plainText) {
        ok = SecError(errSecDecode, error, CFSTR("ks_decrypt_data: failed to allocate data for plain text"));
        goto out;
    }
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
            /* TODO: Should this be errSecDecode once AppleKeyStore correctly
             identifies uuid unwrap failures? */
            /* errSecInteractionNotAllowed; */
            ok = SecError(errSecDecode, error, CFSTR("ks_decrypt_data: CCCryptorGCM failed: %d"), ccerr);
            goto out;
        }
        if (tagLen != 16) {
            ok = SecError(errSecInternal, error, CFSTR("ks_decrypt_data: CCCryptorGCM expected: 16 got: %ld byte tag"), tagLen);
            goto out;
        }
        cursor += ctLen;
        if (memcmp(tag, cursor, tagLen)) {
            ok = SecError(errSecDecode, error, CFSTR("ks_decrypt_data: CCCryptorGCM computed tag not same as tag in blob"));
            goto out;
        }
    } else {
        size_t ptLen;
        ccerr = CCCrypt(kCCDecrypt, kCCAlgorithmAES128, kCCOptionPKCS7Padding,
                        bulkKey, bulkKeySize, NULL, cursor, ctLen,
                        CFDataGetMutableBytePtr(plainText), ctLen, &ptLen);
        if (ccerr) {
            /* TODO: Should this be errSecDecode once AppleKeyStore correctly
               identifies uuid unwrap failures? */
            /* errSecInteractionNotAllowed; */
            ok = SecError(errSecDecode, error, CFSTR("ks_decrypt_data: CCCrypt failed: %d"), ccerr);
            goto out;
        }
        CFDataSetLength(plainText, ptLen);
    }
    if (version_p) *version_p = version;
out:
    memset(bulkKey, 0, bulkKeySize);
	if (!ok) {
		CFReleaseSafe(plainText);
	} else {
		*pPlainText = plainText;
	}
    return ok;
}

static bool use_hwaes() {
    static bool use_hwaes;
    static dispatch_once_t check_once;
    dispatch_once(&check_once, ^{
        use_hwaes = hwaes_key_available();
        if (use_hwaes) {
            asl_log(NULL, NULL, ASL_LEVEL_INFO, "using hwaes key");
        } else {
            asl_log(NULL, NULL, ASL_LEVEL_ERR, "unable to access hwaes key");
        }
    });
    return use_hwaes;
}

/* Upper limit for number of keys in a QUERY dictionary. */
#define QUERY_KEY_LIMIT_BASE    (128)
#ifdef NO_SERVER
#define QUERY_KEY_LIMIT  (31 + QUERY_KEY_LIMIT_BASE)
#else
#define QUERY_KEY_LIMIT  QUERY_KEY_LIMIT_BASE
#endif

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
        SecError(errSecParam, &q->q_error, CFSTR("accessible attribute %@ not a string"), value);
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
        SecError(errSecParam, &q->q_error, CFSTR("accessible attribute %@ unknown"), value);
        return;
    }
    //q->q_keyclass_s = value;
}

static const SecDbClass *kc_class_with_name(CFStringRef name) {
    if (isString(name)) {
#if 0
        // TODO Iterate kc_db_classes and look for name == class->name.
        // Or get clever and switch on first letter of class name and compare to verify
        static const void *kc_db_classes[] = {
            &genp_class,
            &inet_class,
            &cert_class,
            &keys_class,
            &identity_class
        };
#endif
        if (CFEqual(name, kSecClassGenericPassword))
            return &genp_class;
        else if (CFEqual(name, kSecClassInternetPassword))
            return &inet_class;
        else if (CFEqual(name, kSecClassCertificate))
            return &cert_class;
        else if (CFEqual(name, kSecClassKey))
            return &keys_class;
        else if (CFEqual(name, kSecClassIdentity))
            return &identity_class;
    }
    return NULL;
}

/* AUDIT[securityd](done):
   key (ok) is a caller provided, string or number of length 4.
   value (ok) is a caller provided, non NULL CFTypeRef.
 */
static void query_add_attribute_with_desc(const SecDbAttr *desc, const void *value, Query *q)
{
    if (CFEqual(desc->name, kSecAttrSynchronizable)) {
        q->q_sync = true;
        if (CFEqual(value, kSecAttrSynchronizableAny))
            return; /* skip the attribute so it isn't part of the search */
    }

    CFTypeRef attr = NULL;
    switch (desc->kind) {
        case kSecDbDataAttr:
            attr = copyData(value);
            break;
        case kSecDbBlobAttr:
            attr = copyBlob(value);
            break;
        case kSecDbDateAttr:
        case kSecDbCreationDateAttr:
        case kSecDbModificationDateAttr:
            attr = copyDate(value);
            break;
        case kSecDbNumberAttr:
        case kSecDbSyncAttr:
        case kSecDbTombAttr:
            attr = copyNumber(value);
            break;
        case kSecDbAccessAttr:
        case kSecDbStringAttr:
            attr = copyString(value);
            break;
        case kSecDbSHA1Attr:
            attr = copySHA1(value);
            break;
        case kSecDbRowIdAttr:
        case kSecDbPrimaryKeyAttr:
        case kSecDbEncryptedDataAttr:
            break;
    }

    if (!attr) {
        SecError(errSecItemInvalidValue, &q->q_error, CFSTR("attribute %@: value: %@ failed to convert"), desc->name, value);
        return;
    }

    /* Store plaintext attr data in q_item unless it's a kSecDbSHA1Attr. */
    if (q->q_item && desc->kind != kSecDbSHA1Attr) {
        CFDictionarySetValue(q->q_item, desc->name, attr);
    }

    if (CFEqual(desc->name, kSecAttrAccessible)) {
        query_parse_keyclass(attr, q);
    }

    /* Convert attr to (sha1) digest if requested. */
    if (desc->flags & kSecDbSHA1ValueInFlag) {
        CFDataRef data = copyData(attr);
        CFRelease(attr);
        if (!data) {
            SecError(errSecInternal, &q->q_error, CFSTR("failed to get attribute %@ data"), desc->name);
            return;
        }

        CFMutableDataRef digest = CFDataCreateMutable(0, CC_SHA1_DIGEST_LENGTH);
        CFDataSetLength(digest, CC_SHA1_DIGEST_LENGTH);
        /* 64 bits cast: worst case is we generate the wrong hash */
        assert((unsigned long)CFDataGetLength(data)<UINT32_MAX); /* Debug check. Correct as long as CFIndex is long */
        CCDigest(kCCDigestSHA1, CFDataGetBytePtr(data), (CC_LONG)CFDataGetLength(data),
                CFDataGetMutableBytePtr(digest));
        CFRelease(data);
        attr = digest;
    }

    /* Record the new attr key, value in q_pairs. */
    q->q_pairs[q->q_attr_end].key = desc->name;
    q->q_pairs[q->q_attr_end++].value = attr;
}

static void query_add_attribute(const void *key, const void *value, Query *q)
{
    const SecDbAttr *desc = SecDbAttrWithKey(q->q_class, key, &q->q_error);
    if (desc)
        query_add_attribute_with_desc(desc, value, q);
}

/* First remove key from q->q_pairs if it's present, then add the attribute again. */
static void query_set_attribute_with_desc(const SecDbAttr *desc, const void *value, Query *q) {
    if (CFDictionaryContainsKey(q->q_item, desc->name)) {
        CFIndex ix;
        for (ix = 0; ix < q->q_attr_end; ++ix) {
            if (CFEqual(desc->name, q->q_pairs[ix].key)) {
                CFReleaseSafe(q->q_pairs[ix].value);
                --q->q_attr_end;
                for (; ix < q->q_attr_end; ++ix) {
                    q->q_pairs[ix] = q->q_pairs[ix + 1];
                }
                CFDictionaryRemoveValue(q->q_item, desc->name);
                break;
            }
        }
    }
    query_add_attribute_with_desc(desc, value, q);
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
                SecError(errSecItemInvalidValue, &q->q_error, CFSTR("failed to convert match limit %@ to CFIndex"), value);
        } else if (CFEqual(kSecMatchLimitAll, value)) {
            q->q_limit = kSecMatchUnlimited;
        } else if (CFEqual(kSecMatchLimitOne, value)) {
            q->q_limit = 1;
        } else {
            SecError(errSecItemInvalidValue, &q->q_error, CFSTR("unsupported match limit %@"), value);
        }
    } else if (CFEqual(kSecMatchIssuers, key) &&
               (CFGetTypeID(value) == CFArrayGetTypeID()))
    {
        CFMutableArrayRef canonical_issuers = CFArrayCreateMutable(kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks);
        if (canonical_issuers) {
            CFIndex i, count = CFArrayGetCount(value);
            for (i = 0; i < count; i++) {
                CFTypeRef issuer_data = CFArrayGetValueAtIndex(value, i);
                CFDataRef issuer_canonical = NULL;
                if (CFDataGetTypeID() == CFGetTypeID(issuer_data))
                    issuer_canonical = SecDistinguishedNameCopyNormalizedContent((CFDataRef)issuer_data);
                if (issuer_canonical) {
                    CFArrayAppendValue(canonical_issuers, issuer_canonical);
                    CFRelease(issuer_canonical);
                }
            }

            if (CFArrayGetCount(canonical_issuers) > 0) {
                q->q_match_issuer = canonical_issuers;
            } else
                CFRelease(canonical_issuers);
        }
    }
}

static bool query_set_class(Query *q, CFStringRef c_name, CFErrorRef *error) {
    const SecDbClass *value;
    if (c_name && CFGetTypeID(c_name) == CFStringGetTypeID() &&
        (value = kc_class_with_name(c_name)) &&
        (q->q_class == 0 || q->q_class == value)) {
        q->q_class = value;
        return true;
    }

    if (error && !*error)
        SecError((c_name ? errSecNoSuchClass : errSecItemClassMissing), error, CFSTR("can find class named: %@"), c_name);


    return false;
}

static const SecDbClass *query_get_class(CFDictionaryRef query, CFErrorRef *error) {
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

    if (c_name && (value = kc_class_with_name(c_name))) {
        return value;
    } else {
        if (c_name)
            SecError(errSecNoSuchClass, error, CFSTR("can't find class named: %@"), c_name);
        else
            SecError(errSecItemClassMissing, error, CFSTR("query missing class name"));
        return NULL;
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
        SecError(errSecItemInvalidKey, &q->q_error, CFSTR("add_class: key %@ is not %@"), key, kSecClass);
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
        SecError(errSecItemInvalidValue, &q->q_error, CFSTR("add_return: value %@ is not CFBoolean"), value);
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
        SecError(errSecItemInvalidKey, &q->q_error, CFSTR("add_return: unknown key %@"), key);
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
    } else if (CFEqual(key, kSecUseTombstones)) {
        if (CFGetTypeID(value) == CFBooleanGetTypeID()) {
            q->q_use_tomb = value;
        } else if (CFGetTypeID(value) == CFNumberGetTypeID()) {
            q->q_use_tomb = CFBooleanGetValue(value) ? kCFBooleanTrue : kCFBooleanFalse;
        } else if (CFGetTypeID(value) == CFStringGetTypeID()) {
            q->q_use_tomb = CFStringGetIntValue(value) ? kCFBooleanTrue : kCFBooleanFalse;
        } else {
            SecError(errSecItemInvalidValue, &q->q_error, CFSTR("add_use: value %@ for key %@ is neither CFBoolean nor CFNumber"), value, key);
            return;
        }
#if defined(MULTIPLE_KEYCHAINS)
    } else if (CFEqual(key, kSecUseKeychain)) {
        q->q_use_keychain = value;
    } else if (CFEqual(key, kSecUseKeychainList)) {
        q->q_use_keychain_list = value;
#endif /* !defined(MULTIPLE_KEYCHAINS) */
    } else {
        SecError(errSecItemInvalidKey, &q->q_error, CFSTR("add_use: unknown key %@"), key);
        return;
    }
}

static void query_set_data(const void *value, Query *q) {
    if (!isData(value)) {
        SecError(errSecItemInvalidValue, &q->q_error, CFSTR("set_data: value %@ is not type data"), value);
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
#ifdef NO_SERVER
    } else if (CFEqual(key, kSecValueRef)) {
        q->q_ref = value;
        /* TODO: Add value type sanity checking. */
#endif
    } else if (CFEqual(key, kSecValuePersistentRef)) {
        CFStringRef c_name;
        if (_SecItemParsePersistentRef(value, &c_name, &q->q_row_id))
            query_set_class(q, c_name, &q->q_error);
        else
            SecError(errSecItemInvalidValue, &q->q_error, CFSTR("add_value: value %@ is not a valid persitent ref"), value);
    } else {
        SecError(errSecItemInvalidKey, &q->q_error, CFSTR("add_value: unknown key %@"), key);
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
        SecError(errSecItemInvalidKeyType, &q->q_error, CFSTR("update_applier: unknown key type %@"), key);
        return;
    }

    if (!value) {
        SecError(errSecItemInvalidValue, &q->q_error, CFSTR("update_applier: key %@ has NULL value"), key);
        return;
    }

    if (CFEqual(key, kSecValueData)) {
        query_set_data(value, q);
    } else {
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
        SecError(errSecItemInvalidKeyType, &q->q_error, CFSTR("applier: NULL key"));
        return;
    }

    /* Make sure we have a value. */
    if (!value) {
        SecError(errSecItemInvalidValue, &q->q_error, CFSTR("applier: key %@ has NULL value"), key);
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
                SecError(errSecItemInvalidKey, &q->q_error, CFSTR("applier: key %@ invalid"), key);
                break;
            }
        } else {
            SecError(errSecItemInvalidKey, &q->q_error, CFSTR("applier: key %@ invalid length"), key);
        }
    } else if (key_id == CFNumberGetTypeID()) {
        /* Numeric keys are always (extended) attributes. */
        /* TODO: Why is this here? query_add_attribute() doesn't take numbers. */
        query_add_attribute(key, value, q);
    } else {
        /* We only support string and number type keys. */
        SecError(errSecItemInvalidKeyType, &q->q_error, CFSTR("applier: key %@ neither string nor number"), key);
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

static bool query_error(Query *q, CFErrorRef *error) {
    if (q->q_error) {
        CFErrorRef tmp = q->q_error;
        q->q_error = NULL;
        if (error && !*error) {
            *error = tmp;
        } else {
            CFRelease(tmp);
        }
        return false;
    }
    return true;
}

bool query_destroy(Query *q, CFErrorRef *error) {
    bool ok = query_error(q, error);
    CFIndex ix, attr_count = query_attr_count(q);
    for (ix = 0; ix < attr_count; ++ix) {
        CFReleaseSafe(query_attr_at(q, ix).value);
    }
    CFReleaseSafe(q->q_item);
    CFReleaseSafe(q->q_primary_key_digest);
    CFReleaseSafe(q->q_match_issuer);

    free(q);
    return ok;
}

static void SecKeychainChanged(bool syncWithPeers) {
    uint32_t result = notify_post(g_keychain_changed_notification);
    if (syncWithPeers)
        SOSCCSyncWithAllPeers();
    if (result == NOTIFY_STATUS_OK)
        secnotice("item", "Sent %s%s", syncWithPeers ? "SyncWithAllPeers and " : "", g_keychain_changed_notification);
    else
        secerror("%snotify_post %s returned: %" PRIu32, syncWithPeers ? "Sent SyncWithAllPeers, " : "", g_keychain_changed_notification, result);
}

static bool query_notify_and_destroy(Query *q, bool ok, CFErrorRef *error) {
    if (ok && !q->q_error && q->q_sync_changed) {
        SecKeychainChanged(true);
    }
    return query_destroy(q, error) && ok;
}

/* Allocate and initialize a Query object for query. */
Query *query_create(const SecDbClass *qclass, CFDictionaryRef query,
                    CFErrorRef *error)
{
    if (!qclass) {
        if (error && !*error)
            SecError(errSecItemClassMissing, error, CFSTR("Missing class"));
        return NULL;
    }

    /* Number of pairs we need is the number of attributes in this class
       plus the number of keys in the dictionary, minus one for each key in
       the dictionary that is a regular attribute. */
    CFIndex key_count = SecDbClassAttrCount(qclass);
    if (key_count == 0) {
        // Identities claim to have 0 attributes, but they really support any keys or cert attribute.
        key_count = SecDbClassAttrCount(&cert_class) + SecDbClassAttrCount(&keys_class);
    }

    if (query) {
        key_count += CFDictionaryGetCount(query);
        SecDbForEachAttr(qclass, attr) {
            if (CFDictionaryContainsKey(query, attr->name))
                --key_count;
        }
    }

    if (key_count > QUERY_KEY_LIMIT) {
        if (error && !*error)
        {
            secerror("key_count: %ld, QUERY_KEY_LIMIT: %d", (long)key_count, QUERY_KEY_LIMIT);
            SecError(errSecItemIllegalQuery, error, CFSTR("Past query key limit"));
        }
        return NULL;
    }

    Query *q = calloc(1, sizeof(Query) + sizeof(Pair) * key_count);
    if (q == NULL) {
        if (error && !*error)
            SecError(errSecAllocate, error, CFSTR("Out of memory"));
        return NULL;
    }

    q->q_keybag = KEYBAG_DEVICE;
    q->q_class = qclass;
    q->q_match_begin = q->q_match_end = key_count;
    q->q_item = CFDictionaryCreateMutable(0, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);

    return q;
}

/* Parse query for a Query object q. */
static bool query_parse_with_applier(Query *q, CFDictionaryRef query,
                                     CFDictionaryApplierFunction applier,
                                     CFErrorRef *error) {
    CFDictionaryApplyFunction(query, applier, q);
    return query_error(q, error);
}

/* Parse query for a Query object q. */
static bool query_parse(Query *q, CFDictionaryRef query,
                        CFErrorRef *error) {
    return query_parse_with_applier(q, query, query_applier, error);
}

/* Parse query for a Query object q. */
static bool query_update_parse(Query *q, CFDictionaryRef update,
                               CFErrorRef *error) {
    return query_parse_with_applier(q, update, query_update_applier, error);
}

static Query *query_create_with_limit(CFDictionaryRef query, CFIndex limit,
                                      CFErrorRef *error) {
    Query *q;
    q = query_create(query_get_class(query, error), query, error);
    if (q) {
        q->q_limit = limit;
        if (!query_parse(q, query, error)) {
            query_destroy(q, error);
            return NULL;
        }
        if (!q->q_sync && !q->q_row_id) {
            /* query did not specify a kSecAttrSynchronizable attribute,
             * and did not contain a persistent reference. */
            query_add_attribute(kSecAttrSynchronizable, kCFBooleanFalse, q);
        }
    }
    return q;
}


//TODO: Move this to SecDbItemRef

/* Make sure all attributes that are marked as not_null have a value.  If
   force_date is false, only set mdat and cdat if they aren't already set. */
static void
query_pre_add(Query *q, bool force_date) {
    CFDateRef now = CFDateCreate(0, CFAbsoluteTimeGetCurrent());
    SecDbForEachAttrWithMask(q->q_class, desc, kSecDbInFlag) {
        if (desc->kind == kSecDbCreationDateAttr ||
            desc->kind == kSecDbModificationDateAttr) {
            if (force_date) {
                query_set_attribute_with_desc(desc, now, q);
            } else if (!CFDictionaryContainsKey(q->q_item, desc->name)) {
                query_add_attribute_with_desc(desc, now, q);
            }
        } else if ((desc->flags & kSecDbNotNullFlag) &&
                   !CFDictionaryContainsKey(q->q_item, desc->name)) {
            CFTypeRef value = NULL;
            if (desc->flags & kSecDbDefault0Flag) {
                if (desc->kind == kSecDbDateAttr)
                    value = CFDateCreate(kCFAllocatorDefault, 0.0);
                else {
                    SInt32 vzero = 0;
                    value = CFNumberCreate(0, kCFNumberSInt32Type, &vzero);
                }
            } else if (desc->flags & kSecDbDefaultEmptyFlag) {
                if (desc->kind == kSecDbDataAttr)
                    value = CFDataCreate(kCFAllocatorDefault, NULL, 0);
                else {
                    value = CFSTR("");
                    CFRetain(value);
                }
            }
            if (value) {
                /* Safe to use query_add_attribute here since the attr wasn't
                 set yet. */
                query_add_attribute_with_desc(desc, value, q);
                CFRelease(value);
            }
        }
    }
    CFReleaseSafe(now);
}

//TODO: Move this to SecDbItemRef

/* Update modification_date if needed. */
static void
query_pre_update(Query *q) {
    SecDbForEachAttr(q->q_class, desc) {
        if (desc->kind == kSecDbModificationDateAttr) {
            CFDateRef now = CFDateCreate(0, CFAbsoluteTimeGetCurrent());
            query_set_attribute_with_desc(desc, now, q);
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
   optional data, class and persistent ref results.  This is so we can use
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

static CFDataRef SecDbItemMakePersistentRef(SecDbItemRef item, CFErrorRef *error) {
    sqlite3_int64 row_id = SecDbItemGetRowId(item, error);
    if (row_id)
        return _SecItemMakePersistentRef(SecDbItemGetClass(item)->name, row_id);
    return NULL;
}

static CFTypeRef SecDbItemCopyResult(SecDbItemRef item, ReturnTypeMask return_type, CFErrorRef *error) {
    CFTypeRef a_result;

	if (return_type == 0) {
		/* Caller isn't interested in any results at all. */
		a_result = kCFNull;
	} else if (return_type == kSecReturnDataMask) {
        a_result = SecDbItemGetCachedValueWithName(item, kSecValueData);
        if (a_result) {
            CFRetainSafe(a_result);
        } else {
            a_result = CFDataCreate(kCFAllocatorDefault, NULL, 0);
        }
	} else if (return_type == kSecReturnPersistentRefMask) {
		a_result = SecDbItemMakePersistentRef(item, error);
	} else {
        CFMutableDictionaryRef dict = CFDictionaryCreateMutableForCFTypes(CFGetAllocator(item));
		/* We need to return more than one value. */
        if (return_type & kSecReturnRefMask) {
            CFDictionarySetValue(dict, kSecClass, SecDbItemGetClass(item)->name);
        }
        CFOptionFlags mask = (((return_type & kSecReturnDataMask || return_type & kSecReturnRefMask) ? kSecDbReturnDataFlag : 0) |
                              ((return_type & kSecReturnAttributesMask || return_type & kSecReturnRefMask) ? kSecDbReturnAttrFlag : 0));
        SecDbForEachAttr(SecDbItemGetClass(item), desc) {
            if ((desc->flags & mask) != 0) {
                CFTypeRef value = SecDbItemGetValue(item, desc, error);
                if (value && !CFEqual(kCFNull, value)) {
                    CFDictionarySetValue(dict, desc->name, value);
                } else if (value == NULL) {
                    CFReleaseNull(dict);
                    break;
                }
            }
        }
		if (return_type & kSecReturnPersistentRefMask) {
            CFDataRef pref = SecDbItemMakePersistentRef(item, error);
			CFDictionarySetValue(dict, kSecValuePersistentRef, pref);
            CFRelease(pref);
		}

		a_result = dict;
	}

	return a_result;
}


// MARK: -
// MARK: Forward declarations

static CFMutableDictionaryRef
s3dl_item_from_col(sqlite3_stmt *stmt, Query *q, int col,
                   CFArrayRef accessGroups, keyclass_t *keyclass, CFErrorRef *error);

/* AUDIT[securityd](done):
   attributes (ok) is a caller provided dictionary, only its cf type has
       been checked.
 */
static bool
s3dl_query_add(SecDbConnectionRef dbt, Query *q, CFTypeRef *result, CFErrorRef *error)
{
    if (query_match_count(q) != 0)
        return errSecItemMatchUnsupported;

    /* Add requires a class to be specified unless we are adding a ref. */
    if (q->q_use_item_list)
        return errSecUseItemListUnsupported;

    /* Actual work here. */
    SecDbItemRef item = SecDbItemCreateWithAttributes(kCFAllocatorDefault, q->q_class, q->q_item, KEYBAG_DEVICE, error);
    if (!item)
        return false;

    bool ok = true;
    if (q->q_data)
        ok = SecDbItemSetValueWithName(item, CFSTR("v_Data"), q->q_data, error);
    if (q->q_row_id)
        ok = SecDbItemSetRowId(item, q->q_row_id, error);

    if (ok)
        ok = SecDbItemInsert(item, dbt, error);
    if (ok) {
        if (result && q->q_return_type) {
            *result = SecDbItemCopyResult(item, q->q_return_type, error);
        }
    }
    if (!ok && error && *error) {
        if (CFEqual(CFErrorGetDomain(*error), kSecDbErrorDomain) && CFErrorGetCode(*error) == SQLITE_CONSTRAINT) {
            CFReleaseNull(*error);
            SecError(errSecDuplicateItem, error, CFSTR("duplicate item %@"), item);
        }
    }

    if (ok) {
        q->q_changed = true;
        if (SecDbItemIsSyncable(item))
            q->q_sync_changed = true;
    }

    secdebug("dbitem", "inserting item %@%s%@", item, ok ? "" : "failed: ", ok || error == NULL ? (CFErrorRef)CFSTR("") : *error);

    CFRelease(item);

	return ok;
}

typedef void (*s3dl_handle_row)(sqlite3_stmt *stmt, void *context);

/* Return a (mutable) dictionary if plist is a dictionary, return NULL and set error otherwise.  Does nothing if plist is already NULL. */
static CFMutableDictionaryRef dictionaryFromPlist(CFPropertyListRef plist, CFErrorRef *error) {
    if (plist && !isDictionary(plist)) {
        CFStringRef typeName = CFCopyTypeIDDescription(CFGetTypeID((CFTypeRef)plist));
        SecError(errSecDecode, error, CFSTR("plist is a %@, expecting a dictionary"), typeName);
        CFReleaseSafe(typeName);
        CFReleaseNull(plist);
    }
    return (CFMutableDictionaryRef)plist;
}

static CFMutableDictionaryRef s3dl_item_v2_decode(CFDataRef plain, CFErrorRef *error) {
    CFPropertyListRef item;
    item = CFPropertyListCreateWithData(0, plain, kCFPropertyListMutableContainers, NULL, error);
    return dictionaryFromPlist(item, error);
}

static CFMutableDictionaryRef s3dl_item_v3_decode(CFDataRef plain, CFErrorRef *error) {
    CFPropertyListRef item = NULL;
    const uint8_t *der = CFDataGetBytePtr(plain);
    const uint8_t *der_end = der + CFDataGetLength(plain);
    der = der_decode_plist(0, kCFPropertyListMutableContainers, &item, error, der, der_end);
    if (der && der != der_end) {
        SecCFCreateError(errSecDecode, kSecErrorDomain, CFSTR("trailing garbage at end of decrypted item"), NULL, error);
        CFReleaseNull(item);
    }
    return dictionaryFromPlist(item, error);
}

static CFMutableDictionaryRef
s3dl_item_from_data(CFDataRef edata, Query *q, CFArrayRef accessGroups, keyclass_t *keyclass, CFErrorRef *error) {
    CFMutableDictionaryRef item = NULL;
    CFDataRef plain =  NULL;

    /* Decrypt and decode the item and check the decoded attributes against the query. */
    uint32_t version;
    require_quiet((ks_decrypt_data(q->q_keybag, keyclass, edata, &plain, &version, error)), out);
    if (version < 2) {
        goto out;
    }

    if (version < 3) {
        item = s3dl_item_v2_decode(plain, error);
    } else {
        item = s3dl_item_v3_decode(plain, error);
    }

    if (!item && plain) {
        secerror("decode v%d failed: %@ [item: %@]", version, error ? *error : NULL, plain);
    }
    if (item && !itemInAccessGroup(item, accessGroups)) {
        secerror("items accessGroup %@ not in %@",
                 CFDictionaryGetValue(item, kSecAttrAccessGroup),
                 accessGroups);
        CFReleaseNull(item);
    }
    /* TODO: Validate keyclass attribute. */

out:
    CFReleaseSafe(plain);
    return item;
}

static CFDataRef
s3dl_copy_data_from_col(sqlite3_stmt *stmt, int col, CFErrorRef *error) {
    return CFDataCreateWithBytesNoCopy(0, sqlite3_column_blob(stmt, col),
                                        sqlite3_column_bytes(stmt, col),
                                        kCFAllocatorNull);
}

static CFMutableDictionaryRef
s3dl_item_from_col(sqlite3_stmt *stmt, Query *q, int col,
                   CFArrayRef accessGroups, keyclass_t *keyclass, CFErrorRef *error) {
    CFMutableDictionaryRef item = NULL;
    CFDataRef edata = NULL;
    require(edata = s3dl_copy_data_from_col(stmt, col, error), out);
    item = s3dl_item_from_data(edata, q, accessGroups, keyclass, error);

out:
    CFReleaseSafe(edata);
    return item;
}

struct s3dl_query_ctx {
    Query *q;
    CFArrayRef accessGroups;
    CFTypeRef result;
    int found;
};

static bool match_item(Query *q, CFArrayRef accessGroups, CFDictionaryRef item);

static void s3dl_query_row(sqlite3_stmt *stmt, void *context) {
    struct s3dl_query_ctx *c = context;
    Query *q = c->q;

    CFMutableDictionaryRef item = s3dl_item_from_col(stmt, q, 1,
                                                     c->accessGroups, NULL, &q->q_error);
    sqlite_int64 rowid = sqlite3_column_int64(stmt, 0);
    if (!item) {
        secerror("decode %@,rowid=%" PRId64 " failed (%ld): %@", q->q_class->name, rowid, CFErrorGetCode(q->q_error), q->q_error);
        // errSecDecode means the tiem is corrupted, stash it for delete.
        if(CFErrorGetCode(q->q_error)==errSecDecode)
        {
            secerror("We should attempt to delete this row (%lld)", rowid);

            {
                CFDataRef edata = s3dl_copy_data_from_col(stmt, 1, NULL);
                CFMutableStringRef edatastring =  CFStringCreateMutable(kCFAllocatorDefault, 0);
                if(edatastring) {
                    CFStringAppendEncryptedData(edatastring, edata);
                    secnotice("item", "corrupted edata=%@", edatastring);
                }
                CFReleaseSafe(edata);
                CFReleaseSafe(edatastring);
            }

            if(q->corrupted_rows==NULL) {
                q->corrupted_rows=CFArrayCreateMutableForCFTypes(kCFAllocatorDefault);
            }

            if(q->corrupted_rows==NULL) {
                secerror("Could not create a mutable array to store corrupted row! No memory ?");
            } else {
                long long row=rowid;
                CFNumberRef number = CFNumberCreate(kCFAllocatorDefault, kCFNumberLongLongType, &row);
                if(number==NULL) {
                    secerror("Could not create a CFNumber to store corrupted row! No memory ?");
                } else {
                    CFArrayAppendValue(q->corrupted_rows, number);
                    CFReleaseNull(number);
                    /* Hide this error, this will just pretend the item didnt exist at all */
                    CFReleaseNull(q->q_error);
                }
            }
        }
        // q->q_error will be released appropriately by a call to query_error
        return;
    }

    if (q->q_class == &identity_class) {
        // TODO: Use col 2 for key rowid and use both rowids in persistent ref.
        CFMutableDictionaryRef key = s3dl_item_from_col(stmt, q, 3,
                                                        c->accessGroups, NULL, &q->q_error);

        /* TODO : if there is a errSecDecode error here, we should cleanup */
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

    if (!match_item(q, c->accessGroups, item))
        goto out;

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

static void
SecDbAppendWhereROWID(CFMutableStringRef sql,
                    CFStringRef col, sqlite_int64 row_id,
                    bool *needWhere) {
    if (row_id > 0) {
        SecDbAppendWhereOrAnd(sql, needWhere);
        CFStringAppendFormat(sql, NULL, CFSTR("%@=%lld"), col, row_id);
    }
}

static void
SecDbAppendWhereAttrs(CFMutableStringRef sql, const Query *q, bool *needWhere) {
    CFIndex ix, attr_count = query_attr_count(q);
    for (ix = 0; ix < attr_count; ++ix) {
        SecDbAppendWhereOrAndEquals(sql, query_attr_at(q, ix).key, needWhere);
    }
}

static void
SecDbAppendWhereAccessGroups(CFMutableStringRef sql,
                           CFStringRef col,
                           CFArrayRef accessGroups,
                           bool *needWhere) {
    CFIndex ix, ag_count;
    if (!accessGroups || 0 == (ag_count = CFArrayGetCount(accessGroups))) {
        return;
    }

    SecDbAppendWhereOrAnd(sql, needWhere);
    CFStringAppend(sql, col);
    CFStringAppend(sql, CFSTR(" IN (?"));
    for (ix = 1; ix < ag_count; ++ix) {
        CFStringAppend(sql, CFSTR(",?"));
    }
    CFStringAppend(sql, CFSTR(")"));
}

static void SecDbAppendWhereClause(CFMutableStringRef sql, const Query *q,
    CFArrayRef accessGroups) {
    bool needWhere = true;
    SecDbAppendWhereROWID(sql, CFSTR("ROWID"), q->q_row_id, &needWhere);
    SecDbAppendWhereAttrs(sql, q, &needWhere);
    SecDbAppendWhereAccessGroups(sql, CFSTR("agrp"), accessGroups, &needWhere);
}

static void SecDbAppendLimit(CFMutableStringRef sql, CFIndex limit) {
    if (limit != kSecMatchUnlimited)
        CFStringAppendFormat(sql, NULL, CFSTR(" LIMIT %" PRIdCFIndex), limit);
}

static CFStringRef s3dl_select_sql(Query *q, CFArrayRef accessGroups) {
    CFMutableStringRef sql = CFStringCreateMutable(NULL, 0);
	if (q->q_class == &identity_class) {
        CFStringAppendFormat(sql, NULL, CFSTR("SELECT crowid, "
            CERTIFICATE_DATA_COLUMN_LABEL ", rowid,data FROM "
            "(SELECT cert.rowid AS crowid, cert.labl AS labl,"
            " cert.issr AS issr, cert.slnr AS slnr, cert.skid AS skid,"
            " keys.*,cert.data AS " CERTIFICATE_DATA_COLUMN_LABEL
            " FROM keys, cert"
            " WHERE keys.priv == 1 AND cert.pkhh == keys.klbl"));
        SecDbAppendWhereAccessGroups(sql, CFSTR("cert.agrp"), accessGroups, 0);
        /* The next 3 SecDbAppendWhere calls are in the same order as in
           SecDbAppendWhereClause().  This makes sqlBindWhereClause() work,
           as long as we do an extra sqlBindAccessGroups first. */
        SecDbAppendWhereROWID(sql, CFSTR("crowid"), q->q_row_id, 0);
        CFStringAppend(sql, CFSTR(")"));
        bool needWhere = true;
        SecDbAppendWhereAttrs(sql, q, &needWhere);
        SecDbAppendWhereAccessGroups(sql, CFSTR("agrp"), accessGroups, &needWhere);
	} else {
        CFStringAppend(sql, CFSTR("SELECT rowid, data FROM "));
		CFStringAppend(sql, q->q_class->name);
        SecDbAppendWhereClause(sql, q, accessGroups);
    }
    SecDbAppendLimit(sql, q->q_limit);

    return sql;
}

static bool sqlBindAccessGroups(sqlite3_stmt *stmt, CFArrayRef accessGroups,
                               int *pParam, CFErrorRef *error) {
    bool result = true;
    int param = *pParam;
    CFIndex ix, count = accessGroups ? CFArrayGetCount(accessGroups) : 0;
    for (ix = 0; ix < count; ++ix) {
        result = SecDbBindObject(stmt, param++,
                                  CFArrayGetValueAtIndex(accessGroups, ix),
                                  error);
        if (!result)
            break;
    }
    *pParam = param;
    return result;
}

static bool sqlBindWhereClause(sqlite3_stmt *stmt, const Query *q,
    CFArrayRef accessGroups, int *pParam, CFErrorRef *error) {
    bool result = true;
    int param = *pParam;
    CFIndex ix, attr_count = query_attr_count(q);
    for (ix = 0; ix < attr_count; ++ix) {
        result = SecDbBindObject(stmt, param++, query_attr_at(q, ix).value, error);
        if (!result)
            break;
	}

    /* Bind the access group to the sql. */
    if (result) {
        result = sqlBindAccessGroups(stmt, accessGroups, &param, error);
    }

    *pParam = param;
    return result;
}

static bool SecDbItemQuery(SecDbQueryRef query, CFArrayRef accessGroups, SecDbConnectionRef dbconn, CFErrorRef *error,
                    void (^handle_row)(SecDbItemRef item, bool *stop)) {
    __block bool ok = true;
    /* Sanity check the query. */
    if (query->q_ref)
        return SecError(errSecValueRefUnsupported, error, CFSTR("value ref not supported by queries"));

    bool (^return_attr)(const SecDbAttr *attr) = ^bool (const SecDbAttr * attr) {
        return attr->kind == kSecDbRowIdAttr || attr->kind == kSecDbEncryptedDataAttr;
    };

    CFStringRef sql = s3dl_select_sql(query, accessGroups);
    ok = sql;
    if (sql) {
        ok &= SecDbPrepare(dbconn, sql, error, ^(sqlite3_stmt *stmt) {
            /* Bind the values being searched for to the SELECT statement. */
            int param = 1;
            if (query->q_class == &identity_class) {
                /* Bind the access groups to cert.agrp. */
                ok &= sqlBindAccessGroups(stmt, accessGroups, &param, error);
            }
            if (ok)
                ok &= sqlBindWhereClause(stmt, query, accessGroups, &param, error);
            if (ok) {
                SecDbStep(dbconn, stmt, error, ^(bool *stop) {
                    SecDbItemRef item = SecDbItemCreateWithStatement(kCFAllocatorDefault, query->q_class, stmt, query->q_keybag, error, return_attr);
                    if (item) {
                        if (match_item(query, accessGroups, item->attributes))
                            handle_row(item, stop);
                        CFRelease(item);
                    } else {
                        secerror("failed to create item from stmt: %@", error ? *error : (CFErrorRef)"no error");
                        if (error) {
                            CFReleaseNull(*error);
                        }
                        //*stop = true;
                        //ok = false;
                    }
                });
            }
        });
        CFRelease(sql);
    }

    return ok;
}

static bool
s3dl_query(SecDbConnectionRef dbt, s3dl_handle_row handle_row,
           void *context, CFErrorRef *error)
{
    struct s3dl_query_ctx *c = context;
    Query *q = c->q;
    CFArrayRef accessGroups = c->accessGroups;

    /* Sanity check the query. */
    if (q->q_ref)
        return SecError(errSecValueRefUnsupported, error, CFSTR("value ref not supported by queries"));

	/* Actual work here. */
    if (q->q_limit == 1) {
        c->result = NULL;
    } else {
        c->result = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
    }
    CFStringRef sql = s3dl_select_sql(q, accessGroups);
    bool ok = SecDbWithSQL(dbt, sql, error, ^(sqlite3_stmt *stmt) {
        bool sql_ok = true;
        /* Bind the values being searched for to the SELECT statement. */
        int param = 1;
        if (q->q_class == &identity_class) {
            /* Bind the access groups to cert.agrp. */
            sql_ok = sqlBindAccessGroups(stmt, accessGroups, &param, error);
        }
        if (sql_ok)
            sql_ok = sqlBindWhereClause(stmt, q, accessGroups, &param, error);
        if (sql_ok) {
            SecDbForEach(stmt, error, ^bool (int row_index) {
                handle_row(stmt, context);
                return (!q->q_error) && (q->q_limit == kSecMatchUnlimited || c->found < q->q_limit);
            });
        }
        return sql_ok;
    });

    CFRelease(sql);

    // First get the error from the query, since errSecDuplicateItem from an
    // update query should superceed the errSecItemNotFound below.
    if (!query_error(q, error))
        ok = false;
    if (ok && c->found == 0)
        ok = SecError(errSecItemNotFound, error, CFSTR("no matching items found"));

    return ok;
}

#if 0
/* Gross hack to recover from item corruption */
static void
s3dl_cleanup_corrupted(SecDbConnectionRef dbt, Query *q, CFErrorRef *error)
{

    if(q->corrupted_rows==NULL)
        return;

    __security_simulatecrash(CFSTR("Corrupted items found in keychain"));

    if (q->q_class == &identity_class) {
        /* TODO: how to cleanup in that case */
        secerror("Cleaning up corrupted identities is not implemented yet");
        goto out;
    }

    CFArrayForEach(q->corrupted_rows, ^(const void *value) {
        CFMutableStringRef sql=CFStringCreateMutable(kCFAllocatorDefault, 0);

        if(sql==NULL) {
            secerror("Could not allocate CFString for sql, out of memory ?");
        } else {
            CFStringAppend(sql, CFSTR("DELETE FROM "));
            CFStringAppend(sql, q->q_class->name);
            CFStringAppendFormat(sql, NULL, CFSTR(" WHERE rowid=%@"), value);

            secerror("Attempting cleanup with %@", sql);

            if(!SecDbExec(dbt, sql, error)) {
                secerror("Cleanup Failed using %@, error: %@", sql, error?NULL:*error);
            } else {
                secerror("Cleanup Succeeded using %@", sql);
            }

            CFReleaseSafe(sql);
        }
    });

out:
    CFReleaseNull(q->corrupted_rows);
}
#endif

static bool
s3dl_copy_matching(SecDbConnectionRef dbt, Query *q, CFTypeRef *result,
                   CFArrayRef accessGroups, CFErrorRef *error)
{
    struct s3dl_query_ctx ctx = {
        .q = q, .accessGroups = accessGroups,
    };
    if (q->q_row_id && query_attr_count(q))
        return SecError(errSecItemIllegalQuery, error,
                        CFSTR("attributes to query illegal; both row_id and other attributes can't be searched at the same time"));

    // Only copy things that aren't tombstones unless the client explicitly asks otherwise.
    if (!CFDictionaryContainsKey(q->q_item, kSecAttrTombstone))
        query_add_attribute(kSecAttrTombstone, kCFBooleanFalse, q);
    bool ok = s3dl_query(dbt, s3dl_query_row, &ctx, error);
    if (ok && result)
        *result = ctx.result;
    else
        CFReleaseSafe(ctx.result);

    // s3dl_cleanup_corrupted(dbt, q, error);

    return ok;
}

/* AUDIT[securityd](done):
   attributesToUpdate (ok) is a caller provided dictionary,
       only its cf types have been checked.
 */
static bool
s3dl_query_update(SecDbConnectionRef dbt, Query *q,
    CFDictionaryRef attributesToUpdate, CFArrayRef accessGroups, CFErrorRef *error)
{
    /* Sanity check the query. */
    if (query_match_count(q) != 0)
        return SecError(errSecItemMatchUnsupported, error, CFSTR("match not supported in attributes to update"));
    if (q->q_ref)
        return SecError(errSecValueRefUnsupported, error, CFSTR("value ref not supported in attributes to update"));
    if (q->q_row_id && query_attr_count(q))
        return SecError(errSecItemIllegalQuery, error, CFSTR("attributes to update illegal; both row_id and other attributes can't be updated at the same time"));

    __block bool result = true;
    Query *u = query_create(q->q_class, attributesToUpdate, error);
    if (u == NULL) return false;
    require_action_quiet(query_update_parse(u, attributesToUpdate, error), errOut, result = false);
    query_pre_update(u);
    result &= SecDbTransaction(dbt, kSecDbExclusiveTransactionType, error, ^(bool *commit) {
        // Make sure we only update real items, not tombstones, unless the client explicitly asks otherwise.
        if (!CFDictionaryContainsKey(q->q_item, kSecAttrTombstone))
            query_add_attribute(kSecAttrTombstone, kCFBooleanFalse, q);
        result &= SecDbItemQuery(q, accessGroups, dbt, error, ^(SecDbItemRef item, bool *stop) {
            //We always need to know the error here.
            CFErrorRef localError = NULL;
            SecDbItemRef new_item = SecDbItemCopyWithUpdates(item, u->q_item, &localError);
            if(SecErrorGetOSStatus(localError)==errSecDecode) {
                // We just ignore this, and treat as if item is not found
                secerror("Trying to update to a corrupted item");
                CFReleaseSafe(localError);
                return;
            }

            if (error && *error == NULL) {
                *error = localError;
                localError = NULL;
            }
            CFReleaseSafe(localError);

            result = new_item;
            if (new_item) {
                bool item_is_sync = SecDbItemIsSyncable(item);
                bool makeTombstone = q->q_use_tomb ? CFBooleanGetValue(q->q_use_tomb) : (item_is_sync && !SecDbItemIsTombstone(item));
                result = SecDbItemUpdate(item, new_item, dbt, makeTombstone, error);
                if (result) {
                    q->q_changed = true;
                    if (item_is_sync || SecDbItemIsSyncable(new_item))
                        q->q_sync_changed = true;
                }
                CFRelease(new_item);
            }
            if (!result)
                *stop = true;
        });
        if (!result)
            *commit = false;
    });
    if (result && !q->q_changed)
        result = SecError(errSecItemNotFound, error, CFSTR("No items updated"));
errOut:
    if (!query_destroy(u, error))
        result = false;
    return result;
}

static bool
s3dl_query_delete(SecDbConnectionRef dbt, Query *q, CFArrayRef accessGroups, CFErrorRef *error)
{
    __block bool ok = true;
    // Only delete things that aren't tombstones, unless the client explicitly asks otherwise.
    if (!CFDictionaryContainsKey(q->q_item, kSecAttrTombstone))
        query_add_attribute(kSecAttrTombstone, kCFBooleanFalse, q);
    ok &= SecDbItemSelect(q, dbt, error, ^bool(const SecDbAttr *attr) {
        return false;
    },^bool(CFMutableStringRef sql, bool *needWhere) {
        SecDbAppendWhereClause(sql, q, accessGroups);
        return true;
    },^bool(sqlite3_stmt * stmt, int col) {
        return sqlBindWhereClause(stmt, q, accessGroups, &col, error);
    }, ^(SecDbItemRef item, bool *stop) {
        bool item_is_sync = SecDbItemIsSyncable(item);
        bool makeTombstone = q->q_use_tomb ? CFBooleanGetValue(q->q_use_tomb) : (item_is_sync && !SecDbItemIsTombstone(item));
        ok = SecDbItemDelete(item, dbt, makeTombstone, error);
        if (ok) {
            q->q_changed = true;
            if (item_is_sync)
                q->q_sync_changed = true;
        }
    });
    if (ok && !q->q_changed) {
        ok = SecError(errSecItemNotFound, error, CFSTR("Delete failed to delete anything"));
    }
    return ok;
}

/* Return true iff the item in question should not be backed up, nor restored,
   but when restoring a backup the original version of the item should be
   added back to the keychain again after the restore completes. */
static bool SecItemIsSystemBound(CFDictionaryRef item, const SecDbClass *class) {
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
static bool SecServerDeleteAll(SecDbConnectionRef dbt, CFErrorRef *error) {
    return kc_transaction(dbt, error, ^{
        bool ok = (SecDbExec(dbt, CFSTR("DELETE from genp;"), error) &&
                   SecDbExec(dbt, CFSTR("DELETE from inet;"), error) &&
                   SecDbExec(dbt, CFSTR("DELETE from cert;"), error) &&
                   SecDbExec(dbt, CFSTR("DELETE from keys;"), error));
        return ok;
    });
}

struct s3dl_export_row_ctx {
    struct s3dl_query_ctx qc;
    keybag_handle_t dest_keybag;
    enum SecItemFilter filter;
    SecDbConnectionRef dbt;
};

static void s3dl_export_row(sqlite3_stmt *stmt, void *context) {
    struct s3dl_export_row_ctx *c = context;
    Query *q = c->qc.q;
    keyclass_t keyclass = 0;
    CFErrorRef localError = NULL;

    sqlite_int64 rowid = sqlite3_column_int64(stmt, 0);
    CFMutableDictionaryRef item = s3dl_item_from_col(stmt, q, 1, c->qc.accessGroups, &keyclass, &localError);

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
                    CFDataRef plain = kc_plist_copy_der(item, &q->q_error);
                    CFDictionaryRemoveAllValues(item);
                    if (plain) {
                        CFDataRef edata = NULL;
                        if (ks_encrypt_data(c->dest_keybag, keyclass, plain, &edata, &q->q_error)) {
                            CFDictionarySetValue(item, kSecValueData, edata);
                            CFReleaseSafe(edata);
                        } else {
                            seccritical("ks_encrypt_data %@,rowid=%" PRId64 ": failed: %@", q->q_class->name, rowid, q->q_error);
                            CFReleaseNull(q->q_error);
                        }
                        CFRelease(plain);
                    }
                }
                if (CFDictionaryGetCount(item)) {
                    CFDictionarySetValue(item, kSecValuePersistentRef, pref);
                    CFArrayAppendValue((CFMutableArrayRef)c->qc.result, item);
                    c->qc.found++;
                }
                CFReleaseSafe(pref);
            }
        }
        CFRelease(item);
    } else {
        /* This happens a lot when trying to migrate keychain before first unlock, so only a notice */
        /* If the error is "corrupted item" then we just ignore it, otherwise we save it in the query */
        secnotice("item","Could not export item for rowid %llu: %@", rowid, localError);
        if(SecErrorGetOSStatus(localError)==errSecDecode) {
            CFReleaseNull(localError);
        } else {
            CFReleaseSafe(q->q_error);
            q->q_error=localError;
        }
    }
}

static CF_RETURNS_RETAINED CFDictionaryRef SecServerExportKeychainPlist(SecDbConnectionRef dbt,
    keybag_handle_t src_keybag, keybag_handle_t dest_keybag,
    enum SecItemFilter filter, CFErrorRef *error) {
    CFMutableDictionaryRef keychain;
    keychain = CFDictionaryCreateMutable(kCFAllocatorDefault, 0,
        &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    if (!keychain) {
        if (error && !*error)
            SecError(errSecAllocate, error, CFSTR("Can't create keychain dictionary"));
         goto errOut;
    }
    unsigned class_ix;
    Query q = { .q_keybag = src_keybag };
    q.q_return_type = kSecReturnDataMask | kSecReturnAttributesMask | \
        kSecReturnPersistentRefMask;
    q.q_limit = kSecMatchUnlimited;

    /* Get rid of this duplicate. */
    const SecDbClass *SecDbClasses[] = {
        &genp_class,
        &inet_class,
        &cert_class,
        &keys_class
    };

    for (class_ix = 0; class_ix < array_size(SecDbClasses);
        ++class_ix) {
        q.q_class = SecDbClasses[class_ix];
        struct s3dl_export_row_ctx ctx = {
            .qc = { .q = &q, },
            .dest_keybag = dest_keybag, .filter = filter,
            .dbt = dbt,
        };

        secnotice("item", "exporting class '%@'", q.q_class->name);

        CFErrorRef localError = NULL;
        if (s3dl_query(dbt, s3dl_export_row, &ctx, &localError)) {
            if (CFArrayGetCount(ctx.qc.result))
                CFDictionaryAddValue(keychain, q.q_class->name, ctx.qc.result);

        } else {
            OSStatus status = (OSStatus)CFErrorGetCode(localError);
            if (status == errSecItemNotFound) {
                CFRelease(localError);
            } else {
                secerror("Export failed: %@", localError);
                if (error) {
                    CFReleaseSafe(*error);
                    *error = localError;
                } else {
                    CFRelease(localError);
                }
                CFReleaseNull(keychain);
                break;
            }
        }
        CFReleaseNull(ctx.qc.result);
    }
errOut:
    return keychain;
}

static CF_RETURNS_RETAINED CFDataRef SecServerExportKeychain(SecDbConnectionRef dbt,
    keybag_handle_t src_keybag, keybag_handle_t dest_keybag, CFErrorRef *error) {
    CFDataRef data_out = NULL;
    /* Export everything except the items for which SecItemIsSystemBound()
       returns true. */
    CFDictionaryRef keychain = SecServerExportKeychainPlist(dbt,
        src_keybag, dest_keybag, kSecBackupableItemFilter,
        error);
    if (keychain) {
        data_out = CFPropertyListCreateData(kCFAllocatorDefault, keychain,
                                             kCFPropertyListBinaryFormat_v1_0,
                                             0, error);
        CFRelease(keychain);
    }

    return data_out;
}

struct SecServerImportClassState {
	SecDbConnectionRef dbt;
    CFErrorRef error;
    keybag_handle_t src_keybag;
    keybag_handle_t dest_keybag;
    enum SecItemFilter filter;
};

struct SecServerImportItemState {
    const SecDbClass *class;
	struct SecServerImportClassState *s;
};

/* Infer accessibility and access group for pre-v2 (iOS4.x and earlier) items
 being imported from a backup.  */
static bool SecDbItemImportMigrate(SecDbItemRef item, CFErrorRef *error) {
    bool ok = true;
    CFStringRef agrp = SecDbItemGetCachedValueWithName(item, kSecAttrAccessGroup);
    CFStringRef accessible = SecDbItemGetCachedValueWithName(item, kSecAttrAccessible);

    if (!isString(agrp) || !isString(accessible))
        return ok;
    if (SecDbItemGetClass(item) == &genp_class && CFEqual(accessible, kSecAttrAccessibleAlways)) {
        CFStringRef svce = SecDbItemGetCachedValueWithName(item, kSecAttrService);
        if (!isString(svce)) return ok;
        if (CFEqual(agrp, CFSTR("apple"))) {
            if (CFEqual(svce, CFSTR("AirPort"))) {
                ok = SecDbItemSetValueWithName(item, kSecAttrAccessible, kSecAttrAccessibleAfterFirstUnlock, error);
            } else if (CFEqual(svce, CFSTR("com.apple.airplay.password"))) {
                ok = SecDbItemSetValueWithName(item, kSecAttrAccessible, kSecAttrAccessibleWhenUnlocked, error);
            } else if (CFEqual(svce, CFSTR("YouTube"))) {
                ok = (SecDbItemSetValueWithName(item, kSecAttrAccessible, kSecAttrAccessibleWhenUnlocked, error) &&
                      SecDbItemSetValueWithName(item, kSecAttrAccessGroup, CFSTR("com.apple.youtube.credentials"), error));
            } else {
                CFStringRef desc = SecDbItemGetCachedValueWithName(item, kSecAttrDescription);
                if (!isString(desc)) return ok;
                if (CFEqual(desc, CFSTR("IPSec Shared Secret")) || CFEqual(desc, CFSTR("PPP Password"))) {
                    ok = SecDbItemSetValueWithName(item, kSecAttrAccessible, kSecAttrAccessibleAfterFirstUnlock, error);
                }
            }
        }
    } else if (SecDbItemGetClass(item) == &inet_class && CFEqual(accessible, kSecAttrAccessibleAlways)) {
        if (CFEqual(agrp, CFSTR("PrintKitAccessGroup"))) {
            ok = SecDbItemSetValueWithName(item, kSecAttrAccessible, kSecAttrAccessibleWhenUnlocked, error);
        } else if (CFEqual(agrp, CFSTR("apple"))) {
            CFTypeRef ptcl = SecDbItemGetCachedValueWithName(item, kSecAttrProtocol);
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
                ok = SecDbItemSetValueWithName(item, kSecAttrAccessible, kSecAttrAccessibleWhenUnlocked, error);
        }
    }
    return ok;
}

bool SecDbItemDecrypt(SecDbItemRef item, CFDataRef edata, CFErrorRef *error) {
    bool ok = true;
    CFDataRef pdata = NULL;
    keyclass_t keyclass;
    uint32_t version;
    ok = ks_decrypt_data(SecDbItemGetKeybag(item), &keyclass, edata, &pdata, &version, error);
    if (!ok)
        return ok;

    if (version < 2) {
        /* Old V4 style keychain backup being imported. */
        ok = SecDbItemSetValueWithName(item, CFSTR("v_Data"), pdata, error) &&
        SecDbItemImportMigrate(item, error);
    } else {
        CFDictionaryRef dict;
        if (version < 3) {
            dict = s3dl_item_v2_decode(pdata, error);
        } else {
            dict = s3dl_item_v3_decode(pdata, error);
        }
        ok = dict && SecDbItemSetValues(item, dict, error);
	CFReleaseSafe(dict);
    }

    CFReleaseSafe(pdata);

    keyclass_t my_keyclass = SecDbItemGetKeyclass(item, NULL);
    if (!my_keyclass) {
        ok = ok && SecDbItemSetKeyclass(item, keyclass, error);
    } else {
        /* Make sure the keyclass in the dictionary matched what we got
           back from decoding the data blob. */
        if (my_keyclass != keyclass) {
            ok = SecError(errSecDecode, error, CFSTR("keyclass attribute %d doesn't match keyclass in blob %d"), my_keyclass, keyclass);
        }
    }

    return ok;
}

/* Automagically make a item syncable, based on various attributes. */
static bool SecDbItemInferSyncable(SecDbItemRef item, CFErrorRef *error)
{
    CFStringRef agrp = SecDbItemGetCachedValueWithName(item, kSecAttrAccessGroup);

    if (!isString(agrp))
        return true;

    if (CFEqual(agrp, CFSTR("com.apple.cfnetwork")) && SecDbItemGetClass(item) == &inet_class) {
        CFTypeRef srvr = SecDbItemGetCachedValueWithName(item, kSecAttrServer);
        CFTypeRef ptcl = SecDbItemGetCachedValueWithName(item, kSecAttrProtocol);
        CFTypeRef atyp = SecDbItemGetCachedValueWithName(item, kSecAttrAuthenticationType);

        if (isString(srvr) && isString(ptcl) && isString(atyp)) {
            /* This looks like a Mobile Safari Password,  make syncable */
            secnotice("item", "Make this item syncable: %@", item);
            return SecDbItemSetSyncable(item, true, error);
        }
    }

    return true;
}

/* This create a SecDbItem from the item dictionnary that are exported for backups.
   Item are stored in the backup as a dictionary containing two keys:
    - v_Data: the encrypted data blob
    - v_PersistentRef: a persistent Ref.
   src_keybag is normally the backup keybag.
   dst_keybag is normally the device keybag.
*/
static SecDbItemRef SecDbItemCreateWithBackupDictionary(CFAllocatorRef allocator, const SecDbClass *dbclass, CFDictionaryRef dict, keybag_handle_t src_keybag, keybag_handle_t dst_keybag, CFErrorRef *error)
{
    CFDataRef edata = CFDictionaryGetValue(dict, CFSTR("v_Data"));
    SecDbItemRef item = NULL;

    if (edata) {
        item = SecDbItemCreateWithEncryptedData(kCFAllocatorDefault, dbclass, edata, src_keybag, error);
        if (item)
            if (!SecDbItemSetKeybag(item, dst_keybag, error))
                CFReleaseNull(item);
    } else {
        SecError(errSecDecode, error, CFSTR("No v_Data in backup dictionary %@"), dict);
    }

    return item;
}

static bool SecDbItemExtractRowIdFromBackupDictionary(SecDbItemRef item, CFDictionaryRef dict, CFErrorRef *error) {
    CFDataRef ref = CFDictionaryGetValue(dict, CFSTR("v_PersistentRef"));
    if (!ref)
        return SecError(errSecDecode, error, CFSTR("No v_PersistentRef in backup dictionary %@"), dict);

    CFStringRef className;
    sqlite3_int64 rowid;
    if (!_SecItemParsePersistentRef(ref, &className, &rowid))
        return SecError(errSecDecode, error, CFSTR("v_PersistentRef %@ failed to decode"), ref);

    if (!CFEqual(SecDbItemGetClass(item)->name, className))
        return SecError(errSecDecode, error, CFSTR("v_PersistentRef has unexpected class %@"), className);

    return SecDbItemSetRowId(item, rowid, error);
}

static void SecServerImportItem(const void *value, void *context) {
    struct SecServerImportItemState *state =
        (struct SecServerImportItemState *)context;
    if (state->s->error)
        return;
    if (!isDictionary(value)) {
        SecError(errSecParam, &state->s->error, CFSTR("value %@ is not a dictionary"), value);
        return;
    }

    CFDictionaryRef dict = (CFDictionaryRef)value;

    secdebug("item", "Import Item : %@", dict);

    /* We don't filter non sys_bound items during import since we know we
       will never have any in this case, we use the kSecSysBoundItemFilter
       to indicate that we don't preserve rowid's during import instead. */
    if (state->s->filter == kSecBackupableItemFilter &&
        SecItemIsSystemBound(dict, state->class))
        return;

    SecDbItemRef item;

    /* This is sligthly confusing:
       - During upgrade all items are exported with KEYBAG_NONE.
       - During restore from backup, existing sys_bound items are exported with KEYBAG_NONE, and are exported as dictionary of attributes.
       - Item in the actual backup are export with a real keybag, and are exported as encrypted v_Data and v_PersistentRef
    */
    if (state->s->src_keybag == KEYBAG_NONE) {
        item = SecDbItemCreateWithAttributes(kCFAllocatorDefault, state->class, dict, state->s->dest_keybag,  &state->s->error);
    } else {
        item = SecDbItemCreateWithBackupDictionary(kCFAllocatorDefault, state->class, dict, state->s->src_keybag, state->s->dest_keybag, &state->s->error);
    }

    if (item) {
        if(state->s->filter != kSecSysBoundItemFilter) {
            SecDbItemExtractRowIdFromBackupDictionary(item, dict, &state->s->error);
        }
        SecDbItemInferSyncable(item, &state->s->error);
        SecDbItemInsert(item, state->s->dbt, &state->s->error);
    }

    /* Reset error if we had one, since we just skip the current item
       and continue importing what we can. */
    if (state->s->error) {
        secwarning("Failed to import an item (%@) of class '%@': %@ - ignoring error.",
                 item, state->class->name, state->s->error);
        CFReleaseNull(state->s->error);
    }

    CFReleaseSafe(item);
}

static void SecServerImportClass(const void *key, const void *value,
    void *context) {
    struct SecServerImportClassState *state =
        (struct SecServerImportClassState *)context;
    if (state->error)
        return;
    if (!isString(key)) {
        SecError(errSecParam, &state->error, CFSTR("class name %@ is not a string"), key);
        return;
    }
    const SecDbClass *class = kc_class_with_name(key);
    if (!class || class == &identity_class) {
        SecError(errSecParam, &state->error, CFSTR("attempt to import an identity"));
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

static bool SecServerImportKeychainInPlist(SecDbConnectionRef dbt,
    keybag_handle_t src_keybag, keybag_handle_t dest_keybag,
    CFDictionaryRef keychain, enum SecItemFilter filter, CFErrorRef *error) {
    bool ok = true;

    CFDictionaryRef sys_bound = NULL;
    if (filter == kSecBackupableItemFilter) {
        /* Grab a copy of all the items for which SecItemIsSystemBound()
           returns true. */
        require(sys_bound = SecServerExportKeychainPlist(dbt, KEYBAG_DEVICE,
                                                         KEYBAG_NONE, kSecSysBoundItemFilter,
                                                         error), errOut);
    }

    /* Delete everything in the keychain. */
    require(ok = SecServerDeleteAll(dbt, error), errOut);

    struct SecServerImportClassState state = {
        .dbt = dbt,
        .src_keybag = src_keybag,
        .dest_keybag = dest_keybag,
        .filter = filter,
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
    if (state.error) {
        if (error) {
            CFReleaseSafe(*error);
            *error = state.error;
        } else {
            CFRelease(state.error);
        }
        ok = false;
    }

errOut:
    return ok;
}

static bool SecServerImportKeychain(SecDbConnectionRef dbt,
    keybag_handle_t src_keybag,
    keybag_handle_t dest_keybag, CFDataRef data, CFErrorRef *error) {
    return kc_transaction(dbt, error, ^{
        bool ok = false;
        CFDictionaryRef keychain;
        keychain = CFPropertyListCreateWithData(kCFAllocatorDefault, data,
                                                kCFPropertyListImmutable, NULL,
                                                error);
        if (keychain) {
            if (isDictionary(keychain)) {
                ok = SecServerImportKeychainInPlist(dbt, src_keybag,
                                                    dest_keybag, keychain,
                                                    kSecBackupableItemFilter,
                                                    error);
            } else {
                ok = SecError(errSecParam, error, CFSTR("import: keychain is not a dictionary"));
            }
            CFRelease(keychain);
        }
        return ok;
    });
}

static bool ks_open_keybag(CFDataRef keybag, CFDataRef password, keybag_handle_t *handle, CFErrorRef *error) {
#if USE_KEYSTORE
    kern_return_t kernResult;
    kernResult = aks_load_bag(CFDataGetBytePtr(keybag), (int)CFDataGetLength(keybag), handle);
    if (kernResult)
        return SecKernError(kernResult, error, CFSTR("aks_load_bag failed: %@"), keybag);

    if (password) {
        kernResult = aks_unlock_bag(*handle, CFDataGetBytePtr(password), (int)CFDataGetLength(password));
        if (kernResult) {
            aks_unload_bag(*handle);
            return SecKernError(kernResult, error, CFSTR("aks_unlock_bag failed"));
        }
    }
    return true;
#else /* !USE_KEYSTORE */
    *handle = KEYBAG_NONE;
    return true;
#endif /* USE_KEYSTORE */
}

static bool ks_close_keybag(keybag_handle_t keybag, CFErrorRef *error) {
#if USE_KEYSTORE
	IOReturn kernResult = aks_unload_bag(keybag);
    if (kernResult) {
        return SecKernError(kernResult, error, CFSTR("aks_unload_bag failed"));
    }
#endif /* USE_KEYSTORE */
    return true;
}

static CF_RETURNS_RETAINED CFDataRef SecServerKeychainBackup(SecDbConnectionRef dbt, CFDataRef keybag,
    CFDataRef password, CFErrorRef *error) {
    CFDataRef backup = NULL;
    keybag_handle_t backup_keybag;
    if (ks_open_keybag(keybag, password, &backup_keybag, error)) {
        /* Export from system keybag to backup keybag. */
        backup = SecServerExportKeychain(dbt, KEYBAG_DEVICE, backup_keybag, error);
        if (!ks_close_keybag(backup_keybag, error)) {
            CFReleaseNull(backup);
        }
    }
    return backup;
}

static bool SecServerKeychainRestore(SecDbConnectionRef dbt, CFDataRef backup,
    CFDataRef keybag, CFDataRef password, CFErrorRef *error) {
    keybag_handle_t backup_keybag;
    if (!ks_open_keybag(keybag, password, &backup_keybag, error))
        return false;

    /* Import from backup keybag to system keybag. */
    bool ok = SecServerImportKeychain(dbt, backup_keybag, KEYBAG_DEVICE,
                                      backup, error);
    ok &= ks_close_keybag(backup_keybag, error);

    return ok;
}


// MARK - External SPI support code.

CFStringRef __SecKeychainCopyPath(void) {
    CFStringRef kcRelPath = NULL;
    if (use_hwaes()) {
        kcRelPath = CFSTR("keychain-2.db");
    } else {
        kcRelPath = CFSTR("keychain-2-debug.db");
    }

    CFStringRef kcPath = NULL;
    CFURLRef kcURL = SecCopyURLForFileInKeychainDirectory(kcRelPath);
    if (kcURL) {
        kcPath = CFURLCopyFileSystemPath(kcURL, kCFURLPOSIXPathStyle);
        CFRelease(kcURL);
    }
    return kcPath;

}

// MARK; -
// MARK: kc_dbhandle init and reset

static SecDbRef SecKeychainDbCreate(CFStringRef path) {
    return SecDbCreate(path, ^bool (SecDbConnectionRef dbconn, bool didCreate, CFErrorRef *localError) {
        bool ok;
        if (didCreate)
            ok = s3dl_dbt_upgrade_from_version(dbconn, 0, localError);
        else
            ok = s3dl_dbt_upgrade(dbconn, localError);

        if (!ok)
            secerror("Upgrade %sfailed: %@", didCreate ? "from v0 " : "", localError ? *localError : NULL);

        return ok;
    });
}

static SecDbRef _kc_dbhandle = NULL;

static void kc_dbhandle_init(void) {
    SecDbRef oldHandle = _kc_dbhandle;
    _kc_dbhandle = NULL;
    CFStringRef dbPath = __SecKeychainCopyPath();
    if (dbPath) {
        _kc_dbhandle = SecKeychainDbCreate(dbPath);
        CFRelease(dbPath);
    }
    if (oldHandle) {
        secerror("replaced %@ with %@", oldHandle, _kc_dbhandle);
        CFRelease(oldHandle);
    }
}

static dispatch_once_t _kc_dbhandle_once;

static SecDbRef kc_dbhandle(void) {
    dispatch_once(&_kc_dbhandle_once, ^{
        kc_dbhandle_init();
    });
    return _kc_dbhandle;
}

/* For whitebox testing only */
void kc_dbhandle_reset(void);
void kc_dbhandle_reset(void)
{
    __block bool done = false;
    dispatch_once(&_kc_dbhandle_once, ^{
        kc_dbhandle_init();
        done = true;
    });
    // TODO: Not thread safe at all! - FOR DEBUGGING ONLY
    if (!done)
        kc_dbhandle_init();
}

static SecDbConnectionRef kc_aquire_dbt(bool writeAndRead, CFErrorRef *error) {
    return SecDbConnectionAquire(kc_dbhandle(), !writeAndRead, error);
}

/* Return a per thread dbt handle for the keychain.  If create is true create
 the database if it does not yet exist.  If it is false, just return an
 error if it fails to auto-create. */
static bool kc_with_dbt(bool writeAndRead, CFErrorRef *error, bool (^perform)(SecDbConnectionRef dbt))
{
    bool ok = false;
    SecDbConnectionRef dbt = kc_aquire_dbt(writeAndRead, error);
    if (dbt) {
        ok = perform(dbt);
        SecDbConnectionRelease(dbt);
    }
    return ok;
}

static bool
items_matching_issuer_parent(CFArrayRef accessGroups,
                            CFDataRef issuer, CFArrayRef issuers, int recurse)
{
    Query *q;
    CFArrayRef results = NULL;
    SecDbConnectionRef dbt = NULL;
    CFIndex i, count;
    bool found = false;

    if (CFArrayContainsValue(issuers, CFRangeMake(0, CFArrayGetCount(issuers)), issuer))
        return true;

    const void *keys[] = { kSecClass, kSecReturnRef, kSecAttrSubject };
    const void *vals[] = { kSecClassCertificate, kCFBooleanTrue, issuer };
    CFDictionaryRef query = CFDictionaryCreate(kCFAllocatorDefault, keys, vals, array_size(keys), NULL, NULL);

    if (!query)
        return false;

    CFErrorRef localError = NULL;
    q = query_create_with_limit(query, kSecMatchUnlimited, &localError);
    CFRelease(query);
    if (q) {
        if ((dbt = SecDbConnectionAquire(kc_dbhandle(), true, &localError))) {
            s3dl_copy_matching(dbt, q, (CFTypeRef*)&results, accessGroups, &localError);
            SecDbConnectionRelease(dbt);
        }
        query_destroy(q, &localError);
    }
    if (localError) {
        secerror("items matching issuer parent: %@", localError);
        CFReleaseNull(localError);
        return false;
    }

    count = CFArrayGetCount(results);
    for (i = 0; (i < count) && !found; i++) {
        CFDictionaryRef cert_dict = (CFDictionaryRef)CFArrayGetValueAtIndex(results, i);
        CFDataRef cert_issuer = CFDictionaryGetValue(cert_dict, kSecAttrIssuer);
        if (CFEqual(cert_issuer, issuer))
            continue;
        if (recurse-- > 0)
            found = items_matching_issuer_parent(accessGroups, cert_issuer, issuers, recurse);
    }
    CFRelease(results);

    return found;
}

static bool match_item(Query *q, CFArrayRef accessGroups, CFDictionaryRef item)
{
    if (q->q_match_issuer) {
        CFDataRef issuer = CFDictionaryGetValue(item, kSecAttrIssuer);
        if (!items_matching_issuer_parent(accessGroups, issuer, q->q_match_issuer, 10 /*max depth*/))
            return false;
    }

    /* Add future match checks here. */

    return true;
}

/****************************************************************************
 **************** Beginning of Externally Callable Interface ****************
 ****************************************************************************/

#if 0
// TODO Use as a safety wrapper
static bool SecErrorWith(CFErrorRef *in_error, bool (^perform)(CFErrorRef *error)) {
    CFErrorRef error = in_error ? *in_error : NULL;
    bool ok;
    if ((ok = perform(&error))) {
        assert(error == NULL);
        if (error)
            secerror("error + success: %@", error);
    } else {
        assert(error);
        OSStatus status = SecErrorGetOSStatus(error);
        if (status != errSecItemNotFound)           // Occurs in normal operation, so exclude
            secerror("error:[%" PRIdOSStatus "] %@", status, error);
        if (in_error) {
            *in_error = error;
        } else {
            CFReleaseNull(error);
        }
    }
    return ok;
}
#endif

/* AUDIT[securityd](done):
   query (ok) is a caller provided dictionary, only its cf type has been checked.
 */
static bool
SecItemServerCopyMatching(CFDictionaryRef query, CFTypeRef *result,
    CFArrayRef accessGroups, CFErrorRef *error)
{
    CFIndex ag_count;
    if (!accessGroups || 0 == (ag_count = CFArrayGetCount(accessGroups))) {
        return SecError(errSecMissingEntitlement, error,
                         CFSTR("client has neither application-identifier nor keychain-access-groups entitlements"));
    }

    if (CFArrayContainsValue(accessGroups, CFRangeMake(0, ag_count), CFSTR("*"))) {
        /* Having the special accessGroup "*" allows access to all accessGroups. */
        accessGroups = NULL;
    }

    bool ok = false;
    Query *q = query_create_with_limit(query, 1, error);
    if (q) {
        CFStringRef agrp = CFDictionaryGetValue(q->q_item, kSecAttrAccessGroup);
        if (agrp && accessGroupsAllows(accessGroups, agrp)) {
            // TODO: Return an error if agrp is not NULL and accessGroupsAllows() fails above.
            const void *val = agrp;
            accessGroups = CFArrayCreate(0, &val, 1, &kCFTypeArrayCallBacks);
        } else {
            CFRetainSafe(accessGroups);
        }

        /* Sanity check the query. */
        if (q->q_use_item_list) {
            ok = SecError(errSecUseItemListUnsupported, error, CFSTR("use item list unsupported"));
#if defined(MULTIPLE_KEYCHAINS)
        } else if (q->q_use_keychain) {
            ok = SecError(errSecUseKeychainUnsupported, error, CFSTR("use keychain list unsupported"));
#endif
        } else if (q->q_match_issuer && ((q->q_class != &cert_class) &&
                    (q->q_class != &identity_class))) {
            ok = SecError(errSecUnsupportedOperation, error, CFSTR("unsupported match attribute"));
        } else if (q->q_return_type != 0 && result == NULL) {
            ok = SecError(errSecReturnMissingPointer, error, CFSTR("missing pointer"));
        } else if (!q->q_error) {
            ok = kc_with_dbt(false, error, ^(SecDbConnectionRef dbt) {
                return s3dl_copy_matching(dbt, q, result, accessGroups, error);
            });
        }

        CFReleaseSafe(accessGroups);
        if (!query_destroy(q, error))
            ok = false;
    }

	return ok;
}

bool
_SecItemCopyMatching(CFDictionaryRef query, CFArrayRef accessGroups, CFTypeRef *result, CFErrorRef *error) {
    return SecItemServerCopyMatching(query, result, accessGroups, error);
}

/* AUDIT[securityd](done):
   attributes (ok) is a caller provided dictionary, only its cf type has
       been checked.
 */
bool
_SecItemAdd(CFDictionaryRef attributes, CFArrayRef accessGroups,
            CFTypeRef *result, CFErrorRef *error)
{
    bool ok = true;
    CFIndex ag_count;
    if (!accessGroups || 0 == (ag_count = CFArrayGetCount(accessGroups)))
        return SecError(errSecMissingEntitlement, error,
                           CFSTR("client has neither application-identifier nor keychain-access-groups entitlements"));

    Query *q = query_create_with_limit(attributes, 0, error);
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
                return SecError(errSecNoAccessForItem, error, CFSTR("NoAccessForItem"));
        } else {
            agrp = (CFStringRef)CFArrayGetValueAtIndex(ag, 0);

            /* We are using an implicit access group, add it as if the user
               specified it as an attribute. */
            query_add_attribute(kSecAttrAccessGroup, agrp, q);
        }

        query_ensure_keyclass(q, agrp);

        if (q->q_row_id)
            ok = SecError(errSecValuePersistentRefUnsupported, error, CFSTR("q_row_id"));  // TODO: better error string
    #if defined(MULTIPLE_KEYCHAINS)
        else if (q->q_use_keychain_list)
            ok = SecError(errSecUseKeychainListUnsupported, error, CFSTR("q_use_keychain_list"));  // TODO: better error string;
    #endif
        else if (!q->q_error) {
            ok = kc_with_dbt(true, error, ^(SecDbConnectionRef dbt){
                return kc_transaction(dbt, error, ^{
                    query_pre_add(q, true);
                    return s3dl_query_add(dbt, q, result, error);
                });
            });
        }
        ok = query_notify_and_destroy(q, ok, error);
    } else {
        ok = false;
    }
    return ok;
}

/* AUDIT[securityd](done):
   query (ok) and attributesToUpdate (ok) are a caller provided dictionaries,
       only their cf types have been checked.
 */
bool
_SecItemUpdate(CFDictionaryRef query, CFDictionaryRef attributesToUpdate,
               CFArrayRef accessGroups, CFErrorRef *error)
{
    CFIndex ag_count;
    if (!accessGroups || 0 == (ag_count = CFArrayGetCount(accessGroups))) {
        return SecError(errSecMissingEntitlement, error,
                         CFSTR("client has neither application-identifier nor keychain-access-groups entitlements"));
    }

    if (CFArrayContainsValue(accessGroups, CFRangeMake(0, ag_count), CFSTR("*"))) {
        /* Having the special accessGroup "*" allows access to all accessGroups. */
        accessGroups = NULL;
    }

    bool ok = true;
    Query *q = query_create_with_limit(query, kSecMatchUnlimited, error);
    if (!q) {
        ok = false;
    }
    if (ok) {
        /* Sanity check the query. */
        if (q->q_use_item_list) {
            ok = SecError(errSecUseItemListUnsupported, error, CFSTR("use item list not supported"));
        } else if (q->q_return_type & kSecReturnDataMask) {
            /* Update doesn't return anything so don't ask for it. */
            ok = SecError(errSecReturnDataUnsupported, error, CFSTR("return data not supported by update"));
        } else if (q->q_return_type & kSecReturnAttributesMask) {
            ok = SecError(errSecReturnAttributesUnsupported, error, CFSTR("return attributes not supported by update"));
        } else if (q->q_return_type & kSecReturnRefMask) {
            ok = SecError(errSecReturnRefUnsupported, error, CFSTR("return ref not supported by update"));
        } else if (q->q_return_type & kSecReturnPersistentRefMask) {
            ok = SecError(errSecReturnPersitentRefUnsupported, error, CFSTR("return persistent ref not supported by update"));
        } else {
            /* Access group sanity checking. */
            CFStringRef agrp = (CFStringRef)CFDictionaryGetValue(attributesToUpdate,
                kSecAttrAccessGroup);
            if (agrp) {
                /* The user is attempting to modify the access group column,
                   validate it to make sure the new value is allowable. */
                if (!accessGroupsAllows(accessGroups, agrp)) {
                    ok = SecError(errSecNoAccessForItem, error, CFSTR("accessGroup %@ not in %@"), agrp, accessGroups);
                }
            }
        }
    }
    if (ok) {
        if (!q->q_use_tomb && SOSCCThisDeviceDefinitelyNotActiveInCircle()) {
            q->q_use_tomb = kCFBooleanFalse;
        }
        ok = kc_with_dbt(true, error, ^(SecDbConnectionRef dbt) {
            return s3dl_query_update(dbt, q, attributesToUpdate, accessGroups, error);
        });
    }
    if (q) {
        ok = query_notify_and_destroy(q, ok, error);
    }
    return ok;
}


/* AUDIT[securityd](done):
   query (ok) is a caller provided dictionary, only its cf type has been checked.
 */
bool
_SecItemDelete(CFDictionaryRef query, CFArrayRef accessGroups, CFErrorRef *error)
{
    CFIndex ag_count;
    if (!accessGroups || 0 == (ag_count = CFArrayGetCount(accessGroups))) {
        return SecError(errSecMissingEntitlement, error,
                           CFSTR("client has neither application-identifier nor keychain-access-groups entitlements"));
    }

    if (CFArrayContainsValue(accessGroups, CFRangeMake(0, ag_count), CFSTR("*"))) {
        /* Having the special accessGroup "*" allows access to all accessGroups. */
        accessGroups = NULL;
    }

    Query *q = query_create_with_limit(query, kSecMatchUnlimited, error);
    bool ok;
    if (q) {
        /* Sanity check the query. */
        if (q->q_limit != kSecMatchUnlimited)
            ok = SecError(errSecMatchLimitUnsupported, error, CFSTR("match limit not supported by delete"));
        else if (query_match_count(q) != 0)
            ok = SecError(errSecItemMatchUnsupported, error, CFSTR("match not supported by delete"));
        else if (q->q_ref)
            ok = SecError(errSecValueRefUnsupported, error, CFSTR("value ref not supported by delete"));
        else if (q->q_row_id && query_attr_count(q))
            ok = SecError(errSecItemIllegalQuery, error, CFSTR("rowid and other attributes are mutually exclusive"));
        else {
            if (!q->q_use_tomb && SOSCCThisDeviceDefinitelyNotActiveInCircle()) {
                q->q_use_tomb = kCFBooleanFalse;
            }
            ok = kc_with_dbt(true, error, ^(SecDbConnectionRef dbt) {
                return s3dl_query_delete(dbt, q, accessGroups, error);
            });
        }
        ok = query_notify_and_destroy(q, ok, error);
    } else {
        ok = false;
    }
    return ok;
}


/* AUDIT[securityd](done):
   No caller provided inputs.
 */
static bool
SecItemServerDeleteAll(CFErrorRef *error) {
    return kc_with_dbt(true, error, ^bool (SecDbConnectionRef dbt) {
        return (kc_transaction(dbt, error, ^bool {
            return (SecDbExec(dbt, CFSTR("DELETE from genp;"), error) &&
                    SecDbExec(dbt, CFSTR("DELETE from inet;"), error) &&
                    SecDbExec(dbt, CFSTR("DELETE from cert;"), error) &&
                    SecDbExec(dbt, CFSTR("DELETE from keys;"), error));
        }) && SecDbExec(dbt, CFSTR("VACUUM;"), error));
    });
}

bool
_SecItemDeleteAll(CFErrorRef *error) {
    return SecItemServerDeleteAll(error);
}

CFDataRef
_SecServerKeychainBackup(CFDataRef keybag, CFDataRef passcode, CFErrorRef *error) {
    CFDataRef backup;
	SecDbConnectionRef dbt = SecDbConnectionAquire(kc_dbhandle(), false, error);

	if (!dbt)
		return NULL;

    if (keybag == NULL && passcode == NULL) {
#if USE_KEYSTORE
        backup = SecServerExportKeychain(dbt, KEYBAG_DEVICE, backup_keybag_handle, error);
#else /* !USE_KEYSTORE */
        SecError(errSecParam, error, CFSTR("Why are you doing this?"));
        backup = NULL;
#endif /* USE_KEYSTORE */
    } else {
        backup = SecServerKeychainBackup(dbt, keybag, passcode, error);
    }

    SecDbConnectionRelease(dbt);

    return backup;
}

bool
_SecServerKeychainRestore(CFDataRef backup, CFDataRef keybag, CFDataRef passcode, CFErrorRef *error) {
    if (backup == NULL || keybag == NULL)
        return SecError(errSecParam, error, CFSTR("backup or keybag missing"));

    __block bool ok = true;
    ok &= SecDbPerformWrite(kc_dbhandle(), error, ^(SecDbConnectionRef dbconn) {
        ok = SecServerKeychainRestore(dbconn, backup, keybag, passcode, error);
    });

    if (ok) {
        SecKeychainChanged(true);
    }

    return ok;
}


/*
 *
 *
 * SecItemDataSource
 *
 *
 */
static CFStringRef kSecItemDataSourceErrorDomain = CFSTR("com.apple.secitem.datasource");

enum {
    kSecObjectMallocFailed = 1,
    kSecAddDuplicateEntry,
    kSecObjectNotFoundError,
    kSOSAccountCreationFailed,
};

typedef struct SecItemDataSource *SecItemDataSourceRef;

struct SecItemDataSource {
    struct SOSDataSource ds;
    SecDbRef db;
    bool readOnly;
    SecDbConnectionRef _dbconn;
    unsigned gm_count;
    unsigned cm_count;
    unsigned co_count;
    bool dv_loaded;
    struct SOSDigestVector dv;
    struct SOSDigestVector toadd;
    struct SOSDigestVector todel;
    SOSManifestRef manifest;
    uint8_t manifest_digest[SOSDigestSize];
    bool changed;
    bool syncWithPeersWhenDone;
};

static SecDbConnectionRef SecItemDataSourceGetConnection(SecItemDataSourceRef ds, CFErrorRef *error) {
    if (!ds->_dbconn) {
        ds->_dbconn = SecDbConnectionAquire(ds->db, ds->readOnly, error);
        if (ds->_dbconn) {
            ds->changed = false;
        } else {
            secerror("SecDbConnectionAquire failed: %@", error ? *error : NULL);
        }
    }
    return ds->_dbconn;
}

static bool SecItemDataSourceRecordUpdate(SecItemDataSourceRef ds, SecDbItemRef deleted, SecDbItemRef inserted, CFErrorRef *error) {
    bool ok = true;
    CFDataRef digest;
    if (ds->dv_loaded) {
        if (inserted) {
            ok = digest = SecDbItemGetSHA1(inserted, error);
            if (ok) SOSDigestVectorAppend(&ds->toadd, CFDataGetBytePtr(digest));
        }
        if (ok && deleted) {
            ok = digest = SecDbItemGetSHA1(deleted, error);
            if (ok) SOSDigestVectorAppend(&ds->todel, CFDataGetBytePtr(digest));
        }
        if (inserted || deleted) {
            CFReleaseNull(ds->manifest);
            ds->changed = true;
        }

        if (!ok) {
            ds->dv_loaded = false;
        }
    }
    return ok;
}

static bool SecItemDataSourceRecordAdd(SecItemDataSourceRef ds, SecDbItemRef inserted, CFErrorRef *error) {
    return SecItemDataSourceRecordUpdate(ds, NULL, inserted, error);
}

static bool SecDbItemSelectSHA1(SecDbQueryRef query, SecDbConnectionRef dbconn, CFErrorRef *error,
                                bool (^use_attr_in_where)(const SecDbAttr *attr),
                                bool (^add_where_sql)(CFMutableStringRef sql, bool *needWhere),
                                bool (^bind_added_where)(sqlite3_stmt *stmt, int col),
                                void (^row)(sqlite3_stmt *stmt, bool *stop)) {
    __block bool ok = true;
    bool (^return_attr)(const SecDbAttr *attr) = ^bool (const SecDbAttr * attr) {
        return attr->kind == kSecDbSHA1Attr;
    };
    CFStringRef sql = SecDbItemCopySelectSQL(query, return_attr, use_attr_in_where, add_where_sql);
    if (sql) {
        ok &= SecDbPrepare(dbconn, sql, error, ^(sqlite3_stmt *stmt) {
            ok = (SecDbItemSelectBind(query, stmt, error, use_attr_in_where, bind_added_where) &&
                  SecDbStep(dbconn, stmt, error, ^(bool *stop){ row(stmt, stop); }));
        });
        CFRelease(sql);
    } else {
        ok = false;
    }
    return ok;
}

static bool SecItemDataSourceLoadManifest(SecItemDataSourceRef ds, CFErrorRef *error) {
    bool ok = true;
    SecDbConnectionRef dbconn;
    if (!(dbconn = SecItemDataSourceGetConnection(ds, error))) return false;

    /* Fetch all syncable items. */
    const SecDbClass *synced_classes[] = {
        &genp_class,
        &inet_class,
        &keys_class,
    };

    ds->dv.count = 0; // Empty the digest vectory before we begin
    CFErrorRef localError = NULL;
    for (size_t class_ix = 0; class_ix < array_size(synced_classes);
         ++class_ix) {
        Query *q = query_create(synced_classes[class_ix], NULL, &localError);
        if (q) {
            q->q_return_type = kSecReturnDataMask | kSecReturnAttributesMask;
            q->q_limit = kSecMatchUnlimited;
            q->q_keybag = KEYBAG_DEVICE;
            query_add_attribute(kSecAttrSynchronizable, kCFBooleanTrue, q);
            //query_add_attribute(kSecAttrAccessible, ds->name, q);
            // Select everything including tombstones that is synchronizable.
            if (!SecDbItemSelectSHA1(q, dbconn, &localError, ^bool(const SecDbAttr *attr) {
                return attr->kind == kSecDbSyncAttr;
            }, NULL, NULL, ^(sqlite3_stmt *stmt, bool *stop) {
                const uint8_t *digest = sqlite3_column_blob(stmt, 0);
                size_t digestLen = sqlite3_column_bytes(stmt, 0);
                if (digestLen != SOSDigestSize) {
                    secerror("digest %zu bytes", digestLen);
                } else {
                    SOSDigestVectorAppend(&ds->dv, digest);
                }
            })) {
                secerror("SecDbItemSelect failed: %@", localError);
                CFReleaseNull(localError);
            }
            query_destroy(q, &localError);
            if (localError) {
                secerror("query_destroy failed: %@", localError);
                CFReleaseNull(localError);
            }
        } else if (localError) {
            secerror("query_create failed: %@", localError);
            CFReleaseNull(localError);
        }
    }
    SOSDigestVectorSort(&ds->dv);
    return ok;
}

static bool SecItemDataSourceEnsureFreshManifest(SecItemDataSourceRef ds, CFErrorRef *error) {
    bool ok = true;
    if (ds->dv_loaded && (ds->toadd.count || ds->todel.count)) {
        CFErrorRef patchError = NULL;
        struct SOSDigestVector new_dv = SOSDigestVectorInit;
        ok = SOSDigestVectorPatch(&ds->dv, &ds->todel, &ds->toadd, &new_dv, &patchError);
        if (!ok) secerror("patch failed %@ manifest: %@ toadd: %@ todel: %@", patchError, &ds->dv, &ds->todel, &ds->toadd);
        CFReleaseSafe(patchError);
        SOSDigestVectorFree(&ds->dv);
        SOSDigestVectorFree(&ds->toadd);
        SOSDigestVectorFree(&ds->todel);
        ds->dv = new_dv;
    }
    // If we failed to patch or we haven't loaded yet, force a load from the db.
    return (ok && ds->dv_loaded) || (ds->dv_loaded = SecItemDataSourceLoadManifest(ds, error));
}

/* DataSource protocol. */
static bool ds_get_manifest_digest(SOSDataSourceRef data_source, uint8_t *out_digest, CFErrorRef *error) {
    struct SecItemDataSource *ds = (struct SecItemDataSource *)data_source;
    if (!ds->manifest) {
        SOSManifestRef mf = data_source->copy_manifest(data_source, error);
        if (mf) {
            CFRelease(mf);
        } else {
            return false;
        }
    }
    memcpy(out_digest, ds->manifest_digest, SOSDigestSize);
    ds->gm_count++;
    return true;
}

static SOSManifestRef ds_copy_manifest(SOSDataSourceRef data_source, CFErrorRef *error) {
    struct SecItemDataSource *ds = (struct SecItemDataSource *)data_source;
    ds->cm_count++;
    if (ds->manifest) {
        CFRetain(ds->manifest);
        return ds->manifest;
    }

    if (!SecItemDataSourceEnsureFreshManifest(ds, error)) return NULL;

    ds->manifest = SOSManifestCreateWithBytes((const uint8_t *)ds->dv.digest, ds->dv.count * SOSDigestSize, error);
    // TODO move digest
    ccdigest(ccsha1_di(), SOSManifestGetSize(ds->manifest), SOSManifestGetBytePtr(ds->manifest), ds->manifest_digest);

    return (SOSManifestRef)CFRetain(ds->manifest);
}

static bool ds_foreach_object(SOSDataSourceRef data_source, SOSManifestRef manifest, CFErrorRef *error, bool (^handle_object)(SOSObjectRef object, CFErrorRef *error)) {
    struct SecItemDataSource *ds = (struct SecItemDataSource *)data_source;
    ds->co_count++;
    __block bool result = true;
    const SecDbAttr *sha1Attr = SecDbClassAttrWithKind(&genp_class, kSecDbSHA1Attr, error);
    if (!sha1Attr) return false;
    bool (^return_attr)(const SecDbAttr *attr) = ^bool (const SecDbAttr * attr) {
        return attr->kind == kSecDbRowIdAttr || attr->kind == kSecDbEncryptedDataAttr;
    };
    bool (^use_attr_in_where)(const SecDbAttr *attr) = ^bool (const SecDbAttr * attr) {
        return attr->kind == kSecDbSHA1Attr;
    };
    const SecDbClass *synced_classes[] = {
        &genp_class,
        &inet_class,
        &keys_class,
    };
    Query *select_queries[array_size(synced_classes)];
    CFStringRef select_sql[array_size(synced_classes)];
    sqlite3_stmt *select_stmts[array_size(synced_classes)];

    __block Query **queries = select_queries;
    __block CFStringRef *sqls = select_sql;
    __block sqlite3_stmt **stmts = select_stmts;

    SecDbConnectionRef dbconn;
    result = dbconn = SecItemDataSourceGetConnection(ds, error);

    // Setup
    for (size_t class_ix = 0; class_ix < array_size(synced_classes); ++class_ix) {
        result = (result
                  && (queries[class_ix] = query_create(synced_classes[class_ix], NULL, error))
                  && (sqls[class_ix] = SecDbItemCopySelectSQL(queries[class_ix], return_attr, use_attr_in_where, NULL))
                  && (stmts[class_ix] = SecDbCopyStmt(dbconn, sqls[class_ix], NULL, error)));
    }

    if (result) SOSManifestForEach(manifest, ^(CFDataRef key) {
        __block bool gotItem = false;
        for (size_t class_ix = 0; result && !gotItem && class_ix < array_size(synced_classes); ++class_ix) {
            CFDictionarySetValue(queries[class_ix]->q_item, sha1Attr->name, key);
            result &= (SecDbItemSelectBind(queries[class_ix], stmts[class_ix], error, use_attr_in_where, NULL) && SecDbStep(dbconn, stmts[class_ix], error, ^(bool *stop) {
                SecDbItemRef item = SecDbItemCreateWithStatement(kCFAllocatorDefault, queries[class_ix]->q_class, stmts[class_ix], KEYBAG_DEVICE, error, return_attr);
                if (item) {
                    CFErrorRef localError=NULL;
                    gotItem = true;
                    // Stop on errors from handle_object, except decode errors
                    if(!(result=handle_object((SOSObjectRef)item, &localError))){
                        if (SecErrorGetOSStatus(localError) == errSecDecode) {
                            const uint8_t *p=CFDataGetBytePtr(key);
                            secnotice("item", "Found corrupted item, removing from manifest key=%02X%02X%02X%02X, item=%@", p[0],p[1],p[2],p[3], item);
                            /* Removing from Manifest: */
                            SOSDigestVectorAppend(&ds->todel, p);
                            CFReleaseNull(ds->manifest);
                        } else {
                            *stop=true;
                        }
                        if(error && *error == NULL) {
                            *error = localError;
                            localError = NULL;
                        }
                    }
                    CFRelease(item);
                    CFReleaseSafe(localError);
                }
            })) && SecDbReset(stmts[class_ix], error);
        }
        if (!gotItem) {
            result = false;
            if (error && !*error) {
                SecCFCreateErrorWithFormat(kSecObjectNotFoundError, kSecItemDataSourceErrorDomain, NULL, error, 0, CFSTR("key %@ not in database"), key);
            }
        }
    });

    // Cleanup
    for (size_t class_ix = 0; class_ix < array_size(synced_classes); ++class_ix) {
        result &= SecDbReleaseCachedStmt(dbconn, sqls[class_ix], stmts[class_ix], error);
        CFReleaseSafe(sqls[class_ix]);
        result &= query_destroy(queries[class_ix], error);
    }
    return result;
}

static void ds_dispose(SOSDataSourceRef data_source) {
    struct SecItemDataSource *ds = (struct SecItemDataSource *)data_source;
    if (ds->_dbconn)
        SecDbConnectionRelease(ds->_dbconn);
    if (ds->changed)
        SecKeychainChanged(ds->syncWithPeersWhenDone);
    CFReleaseSafe(ds->manifest);
    SOSDigestVectorFree(&ds->dv);
    free(ds);
}

static SOSObjectRef ds_create_with_property_list(SOSDataSourceRef ds, CFDictionaryRef plist, CFErrorRef *error) {
    SecDbItemRef item = NULL;
    const SecDbClass *class = NULL;
    CFTypeRef cname = CFDictionaryGetValue(plist, kSecClass);
    if (cname) {
        class = kc_class_with_name(cname);
        if (class) {
            item = SecDbItemCreateWithAttributes(kCFAllocatorDefault, class, plist, KEYBAG_DEVICE, error);
        } else {
            SecError(errSecNoSuchClass, error, CFSTR("can find class named: %@"), cname);
        }
    } else {
        SecError(errSecItemClassMissing, error, CFSTR("query missing %@ attribute"), kSecClass);
    }
    return (SOSObjectRef)item;
}

static CFDataRef ds_copy_digest(SOSObjectRef object, CFErrorRef *error) {
    SecDbItemRef item = (SecDbItemRef) object;
    CFDataRef digest = SecDbItemGetSHA1(item, error);
    CFRetainSafe(digest);
    return digest;
}

static CFDataRef ds_copy_primary_key(SOSObjectRef object, CFErrorRef *error) {
    SecDbItemRef item = (SecDbItemRef) object;
    CFDataRef pk = SecDbItemGetPrimaryKey(item, error);
    CFRetainSafe(pk);
    return pk;
}

static CFDictionaryRef ds_copy_property_list(SOSObjectRef object, CFErrorRef *error) {
    SecDbItemRef item = (SecDbItemRef) object;
    CFMutableDictionaryRef plist = SecDbItemCopyPListWithMask(item, kSecDbInCryptoDataFlag, error);
    if (plist)
        CFDictionaryAddValue(plist, kSecClass, SecDbItemGetClass(item)->name);
    return plist;
}

// Return the newest object
static SOSObjectRef ds_copy_merged_object(SOSObjectRef object1, SOSObjectRef object2, CFErrorRef *error) {
    SecDbItemRef item1 = (SecDbItemRef) object1;
    SecDbItemRef item2 = (SecDbItemRef) object2;
    SOSObjectRef result = NULL;
    CFDateRef m1, m2;
    const SecDbAttr *desc = SecDbAttrWithKey(SecDbItemGetClass(item1), kSecAttrModificationDate, error);
    m1 = SecDbItemGetValue(item1, desc, error);
    if (!m1)
        return NULL;
    m2 = SecDbItemGetValue(item2, desc, error);
    if (!m2)
        return NULL;
    switch (CFDateCompare(m1, m2, NULL)) {
        case kCFCompareGreaterThan:
            result = (SOSObjectRef)item1;
            break;
        case kCFCompareLessThan:
            result = (SOSObjectRef)item2;
            break;
        case kCFCompareEqualTo:
        {
            // Return the item with the smallest digest.
            CFDataRef digest1 = ds_copy_digest(object1, error);
            CFDataRef digest2 = ds_copy_digest(object2, error);
            if (digest1 && digest2) switch (CFDataCompare(digest1, digest2)) {
                case kCFCompareGreaterThan:
                case kCFCompareEqualTo:
                    result = (SOSObjectRef)item2;
                    break;
                case kCFCompareLessThan:
                    result = (SOSObjectRef)item1;
                    break;
            }
            CFReleaseSafe(digest2);
            CFReleaseSafe(digest1);
            break;
        }
    }
    CFRetainSafe(result);
    return result;
}

static SOSMergeResult dsMergeObject(SOSDataSourceRef data_source, SOSObjectRef peersObject, CFErrorRef *error) {
    struct SecItemDataSource *ds = (struct SecItemDataSource *)data_source;
    SecDbItemRef peersItem = (SecDbItemRef)peersObject;
    SecDbConnectionRef dbconn = SecItemDataSourceGetConnection(ds, error);
    __block SOSMergeResult mr = kSOSMergeFailure;
    __block SecDbItemRef mergedItem = NULL;
    __block SecDbItemRef replacedItem = NULL;
    if (!peersItem || !dbconn || !SecDbItemSetKeybag(peersItem, KEYBAG_DEVICE, error)) return mr;
    if (SecDbItemInsertOrReplace(peersItem, dbconn, error, ^(SecDbItemRef myItem, SecDbItemRef *replace) {
        // An item with the same primary key as dbItem already exists in the the database.  That item is old_item.
        // Let the conflict resolver choose which item to keep.
        mergedItem = (SecDbItemRef)ds_copy_merged_object(peersObject, (SOSObjectRef)myItem, error);
        if (!mergedItem) return;
        if (CFEqual(mergedItem, myItem)) {
            // Conflict resolver choose my (local) item
            mr = kSOSMergeLocalObject;
        } else {
            CFRetainSafe(myItem);
            replacedItem = myItem;
            CFRetainSafe(mergedItem);
            *replace = mergedItem;
            if (CFEqual(mergedItem, peersItem)) {
                // Conflict resolver choose peers item
                mr = kSOSMergePeersObject;
            } else {
                mr = kSOSMergeCreatedObject;
            }
        }
    })) {
        if (mr == kSOSMergeFailure) {
            mr = kSOSMergePeersObject;
            SecItemDataSourceRecordAdd(ds, peersItem, error);
        } else if (mr != kSOSMergeLocalObject) {
            SecItemDataSourceRecordUpdate(ds, replacedItem, mergedItem, error);
        }
    }

    if (error && *error && mr != kSOSMergeFailure)
        CFReleaseNull(*error);

    CFReleaseSafe(mergedItem);
    CFReleaseSafe(replacedItem);
    return mr;
}


/*
    Truthy backup format is a dictionary from sha1 => item.
    Each item has class, hash and item data.

    TODO: sha1 is included as binary blob to avoid parsing key.
 */
enum {
    kSecBackupIndexHash = 0,
    kSecBackupIndexClass,
    kSecBackupIndexData,
};

static const void *kSecBackupKeys[] = {
    [kSecBackupIndexHash] = CFSTR("hash"),
    [kSecBackupIndexClass] = CFSTR("class"),
    [kSecBackupIndexData] = CFSTR("data"),
};

#define kSecBackupHash kSecBackupKeys[kSecBackupIndexHash]
#define kSecBackupClass kSecBackupKeys[kSecBackupIndexClass]
#define kSecBackupData kSecBackupKeys[kSecBackupIndexData]

static CFDictionaryRef ds_backup_object(SOSObjectRef object, uint64_t handle, CFErrorRef *error) {
    const void *values[array_size(kSecBackupKeys)];
    SecDbItemRef item = (SecDbItemRef)object;
    CFDictionaryRef backup_item = NULL;

    if ((values[kSecBackupIndexHash] = SecDbItemGetSHA1(item, error))) {
        if ((values[kSecBackupIndexData] = SecDbItemCopyEncryptedDataToBackup(item, handle, error))) {
            values[kSecBackupIndexClass] = SecDbItemGetClass(item)->name;
            backup_item = CFDictionaryCreate(kCFAllocatorDefault, kSecBackupKeys, values, array_size(kSecBackupKeys), &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
            CFRelease(values[kSecBackupIndexData]);
        }
    }

    return backup_item;
}

static bool ds_restore_object(SOSDataSourceRef data_source, uint64_t handle, CFDictionaryRef item, CFErrorRef *error) {
    struct SecItemDataSource *ds = (struct SecItemDataSource *)data_source;
    SecDbConnectionRef dbconn = SecItemDataSourceGetConnection(ds, error);
    if (!dbconn) return false;

    CFStringRef item_class = CFDictionaryGetValue(item, kSecBackupClass);
    CFDataRef data = CFDictionaryGetValue(item, kSecBackupData);
    const SecDbClass *dbclass = NULL;

    if (!item_class || !data)
        return SecError(errSecDecode, error, CFSTR("no class or data in object"));
    
    dbclass = kc_class_with_name(item_class);
    if (!dbclass)
        return SecError(errSecDecode, error, CFSTR("no such class %@; update kc_class_with_name "), item_class);

    __block SecDbItemRef dbitem = SecDbItemCreateWithEncryptedData(kCFAllocatorDefault, dbclass, data, (keybag_handle_t)handle, error);
    if (!dbitem)
        return false;
    
    __block bool ok = SecDbItemSetKeybag(dbitem, KEYBAG_DEVICE, error);

    if (ok) {
        __block SecDbItemRef replaced_item = NULL;
        ok &= SecDbItemInsertOrReplace(dbitem, dbconn, error, ^(SecDbItemRef old_item, SecDbItemRef *replace) {
            // An item with the same primary key as dbItem already exists in the the database.  That item is old_item.
            // Let the conflict resolver choose which item to keep.
            SecDbItemRef chosen_item = (SecDbItemRef)ds_copy_merged_object((SOSObjectRef)dbitem, (SOSObjectRef)old_item, error);
            if (chosen_item) {
                if (CFEqual(chosen_item, old_item)) {
                    // We're keeping the exisiting item, so we don't need to change anything.
                    CFRelease(chosen_item);
                    CFReleaseNull(dbitem);
                } else {
                    // We choose a different item than what's in the database already.  Let's set dbitem to what
                    // we are replacing the item in the database with, and set replaced_item to the item we are replacing.
                    CFRelease(dbitem); // Release the item created via SecDbItemCreateWithEncryptedData
                    // Record what we put in the database
                    CFRetain(chosen_item); // retain what we are about to return in *replace, since SecDbItemInsertOrReplace() CFReleases it.
                    *replace = dbitem = chosen_item;
                    // Record that we are replaced old_item in replaced_item.
                    CFRetain(old_item);
                    replaced_item = old_item;
                }
            } else {
                ok = false;
            }
        })
        && SecItemDataSourceRecordUpdate(ds, replaced_item, dbitem, error);
        CFReleaseSafe(replaced_item);
    }
    CFReleaseSafe(dbitem);

    return ok;
}


static SOSDataSourceRef SecItemDataSourceCreate(SecDbRef db, bool readOnly, bool syncWithPeersWhenDone, CFErrorRef *error) {
    __block SecItemDataSourceRef ds = calloc(1, sizeof(struct SecItemDataSource));
    ds->ds.get_manifest_digest = ds_get_manifest_digest;
    ds->ds.copy_manifest = ds_copy_manifest;
    ds->ds.foreach_object = ds_foreach_object;
    ds->ds.release = ds_dispose;
    ds->ds.add = dsMergeObject;

    ds->ds.createWithPropertyList = ds_create_with_property_list;
    ds->ds.copyDigest = ds_copy_digest;
    ds->ds.copyPrimaryKey = ds_copy_primary_key;
    ds->ds.copyPropertyList = ds_copy_property_list;
    ds->ds.copyMergedObject = ds_copy_merged_object;
    ds->ds.backupObject = ds_backup_object;
    ds->ds.restoreObject = ds_restore_object;

    ds->syncWithPeersWhenDone = syncWithPeersWhenDone;
    ds->db = (SecDbRef)CFRetain(db);
    ds->readOnly = readOnly;

    ds->changed = false;
    struct SOSDigestVector dv = SOSDigestVectorInit;
    ds->dv = dv;

    return (SOSDataSourceRef)ds;
}

static CFArrayRef SecItemDataSourceFactoryCopyNames(SOSDataSourceFactoryRef factory)
{
    return CFArrayCreateForCFTypes(kCFAllocatorDefault,
                                   kSecAttrAccessibleWhenUnlocked,
                                   //kSecAttrAccessibleAfterFirstUnlock,
                                   //kSecAttrAccessibleAlways,
                                   NULL);
}

struct SecItemDataSourceFactory {
    struct SOSDataSourceFactory factory;
    SecDbRef db;
};


static SOSDataSourceRef SecItemDataSourceFactoryCopyDataSource(SOSDataSourceFactoryRef factory, CFStringRef dataSourceName, bool readOnly, CFErrorRef *error)
{
    struct SecItemDataSourceFactory *f = (struct SecItemDataSourceFactory *)factory;
    return SecItemDataSourceCreate(f->db, readOnly, false, error);
}

static void SecItemDataSourceFactoryDispose(SOSDataSourceFactoryRef factory)
{
    struct SecItemDataSourceFactory *f = (struct SecItemDataSourceFactory *)factory;
    CFReleaseSafe(f->db);
    free(f);
}

SOSDataSourceFactoryRef SecItemDataSourceFactoryCreate(SecDbRef db) {
    struct SecItemDataSourceFactory *dsf = calloc(1, sizeof(struct SecItemDataSourceFactory));
    dsf->factory.copy_names = SecItemDataSourceFactoryCopyNames;
    dsf->factory.create_datasource = SecItemDataSourceFactoryCopyDataSource;
    dsf->factory.release = SecItemDataSourceFactoryDispose;
    CFRetainSafe(db);
    dsf->db = db;

    return &dsf->factory;
}

SOSDataSourceFactoryRef SecItemDataSourceFactoryCreateDefault(void) {
    return SecItemDataSourceFactoryCreate(kc_dbhandle());
}

void SecItemServerAppendItemDescription(CFMutableStringRef desc, CFDictionaryRef object) {
    SOSObjectRef item = ds_create_with_property_list(NULL, object, NULL);
    if (item) {
        CFStringRef itemDesc = CFCopyDescription(item);
        if (itemDesc) {
            CFStringAppend(desc, itemDesc);
            CFReleaseSafe(itemDesc);
        }
        CFRelease(item);
    }
}

/* AUDIT[securityd]:
   args_in (ok) is a caller provided, CFDictionaryRef.
 */
bool
_SecServerKeychainSyncUpdate(CFDictionaryRef updates, CFErrorRef *error) {
    // This never fails, trust us!
    CFRetainSafe(updates);
    dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
        SOSCCHandleUpdate(updates);
        CFReleaseSafe(updates);
    });
    return true;
}

//
// Truthiness in the cloud backup/restore support.
//

static CFStringRef SOSCopyItemKey(SOSDataSourceRef ds, SOSObjectRef object, CFErrorRef *error)
{
    CFStringRef item_key = NULL;
    CFDataRef digest_data = ds->copyDigest(object, error);
    if (digest_data) {
        item_key = CFDataCopyHexString(digest_data);
        CFRelease(digest_data);
    }
    return item_key;
}

static SOSManifestRef SOSCopyManifestFromBackup(CFDictionaryRef backup)
{
    CFMutableDataRef manifest = CFDataCreateMutable(kCFAllocatorDefault, 0);
    if (backup) {
        CFDictionaryForEach(backup, ^void (const void * key, const void * value) {
            if (isDictionary(value)) {
                /* converting key back to binary blob is horrible */
                CFDataRef sha1 = CFDictionaryGetValue(value, kSecBackupHash);
                if (isData(sha1))
                    CFDataAppend(manifest, sha1);
            }
        });
    }
    return (SOSManifestRef)manifest;
}

static CFDictionaryRef
_SecServerCopyTruthInTheCloud(CFDataRef keybag, CFDataRef password,
    CFDictionaryRef backup, CFErrorRef *error)
{
    SOSManifestRef mold = NULL, mnow = NULL, mdelete = NULL, madd = NULL;
    CFErrorRef foreachError = NULL;
    CFDictionaryRef backup_out = NULL;
    keybag_handle_t bag_handle;
    if (!ks_open_keybag(keybag, password, &bag_handle, error))
        return NULL;

    CFMutableDictionaryRef backup_new = NULL;
    SOSDataSourceRef ds = SecItemDataSourceCreate(kc_dbhandle(), true, false, error);
    if (ds) {
        backup_new = backup ? CFDictionaryCreateMutableCopy(kCFAllocatorDefault, 0, backup) : CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
        mold = SOSCopyManifestFromBackup(backup);
        mnow = ds->copy_manifest(ds, error);
        SOSManifestDiff(mold, mnow, &mdelete, &madd, error);

        // Delete everything from the new_backup that is no longer in the datasource according to the datasources manifest.
        SOSManifestForEach(mdelete, ^(CFDataRef digest_data) {
            CFStringRef deleted_item_key = CFDataCopyHexString(digest_data);
            CFDictionaryRemoveValue(backup_new, deleted_item_key);
            CFRelease(deleted_item_key);
        });

        if(!ds->foreach_object(ds, madd, &foreachError, ^bool(SOSObjectRef object, CFErrorRef *localError) {
            bool ok = true;
            CFStringRef key = SOSCopyItemKey(ds, object, localError);
            CFTypeRef value = ds->backupObject(object, bag_handle, localError);

            if (!key || !value) {
                ok = false;
            } else {
                CFDictionarySetValue(backup_new, key, value);
            }
            CFReleaseSafe(key);
            CFReleaseSafe(value);
            return ok;
        })) {
            if(!SecErrorGetOSStatus(foreachError)==errSecDecode) {
                if(error && *error==NULL) {
                    *error = foreachError;
                    foreachError = NULL;
                }
                goto out;
            }
        }

        backup_out = backup_new;
        backup_new = NULL;
    }

out:
    if(ds)
        ds->release(ds);

    CFReleaseSafe(foreachError);
    CFReleaseSafe(mold);
    CFReleaseSafe(mnow);
    CFReleaseSafe(madd);
    CFReleaseSafe(mdelete);
    CFReleaseSafe(backup_new);

    if (!ks_close_keybag(bag_handle, error))
        CFReleaseNull(backup_out);

    return backup_out;
}

static bool
_SecServerRestoreTruthInTheCloud(CFDataRef keybag, CFDataRef password, CFDictionaryRef backup_in, CFErrorRef *error) {
    __block bool ok = true;
    keybag_handle_t bag_handle;
    if (!ks_open_keybag(keybag, password, &bag_handle, error))
        return false;

    SOSManifestRef mbackup = SOSCopyManifestFromBackup(backup_in);
    if (mbackup) {
        SOSDataSourceRef ds = SecItemDataSourceCreate(kc_dbhandle(), false, true, error);
        if (ds) {
            SOSManifestRef mnow = ds->copy_manifest(ds, error);
            SOSManifestRef mdelete = NULL, madd = NULL;
            SOSManifestDiff(mnow, mbackup, &mdelete, &madd, error);

            // Don't delete everything in datasource not in backup.

            // Add items from the backup
            SOSManifestForEach(madd, ^void(CFDataRef e) {
                CFDictionaryRef item = NULL;
                CFStringRef sha1 = CFDataCopyHexString(e);
                if (sha1) {
                    item = CFDictionaryGetValue(backup_in, sha1);
                    CFRelease(sha1);
                }
                if (item) {
                    CFErrorRef localError = NULL;
                    if (!ds->restoreObject(ds, bag_handle, item, &localError)) {
                        if (SecErrorGetOSStatus(localError) == errSecDuplicateItem) {
                            // Log and ignore duplicate item errors during restore
                            secnotice("titc", "restore %@ not replacing existing item", item);
                        } else {
                            // Propagate the first other error upwards (causing the restore to fail).
                            secerror("restore %@ failed %@", item, localError);
                            ok = false;
                            if (error && !*error) {
                                *error = localError;
                                localError = NULL;
                            }
                        }
                        CFReleaseSafe(localError);
                    }
                }
            });

            ds->release(ds);
            CFReleaseNull(mdelete);
            CFReleaseNull(madd);
            CFReleaseNull(mnow);
        } else {
            ok = false;
        }
        CFRelease(mbackup);
    }

    ok &= ks_close_keybag(bag_handle, error);

    return ok;
}


CF_RETURNS_RETAINED CFDictionaryRef
_SecServerBackupSyncable(CFDictionaryRef backup, CFDataRef keybag, CFDataRef password, CFErrorRef *error) {
    require_action_quiet(isData(keybag), errOut, SecError(errSecParam, error, CFSTR("keybag %@ not a data"), keybag));
    require_action_quiet(!backup || isDictionary(backup), errOut, SecError(errSecParam, error, CFSTR("backup %@ not a dictionary"), backup));
    require_action_quiet(!password || isData(password), errOut, SecError(errSecParam, error, CFSTR("password %@ not a data"), password));

    return _SecServerCopyTruthInTheCloud(keybag, password, backup, error);

errOut:
    return NULL;
}

bool
_SecServerRestoreSyncable(CFDictionaryRef backup, CFDataRef keybag, CFDataRef password, CFErrorRef *error) {
    bool ok;
    require_action_quiet(isData(keybag), errOut, ok = SecError(errSecParam, error, CFSTR("keybag %@ not a data"), keybag));
    require_action_quiet(isDictionary(backup), errOut, ok = SecError(errSecParam, error, CFSTR("backup %@ not a dictionary"), backup));
    if (password) {
        require_action_quiet(isData(password), errOut, ok = SecError(errSecParam, error, CFSTR("password not a data")));
    }

    ok = _SecServerRestoreTruthInTheCloud(keybag, password, backup, error);

errOut:
    return ok;
}



