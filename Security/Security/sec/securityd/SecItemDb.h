/*
 * Copyright (c) 2013-2014 Apple Inc. All Rights Reserved.
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
 @header SecItemDb.h - A Database full of SecDbItems.
 */

#ifndef _SECURITYD_SECITEMDB_H_
#define _SECURITYD_SECITEMDB_H_

#include <securityd/SecDbQuery.h>

#define CURRENT_DB_VERSION 6

__BEGIN_DECLS

#if 0
//
// MARK: SecItemDb (a SecDb of SecDbItems)
//
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
#endif

CFTypeRef SecDbItemCopyResult(SecDbItemRef item, ReturnTypeMask return_type, CFErrorRef *error);

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

bool SecDbItemQuery(SecDbQueryRef query, CFArrayRef accessGroups, SecDbConnectionRef dbconn, CFErrorRef *error,
                    void (^handle_row)(SecDbItemRef item, bool *stop));


//
// MARK: backup restore stuff
//

/* Forward declaration of import export SPIs. */
enum SecItemFilter {
    kSecNoItemFilter,
    kSecSysBoundItemFilter,
    kSecBackupableItemFilter,
};

CF_RETURNS_RETAINED CFDictionaryRef SecServerExportKeychainPlist(SecDbConnectionRef dbt,
                                                                        keybag_handle_t src_keybag, keybag_handle_t dest_keybag,
                                                                        enum SecItemFilter filter, CFErrorRef *error);
bool SecServerImportKeychainInPlist(SecDbConnectionRef dbt,
                                           keybag_handle_t src_keybag, keybag_handle_t dest_keybag,
                                           CFDictionaryRef keychain, enum SecItemFilter filter, CFErrorRef *error);

void SecDbAppendCreateTableWithClass(CFMutableStringRef sql, const SecDbClass *c);
bool kc_transaction(SecDbConnectionRef dbt, CFErrorRef *error, bool(^perform)());
bool s3dl_copy_matching(SecDbConnectionRef dbt, Query *q, CFTypeRef *result,
                        CFArrayRef accessGroups, CFErrorRef *error);
bool s3dl_query_add(SecDbConnectionRef dbt, Query *q, CFTypeRef *result, CFErrorRef *error);
bool s3dl_query_update(SecDbConnectionRef dbt, Query *q,
                  CFDictionaryRef attributesToUpdate, CFArrayRef accessGroups, CFErrorRef *error);
bool s3dl_query_delete(SecDbConnectionRef dbt, Query *q, CFArrayRef accessGroups, CFErrorRef *error);
const SecDbAttr *SecDbAttrWithKey(const SecDbClass *c, CFTypeRef key, CFErrorRef *error);

bool s3dl_dbt_keys_current(SecDbConnectionRef dbt, uint32_t current_generation, CFErrorRef *error);
bool s3dl_dbt_update_keys(SecDbConnectionRef dbt, CFErrorRef *error);
        
__END_DECLS

#endif /* _SECURITYD_SECITEMDB_H_ */
