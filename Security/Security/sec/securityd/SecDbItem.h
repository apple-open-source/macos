/*
 * Copyright (c) 2012-2014 Apple Inc. All Rights Reserved.
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
#include <securityd/SecKeybagSupport.h>
#include <Security/SecAccessControl.h>

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
    kSecDbAccessAttr,
    kSecDbAccessControlAttr
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
    kSecDbInAuthenticatedDataFlag = (1 << 13),
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


#define SecDbForEachAttr(class, attr) for (const SecDbAttr * const* _pattr = (class)->attrs, *attr = *_pattr; attr; attr = *(++_pattr))

#define SecDbForEachAttrWithMask(class, attr, flag_mask) SecDbForEachAttr(class, attr) if ((attr->flags & (flag_mask)) == (flag_mask))

// MARK: Stuff that needs to move out of SecItemServer.c


CFDataRef kc_copy_sha1(size_t len, const void *data, CFErrorRef *error);
CFDataRef kc_copy_plist_sha1(CFPropertyListRef plist, CFErrorRef *error);
CFDataRef kc_plist_copy_der(CFPropertyListRef plist, CFErrorRef *error);

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
    CFTypeRef credHandle;
    enum SecKsCryptoOp cryptoOp;
    CFArrayRef callerAccessGroups;
};

// TODO: Make this a callback to client
bool SecDbItemDecrypt(SecDbItemRef item, CFDataRef edata, CFDataRef *neededAuth, CFErrorRef *error);

CFTypeID SecDbItemGetTypeID(void);

static inline size_t SecDbClassAttrCount(const SecDbClass *dbClass) {
    size_t n_attrs = 0;
    SecDbForEachAttr(dbClass, attr) { n_attrs++; }
    return n_attrs;
}

const SecDbAttr *SecDbClassAttrWithKind(const SecDbClass *class, SecDbAttrKind kind, CFErrorRef *error);

SecDbItemRef SecDbItemCreateWithAttributes(CFAllocatorRef allocator, const SecDbClass *class, CFDictionaryRef attributes, keybag_handle_t keybag, CFErrorRef *error);

const SecDbClass *SecDbItemGetClass(SecDbItemRef item);
keybag_handle_t SecDbItemGetKeybag(SecDbItemRef item);
bool SecDbItemSetKeybag(SecDbItemRef item, keybag_handle_t keybag, CFErrorRef *error);
SecAccessControlRef SecDbItemCopyAccessControl(SecDbItemRef item, CFErrorRef *error);
bool SecDbItemSetAccessControl(SecDbItemRef item, SecAccessControlRef access_control, CFErrorRef *error);
void SecDbItemSetCredHandle(SecDbItemRef item, CFTypeRef cred_handle);
void SecDbItemSetCallerAccessGroups(SecDbItemRef item, CFArrayRef caller_access_groups);

CFTypeRef SecDbItemGetCachedValueWithName(SecDbItemRef item, CFStringRef name);
CFTypeRef SecDbItemGetValue(SecDbItemRef item, const SecDbAttr *desc, CFErrorRef *error);

bool SecDbItemSetValue(SecDbItemRef item, const SecDbAttr *desc, CFTypeRef value, CFErrorRef *error);
bool SecDbItemSetValues(SecDbItemRef item, CFDictionaryRef values, CFErrorRef *error);
bool SecDbItemSetValueWithName(SecDbItemRef item, CFStringRef name, CFTypeRef value, CFErrorRef *error);

sqlite3_int64 SecDbItemGetRowId(SecDbItemRef item, CFErrorRef *error);
bool SecDbItemSetRowId(SecDbItemRef item, sqlite3_int64 rowid, CFErrorRef *error);

bool SecDbItemIsSyncableOrCorrupted(SecDbItemRef item);
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

bool SecDbItemEnsureDecrypted(SecDbItemRef item, CFDataRef *authNeeded, CFTypeRef *credHandle, CFErrorRef *error);

SecDbItemRef SecDbItemCopyWithUpdates(SecDbItemRef item, CFDictionaryRef updates, CFErrorRef *error);

SecDbItemRef SecDbItemCopyTombstone(SecDbItemRef item, CFErrorRef *error);

bool SecDbItemInsertOrReplace(SecDbItemRef item, SecDbConnectionRef dbconn, CFMutableArrayRef authlist, CFErrorRef *error, void(^duplicate)(SecDbItemRef item, SecDbItemRef *replace));

bool SecDbItemInsert(SecDbItemRef item, SecDbConnectionRef dbconn, CFMutableArrayRef authlist, CFErrorRef *error);

bool SecDbItemDelete(SecDbItemRef item, SecDbConnectionRef dbconn, bool makeTombstone, CFErrorRef *error);

// Low level update, just do the update
bool SecDbItemDoUpdate(SecDbItemRef old_item, SecDbItemRef new_item, SecDbConnectionRef dbconn, CFErrorRef *error, bool (^use_attr_in_where)(const SecDbAttr *attr));

// High level update, will replace tombstones and create them if needed.
bool SecDbItemUpdate(SecDbItemRef old_item, SecDbItemRef new_item, SecDbConnectionRef dbconn, bool makeTombstone, CFErrorRef *error);


// MARK: -
// MARK: SQL Construction helpers -- These should become private in the future

void SecDbAppendElement(CFMutableStringRef sql, CFStringRef value, bool *needComma);
void SecDbAppendWhereOrAnd(CFMutableStringRef sql, bool *needWhere);
void SecDbAppendWhereOrAndEquals(CFMutableStringRef sql, CFStringRef col, bool *needWhere);


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
