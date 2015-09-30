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
    @header SecCmsDigestContext.h
    @Copyright (c) 2004,2011,2014 Apple Inc. All Rights Reserved.

    @availability 10.4 and later
    @abstract Interfaces of the CMS implementation.
    @discussion The functions here implement functions calculating digests.
 */

#ifndef _SECURITY_SECCMSDIGESTCONTEXT_H_
#define _SECURITY_SECCMSDIGESTCONTEXT_H_  1

#include <Security/SecCmsBase.h>


#if defined(__cplusplus)
extern "C" {
#endif


/*!
    @function
    @abstract Start digest calculation using all the digest algorithms in "digestalgs" in parallel.
 */
extern SecCmsDigestContextRef
SecCmsDigestContextStartMultiple(SECAlgorithmID **digestalgs);

/*!
    @function
    @abstract Feed more data into the digest machine.
 */
extern void
SecCmsDigestContextUpdate(SecCmsDigestContextRef cmsdigcx, const unsigned char *data, size_t len);

/*!
    @function
    @abstract Cancel digesting operation.
    @discussion Cancel a DigestContext created with @link SecCmsDigestContextStartMultiple SecCmsDigestContextStartMultiple function@/link.
 */
extern void
SecCmsDigestContextCancel(SecCmsDigestContextRef cmsdigcx);

/*!
    @function
    @abstract Finish the digests and put them into an array of CSSM_DATAs (allocated on arena)
 */
extern OSStatus
SecCmsDigestContextFinishMultiple(SecCmsDigestContextRef cmsdigcx, SecArenaPoolRef arena,
			    CSSM_DATA_PTR **digestsp);

#if defined(__cplusplus)
}
#endif

#endif /* _SECURITY_SECCMSDIGESTCONTEXT_H_ */
