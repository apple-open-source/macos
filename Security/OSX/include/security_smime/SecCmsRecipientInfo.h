/*
 *  Copyright (c) 2004,2011,2014 Apple Inc. All Rights Reserved.
 *
 *  @APPLE_LICENSE_HEADER_START@
 *  
 *  This file contains Original Code and/or Modifications of Original Code
 *  as defined in and that are subject to the Apple Public Source License
 *  Version 2.0 (the 'License'). You may not use this file except in
 *  compliance with the License. Please obtain a copy of the License at
 *  http://www.opensource.apple.com/apsl/ and read it before using this
 *  file.
 *  
 *  The Original Code and all software distributed under the License are
 *  distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 *  EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 *  INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 *  FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 *  Please see the License for the specific language governing rights and
 *  limitations under the License.
 *  
 *  @APPLE_LICENSE_HEADER_END@
 */

/*!
    @header SecCmsRecipientInfo.h
    @Copyright (c) 2004,2011,2014 Apple Inc. All Rights Reserved.

    @availability 10.4 and later
    @abstract Interfaces of the CMS implementation.
    @discussion The functions here implement functions for encoding
                and decoding Cryptographic Message Syntax (CMS) objects
                as described in rfc3369.
 */

#ifndef _SECURITY_SECCMSRECIPIENTINFO_H_
#define _SECURITY_SECCMSRECIPIENTINFO_H_  1

#include <Security/SecCmsBase.h>


#if defined(__cplusplus)
extern "C" {
#endif


/*!
    @function
    @abstract Create a recipientinfo
    @discussion We currently do not create KeyAgreement recipientinfos with multiple recipientEncryptedKeys
                the certificate is supposed to have been verified by the caller
 */
extern SecCmsRecipientInfoRef
SecCmsRecipientInfoCreate(SecCmsMessageRef cmsg, SecCertificateRef cert);

/*!
    @function
 */
extern SecCmsRecipientInfoRef
SecCmsRecipientInfoCreateWithSubjKeyID(SecCmsMessageRef cmsg, 
                                         CSSM_DATA_PTR subjKeyID,
                                         SecPublicKeyRef pubKey);

/*!
    @function
 */
extern SecCmsRecipientInfoRef
SecCmsRecipientInfoCreateWithSubjKeyIDFromCert(SecCmsMessageRef cmsg, 
                                                 SecCertificateRef cert);

/*!
    @function
 */
extern void
SecCmsRecipientInfoDestroy(SecCmsRecipientInfoRef ri);


#if defined(__cplusplus)
}
#endif

#endif /* _SECURITY_SECCMSRECIPIENTINFO_H_ */
