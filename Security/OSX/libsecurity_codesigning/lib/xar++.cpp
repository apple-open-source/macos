/*
 * Copyright (c) 2011-2012 Apple Inc. All Rights Reserved.
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

//
// xar++ - interface to XAR-format archive files
//
#include "xar++.h"
#include "notarization.h"
#include <security_utilities/cfutilities.h>
#include <Security/Security.h>


namespace Security {
namespace CodeSigning {


Xar::Xar(const char *path)
{
	mXar = 0;
	mSigCMS = 0;
	mSigClassic = 0;
	if (path)
		open(path);
}

void Xar::open(const char *path)
{
	if ((mXar = ::xar_open(path, READ)) == NULL)
	    return;

	mPath = std::string(path);
    
	xar_signature_t sig = ::xar_signature_first(mXar);
	// read signatures until we find a CMS signature
	while (sig && mSigCMS == NULL) {
		const char *type = ::xar_signature_type(sig);
		if (strcmp(type, "CMS") == 0) {
			mSigCMS = sig;
		} else if (strcmp(type, "RSA") == 0) {
			mSigClassic = sig;
		}
		sig = ::xar_signature_next(sig);
	}
}

Xar::~Xar()
{
	if (mXar)
		::xar_close(mXar);
}

static CFArrayRef copyCertChainFromSignature(xar_signature_t sig)
{
	unsigned count = xar_signature_get_x509certificate_count(sig);
	CFRef<CFMutableArrayRef> certs = makeCFMutableArray(0);
	for (unsigned ix = 0; ix < count; ix++) {
		const uint8_t *data;
		uint32_t length;
		if (xar_signature_get_x509certificate_data(sig, ix, &data, &length) == 0) {
			CFTempData cdata(data, length);
			CFRef<SecCertificateRef> cert = SecCertificateCreateWithData(NULL, cdata);
			CFArrayAppendValue(certs, cert.get());
		}
	}
	return certs.yield();
}

CFArrayRef Xar::copyCertChain()
{
	if (mSigCMS)
		return copyCertChainFromSignature(mSigCMS);
	else if (mSigClassic)
		return copyCertChainFromSignature(mSigClassic);
	return NULL;
}

void Xar::registerStapledNotarization()
{
	registerStapledTicketInPackage(mPath);
}

CFDataRef Xar::createPackageChecksum()
{
	xar_signature_t sig = NULL;

	// Always prefer a CMS signature to a class signature and return early
	// if no appropriate signature has been found.
	if (mSigCMS) {
		sig = mSigCMS;
	} else if (mSigClassic) {
		sig = mSigClassic;
	} else {
		return NULL;
	}

	// Extract the signed data from the xar, which is actually just the checksum
	// we use as an identifying hash.
	uint8_t *data = NULL;
	uint32_t length;
	if (xar_signature_copy_signed_data(sig, &data, &length, NULL, NULL, NULL) != 0) {
		secerror("Unable to extract package hash for package: %s", mPath.c_str());
		return NULL;
	}

	// xar_signature_copy_signed_data returns malloc'd data that can be used without copying
	// but must be free'd properly later.
	return makeCFDataMalloc(data, length);
}

SecCSDigestAlgorithm Xar::checksumDigestAlgorithm()
{
	int32_t error = 0;
	const char* value = NULL;
	unsigned long size = 0;

	if (mXar == NULL) {
		secerror("Evaluating checksum digest on bad xar: %s", mPath.c_str());
		return kSecCodeSignatureNoHash;
	}

	error = xar_prop_get((xar_file_t)mXar, "checksum/size", &value);
	if (error == -1) {
		secerror("Unable to extract package checksum size: %s", mPath.c_str());
		return kSecCodeSignatureNoHash;
	}

	size = strtoul(value, NULL, 10);
	switch (size) {
		case CC_SHA1_DIGEST_LENGTH:
			return kSecCodeSignatureHashSHA1;
		case CC_SHA256_DIGEST_LENGTH:
			return kSecCodeSignatureHashSHA256;
		case CC_SHA512_DIGEST_LENGTH:
			return kSecCodeSignatureHashSHA512;
		case CC_MD5_DIGEST_LENGTH:
		default:
			return kSecCodeSignatureNoHash;
	}
}

} // end namespace CodeSigning
} // end namespace Security
