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

/*!
 * @header DSEncryptedEndpoint
 * Layered endpoint that enciphers data.
 */

/*
	Note: all network addresses in method parameters and return values
	are in host byte order - they are converted to network byte order
	inside the methods for socket calls.

	Note2: need to be aware of which routines are FW or Server exclusive
	for what type of logging
 */

#ifndef _DSEncryptedEndpoint_H_
#define _DSEncryptedEndpoint_H_ 1

#include "DSTCPEndpoint.h"

#include "libCdsaCrypt.h"

#define DH_KEY_SIZE		512		/* size of Diffie-Hellman key in bits */
#define DERIVE_KEY_SIZE	128		/* size of derived key in bits */
#define DERIVE_KEY_ALG	CSSM_ALGID_AES
#define DSTCPAuthTag	'DHN2'

typedef enum {
	eBadCall = -4,
	eDisabled = -3,
	eFail = -2,
	eContinue = -1,
	eSuccess = 0
} eResult ;


// ----------------------------------------------------------------------------
// DSEncryptedEndpoint: implementation of encrypted endpoint.
// ----------------------------------------------------------------------------

class DSEncryptedEndpoint: public DSTCPEndpoint
{
public:
	/**** Instance methods. ****/
	// ctor and dtor.
   					DSEncryptedEndpoint	(	const DSTCPEndpoint *inEndpoint,
											const uInt32 inSessionID );
					DSEncryptedEndpoint	(	int inConnectFD,
										uInt32 inOpenTimeOut = kTCPOpenTimeout,
 										uInt32 inRdWrTimeOut = kTCPRWTimeout );
    virtual			~DSEncryptedEndpoint	( void );

	// Required specialization of pure virtual base class methods.

	// Specialized versions of base class methods.
	virtual void	EncryptData ( void *inData, const uInt32 inBuffSize, void *&outData, uInt32 &outBuffSize );
	virtual void	DecryptData ( void *inData, const uInt32 inBuffSize, void *&outData, uInt32 &outBuffSize );

	// New public methods.
	sInt32			ClientNegotiateKey ( void );
	sInt32			ServerNegotiateKey ( void );

protected:
	/**** Typedefs, enums, and constants. ****/
    typedef DSTCPEndpoint inherited;

	CSSM_CSP_HANDLE 		fcspHandle;
	CSSM_KEY				fOurDerivedKey;
	CSSM_DATA				fOurParamBlock;

private:
	/**** Instance methods accessible only to class. ****/

};

#endif	/* _DSEncryptedEndpoint_H_ */
