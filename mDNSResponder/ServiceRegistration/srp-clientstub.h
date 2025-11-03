/* srp-clientstub.h
 *
 * Copyright (c) 2019-2025 Apple Inc. All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     https://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * This file includes the dnssd client stub implementation locally so that we can add some functionality
 * without exporting data structures that are private to the client stub.
 */

// Replace the current callback and context for a RecordRef with a new callback and new context
//
/* DNSServiceRecordSetCallback() Parameters:
 *
 * sdRef:           Pointer to a DNSServiceRef that is thought to be valid
 *
 * RecordRef:       DNSRecordRef that is thought to have been created using DNSServiceRegisterRecord with sdRef.
 *
 * callBack:        The function to be called when a result is found, or if the call
 *                  asynchronously fails (e.g. because of a name conflict.)
 *
 * context:         An application context pointer which is passed to the callback function
 *                  (may be NULL).
 *
 * return value:    Returns kDNSServiceErr_NoError if sdRef is actually valid, RecordRef
 *                  is also valid, and RecordRef was created using DNSServiceRegisterRecord with sdRef.
 */
void DNSServiceRecordSetCallback(DNSServiceRef sdRef, DNSRecordRef RecordRef,
								 DNSServiceRegisterRecordReply callBack, void *context);

// Validate a DNSRecordRef that is dependent on a particular DNSServiceRef.
//
/* DNSServiceRecordValidate() Parameters:
 *
 * sdRef:           Pointer to a DNSServiceRef that is thought to be valid
 *
 * RecordRef:       DNSRecordRef that is thought to have been created using DNSServiceRegisterRecord with sdRef.
 *
 * return value:    Returns true if sdRef is actually valid, RecordRef
 *                  is also valid, and RecordRef was created using DNSServiceRegisterRecord with sdRef.
 *                  Otherwise returns false.
 */
bool DNSServiceRecordValidate(DNSServiceRef sdRef, DNSRecordRef RecordRef);
