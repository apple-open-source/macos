/*
 * Copyright (c) 2000-2001 Apple Computer, Inc. All Rights Reserved.
 * 
 * The contents of this file constitute Original Code as defined in and are
 * subject to the Apple Public Source License Version 1.2 (the 'License').
 * You may not use this file except in compliance with the License. Please obtain
 * a copy of the License at http://www.apple.com/publicsource and read it before
 * using this file.
 * 
 * This Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER EXPRESS
 * OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES, INCLUDING WITHOUT
 * LIMITATION, ANY WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR
 * PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT. Please see the License for the
 * specific language governing rights and limitations under the License.
 */


//
// osxsigner - MacOS X's standard code signing algorithm.
//
#ifdef __MWERKS__
#define _CPP_OSXSIGNER
#endif

#include <Security/osxsigner.h>
#include <Security/cssmdata.h>
#include <Security/debugging.h>


namespace Security
{

namespace CodeSigning
{

//
// Construct an OSXSigner
//
OSXSigner::OSXSigner() : csp(gGuidAppleCSP)
{
}


//
// Signing/verification implementation
//
OSXSigner::OSXSignature *OSXSigner::sign(const Signable &target)
{
	Digester digester(*this);
	scanContents(digester, target);
	DataBuffer<OSXSignature::hashLength> hash;
	digester(hash);
	IFDUMPING("codesign", Debug::dumpData("sign", hash));
	return new OSXSignature(hash);
}

bool OSXSigner::verify(const Signable &target, const Signature *signature)
{
	if (const OSXSignature *sig = dynamic_cast<const OSXSignature *>(signature)) {
		Digester digester(*this);
		scanContents(digester, target);
		DataBuffer<OSXSignature::hashLength> hash;
		digester(hash);
		IFDUMPING("codesign", Debug::dumpData("verify", hash));
		return (*sig) == hash;
	}
	return false;
}

void OSXSigner::Digester::enumerateContents(const void *data, size_t length)
{
	digest(CssmData(const_cast<void *>(data), length));
}


//
// Re-create a Signature object from its external representation
//
OSXSigner::OSXSignature *OSXSigner::restore(uint32 type, const void *data, size_t length)
{
	switch (type) {
	case CSSM_ACL_CODE_SIGNATURE_OSX:
		if (length != OSXSignature::hashLength)
			CssmError::throwMe(CSSM_ERRCODE_INVALID_DATA);
		return new OSXSignature(data);
	default:
		CssmError::throwMe(CSSM_ERRCODE_UNKNOWN_FORMAT);
	}
}


}; // end namespace CodeSigning

} // end namespace Security
