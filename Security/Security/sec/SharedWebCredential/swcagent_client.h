/*
 * Copyright (c) 2014 Apple Inc. All Rights Reserved.
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
#ifndef	_SWCAGENT_CLIENT_H_
#define _SWCAGENT_CLIENT_H_

#include <stdint.h>

#include <CoreFoundation/CFArray.h>
#include <CoreFoundation/CFDictionary.h>
#include <CoreFoundation/CFError.h>

#include <xpc/xpc.h>
#include <CoreFoundation/CFXPCBridge.h>

// TODO: This should be in client of XPC code locations...
#define kSWCAXPCServiceName "com.apple.security.swcagent"

//
// MARK: XPC Information.
//

extern CFStringRef sSWCAXPCErrorDomain;

//
// MARK: XPC Interfaces
//

extern const char *kSecXPCKeyOperation;
extern const char *kSecXPCKeyResult;
extern const char *kSecXPCKeyError;
extern const char *kSecXPCKeyClientToken;
extern const char *kSecXPCKeyPeerInfos;
extern const char *kSecXPCKeyUserLabel;
extern const char *kSecXPCKeyUserPassword;
extern const char *kSecXPCLimitInMinutes;
extern const char *kSecXPCKeyQuery;
extern const char *kSecXPCKeyAttributesToUpdate;
extern const char *kSecXPCKeyDomain;
extern const char *kSecXPCKeyDigest;
extern const char *kSecXPCKeyCertificate;
extern const char *kSecXPCKeySettings;
extern const char *kSecXPCKeyDeviceID;

//
// MARK: Mach port request IDs
//
enum SWCAXPCOperation {
    swca_add_request_id,
    swca_update_request_id,
    swca_delete_request_id,
    swca_copy_request_id,
    swca_select_request_id,
    swca_copy_pairs_request_id,
    swca_set_selection_request_id,
    swca_enabled_request_id,
};

xpc_object_t swca_message_with_reply_sync(xpc_object_t message, CFErrorRef *error);
xpc_object_t swca_create_message(enum SWCAXPCOperation op, CFErrorRef *error);
bool swca_message_no_error(xpc_object_t message, CFErrorRef *error);
long swca_message_response(xpc_object_t replyMessage, CFErrorRef *error);

bool swca_autofill_enabled(const audit_token_t *auditToken);

bool swca_confirm_operation(enum SWCAXPCOperation op,
                            const audit_token_t *auditToken,
                            CFTypeRef query,
                            CFErrorRef *error,
                            void (^add_negative_entry)(CFStringRef fqdn));

CFTypeRef swca_message_copy_response(xpc_object_t replyMessage, CFErrorRef *error);

CFDictionaryRef swca_copy_selected_dictionary(enum SWCAXPCOperation op,
                                              const audit_token_t *auditToken,
                                              CFTypeRef items,
                                              CFErrorRef *error);

CFArrayRef swca_copy_pairs(enum SWCAXPCOperation op,
                           const audit_token_t *auditToken,
                           CFErrorRef *error);

bool swca_set_selection(enum SWCAXPCOperation op,
                        const audit_token_t *auditToken,
                        CFTypeRef dictionary,
                        CFErrorRef *error);

bool swca_send_sync_and_do(enum SWCAXPCOperation op, CFErrorRef *error,
                                bool (^add_to_message)(xpc_object_t message, CFErrorRef* error),
                                bool (^handle_response)(xpc_object_t response, CFErrorRef* error));


#endif /* _SWCAGENT_CLIENT_H_ */
