/*
 * Copyright (c) 2022 Apple Inc. All Rights Reserved.
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
// CodeRemoteSigner - SecCodeRemoteSigner API objects
//
#include "CodeSignerRemote.h"
#include "signer.h"
#include "csdatabase.h"
#include "drmaker.h"
#include "csutilities.h"
#include <security_utilities/unix++.h>
#include <security_utilities/unixchild.h>
#include <Security/SecCertificate.h>
#include <Security/SecCertificatePriv.h>
#include <Security/SecPolicy.h>
#include <Security/SecPolicyPriv.h>
#include <libDER/oids.h>
#include <vector>
#include <errno.h>

namespace Security {

namespace CodeSigning {

using namespace UnixPlusPlus;

//
// Construct a SecCodeSignerRemote
//
SecCodeSignerRemote::SecCodeSignerRemote(SecCSFlags flags, CFArrayRef certificateChain)
: SecCodeSigner(flags), mCertificateChain(NULL)
{
	// Set here vs the initializer to ensure we take a reference.
	mCertificateChain = certificateChain;
}

//
// Clean up a SecCodeSignerRemote
//
SecCodeSignerRemote::~SecCodeSignerRemote() _NOEXCEPT
{
}

bool
SecCodeSignerRemote::valid() const
{
	bool isValid = true;

	// Must have a certificate chain that is a valid array of at least one certificate.
	bool arrayExists = mCertificateChain.get() != NULL;
	bool arrayHasItems = false;
	bool arrayHasCorrectItems = true;

	if (arrayExists) {
		CFIndex count = CFArrayGetCount(mCertificateChain.get());
		arrayHasItems = count > 0;

		if (arrayHasItems) {
			for (CFIndex i = 0; i < count; i++) {
				CFTypeRef obj = CFArrayGetValueAtIndex(mCertificateChain.get(), i);
				if (SecCertificateGetTypeID() != CFGetTypeID(obj)) {
					arrayHasCorrectItems = false;
					break;
				}
			}
		}
	}

	isValid = arrayExists && arrayHasItems && arrayHasCorrectItems;
	if (!isValid) {
		secerror("Invalid remote signing operation: %p, %@", this, mCertificateChain.get());
	}
	return isValid;
}

void
SecCodeSignerRemote::sign(SecStaticCode *code, SecCSFlags flags, SecCodeRemoteSignHandler handler)
{
	// Never preserve a linker signature.
	if (code->isSigned() &&
		(flags & kSecCSSignPreserveSignature) &&
		!code->flag(kSecCodeSignatureLinkerSigned)) {
		return;
	}

	secinfo("remotesigner", "%p will start remote signature from %p with block %p", this, code, handler);

	code->setValidationFlags(flags);
	Signer operation(*this, code);

	if (!valid()) {
		secerror("Invalid signing operation, bailing.");
		MacOSError::throwMe(errSecCSInvalidObjectRef);
	}
	secinfo("remotesigner", "%p will sign %p (flags 0x%x) with certs: %@", this, code, flags, mCertificateChain.get());
	operation.setupRemoteSigning(mCertificateChain, handler);
	operation.sign(flags);
	code->resetValidity();
}


} // end namespace CodeSigning
} // end namespace Security
