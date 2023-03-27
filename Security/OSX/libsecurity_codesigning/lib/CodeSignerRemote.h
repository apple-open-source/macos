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
// CodeSignerRemote - SecCodeSignerRemote API objects
//
#ifndef _H_CODESIGNERREMOTE
#define _H_CODESIGNERREMOTE

#include "cs.h"
#include "StaticCode.h"
#include "cdbuilder.h"
#include <Security/SecIdentity.h>
#include <security_utilities/utilities.h>
#include "SecCodeSigner.h"
#include "CodeSigner.h"

namespace Security {
namespace CodeSigning {


//
// SecCodeSigner is responsible for signing code objects
//
class SecCodeSignerRemote : public SecCodeSigner {
	NOCOPY(SecCodeSignerRemote)

public:
	SECCFFUNCTIONS(SecCodeSignerRemote, SecCodeSignerRemoteRef, errSecCSInvalidObjectRef, gCFObjects().CodeSignerRemote)

	SecCodeSignerRemote(SecCSFlags flags, CFArrayRef certificateChain);

	virtual bool valid() const;
	void sign(SecStaticCode *code, SecCSFlags flags, SecCodeRemoteSignHandler handler);

	virtual ~SecCodeSignerRemote() _NOEXCEPT;

public:
	SecCodeRemoteSignHandler mSignHandler;
	CFRef<CFArrayRef> mCertificateChain;
};


} // end namespace CodeSigning
} // end namespace Security

#endif // !_H_CODESIGNERREMOTE
