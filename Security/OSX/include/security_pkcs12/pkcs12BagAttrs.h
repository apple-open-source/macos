/*
 * Copyright (c) 2003-2004,2011,2014 Apple Inc. All Rights Reserved.
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
 * pkcs12BagAttrs.h : internal representation of P12 SafeBag 
 *                    attribute, OTHER THAN friendlyName and localKeyId.
 *					  This corresponds to a SecPkcs12AttrsRef at the
 *					  public API layer.
 */
 
#ifndef	_PKCS12_BAG_ATTRS_H_
#define _PKCS12_BAG_ATTRS_H_

#include <Security/keyTemplates.h>		// for NSS_Attribute
#include <security_asn1/SecNssCoder.h>
#include <CoreFoundation/CoreFoundation.h>

class P12BagAttrs {
public:
	/* 
	 * Empty constructor used by P12SafeBag during decoding.
	 * Indivudual attrs not understood by P12SafeBag get added 
	 * via addAttr().
	 */
	P12BagAttrs(
		SecNssCoder &coder) 
		: mAttrs(NULL),
		  mCoder(coder) { }
	
	/* 
	 * Copying constructor used by P12SafeBag during encoding.
	 */
	P12BagAttrs(
		const P12BagAttrs *otherAttrs,		// optional
		SecNssCoder &coder);
		
	~P12BagAttrs() { }

	/* Raw getter used just prior to encode. */
	unsigned numAttrs() const;
	NSS_Attribute *getAttr(
		unsigned			attrNum);
		
	/*
	 * Add an attr during decoding. Only "generic" attrs, other
	 * than friendlyName and localKeyId, are added here. 
	 */
	void addAttr(
		const NSS_Attribute &attr);
	
	/*
	 * Add an attr pre-encode, from SecPkcs12Add*() or 
	 * SecPkcs12AttrsAddAttr().
	 */
	void addAttr(
		const CFDataRef		attrOid,
		const CFArrayRef	attrValues);
		
	/* 
	 * getter, public API version
	 */
	void getAttr(
		unsigned			attrNum,
		CFDataRef			*attrOid,		// RETURNED
		CFArrayRef			*attrValues);	// RETURNED
					
private:
	NSS_Attribute *reallocAttrs(
		size_t numNewAttrs);
		
	void copyAttr(
		const NSS_Attribute &src,
		NSS_Attribute &dst);
		
	/*
	 * Stored in NSS form for easy encode
	 */
	NSS_Attribute		**mAttrs;
	SecNssCoder			&mCoder;
};

/* 
 * In the most common usage, a P12BagAttrs's SecNssCoder is associated 
 * with the owning P12Coder's mCoder. In the case of a "standalone"
 * P12BagAttrs's created by the app via SecPkcs12AttrsCreate(),
 * this subclass is used, proving the P12BadAttr's SecNssCoder
 * directly.
 */
class P12BagAttrsStandAlone : public P12BagAttrs
{
public:
	P12BagAttrsStandAlone() 
		: P12BagAttrs(mPrivCoder)
			{ }

	~P12BagAttrsStandAlone() { }
	
private:
	SecNssCoder			mPrivCoder;
};

#endif	/* _PKCS12_BAG_ATTRS_H_ */

