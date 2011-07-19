
/*
 * Copyright (c) 2002-2009 Apple Inc. All rights reserved.
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

#ifndef _EAP8021X_EAPTLSUTIL_H
#define _EAP8021X_EAPTLSUTIL_H

/*
 * EAPTLSUtil.h
 * - utility functions for dealing with Secure Transport API's
 */

/* 
 * Modification History
 *
 * August 26, 2002	Dieter Siegmund (dieter@apple)
 * - created
 */

#include <Security/SecureTransport.h>
#include <Security/SecCertificate.h>
#include <Security/SecPolicy.h>
#include <CoreFoundation/CFBase.h>
#include <CoreFoundation/CFData.h>
#include <CoreFoundation/CFArray.h>
#include <CoreFoundation/CFDictionary.h>
#include <stdbool.h>
#include <EAP8021X/EAP.h>
#include <EAP8021X/EAPTLS.h>
#include <EAP8021X/EAPClientTypes.h>
#include <TargetConditionals.h>

typedef struct memoryBuffer_s {
    void *			data;
    size_t			length;
    size_t			offset;
} memoryBuffer, *memoryBufferRef;

typedef struct {
    bool			debug;
    memoryBufferRef		read;
    memoryBufferRef		write;
} memoryIO, * memoryIORef;

SSLContextRef
EAPSSLContextCreate(SSLProtocol protocol, bool is_server, 
		    SSLReadFunc func_read, SSLWriteFunc func_write, 
		    void * handle, char * peername, OSStatus * ret_status);

SSLContextRef
EAPTLSMemIOContextCreate(bool is_server, memoryIORef mem_io, 
			 char * peername, OSStatus * ret_status);
#if 0
OSStatus
EAPSSLContextSetCipherRestrictions(SSLContextRef ctx, char cipherRestrict);

const char *
EAPSSLCipherSuiteString(SSLCipherSuite cs);

const char *
EAPSSLProtocolVersionString(SSLProtocol prot);

#endif /* 0 */

const char *
EAPSSLErrorString(OSStatus err);

OSStatus 
EAPSSLMemoryIORead(SSLConnectionRef connection, void * data_buf, 
		  size_t * data_length);

OSStatus
EAPSSLMemoryIOWrite(SSLConnectionRef connection, const void * data_buf, 
		   size_t * data_length);

OSStatus
EAPTLSComputeKeyData(SSLContextRef ssl_context, 
		     const void * label, int label_length,
		     void * key, int key_length);

void
memoryBufferClear(memoryBufferRef buf);

void
memoryBufferInit(memoryBufferRef buf);

void
memoryIOClearBuffers(memoryIORef mem_io);

void
memoryIOInit(memoryIORef mem_io, memoryBufferRef read_buf, 
	     memoryBufferRef write_buf);

void
memoryIOSetDebug(memoryIORef mem_io, bool debug);

EAPPacketRef
EAPTLSPacketCreate(EAPCode code, int type, u_char identifier, int mtu,
		   memoryBufferRef buf, int * ret_fraglen);

EAPPacketRef
EAPTLSPacketCreate2(EAPCode code, int type, u_char identifier, int mtu,
		    memoryBufferRef buf, int * ret_fraglen, 
		    bool always_mark_first);

/*
 * Function: EAPSSLCopyPeerCertificates
 *
 * Purpose:
 *   A wrapper for SSLGetPeerCertificates that matches the CF function 
 *   naming conventions, and allows the certificate array to be released
 *   by simply calling CFRelease on the array. SSLGetPeerCertificates does
 *   not CFRelease each certificate after adding it to the array.
 */
OSStatus 
EAPSSLCopyPeerCertificates(SSLContextRef context, CFArrayRef * certs);

/*
 * Function: EAPTLSVerifyServerCertificateChain
 * Purpose:
 *   Given the configured EAP client properties and the server certificate
 *   determine whether to proceed or not.
 * Returns:
 *   kEAPClientStatusOK if it's OK to proceed.
 */
EAPClientStatus
EAPTLSVerifyServerCertificateChain(CFDictionaryRef properties,
				   CFArrayRef server_certs, 
				   OSStatus * ret_status);

/*
 * Function: EAPSecPolicyCopy
 * Purpose:
 *   Copies the EAP security policy object.
 * Returns:
 *   noErr if successful.
 */
OSStatus
EAPSecPolicyCopy(SecPolicyRef * ret_policy);

#if TARGET_OS_EMBEDDED
/*
 * Function: EAPTLSSecTrustSaveExceptionsBinding
 * Purpose:
 *   Given the evaluated SecTrustRef object, save an exceptions binding for the 
 *   given domain, identifier, and server_hash_str, all of which must be
 *   specified.
 * Returns:
 *   FALSE if the trust object was not in a valid state, 
 *   TRUE otherwise.
 */
bool
EAPTLSSecTrustSaveExceptionsBinding(SecTrustRef trust, 
				    CFStringRef domain, CFStringRef identifier,
				    CFStringRef server_hash_str);
/*
 * Function: EAPTLSSecTrustApplyExceptionsBinding
 * Purpose:
 *   Finds a stored trust exceptions object for the given domain, identifier,
 *   and server_cert_hash.  If it exists, applies the exceptions to the given
 *   trust object.
 */
void
EAPTLSSecTrustApplyExceptionsBinding(SecTrustRef trust, CFStringRef domain, 
				     CFStringRef identifier,
				     CFStringRef server_cert_hash);

/* 
 * Function: EAPTLSRemoveTrustExceptionsBindings
 * Purpose:
 *   Remove all of the trust exceptions bindings for the given 
 *   trust domain and identifier.
 * Example:
 * EAPTLSRemoveTrustExceptionsBindings(kEAPTLSTrustExceptionsDomainWirelessSSID,
 *                                     current_SSID);
 */
void
EAPTLSRemoveTrustExceptionsBindings(CFStringRef domain,
				    CFStringRef identifier);

/*
 * Function: EAPTLSCreateSecTrust
 * Purpose:
 *   Allocates and configures a SecTrustRef object using the
 *   EAPClientConfiguration dictionary 'properties', the server certificate
 *   chain 'server_certs', the trust execptions domain 'domain', and the
 *   trust exceptions identifier 'identifier'.
 * Returns:
 *   non-NULL SecTrustRef on success, NULL otherwise
 */
SecTrustRef
EAPTLSCreateSecTrust(CFDictionaryRef properties, CFArrayRef server_certs,
		     CFStringRef domain, CFStringRef identifier);

#endif /* TARGET_OS_EMBEDDED */

#endif /* _EAP8021X_EAPTLSUTIL_H */
