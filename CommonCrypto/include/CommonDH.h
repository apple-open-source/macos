/*
 * Copyright (c) 2010 Apple Inc. All Rights Reserved.
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

#ifndef _CC_DH_H_
#define _CC_DH_H_

#include <Availability.h>

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct CCDHRef_s *CCDHRef;

typedef struct CCDHParameters_s *CCDHParameters;

extern const CCDHParameters kCCDHRFC2409Group2
    __OSX_AVAILABLE_STARTING(__MAC_10_8, __IPHONE_5_0);
extern const CCDHParameters kCCDHRFC3526Group5
    __OSX_AVAILABLE_STARTING(__MAC_10_8, __IPHONE_5_0);

/*!
    @function   CCDHCreate
    @abstract   Creates a Diffie-Hellman context. 
    
	@param      dhParameter  The Diffie-Hellman Group to use (provides p and g).
                             The only appropriate values are kCCDHGenerator2 or
                             kCCDHGenerator5, defined above.
                            
    @result     If unable to allocate memory this returns NULL.
*/

CCDHRef
CCDHCreate(CCDHParameters dhParameter)
__OSX_AVAILABLE_STARTING(__MAC_10_8, __IPHONE_5_0);

/*!
    @function   CCDHRelease
    @abstract   Releases a Diffie-Hellman context. 
    
	@param      ref  The Diffie-Hellman context to clear and deallocate.
                            
*/

void
CCDHRelease(CCDHRef ref)
__OSX_AVAILABLE_STARTING(__MAC_10_8, __IPHONE_5_0);

/*!
    @function   CCDHGenerateKey
    @abstract   Generate the public key for use in a Diffie-Hellman handshake. 
                This value is returned as a byte string.
    
	@param      ref  The Diffie-Hellman context.
    @result		returns -1 on failure.
                            
*/


int
CCDHGenerateKey(CCDHRef ref, void *output, size_t *outputLength)
__OSX_AVAILABLE_STARTING(__MAC_10_8, __IPHONE_5_0);

/*!
    @function   CCDHComputeKey
    @abstract   Compute the shared Diffie-Hellman key using the peer's public
                key.
    
	@param      sharedKey  Shared key computed from the peer public key, p, g,
                            and the private key.
	@param      peerPubKey  Public key received from the peer.
    @param		peerPubKeyLen Length of peer public key.
	@param      ref  The Diffie-Hellman context to clear and deallocate.

	@param      returns the length of the shared key.
                            
*/

int
CCDHComputeKey(unsigned char *sharedKey, size_t *sharedKeyLen, const void *peerPubKey, size_t peerPubKeyLen, CCDHRef ref)
__OSX_AVAILABLE_STARTING(__MAC_10_8, __IPHONE_5_0);


CCDHParameters
CCDHParametersCreateFromData(const void *p, size_t pLen, const void *g, size_t gLen, size_t l)
__OSX_AVAILABLE_STARTING(__MAC_10_8, __IPHONE_5_0);

CCDHParameters
CCDHParametersCreateFromPKCS3(const void *data, size_t len)
__OSX_AVAILABLE_STARTING(__MAC_10_8, __IPHONE_5_0);

size_t
CCDHParametersPKCS3EncodeLength(CCDHParameters parms)
__OSX_AVAILABLE_STARTING(__MAC_10_8, __IPHONE_5_0);

size_t
CCDHParametersPKCS3Encode(CCDHParameters parms, void *data, size_t dataAvailable)
__OSX_AVAILABLE_STARTING(__MAC_10_8, __IPHONE_5_0);

void
CCDHParametersRelease(CCDHParameters parameters)
__OSX_AVAILABLE_STARTING(__MAC_10_8, __IPHONE_5_0);

#ifdef __cplusplus
}
#endif

#endif  /* _CC_DH_H_ */
