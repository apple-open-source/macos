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
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <SystemConfiguration/SCValidation.h>
#include <CoreFoundation/CFArray.h>
#include <CoreFoundation/CFDictionary.h>
#include <EAP8021X/EAPClientPlugin.h>
#include <EAP8021X/EAPClientProperties.h>
#include <EAP8021X/EAPTLSUtil.h>
#include <EAP8021X/EAPCertificateUtil.h>
#include <EAP8021X/EAPUtil.h>
#include <EAP8021x/EAPBoringSSLSession.h>
#include <Security/SecureTransport.h>
#include <Security/SecCertificate.h>
#include <Security/SecIdentity.h>
#include <Security/SecureTransportPriv.h>
#include <Security/SecProtocolPriv.h>
#include "myCFUtil.h"
#include "printdata.h"
#include "EAPLog.h"
#include "EAPOLControlPrefs.h"
#include "EAPSecurity.h"
#include "EAPTLSSession.h"

struct __EAPTLSSessionContext
{
    SSLContextRef			secure_transport_context;
    EAPBoringSSLSessionContextRef 	boringssl_context;
};

static OSStatus
EAPTLSSessionMemoryIORead(memoryIORef mem_io, void * data_buf, size_t * data_length)
{
    if (memoryBufferIsComplete(mem_io->read)) {
	OSStatus status = EAPSSLMemoryIORead((SSLConnectionRef)mem_io, data_buf, data_length);
	if (status == errSSLWouldBlock) {
	    /* this should not happen */
	    return errSecParam;
	}
    } else {
	/* done reading complete read buffer */
	*data_length = 0;
    }
    return errSecSuccess;
}

static OSStatus
EAPTLSSessionMemoryIOWrite(memoryIORef mem_io, const void * data_buf,
		   size_t * data_length)
{
    return EAPSSLMemoryIOWrite((SSLConnectionRef)mem_io, data_buf, data_length);

}

static bool
eaptls_use_boringssl(void)
{
#ifdef SEC_PROTOCOL_HAS_EAP_SUPPORT
    return EAPOLControlPrefsGetUseBoringSSL();
#endif
    return false;
}

EAPTLSSessionContextRef
EAPTLSSessionCreateContext(CFDictionaryRef properties, EAPType eapType, memoryIORef mem_io, CFArrayRef client_certificates, OSStatus * ret_status)
{
    OSStatus 				status = errSecSuccess;
    EAPTLSSessionContextRef 		eap_tls_session_context_ref = NULL;
    SSLContextRef 			secure_transport_context = NULL;
    EAPBoringSSLSessionContextRef 	boringssl_context = NULL;
    SecIdentityRef 			client_identity = NULL;
    CFMutableArrayRef 			temp_certs = NULL;

    eap_tls_session_context_ref = (EAPTLSSessionContextRef)malloc(sizeof(*eap_tls_session_context_ref));
    bzero(eap_tls_session_context_ref, sizeof(*eap_tls_session_context_ref));
    if (eaptls_use_boringssl()) {
	EAPBoringSSLSessionParameters session_params = {0};
	tls_protocol_version_t min_tls_ver, max_tls_ver;
	EAPBoringSSLUtilGetPreferredTLSVersions(properties, &min_tls_ver, &max_tls_ver);
	session_params.min_tls_version = min_tls_ver;
	session_params.max_tls_version = max_tls_ver;
	session_params.read_func = EAPTLSSessionMemoryIORead;
	session_params.write_func = EAPTLSSessionMemoryIOWrite;
	session_params.eap_method = eapType;
	session_params.memIO = mem_io;

	if (client_certificates != NULL &&
	    CFArrayGetCount(client_certificates) > 0) {
	    client_identity = (SecIdentityRef)CFArrayGetValueAtIndex(client_certificates, 0);
	    if (isA_SecIdentity(client_identity) != NULL) {
		session_params.client_identity = client_identity;
	    }
	    if (CFArrayGetCount(client_certificates) > 1) {
		temp_certs = CFArrayCreateMutableCopy(NULL,
						      CFArrayGetCount(client_certificates),
						      client_certificates);
		CFArrayRemoveValueAtIndex(temp_certs, 0);
		session_params.client_certificates = temp_certs;
	    }
	}
	boringssl_context = EAPBoringSSLSessionContextCreate(&session_params, NULL);
	my_CFRelease(&temp_certs);
	if (boringssl_context == NULL) {
	    EAPLOG_FL(LOG_ERR, "EAPBoringSSLSessionContextCreate failed");
	    status = errSecInternalError;
	    goto failed;
	}
	eap_tls_session_context_ref->boringssl_context = boringssl_context;
	EAPBoringSSLSessionStart(boringssl_context);
	EAPLOG_FL(LOG_INFO, "TLS(boringssl) session started");
    } else {
	secure_transport_context = EAPTLSMemIOContextCreate(properties, FALSE, mem_io, NULL,
							    &status);
	if (secure_transport_context == NULL || status != errSecSuccess) {
	    EAPLOG_FL(LOG_NOTICE, "EAPTLSMemIOContextCreate failed, %s (%ld)",
		      EAPSSLErrorString(status), (long)status);
	    goto failed;
	}
	if (client_certificates != NULL && CFArrayGetCount(client_certificates) > 0) {
	    status = SSLSetCertificate(secure_transport_context, client_certificates);
	    if (status != errSecSuccess) {
		EAPLOG_FL(LOG_NOTICE, "SSLSetCertificate failed, %s (%ld)",
			  EAPSSLErrorString(status), (long)status);
		goto failed;
	    }
	}
	eap_tls_session_context_ref->secure_transport_context = secure_transport_context;
	EAPLOG_FL(LOG_INFO, "TLS(SecureTransport) session started");
    }
    if (ret_status != NULL) {
	*ret_status = status;
    }
    return eap_tls_session_context_ref;

failed:
    if (boringssl_context != NULL) {
	EAPBoringSSLSessionContextFree(boringssl_context);
    }
    if (secure_transport_context != NULL) {
	CFRelease(secure_transport_context);
    }
    if (ret_status != NULL) {
	*ret_status = status;
    }
    return eap_tls_session_context_ref;
}

void
EAPTLSSessionFreeContext(EAPTLSSessionContextRef session_context)
{
    if (session_context == NULL) {
	return;
    }
    if (session_context->boringssl_context != NULL) {
	EAPBoringSSLSessionContextFree(session_context->boringssl_context);
    } else if (session_context->secure_transport_context != NULL) {
	CFRelease(session_context->secure_transport_context);
    }
    free(session_context);
    return;
}

OSStatus
EAPTLSSessionClose(EAPTLSSessionContextRef session_context)
{
    if (session_context == NULL) {
	return errSecParam;
    }
    if (session_context->boringssl_context != NULL) {
	EAPBoringSSLSessionStop(session_context->boringssl_context);
    } else if (session_context->secure_transport_context != NULL) {
	SSLClose(session_context->secure_transport_context);
    }
    return errSecSuccess;
}

OSStatus
EAPTLSSessionSetPeerID(EAPTLSSessionContextRef session_context, const void *peer_id, size_t peer_id_len)
{
    OSStatus status;

    if (session_context == NULL || peer_id == NULL || peer_id_len == 0) {
	return errSecParam;
    }
    if (session_context->boringssl_context != NULL) {
	return errSecSuccess;
    } else if (session_context->secure_transport_context != NULL) {
	status = SSLSetPeerID(session_context->secure_transport_context, peer_id, peer_id_len);
    }
    return status;
}

static SSLSessionState
EAPTLSSessionGetSSLStateFromBoringSSLState(EAPBoringSSLSessionState state)
{
    switch (state) {
	case EAPBoringSSLSessionStateIdle:
	    return kSSLIdle;
	case EAPBoringSSLSessionStateConnecting:
	    return kSSLHandshake;
	case EAPBoringSSLSessionStateConnected:
	    return kSSLConnected;
	default:
	    return kSSLClosed;
    }
}

OSStatus
EAPTLSSessionGetState(EAPTLSSessionContextRef session_context, SSLSessionState *state)
{
    OSStatus status = errSecSuccess;

    if (state == NULL) {
	return errSecParam;
    }
    *state = kSSLIdle;
    if (session_context != NULL) {
	if (session_context->boringssl_context != NULL) {
	    EAPBoringSSLSessionState boringssl_state;
	    status = EAPBoringSSLSessionGetCurrentState(session_context->boringssl_context, &boringssl_state);
	    *state = EAPTLSSessionGetSSLStateFromBoringSSLState(boringssl_state);
	} else if (session_context->secure_transport_context != NULL) {
	    status = SSLGetSessionState(session_context->secure_transport_context, state);
	}
    }
    return status;
}

OSStatus
EAPTLSSessionHandshake(EAPTLSSessionContextRef session_context)
{
    OSStatus status = errSecSuccess;

    if (session_context == NULL) {
	return errSecParam;
    }
    if (session_context->boringssl_context != NULL) {
	status = EAPBoringSSLSessionHandshake(session_context->boringssl_context);
    } else if (session_context->secure_transport_context != NULL) {
	status = SSLHandshake(session_context->secure_transport_context);
    }
    EAPLOG_FL(LOG_DEBUG, "received handshake status [%s]:[%d]", EAPSecurityErrorString(status), (int)status);
    return status;
}

void
EAPTLSSessionCopyPeerCertificates(EAPTLSSessionContextRef session_context, CFArrayRef * peer_certificates)
{
    if (session_context == NULL || peer_certificates == NULL) {
	return;
    }
    *peer_certificates = NULL;
    if (session_context->boringssl_context != NULL) {
	(void)EAPBoringSSLSessionCopyServerCertificates(session_context->boringssl_context, peer_certificates);
    } else if (session_context->secure_transport_context != NULL) {
	(void)EAPSSLCopyPeerCertificates(session_context->secure_transport_context, peer_certificates);
    }
    return;
}

SecTrustRef
EAPTLSSessionGetSecTrust(EAPTLSSessionContextRef session_context)
{
    if (session_context->boringssl_context != NULL) {
	return EAPBoringSSLSessionGetSecTrust(session_context->boringssl_context);
    }
    return NULL;
}

Boolean
EAPTLSSessionIsRevocationStatusCheckRequired(EAPTLSSessionContextRef session_context)
{
    if (session_context == NULL) {
	return (FALSE);
    }
    if (EAPOLControlPrefsGetRevocationCheck() == TRUE) {
	EAPLOG_FL(LOG_DEBUG, "revocation check preference is enabled");
	if (session_context->boringssl_context != NULL) {
	    tls_protocol_version_t 	tls_version;
	    OSStatus			status;
	    status = EAPBoringSSLSessionGetNegotiatedTLSVersion(session_context->boringssl_context, &tls_version);
	    EAPLOG_FL(LOG_DEBUG, "negotiated TLS protocol version is [%04X]", tls_version);
	    if (status == errSecSuccess && tls_version == tls_protocol_version_TLSv13) {
		return (TRUE);
	    }
	}
    }
    return (FALSE);
}

OSStatus
EAPTLSSessionComputeSessionKey(EAPTLSSessionContextRef session_context, const void * label, int label_length, void * key, int key_length)
{
    OSStatus status = errSecSuccess;

    if (session_context == NULL) {
	return errSecParam;
    }
    if (session_context->boringssl_context != NULL) {
	status = EAPBoringSSLSessionComputeKeyData(session_context->boringssl_context,
						   key,
						   key_length);
    } else if (session_context->secure_transport_context != NULL) {
	status = EAPTLSComputeKeyData(session_context->secure_transport_context,
				      label,
				      label_length,
				      key,
				      key_length);
    } else {
	status = errSecParam;
    }
    return status;
}

void
EAPTLSSessionGetSessionResumed(EAPTLSSessionContextRef session_context, Boolean *session_resumed)
{
    Boolean			resumed = FALSE;
    OSStatus			status = errSecSuccess;

    if (session_context == NULL || session_resumed == NULL) {
	return;
    }
    if (session_context->boringssl_context != NULL) {
	status = EAPBoringSSLSessionGetSessionResumed(session_context->boringssl_context,
						      (bool *)&resumed);
    } else if (session_context->secure_transport_context != NULL) {
	char		buf[MAX_SESSION_ID_LENGTH];
	size_t		buf_len = sizeof(buf);
	status = SSLGetResumableSessionInfo(session_context->secure_transport_context,
					    &resumed, buf, &buf_len);
    }
    if (status == errSecSuccess) {
	*session_resumed = resumed ? TRUE : FALSE;;
    } else {
	EAPLOG_FL(LOG_ERR, "EAP-TLS session failed to get session resumed info, %s (%ld)",
		  EAPSSLErrorString(status), (long)status);
    }
    return;
}

CFStringRef
EAPTLSSessionGetNegotiatedTLSProtocolVersion(EAPTLSSessionContextRef session_context)
{
    tls_protocol_version_t	tls_version;
    OSStatus			status;

    if (session_context == NULL) {
	return NULL;
    }
    if (session_context->secure_transport_context != NULL ||
	session_context->boringssl_context == NULL) {
	return NULL;
    }
    status = EAPBoringSSLSessionGetNegotiatedTLSVersion(session_context->boringssl_context, &tls_version);
    if (status != errSecSuccess) {
	return NULL;
    }
    switch (tls_version) {
	case tls_protocol_version_TLSv10:
	    return kEAPTLSVersion1_0;
	case tls_protocol_version_TLSv11:
	    return kEAPTLSVersion1_1;
	case tls_protocol_version_TLSv12:
	    return kEAPTLSVersion1_2;
	case tls_protocol_version_TLSv13:
	    return kEAPTLSVersion1_3;
	default:
	    return NULL;
    }
    return NULL;
}

void
EAPTLSSessionGetNegotiatedCipher(EAPTLSSessionContextRef session_context, SSLCipherSuite *cipher_suite)
{
    if (session_context == NULL || cipher_suite == NULL) {
	return;
    }
    *cipher_suite = SSL_NULL_WITH_NULL_NULL;
    if (session_context->secure_transport_context != NULL) {
	(void)SSLGetNegotiatedCipher(session_context->secure_transport_context, cipher_suite);
    } else if (session_context->boringssl_context != NULL) {
	/* TBD */
    }
    return;
}

