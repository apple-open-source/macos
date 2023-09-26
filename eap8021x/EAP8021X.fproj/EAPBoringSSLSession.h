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

typedef CFTypeRef EAPBoringSSLSessionContextRef;
typedef CFTypeRef EAPBoringSSLClientContextRef;

typedef enum {
    EAPBoringSSLSessionStateIdle,
    EAPBoringSSLSessionStateConnecting,
    EAPBoringSSLSessionStateConnected,
    EAPBoringSSLSessionStateDisconnected,
} EAPBoringSSLSessionState;

typedef OSStatus
(*EAPBoringSSLSessionReadFunc)(memoryIORef mem_io,
			       void *data, 		/* owned by* caller, data* RETURNED */
			       size_t *dataLength);	/* IN/OUT */

typedef OSStatus
(*EAPBoringSSLSessionWriteFunc)(memoryIORef mem_io,
				const void *data,
				size_t *dataLength);	/* IN/OUT */

typedef struct EAPBoringSSLSessionParameters_s {
    SecIdentityRef 			client_identity; 	/* SecIdentityRef */
    CFArrayRef 				client_certificates; 	/* Array of SecCertifictaeRef */
    tls_protocol_version_t		min_tls_version;
    tls_protocol_version_t 		max_tls_version;
    EAPBoringSSLSessionReadFunc 	read_func;
    EAPBoringSSLSessionWriteFunc 	write_func;
    EAPType 				eap_method;
    memoryIORef 			memIO;
} EAPBoringSSLSessionParameters, *EAPBoringSSLSessionParametersRef;


EAPBoringSSLSessionContextRef
EAPBoringSSLSessionContextCreate(EAPBoringSSLSessionParametersRef sessionParameters, EAPBoringSSLClientContextRef clientContext);

void
EAPBoringSSLSessionStart(EAPBoringSSLSessionContextRef sessionContext);

void
EAPBoringSSLSessionStop(EAPBoringSSLSessionContextRef sessionContext);

void
EAPBoringSSLSessionContextFree(EAPBoringSSLSessionContextRef sessionContext);

OSStatus
EAPBoringSSLSessionGetCurrentState(EAPBoringSSLSessionContextRef sessionContext, EAPBoringSSLSessionState *state);

CFStringRef
EAPBoringSSLSessionGetCurrentStateDescription(EAPBoringSSLSessionState state);

void
EAPBoringSSLUtilGetPreferredTLSVersions(CFDictionaryRef properties, tls_protocol_version_t *min, tls_protocol_version_t *max);

OSStatus
EAPBoringSSLSessionHandshake(EAPBoringSSLSessionContextRef sessionContext);

OSStatus
EAPBoringSSLSessionCopyServerCertificates(EAPBoringSSLSessionContextRef sessionContext, CFArrayRef *certs);

SecTrustRef
EAPBoringSSLSessionGetSecTrust(EAPBoringSSLSessionContextRef sessionContext);


OSStatus
EAPBoringSSLSessionComputeKeyData(EAPBoringSSLSessionContextRef sessionContext, void *key, int key_length);

OSStatus
EAPBoringSSLSessionGetNegotiatedTLSVersion(EAPBoringSSLSessionContextRef sessionContext, tls_protocol_version_t *tlsVersion);

OSStatus
EAPBoringSSLSessionGetSessionResumed(EAPBoringSSLSessionContextRef sessionContext, bool *sessionResumed);
