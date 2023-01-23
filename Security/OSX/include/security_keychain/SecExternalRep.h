/*
 * Copyright (c) 2004,2011,2014 Apple Inc. All Rights Reserved.
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
 * SecExternalRep.h - private classes representing external representations of
 *					  SecKeychainItemRefs, used by SecImportExport.h
 */

#ifndef	_SECURITY_SEC_EXTERNAL_REP_H_
#define _SECURITY_SEC_EXTERNAL_REP_H_

#include <Security/SecImportExport.h>

/*
 * mechanism to limit private key import
 */
typedef enum {
	PIS_NoLimit,			// no limit
	PIS_AllowOne,			// allow import of at most one
	PIS_NoMore				// found one, no more allowed
} ImpPrivKeyImportState;

namespace Security
{

namespace KeychainCore
{

/*
 * When exporting, each instance of this represents one keychain item we're
 * exporting. Note that SecIdentityRefs are represented by two of these - one
 * for the cert, one for the key.
 */
class SecExportRep
{
protected:
	SecExportRep(
		CFTypeRef						kcItemRef);

public:
	/* default constructor throws */
	SecExportRep();
	
	/*
	 * Gleans SecExternalItemType from incoming type, throws MacOSError if
	 * incoming type is bogus, and vends an instance of a suitable subclass.
	 * Vended object holds a reference to kcItem for its lifetime. 
	 */
	static SecExportRep *vend(	
		CFTypeRef						kcItemRef);
		
	virtual ~SecExportRep();
	
	/* 
	 * Append external representation to CFData.
	 * Implemented in subclass.
	 */
	virtual OSStatus exportRep(
		SecExternalFormat					format,	
		SecItemImportExportFlags			flags,	
		const SecKeyImportExportParameters	*keyParams,		// optional 
		CFMutableDataRef					outData,		// data appended here
		const char							**pemHeader);	// e.g., "X509 CERTIFICATE"
		
	/* member variables are read-only to the public */
	SecKeychainItemRef	kcItem()		{ return mKcItem; }
	SecExternalItemType	externType()	{ return mExternType; }
	CFArrayRef pemParamLines()			{ return mPemParamLines; }
	
protected:
	SecKeychainItemRef		mKcItem;
	SecExternalItemType		mExternType;		// inferred in vend()
	CFArrayRef				mPemParamLines;		// optional PEM header strings
	
};

/* 
 * Class representing one item we're importing from external representation.
 * Normally represents the one "thing" the app has provided to 
 * SecKeychainItemImport(), but when importing in kSecFormatPEMSequence
 * format, the inpus is parsed into separate components, each of which 
 * gets one instance of this class. 
 */
class SecImportRep
{
private:
	/* no default constructor */
	SecImportRep() { }
	
public:
	/*
	 * Between constructor and the importRep() call, we're a placeholder to 
	 * allow our owner to keep track of what is in an incoming pile of bits. 
	 * We keep a reference to the incoming CFDataRef for our lifetime. 
	 */
	SecImportRep(
		CFDataRef						external,
		SecExternalItemType				externType,		// may be unknown 
		SecExternalFormat				externFormat,   // may be unknown
		CSSM_ALGORITHMS					keyAlg,		// may be unknown, CSSM_ALGID_NONE
		CFArrayRef						pemParamLines = NULL);

	~SecImportRep();
	
	/* 
	 * Convert to one or more SecKeychainItemRefs and/or import to keychain.
	 * Both mExternType and mExternFormat must be valid at this point. 
	 * Any conversion requiring unwrapping rerquires either a keychain
	 * into which the unwrapped items will be imported) or a CSPDL handle (in 
	 * which case the resulting items are floating and ephemeral). 
	 * If we create a SecKeychainItemRefs, the only ref count held on
	 * return will be that held by outArray. 
	 *
	 * Incoming CSP handle is required; by convention it must be derived from
	 * importKeychain if importKeychain is present. 
	 */
	OSStatus importRep(
		SecKeychainRef						importKeychain,		// optional
		CSSM_CSP_HANDLE						cspHand,			// required
		SecItemImportExportFlags			flags,
		const SecKeyImportExportParameters	*keyParams,			// optional 
		ImpPrivKeyImportState				&keyImportState,	// IN/OUT
		CFMutableArrayRef					outArray);			// optional, append here 
		
private:
	/* implemented in SecWrappedKeys.cpp */
	OSStatus importWrappedKeyOpenssl(
		SecKeychainRef						importKeychain, // optional
		CSSM_CSP_HANDLE						cspHand,		// required
		SecItemImportExportFlags			flags,
		const SecKeyImportExportParameters	*keyParams,		// optional 
		CFMutableArrayRef					outArray);		// optional, append here 

	/* optional inferred PrintName attribute */
	char *mPrintName;	
	
public:
	/* Just keep these public, it simplifies things */
	CFDataRef				mExternal;	
	SecExternalItemType		mExternType;	
	SecExternalFormat		mExternFormat;
	CSSM_ALGORITHMS			mKeyAlg;
	CFArrayRef				mPemParamLines;		// optional PEM header strings
};

} // end namespace KeychainCore

} // end namespace Security

extern "C" {

/* implemented in SecWrappedKeys.cpp */
OSStatus impExpWrappedKeyOpenSslExport(
	SecKeyRef							secKey,
	SecItemImportExportFlags			flags,		
	const SecKeyImportExportParameters	*keyParams,		// optional 
	CFMutableDataRef					outData,		// output appended here
	const char							**pemHeader,	// RETURNED
	CFArrayRef							*pemParamLines);// RETURNED

}

#endif	/* _SECURITY_SEC_IMPORT_EXPORT_H_ */
