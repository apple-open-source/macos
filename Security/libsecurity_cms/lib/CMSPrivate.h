/*
 * Copyright (c) 2006 Apple Computer, Inc. All Rights Reserved.
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

/* 
 * CMSPrivate.h - access to low-level CMS objects used by CMSDecoder and CMSEncoder.
 */
#ifndef	_CMS_PRIVATE_H_
#define _CMS_PRIVATE_H_

#include <Security/CMSEncoder.h>	
#include <Security/CMSDecoder.h>	
#include <Security/SecCmsEncoder.h>
#include <Security/SecCmsDecoder.h>
#include <Security/SecCmsMessage.h>

#ifdef __cplusplus
extern "C" {
#endif

/***
 *** Private CMSEncoder routines
 ***/
 
/*
 * Obtain the SecCmsMessageRef associated with a CMSEncoderRef. Intended 
 * to be called after (optionally) setting the encoder's various attributes 
 * via CMSEncoderAddSigners(), CMSEncoderAddRecipients(), etc. and before 
 * the first call to CMSEncoderUpdateContent(). The returned SecCmsMessageRef 
 * will be initialized per the previously specified attributes; the caller 
 * can manipulate the SecCmsMessageRef prior to proceeding with 
 * CMSEncoderUpdateContent() calls. 
 */
OSStatus CMSEncoderGetCmsMessage(
	CMSEncoderRef		cmsEncoder,
	SecCmsMessageRef	*cmsMessage);		/* RETURNED */
	
/* 
 * Optionally specify a SecCmsEncoderRef to use with a CMSEncoderRef.
 * If this is called, it must be called before the first call to 
 * CMSEncoderUpdateContent(). The CMSEncoderRef takes ownership of the
 * incoming SecCmsEncoderRef.
 */
OSStatus CMSEncoderSetEncoder(
	CMSEncoderRef		cmsEncoder,
	SecCmsEncoderRef	encoder);
	
/* 
 * Obtain the SecCmsEncoderRef associated with a CMSEncoderRef. 
 * Returns a NULL SecCmsEncoderRef if neither CMSEncoderSetEncoder nor
 * CMSEncoderUpdateContent() has been called. 
 * The CMSEncoderRef retains ownership of the SecCmsEncoderRef.
 */
OSStatus CMSEncoderGetEncoder(
	CMSEncoderRef		cmsEncoder,
	SecCmsEncoderRef	*encoder);			/* RETURNED */

/*
 * Set the signing time for a CMSEncoder.
 * This is only used if the kCMSAttrSigningTime attribute is included.
 */
OSStatus CMSEncoderSetSigningTime(
	CMSEncoderRef		cmsEncoder,
	CFAbsoluteTime		time);

void
CmsMessageSetTSAContext(CMSEncoderRef cmsEncoder, CFTypeRef tsaContext);

/***
 *** Private CMSDecoder routines
 ***/
 
/*
 * Obtain the SecCmsMessageRef associated with a CMSDecoderRef. Intended 
 * to be called after decoding the message (i.e., after 
 * CMSDecoderFinalizeMessage() to gain finer access to the contents of the
 * SecCmsMessageRef than is otherwise available via the CMSDecoder interface. 
 * Returns a NULL SecCmsMessageRef if CMSDecoderFinalizeMessage() has not been
 * called. 
 *
 * The CMSDecoder retains ownership of the returned SecCmsMessageRef.
 */
OSStatus CMSDecoderGetCmsMessage(
	CMSDecoderRef		cmsDecoder,
	SecCmsMessageRef	*cmsMessage);		/* RETURNED */
	

/* 
 * Optionally specify a SecCmsDecoderRef to use with a CMSDecoderRef.
 * If this is called, it must be called before the first call to 
 * CMSDecoderUpdateMessage(). The CMSDecoderRef takes ownership of the
 * incoming SecCmsDecoderRef.
 */
OSStatus CMSDecoderSetDecoder(
	CMSDecoderRef		cmsDecoder,
	SecCmsDecoderRef	decoder);
	
/* 
 * Obtain the SecCmsDecoderRef associated with a CMSDecoderRef. 
 * Returns a NULL SecCmsDecoderRef if neither CMSDecoderSetDecoder() nor
 * CMSDecoderUpdateMessage() has been called. 
 * The CMSDecoderRef retains ownership of the SecCmsDecoderRef.
 */
OSStatus CMSDecoderGetDecoder(
	CMSDecoderRef		cmsDecoder,
	SecCmsDecoderRef	*decoder);			/* RETURNED */
	
#ifdef __cplusplus
}
#endif

#endif	/* _CMS_PRIVATE_H_ */

