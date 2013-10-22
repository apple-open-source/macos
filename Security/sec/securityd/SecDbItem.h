/*
 * Copyright (c) 2012 Apple Inc. All Rights Reserved.
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

/*!
 @header SecDbItem
 The functions provided in SecDbItem provide an interface to
 database items (certificates, keys, identities, and passwords).
 */

#ifndef _SECURITYD_SECDBITEM_H_
#define _SECURITYD_SECDBITEM_H_

#include <CoreFoundation/CoreFoundation.h>
#include <TargetConditionals.h>
#include <corecrypto/ccsha1.h> // For CCSHA1_OUTPUT_SIZE
#include <sqlite3.h>
#include <utilities/SecCFError.h>
#include <utilities/SecCFWrappers.h>
#include <utilities/SecDb.h>
#include <utilities/SecAKSWrappers.h>

#if TARGET_OS_MAC && !(TARGET_OS_EMBEDDED || TARGET_IPHONE_SIMULATOR)
#define USE_KEYSTORE  1
#elif TARGET_OS_EMBEDDED && !TARGET_IPHONE_SIMULATOR
#define USE_KEYSTORE  1
#else /* no keystore on this platform */
#define USE_KEYSTORE  0
#endif

#if USE_KEYSTORE
#include <Kernel/IOKit/crypto/AppleKeyStoreDefs.h>
#endif /* USE_KEYSTORE */

__BEGIN_DECLS

// TODO: Get this out of this file
#if USE_KEYSTORE
typedef int32_t keyclass_t;
#else

/* TODO: this needs to be available in the sim! */
#define kAppleKeyStoreKeyWrap 0
#define kAppleKeyStoreKeyUnwrap 1
typedef int32_t keyclass_t;
typedef int32_t key_handle_t;
enum key_classes {
    key_class_ak = 6,
    key_class_ck,
    key_class_dk,
    key_class_aku,
    key_class_cku,
    key_class_dku
};
#endif /* !USE_KEYSTORE */

// MARK SecDbAttrKind, SecDbFlag

typedef enum {
    kSecDbBlobAttr,  // CFString or CFData, preserves caller provided type.
    kSecDbDataAttr,
    kSecDbStringAttr,
    kSecDbNumberAttr,
    kSecDbDateAttr,
    kSecDbCreationDateAttr,
    kSecDbModificationDateAttr,
    kSecDbSHA1Attr,
    kSecDbRowIdAttr,
    kSecDbEncryptedDataAttr,
    kSecDbPrimaryKeyAttr,
    kSecDbSyncAttr,
    kSecDbTombAttr,
    kSecDbAccessAttr
} SecDbAttrKind;

enum {
    kSecDbPrimaryKeyFlag    = (1 <<  0),    // attr is part of primary key
    kSecDbInFlag            = (1 <<  1),    // attr exists in db
    kSecDbIndexFlag         = (1 <<  2),    // attr should have a db index
    kSecDbSHA1ValueInFlag   = (1 <<  3),    // col in db is sha1 of attr value
    kSecDbReturnAttrFlag    = (1 <<  4),
    kSecDbReturnDataFlag    = (1 <<  5),
    kSecDbReturnRefFlag     = (1 <<  6),
    kSecDbInCryptoDataFlag  = (1 <<  7),
    kSecDbInHashFlag        = (1 <<  8),
    kSecDbInBackupFlag      = (1 <<  9),
    kSecDbDefault0Flag      = (1 << 10),    // default attr value is 0
    kSecDbDefaultEmptyFlag  = (1 << 11),    // default attr value is ""
    kSecDbNotNullFlag       = (1 << 12),    // attr value can't be null
};

#define SecVersionDbFlag(v) ((v & 0xFF) << 8)

#define SecDbFlagGetVersion(flags) ((flags >> 8) & 0xFF)

#define SECDB_ATTR(var, name, kind, flags) const SecDbAttr var = { CFSTR(name), kSecDb ## kind ## Attr, flags }

typedef struct SecDbAttr {
    CFStringRef name;
    SecDbAttrKind kind;
    CFOptionFlags flags;
} SecDbAttr;

typedef struct SecDbClass {
    CFStringRef name;
    const SecDbAttr *attrs[];
} SecDbClass;

typedef struct Pair *SecDbPairRef;
typedef struct Query *SecDbQueryRef;

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
    const SecDbClass *q_class;

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

    CFErrorRef q_error;
    ReturnTypeMask q_return_type;

    CFDataRef q_data;
    CFTypeRef q_ref;
    sqlite_int64 q_row_id;

    CFArrayRef q_use_item_list;
    CFBooleanRef q_use_tomb;
#if defined(MULTIPLE_KEYCHAINS)
    CFArrayRef q_use_keychain;
    CFArrayRef q_use_keychain_list;
#endif /* !defined(MULTIPLE_KEYCHAINS) */

    /* Value of kSecMatchLimit key if present. */
    CFIndex q_limit;

    /* True if query contained a kSecAttrSynchronizable attribute,
     * regardless of its actual value. If this is false, then we
     * will add an explicit sync=0 to the query. */
    bool q_sync;

    // Set to true if we modified any item as part of executing this query
    bool q_changed;

    // Set to true if we modified any synchronizable item as part of executing this query
    bool q_sync_changed;

    /* Keybag handle to use for this item. */
    keybag_handle_t q_keybag;
    keyclass_t q_keyclass;
    //CFStringRef q_keyclass_s;

    // SHA1 digest of DER encoded primary key
    CFDataRef q_primary_key_digest;

    CFArrayRef q_match_issuer;

    /* Store all the corrupted rows found during the query */
    CFMutableArrayRef corrupted_rows;

    Pair q_pairs[];
} Query;


#define SecDbForEachAttr(class, attr) for (const SecDbAttr * const* _pattr = (class)->attrs, *attr = *_pattr; attr; attr = *(++_pattr))

#define SecDbForEachAttrWithMask(class, attr, flag_mask) SecDbForEachAttr(class, attr) if ((attr->flags & (flag_mask)) == (flag_mask))

// MARK: Stuff that needs to move out of SecItemServer.c

// Move this or do crypto in a block
bool ks_encrypt_data(keybag_handle_t keybag, keyclass_t keyclass, CFDataRef plainText, CFDataRef *pBlob, CFErrorRef *error);
bool ks_decrypt_data(keybag_handle_t keybag, keyclass_t *pkeyclass, CFDataRef blob, CFDataRef *pPlainText,
                            uint32_t *version_p, CFErrorRef *error);

CFDataRef kc_copy_sha1(size_t len, const void *data, CFErrorRef *error);
CFDataRef kc_copy_plist_sha1(CFPropertyListRef plist, CFErrorRef *error);
CFDataRef kc_plist_copy_der(CFPropertyListRef plist, CFErrorRef *error);
Query *query_create(const SecDbClass *qclass, CFDictionaryRef query, CFErrorRef *error);
bool query_destroy(Query *q, CFErrorRef *error);

// MARK: SecDbItem

typedef struct SecDbItem *SecDbItemRef;

enum SecDbItemState {
    kSecDbItemDirty,          // We have no edata (or if we do it's invalid), attributes are the truth
    kSecDbItemEncrypted,      // Attributes haven't been decrypted yet from edata
    kSecDbItemClean,          // Attributes and _edata are in sync.
    kSecDbItemDecrypting,     // Temporary state while we are decrypting so set knows not to blow away the edata.
    kSecDbItemEncrypting,     // Temporary state while we are encrypting so set knows to move to clean.
};

struct SecDbItem {
    CFRuntimeBase _base;
    const SecDbClass *class;
    keyclass_t keyclass;
    keybag_handle_t keybag;
    //sqlite3_int64 _rowid;
    //CFDataRef _primaryKey;
    //CFDataRef _sha1;
    //CFDataRef _edata;
    enum SecDbItemState _edataState;
    CFMutableDictionaryRef attributes;
};

// TODO: Make this a callback to client
bool SecDbItemDecrypt(SecDbItemRef item, CFDataRef edata, CFErrorRef *error);

CFTypeID SecDbItemGetTypeID(void);

static inline size_t SecDbClassAttrCount(const SecDbClass *dbClass) {
    size_t n_attrs = 0;
    SecDbForEachAttr(dbClass, attr) { n_attrs++; }
    return n_attrs;
}

const SecDbAttr *SecDbClassAttrWithKind(const SecDbClass *class, SecDbAttrKind kind, CFErrorRef *error);

SecDbItemRef SecDbItemCreateWithAttributes(CFAllocatorRef allocator, const SecDbClass *class, CFDictionaryRef attributes, keybag_handle_t keybag, CFErrorRef *error);

const SecDbClass *SecDbItemGetClass(SecDbItemRef item);
const keybag_handle_t SecDbItemGetKeybag(SecDbItemRef item);
bool SecDbItemSetKeybag(SecDbItemRef item, keybag_handle_t keybag, CFErrorRef *error);
keyclass_t SecDbItemGetKeyclass(SecDbItemRef item, CFErrorRef *error);
bool SecDbItemSetKeyclass(SecDbItemRef item, keyclass_t keyclass, CFErrorRef *error);

CFTypeRef SecDbItemGetCachedValueWithName(SecDbItemRef item, CFStringRef name);
CF_RETURNS_NOT_RETAINED CFTypeRef SecDbItemGetValue(SecDbItemRef item, const SecDbAttr *desc, CFErrorRef *error);

bool SecDbItemSetValue(SecDbItemRef item, const SecDbAttr *desc, CFTypeRef value, CFErrorRef *error);
bool SecDbItemSetValues(SecDbItemRef item, CFDictionaryRef values, CFErrorRef *error);
bool SecDbItemSetValueWithName(SecDbItemRef item, CFStringRef name, CFTypeRef value, CFErrorRef *error);

sqlite3_int64 SecDbItemGetRowId(SecDbItemRef item, CFErrorRef *error);
bool SecDbItemSetRowId(SecDbItemRef item, sqlite3_int64 rowid, CFErrorRef *error);

bool SecDbItemIsSyncable(SecDbItemRef item);

bool SecDbItemSetSyncable(SecDbItemRef item, bool sync, CFErrorRef *error);

bool SecDbItemIsTombstone(SecDbItemRef item);

CFMutableDictionaryRef SecDbItemCopyPListWithMask(SecDbItemRef item, CFOptionFlags mask, CFErrorRef *error);

CFDataRef SecDbItemGetPrimaryKey(SecDbItemRef item, CFErrorRef *error);
CFDataRef SecDbItemGetSHA1(SecDbItemRef item, CFErrorRef *error);

CFDataRef SecDbItemCopyEncryptedDataToBackup(SecDbItemRef item, uint64_t handle, CFErrorRef *error);

SecDbItemRef SecDbItemCreateWithStatement(CFAllocatorRef allocator, const SecDbClass *class, sqlite3_stmt *stmt, keybag_handle_t keybag, CFErrorRef *error, bool (^return_attr)(const SecDbAttr *attr));

SecDbItemRef SecDbItemCreateWithEncryptedData(CFAllocatorRef allocator, const SecDbClass *class,
                                              CFDataRef edata, keybag_handle_t keybag, CFErrorRef *error);

SecDbItemRef SecDbItemCreateWithPrimaryKey(CFAllocatorRef allocator, const SecDbClass *class, CFDataRef primary_key);

#if 0
SecDbItemRef SecDbItemCreateWithRowId(CFAllocatorRef allocator, const SecDbClass *class, sqlite_int64 row_id, keybag_handle_t keybag, CFErrorRef *error);
#endif

SecDbItemRef SecDbItemCopyWithUpdates(SecDbItemRef item, CFDictionaryRef updates, CFErrorRef *error);

SecDbItemRef SecDbItemCopyTombstone(SecDbItemRef item, CFErrorRef *error);

bool SecDbItemInsertOrReplace(SecDbItemRef item, SecDbConnectionRef dbconn, CFErrorRef *error, void(^duplicate)(SecDbItemRef item, SecDbItemRef *replace));

bool SecDbItemInsert(SecDbItemRef item, SecDbConnectionRef dbconn, CFErrorRef *error);

bool SecDbItemDelete(SecDbItemRef item, SecDbConnectionRef dbconn, bool makeTombstone, CFErrorRef *error);

// Low level update, just do the update
bool SecDbItemDoUpdate(SecDbItemRef old_item, SecDbItemRef new_item, SecDbConnectionRef dbconn, CFErrorRef *error, bool (^use_attr_in_where)(const SecDbAttr *attr));

// High level update, will replace tombstones and create them if needed.
bool SecDbItemUpdate(SecDbItemRef old_item, SecDbItemRef new_item, SecDbConnectionRef dbconn, bool makeTombstone, CFErrorRef *error);

bool SecDbItemSelect(SecDbQueryRef query, SecDbConnectionRef dbconn, CFErrorRef *error,
                     bool (^use_attr_in_where)(const SecDbAttr *attr),
                     bool (^add_where_sql)(CFMutableStringRef sql, bool *needWhere),
                     bool (^bind_added_where)(sqlite3_stmt *stmt, int col),
                     void (^handle_row)(SecDbItemRef item, bool *stop));

CFStringRef SecDbItemCopySelectSQL(SecDbQueryRef query,
                                   bool (^return_attr)(const SecDbAttr *attr),
                                   bool (^use_attr_in_where)(const SecDbAttr *attr),
                                   bool (^add_where_sql)(CFMutableStringRef sql, bool *needWhere));
bool SecDbItemSelectBind(SecDbQueryRef query, sqlite3_stmt *stmt, CFErrorRef *error,
                         bool (^use_attr_in_where)(const SecDbAttr *attr),
                         bool (^bind_added_where)(sqlite3_stmt *stmt, int col));


// MARK: -
// MARK: SQL Construction helpers -- These should become private in the future

void SecDbAppendElement(CFMutableStringRef sql, CFStringRef value, bool *needComma);
void SecDbAppendWhereOrAnd(CFMutableStringRef sql, bool *needWhere);
void SecDbAppendWhereOrAndEquals(CFMutableStringRef sql, CFStringRef col, bool *needWhere);

// MARK: -
// MARK: SecItemDb (a SecDb of SecDbItems)

typedef struct SecItemDb *SecItemDbRef;
typedef struct SecItemDbConnection *SecItemDbConnectionRef;

struct SecItemDb {
    CFRuntimeBase _base;
    SecDbRef db;
    CFDictionaryRef classes; // className -> SecItemClass mapping
};

struct SecItemDbConnection {
    SecDbConnectionRef db;
};

SecItemDbRef SecItemDbCreate(SecDbRef db);
SecItemDbRef SecItemDbRegisterClass(SecItemDbRef db, const SecDbClass *class, void(^upgrade)(SecDbItemRef item, uint32_t current_version));

SecItemDbConnectionRef SecItemDbAquireConnection(SecItemDbRef db);
void SecItemDbReleaseConnection(SecItemDbRef db, SecItemDbConnectionRef dbconn);

bool SecItemDbInsert(SecItemDbConnectionRef dbconn, SecDbItemRef item, CFErrorRef *error);

bool SecItemDbDelete(SecItemDbConnectionRef dbconn, SecDbItemRef item, CFErrorRef *error);

// Low level update, just do the update
bool SecItemDbDoUpdate(SecItemDbConnectionRef dbconn, SecDbItemRef old_item, SecDbItemRef new_item, CFErrorRef *error,
                       bool (^use_attr_in_where)(const SecDbAttr *attr));

// High level update, will replace tombstones and create them if needed.
bool SecItemDbUpdate(SecItemDbConnectionRef dbconn, SecDbItemRef old_item, SecDbItemRef new_item, CFErrorRef *error);

bool SecItemDbSelect(SecItemDbConnectionRef dbconn, SecDbQueryRef query, CFErrorRef *error,
                     bool (^use_attr_in_where)(const SecDbAttr *attr),
                     bool (^add_where_sql)(CFMutableStringRef sql, bool *needWhere),
                     bool (^bind_added_where)(sqlite3_stmt *stmt, int col),
                     void (^handle_row)(SecDbItemRef item, bool *stop));

// MARK: type converters.
// TODO: these should be static and private to SecDbItem, or part of the schema

CFStringRef copyString(CFTypeRef obj);
CFDataRef copyData(CFTypeRef obj);
CFTypeRef copyBlob(CFTypeRef obj);
CFDataRef copySHA1(CFTypeRef obj);
CFTypeRef copyNumber(CFTypeRef obj);
CFDateRef copyDate(CFTypeRef obj);

__END_DECLS

#endif /* _SECURITYD_SECDBITEM_H_ */
