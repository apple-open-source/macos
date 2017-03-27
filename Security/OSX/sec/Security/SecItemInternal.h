/*
 * Copyright (c) 2009,2012-2014 Apple Inc. All Rights Reserved.
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
    @header SecItemInternal
    SecItemInternal defines SPI functions dealing with persistent refs
*/

#ifndef _SECURITY_SECITEMINTERNAL_H_
#define _SECURITY_SECITEMINTERNAL_H_

#include <CoreFoundation/CFData.h>
#include <sqlite3.h>
#include <ipc/securityd_client.h>
#include <ctkclient.h>

__BEGIN_DECLS

#define kSecServerKeychainChangedNotification   "com.apple.security.keychainchanged"
#define kSecServerCertificateTrustNotification  "com.apple.security.certificatetrust"

/* label when certificate data is joined with key data */
static const CFStringRef kSecAttrIdentityCertificateData = CFSTR("certdata");
static const CFStringRef kSecAttrIdentityCertificateTokenID = CFSTR("certtkid");

CF_RETURNS_RETAINED CFDataRef _SecItemMakePersistentRef(CFTypeRef iclass, sqlite_int64 rowid);

bool _SecItemParsePersistentRef(CFDataRef persistent_ref, CFStringRef *return_class,
    sqlite_int64 *return_rowid);

OSStatus _SecRestoreKeychain(const char *path);

OSStatus SecOSStatusWith(bool (^perform)(CFErrorRef *error));

bool cftype_client_to_bool_cftype_error_request(enum SecXPCOperation op, CFTypeRef attributes, __unused SecurityClient *client, CFTypeRef *result, CFErrorRef *error);

/* Structure representing copy-on-write dictionary.  Typical use is:
 int bar(CFDictionaryRef input);
 int foo(CFDictionaryRef input) {
     SecCFDictionaryCOW in = { input };
     if (condition) {
         CFDictionarySetValue(SecCFDictionaryCOWGetMutable(&in), key, value);
     }
     bar(in.dictionary);
     CFReleaseSafe(in.mutable_dictionary);
 }
 */
typedef struct {
    // Real dictionary, not owned by this structure, should be accessed directly for read-only access.
    CFDictionaryRef dictionary;

    // On-demand created (and possibly modified), owned writable copy of dictionary.
    CFMutableDictionaryRef mutable_dictionary;
} SecCFDictionaryCOW;

CFMutableDictionaryRef SecCFDictionaryCOWGetMutable(SecCFDictionaryCOW *cow_dictionary);

typedef enum {
    kSecItemAuthResultOK,
    kSecItemAuthResultError,
    kSecItemAuthResultNeedAuth
} SecItemAuthResult;

bool SecItemAuthDo(SecCFDictionaryCOW *auth_params, CFErrorRef *error, SecItemAuthResult (^perform)(CFDictionaryRef auth_params, CFArrayRef *ac_pairs, CFErrorRef *error));

void SecItemAuthCopyParams(SecCFDictionaryCOW *auth_params, SecCFDictionaryCOW *query);

TKTokenRef SecTokenCreate(CFStringRef token_id, CFDictionaryRef auth_params, CFErrorRef *error);

CFDataRef _SecTokenItemCopyValueData(CFDataRef db_value, CFErrorRef *error);

CFDataRef SecItemAttributesCopyPreparedAuthContext(CFTypeRef la_context, CFErrorRef *error);

__END_DECLS

#endif /* !_SECURITY_SECITEMINTERNAL_H_ */
