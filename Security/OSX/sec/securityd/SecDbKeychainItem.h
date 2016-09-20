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
 @header SecDbKeychainItem.h - The thing that does the stuff with the gibli.
 */

#ifndef _SECURITYD_SECKEYCHAINITEM_H_
#define _SECURITYD_SECKEYCHAINITEM_H_

#include <securityd/SecKeybagSupport.h>
#include <securityd/SecDbItem.h>
#include <securityd/SecDbQuery.h>

__BEGIN_DECLS

bool ks_encrypt_data(keybag_handle_t keybag, SecAccessControlRef access_control, CFDataRef acm_context,
                     CFDictionaryRef attributes, CFDictionaryRef authenticated_attributes, CFDataRef *pBlob, bool useDefaultIV, CFErrorRef *error);
bool ks_decrypt_data(keybag_handle_t keybag, CFTypeRef operation, SecAccessControlRef *paccess_control, CFDataRef acm_context,
                     CFDataRef blob, const SecDbClass *db_class, CFArrayRef caller_access_groups,
                     CFMutableDictionaryRef *attributes_p, uint32_t *version_p, CFErrorRef *error);
bool s3dl_item_from_data(CFDataRef edata, Query *q, CFArrayRef accessGroups,
                         CFMutableDictionaryRef *item, SecAccessControlRef *access_control, CFErrorRef *error);
SecDbItemRef SecDbItemCreateWithBackupDictionary(CFAllocatorRef allocator, const SecDbClass *dbclass, CFDictionaryRef dict, keybag_handle_t src_keybag, keybag_handle_t dst_keybag, CFErrorRef *error);
bool SecDbItemExtractRowIdFromBackupDictionary(SecDbItemRef item, CFDictionaryRef dict, CFErrorRef *error);
bool SecDbItemInferSyncable(SecDbItemRef item, CFErrorRef *error);

CFTypeRef SecDbKeychainItemCopyPrimaryKey(SecDbItemRef item, const SecDbAttr *attr, CFErrorRef *error);
CFTypeRef SecDbKeychainItemCopySHA1(SecDbItemRef item, const SecDbAttr *attr, CFErrorRef *error);
CFTypeRef SecDbKeychainItemCopyCurrentDate(SecDbItemRef item, const SecDbAttr *attr, CFErrorRef *error);
CFTypeRef SecDbKeychainItemCopyEncryptedData(SecDbItemRef item, const SecDbAttr *attr, CFErrorRef *error);

SecAccessControlRef SecDbItemCopyAccessControl(SecDbItemRef item, CFErrorRef *error);
bool SecDbItemSetAccessControl(SecDbItemRef item, SecAccessControlRef access_control, CFErrorRef *error);

__END_DECLS

#endif /* _SECURITYD_SECKEYCHAINITEM_H_ */
