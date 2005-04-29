/*
 * Copyright (c) 2003-2004 Apple Computer, Inc. All Rights Reserved.
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
 *
 * SecAsn1Coder.h: ANS1 encode/decode object, ANSI C version.
 */
 
#ifndef	_SEC_ASN1_CODER_H_
#define _SEC_ASN1_CODER_H_

#include <Security/cssmtype.h>
#include <Security/SecBase.h>
#include <Security/secasn1t.h>
#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Opaque reference to a SecAsn1Coder object.
 */
typedef struct SecAsn1Coder *SecAsn1CoderRef;

/*
 * Create/destroy SecAsn1Coder object. 
 */
OSStatus SecAsn1CoderCreate(
	SecAsn1CoderRef  *coder);
	
OSStatus SecAsn1CoderRelease(
	SecAsn1CoderRef  coder);
	
/*
 * DER decode an untyped item per the specified template array. 
 * The result is allocated in this SecAsn1Coder's memory pool and 
 * is freed when this object is released.
 *
 * Returns errSecUnknownFormat on decode-specific error.
 *
 * The dest pointer is a template-specific struct allocated by the caller 
 * and must be zeroed by the caller. 
 */
OSStatus SecAsn1Decode(
	SecAsn1CoderRef			coder,
	const void				*src,		// DER-encoded source
	size_t					len,
	const SecAsn1Template 	*templ,	
	void					*dest);
		
/* 
 * Convenience routine, decode from a CSSM_DATA.
 */
OSStatus SecAsn1DecodeData(
	SecAsn1CoderRef			coder,
	const CSSM_DATA			*src,
	const SecAsn1Template 	*templ,	
	void					*dest);

/*
 * DER encode. The encoded data (in dest.Data) is allocated in this 
 * SecAsn1Coder's memory pool and is freed when this object is released.
 *
 * The src pointer is a template-specific struct.
 */
OSStatus SecAsn1EncodeItem(
	SecAsn1CoderRef			coder,
	const void				*src,
	const SecAsn1Template 	*templ,	
	CSSM_DATA				*dest);
		
/*
 * Some alloc-related methods which come in handy when using
 * this object. All memory is allocated using this object's 
 * memory pool. Caller never has to free it. Used for
 * temp allocs of memory which only needs a scope which is the
 * same as this object. 
 *
 * All except SecAsn1Malloc return a memFullErr in the highly 
 * unlikely event of a malloc failure.
 */
void *SecAsn1Malloc(
	SecAsn1CoderRef			coder,
	size_t					len); 
	
/* malloc item.Data, set item.Length */
OSStatus SecAsn1AllocItem(
	SecAsn1CoderRef			coder,
	CSSM_DATA				*item,
	size_t					len);
	
/* malloc and copy, various forms */
OSStatus SecAsn1AllocCopy(
	SecAsn1CoderRef			coder,
	const void				*src,
	size_t					len,
	CSSM_DATA				*dest);
	
OSStatus SecAsn1AllocCopyItem(
	SecAsn1CoderRef			coder,
	const CSSM_DATA			*src,
	CSSM_DATA				*dest);
		
#ifdef __cplusplus
}
#endif

#endif	/* _SEC_ASN1_CODER_H_ */
