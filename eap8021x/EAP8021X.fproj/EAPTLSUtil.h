
/*
 * Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
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
#include <CoreFoundation/CFBase.h>
#include <CoreFoundation/CFData.h>
#include <CoreFoundation/CFArray.h>
#include <CoreFoundation/CFDictionary.h>
#include <stdbool.h>
#include <EAP8021X/EAPTLS.h>
#include <EAP8021X/EAPClientTypes.h>

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

#endif 0

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
 *   Given the configured EAP client properties, trust proceed count, 
 *   and the server certificate chain, determine whether to proceed or not.
 * Returns:
 *   kEAPClientStatusOK if it's OK to proceed.
 */
EAPClientStatus
EAPTLSVerifyServerCertificateChain(CFDictionaryRef properties,
				   CFArrayRef server_certs, 
				   OSStatus * ret_status);
/*
 * Function: mySecCertificateArrayCreateCFDataArray (deprecated)
 * Purpose:
 *   Convert a CFArray[SecCertificate] to CFArray[CFData].
 * Note:
 *   This is deprecated, use EAPSecCertificateArrayCreateCFDataArray() in
 *   <EAP8021X/EAPCertificateUtils.h> instead.
 */
CFArrayRef
mySecCertificateArrayCreateCFDataArray(CFArrayRef certs);

/*
 * Function: EAPSecPolicyCopy
 * Purpose:
 *   Copies the EAP security policy object.
 * Returns:
 *   noErr if successful.
 */
OSStatus
EAPSecPolicyCopy(SecPolicyRef * ret_policy);
#endif _EAP8021X_EAPTLSUTIL_H
