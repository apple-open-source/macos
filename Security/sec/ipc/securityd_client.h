/*
 * Copyright (c) 2007-2009 Apple Inc. All Rights Reserved.
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
#ifndef	_SECURITYD_CLIENT_H_
#define _SECURITYD_CLIENT_H_

#include <stdint.h>
#include <Security/SecTrustStore.h>
#include <CoreFoundation/CFArray.h>
#include <CoreFoundation/CFDictionary.h>

#if SECITEM_SHIM_OSX
#define SECURITYSERVER_BOOTSTRAP_NAME "com.apple.secd"
#else
#define SECURITYSERVER_BOOTSTRAP_NAME "com.apple.securityd"
#endif // *** END SECITEM_SHIM_OSX ***

#define SECURITYD(SPI, IN, OUT) (gSecurityd \
    ? gSecurityd->SPI(IN, OUT) \
    : ServerCommandSendReceive(SPI ## _id, IN, OUT)) 

#define SECURITYD_AG(SPI, IN, OUT) (gSecurityd \
    ? gSecurityd->SPI(IN, OUT, SecAccessGroupsGetCurrent()) \
    : ServerCommandSendReceive(SPI ## _id, IN, OUT))

enum {
	sec_item_add_id,
	sec_item_copy_matching_id,
	sec_item_update_id,
	sec_item_delete_id,
    sec_trust_store_contains_id,
    sec_trust_store_set_trust_settings_id,
    sec_trust_store_remove_certificate_id,
    sec_delete_all_id,
    sec_trust_evaluate_id,
    sec_restore_keychain_id,
    sec_migrate_keychain_id,
    sec_keychain_backup_id,
    sec_keychain_restore_id
};

struct securityd {
    OSStatus (*sec_item_add)(CFDictionaryRef attributes, CFTypeRef *result,
    CFArrayRef accessGroups);
    OSStatus (*sec_item_copy_matching)(CFDictionaryRef query,
        CFTypeRef *result, CFArrayRef accessGroups);
    OSStatus (*sec_item_update)(CFDictionaryRef query,
        CFDictionaryRef attributesToUpdate, CFArrayRef accessGroups);
    OSStatus (*sec_item_delete)(CFDictionaryRef query, CFArrayRef accessGroups);
    SecTrustStoreRef (*sec_trust_store_for_domain)(CFStringRef domainName);
    bool (*sec_trust_store_contains)(SecTrustStoreRef ts,
        CFDataRef digest);
    OSStatus (*sec_trust_store_set_trust_settings)(SecTrustStoreRef ts,
        SecCertificateRef certificate, CFTypeRef trustSettingsDictOrArray);
    OSStatus (*sec_trust_store_remove_certificate)(SecTrustStoreRef ts,
        CFDataRef digest);
    bool (*sec_truststore_remove_all)(SecTrustStoreRef ts);
    bool (*sec_item_delete_all)(void);
    OSStatus (*sec_trust_evaluate)(CFDictionaryRef args_in,
        CFTypeRef *args_out);
    OSStatus (*sec_restore_keychain)(void);
    OSStatus (*sec_migrate_keychain)(CFArrayRef args_in, CFTypeRef *args_out);
    OSStatus (*sec_keychain_backup)(CFArrayRef args_in, CFTypeRef *args_out);
    OSStatus (*sec_keychain_restore)(CFArrayRef args_in, CFTypeRef *dummy);
};

extern struct securityd *gSecurityd;

CFArrayRef SecAccessGroupsGetCurrent(void);
void SecAccessGroupsSetCurrent(CFArrayRef accessGroups);

OSStatus ServerCommandSendReceive(uint32_t id, CFTypeRef in, CFTypeRef *out);

#endif /* _SECURITYD_CLIENT_H_ */
