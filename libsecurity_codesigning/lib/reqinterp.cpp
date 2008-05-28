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

//
// reqinterp - Requirement language (exprOp) interpreter
//
#include "reqinterp.h"
#include <Security/SecTrustSettingsPriv.h>
#include <Security/SecCertificatePriv.h>
#include <security_utilities/memutils.h>
#include "csutilities.h"

namespace Security {
namespace CodeSigning {


static CFStringRef appleIntermediateCN = CFSTR("Apple Code Signing Certification Authority");
static CFStringRef appleIntermediateO = CFSTR("Apple Inc.");


//
// Construct an interpreter given a Requirement and an evaluation context.
//
Requirement::Interpreter::Interpreter(const Requirement *req, const Context *ctx)
	: Reader(req), mContext(ctx)
{
}


//
// Main interpreter function.
//
// ExprOp code is in Polish Notation (operator followed by operands),
// and this engine uses opportunistic evaluation.
//
bool Requirement::Interpreter::evaluate()
{
	ExprOp op = ExprOp(get<uint32_t>());
	switch (op & ~opFlagMask) {
	case opFalse:
		return false;
	case opTrue:
		return true;
	case opIdent:
		return getString() == mContext->directory->identifier();
	case opAppleAnchor:
		return appleSigned();
	case opAppleGenericAnchor:
		return appleAnchored();
	case opAnchorHash:
		{
			SecCertificateRef cert = mContext->cert(get<int32_t>());
			return verifyAnchor(cert, getSHA1());
		}
	case opInfoKeyValue:	// [legacy; use opInfoKeyField]
		{
			string key = getString();
			return infoKeyValue(key, Match(CFTempString(getString()), matchEqual));
		}
	case opAnd:
		return evaluate() & evaluate();
	case opOr:
		return evaluate() | evaluate();
	case opCDHash:
		{
			SHA1 hash;
			hash(mContext->directory, mContext->directory->length());
			return hash.verify(getHash());
		}
	case opNot:
		return !evaluate();
	case opInfoKeyField:
		{
			string key = getString();
			Match match(*this);
			return infoKeyValue(key, match);
		}
	case opEntitlementField:
		{
			string key = getString();
			Match match(*this);
			return entitlementValue(key, match);
		}
	case opCertField:
		{
			SecCertificateRef cert = mContext->cert(get<int32_t>());
			string key = getString();
			Match match(*this);
			return certFieldValue(key, match, cert);
		}
	case opCertGeneric:
		{
			SecCertificateRef cert = mContext->cert(get<int32_t>());
			string key = getString();
			Match match(*this);
			return certFieldGeneric(key, match, cert);
		}
	case opTrustedCert:
		return trustedCert(get<int32_t>());
	case opTrustedCerts:
		return trustedCerts();
	default:
		// opcode not recognized - handle generically if possible, fail otherwise
		if (op & (opGenericFalse | opGenericSkip)) {
			// unknown opcode, but it has a size field and can be safely bypassed
			skip(get<uint32_t>());
			if (op & opGenericFalse) {
				secdebug("csinterp", "opcode 0x%x interpreted as false", op);
				return false;
			} else {
				secdebug("csinterp", "opcode 0x%x ignored; continuing", op);
				return evaluate();
			}
		}
		// unrecognized opcode and no way to interpret it
		secdebug("csinterp", "opcode 0x%x cannot be handled; aborting", op);
		MacOSError::throwMe(errSecCSUnimplemented);
	}
}


//
// Evaluate an Info.plist key condition
//
bool Requirement::Interpreter::infoKeyValue(const string &key, const Match &match)
{
	if (mContext->info)		// we have an Info.plist
		if (CFTypeRef value = CFDictionaryGetValue(mContext->info, CFTempString(key)))
			return match(value);
	return false;
}


//
// Evaluate an entitlement condition
//
bool Requirement::Interpreter::entitlementValue(const string &key, const Match &match)
{
	if (mContext->entitlements)		// we have an Info.plist
		if (CFTypeRef value = CFDictionaryGetValue(mContext->entitlements, CFTempString(key)))
			return match(value);
	return false;
}


bool Requirement::Interpreter::certFieldValue(const string &key, const Match &match, SecCertificateRef cert)
{
	// no cert, no chance
	if (cert == NULL)
		return false;

	// a table of recognized keys for the "certificate[foo]" syntax
	static const struct CertField {
		const char *name;
		const CSSM_OID *oid;
	} certFields[] = {
		{ "subject.C", &CSSMOID_CountryName },
		{ "subject.CN", &CSSMOID_CommonName },
		{ "subject.D", &CSSMOID_Description },
		{ "subject.L", &CSSMOID_LocalityName },
		{ "subject.O", &CSSMOID_OrganizationName },
		{ "subject.OU", &CSSMOID_OrganizationalUnitName },
		{ "subject.ST", &CSSMOID_StateProvinceName },
		{ "subject.STREET", &CSSMOID_StreetAddress },
		{ NULL, NULL }
	};
	
	// DN-component single-value match
	for (const CertField *cf = certFields; cf->name; cf++)
		if (cf->name == key) {
			CFRef<CFStringRef> value;
			if (IFDEBUG(OSStatus rc =) SecCertificateCopySubjectComponent(cert, cf->oid, &value.aref())) {
				secdebug("csinterp", "cert %p lookup for DN.%s failed rc=%ld", cert, key.c_str(), rc);
				return false;
			}
			return match(value);
		}

	// email multi-valued match (any of...)
	if (key == "email") {
		CFRef<CFArrayRef> value;
		if (OSStatus rc = SecCertificateCopyEmailAddresses(cert, &value.aref())) {
			secdebug("csinterp", "cert %p lookup for email failed rc=%ld", cert, rc);
			return false;
		}
		return match(value);
	}

	// unrecognized key. Fail but do not abort to promote backward compatibility down the road
	secdebug("csinterp", "cert field notation \"%s\" not understood", key.c_str());
	return false;
}


bool Requirement::Interpreter::certFieldGeneric(const string &key, const Match &match, SecCertificateRef cert)
{
	// the key is actually a (binary) OID value
	CssmOid oid((char *)key.data(), key.length());
	return certFieldGeneric(oid, match, cert);
}

bool Requirement::Interpreter::certFieldGeneric(const CssmOid &oid, const Match &match, SecCertificateRef cert)
{
	return cert && certificateHasField(cert, oid) && match(kCFBooleanTrue);
}


//
// Check the Apple-signed condition
//
bool Requirement::Interpreter::appleAnchored()
{
	if (SecCertificateRef cert = mContext->cert(anchorCert))
		if (verifyAnchor(cert, appleAnchorHash())
#if defined(TEST_APPLE_ANCHOR)
			|| verifyAnchor(cert, testAppleAnchorHash())
#endif
		)
		return true;
	return false;
}

bool Requirement::Interpreter::appleSigned()
{
	if (appleAnchored())
			if (SecCertificateRef intermed = mContext->cert(-2))	// first intermediate
				// first intermediate common name match (exact)
				if (certFieldValue("subject.CN", Match(appleIntermediateCN, matchEqual), intermed)
					&& certFieldValue("subject.O", Match(appleIntermediateO, matchEqual), intermed))
					return true;
	return false;
}


//
// Verify an anchor requirement against the context
//
bool Requirement::Interpreter::verifyAnchor(SecCertificateRef cert, const unsigned char *digest)
{
	// get certificate bytes
	if (cert) {
		CSSM_DATA certData;
		MacOSError::check(SecCertificateGetData(cert, &certData));
		
		// verify hash
		//@@@ should get SHA1(cert(-1).data) precalculated during chain verification
		SHA1 hasher;
		hasher(certData.Data, certData.Length);
		return hasher.verify(digest);
	}
	return false;
}


//
// Check one or all certificate(s) in the cert chain against the Trust Settings database.
//
bool Requirement::Interpreter::trustedCerts()
{
	int anchor = mContext->certCount() - 1;
	for (int slot = 0; slot <= anchor; slot++)
		if (SecCertificateRef cert = mContext->cert(slot))
			switch (trustSetting(cert, slot == anchor)) {
			case kSecTrustSettingsResultTrustRoot:
			case kSecTrustSettingsResultTrustAsRoot:
				return true;
			case kSecTrustSettingsResultDeny:
				return false;
			case kSecTrustSettingsResultUnspecified:
				break;
			default:
				assert(false);
				return false;
			}
		else
			return false;
	return false;
}

bool Requirement::Interpreter::trustedCert(int slot)
{
	if (SecCertificateRef cert = mContext->cert(slot)) {
		int anchorSlot = mContext->certCount() - 1;
		switch (trustSetting(cert, slot == anchorCert || slot == anchorSlot)) {
		case kSecTrustSettingsResultTrustRoot:
		case kSecTrustSettingsResultTrustAsRoot:
			return true;
		case kSecTrustSettingsResultDeny:
		case kSecTrustSettingsResultUnspecified:
			return false;
		default:
			assert(false);
			return false;
		}
	} else
		return false;
}


//
// Explicitly check one certificate against the Trust Settings database and report
// the findings. This is a helper for the various Trust Settings evaluators.
//
SecTrustSettingsResult Requirement::Interpreter::trustSetting(SecCertificateRef cert, bool isAnchor)
{
	// the SPI input is the uppercase hex form of the SHA-1 of the certificate...
	assert(cert);
	SHA1::Digest digest;
	hashOfCertificate(cert, digest);
	string Certhex = CssmData(digest, sizeof(digest)).toHex();
	for (string::iterator it = Certhex.begin(); it != Certhex.end(); ++it)
		if (islower(*it))
			*it = toupper(*it);
	
	// call Trust Settings and see what it finds
	SecTrustSettingsDomain domain;
	SecTrustSettingsResult result;
	CSSM_RETURN *errors = NULL;
	uint32 errorCount = 0;
	bool foundMatch, foundAny;
	switch (OSStatus rc = SecTrustSettingsEvaluateCert(
		CFTempString(Certhex),					// settings index
		&CSSMOID_APPLE_TP_CODE_SIGNING,			// standard code signing policy
		NULL, 0,								// policy string (unused)
		kSecTrustSettingsKeyUseAny,				// no restriction on key usage @@@
		isAnchor,								// consult system default anchor set

		&domain,								// domain of found setting
		&errors, &errorCount,					// error set and maximum count
		&result,								// the actual setting
		&foundMatch, &foundAny					// optimization hints (not used)
		)) {
	case noErr:
		::free(errors);
		if (foundMatch)
			return result;
		else
			return kSecTrustSettingsResultUnspecified;
	default:
		::free(errors);
		MacOSError::throwMe(rc);
	}
}


//
// Create a Match object from the interpreter stream
//
Requirement::Interpreter::Match::Match(Interpreter &interp)
{
	switch (mOp = interp.get<MatchOperation>()) {
	case matchExists:
		break;
	case matchEqual:
	case matchContains:
	case matchBeginsWith:
	case matchEndsWith:
	case matchLessThan:
	case matchGreaterThan:
	case matchLessEqual:
	case matchGreaterEqual:
		mValue = makeCFString(interp.getString());
		break;
	default:
		// Assume this (unknown) match type has a single data argument.
		// This gives us a chance to keep the instruction stream aligned.
		interp.getString();			// discard
		break;
	}
}


//
// Execute a match against a candidate value
//
bool Requirement::Interpreter::Match::operator () (CFTypeRef candidate) const
{
	// null candidates always fail
	if (!candidate)
		return false;

	// interpret an array as matching alternatives (any one succeeds)
	if (CFGetTypeID(candidate) == CFArrayGetTypeID()) {
		CFArrayRef array = CFArrayRef(candidate);
		CFIndex count = CFArrayGetCount(array);
		for (CFIndex n = 0; n < count; n++)
			if ((*this)(CFArrayGetValueAtIndex(array, n)))	// yes, it's recursive
				return true;
	}

	switch (mOp) {
	case matchExists:		// anything but NULL and boolean false "exists"
		return !CFEqual(candidate, kCFBooleanFalse);
	case matchEqual:		// equality works for all CF types
		return CFEqual(candidate, mValue);
	case matchContains:
		if (CFGetTypeID(candidate) == CFStringGetTypeID()) {
			CFStringRef value = CFStringRef(candidate);
			if (CFStringFindWithOptions(value, mValue, CFRangeMake(0, CFStringGetLength(value)), 0, NULL))
				return true;
		}
		return false;
	case matchBeginsWith:
		if (CFGetTypeID(candidate) == CFStringGetTypeID()) {
			CFStringRef value = CFStringRef(candidate);
			if (CFStringFindWithOptions(value, mValue, CFRangeMake(0, CFStringGetLength(mValue)), 0, NULL))
				return true;
		}
		return false;
	case matchEndsWith:
		if (CFGetTypeID(candidate) == CFStringGetTypeID()) {
			CFStringRef value = CFStringRef(candidate);
			CFIndex matchLength = CFStringGetLength(mValue);
			CFIndex start = CFStringGetLength(value) - matchLength;
			if (start >= 0)
				if (CFStringFindWithOptions(value, mValue, CFRangeMake(start, matchLength), 0, NULL))
					return true;
		}
		return false;
	case matchLessThan:
		return inequality(candidate, kCFCompareNumerically, kCFCompareLessThan, true);
	case matchGreaterThan:
		return inequality(candidate, kCFCompareNumerically, kCFCompareGreaterThan, true);
	case matchLessEqual:
		return inequality(candidate, kCFCompareNumerically, kCFCompareGreaterThan, false);
	case matchGreaterEqual:
		return inequality(candidate, kCFCompareNumerically, kCFCompareLessThan, false);
	default:
		// unrecognized match types can never match
		return false;
	}
}


bool Requirement::Interpreter::Match::inequality(CFTypeRef candidate, CFStringCompareFlags flags,
	CFComparisonResult outcome, bool negate) const
{
	if (CFGetTypeID(candidate) == CFStringGetTypeID()) {
		CFStringRef value = CFStringRef(candidate);
		if ((CFStringCompare(value, mValue, flags) == outcome) == negate)
			return true;
	}
	return false;
}


}	// CodeSigning
}	// Security
