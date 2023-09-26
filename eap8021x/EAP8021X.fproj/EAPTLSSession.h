/*
 * Copyright (c) 2022-2023 Apple Inc. All rights reserved.
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

typedef struct __EAPTLSSessionContext * EAPTLSSessionContextRef;

EAPTLSSessionContextRef
EAPTLSSessionCreateContext(CFDictionaryRef properties, EAPType eapType, memoryIORef mem_io, CFArrayRef client_certificates, OSStatus * ret_status);

void
EAPTLSSessionFreeContext(EAPTLSSessionContextRef session_context);

OSStatus
EAPTLSSessionClose(EAPTLSSessionContextRef session_context);

OSStatus
EAPTLSSessionSetPeerID(EAPTLSSessionContextRef session_context, const void *peer_id, size_t peer_id_len);

OSStatus
EAPTLSSessionGetState(EAPTLSSessionContextRef session_context, SSLSessionState *state);

OSStatus
EAPTLSSessionHandshake(EAPTLSSessionContextRef session_context);

void
EAPTLSSessionCopyPeerCertificates(EAPTLSSessionContextRef session_context, CFArrayRef * peer_certificates);

SecTrustRef
EAPTLSSessionGetSecTrust(EAPTLSSessionContextRef session_context);

Boolean
EAPTLSSessionIsRevocationStatusCheckRequired(EAPTLSSessionContextRef session_context);

OSStatus
EAPTLSSessionComputeSessionKey(EAPTLSSessionContextRef session_context, const void * label, int label_length, void * key, int key_length);

void
EAPTLSSessionGetSessionResumed(EAPTLSSessionContextRef session_context, Boolean *resumed);

CFStringRef
EAPTLSSessionGetNegotiatedTLSProtocolVersion(EAPTLSSessionContextRef session_context);

void
EAPTLSSessionGetNegotiatedCipher(EAPTLSSessionContextRef session_context, SSLCipherSuite *cipher_suite);
