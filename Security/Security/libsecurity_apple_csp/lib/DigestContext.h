/*
 * Copyright (c) 2000-2001,2011,2014 Apple Inc. All Rights Reserved.
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
// DigestContext.h 
//

#ifndef	_DIGEST_CONTEXT_H_
#define _DIGEST_CONTEXT_H_

#include <security_cdsa_utilities/digestobject.h>
#include "AppleCSPContext.h"

/*
 * This is just a shim to give AppleCSPContext functionality to a 
 * DigestObject subclass (a reference to which is passed to our constructor).
 */
class DigestContext : public AppleCSPContext  {
public:
	DigestContext(
		AppleCSPSession &session,
		DigestObject &digest) : 
			AppleCSPContext(session), mDigest(digest) { }
	~DigestContext() { delete &mDigest; }
	
	void init(const Context &context, bool);
	void update(const CssmData &data);
	void final(CssmData &data);
	CSPFullPluginSession::CSPContext *clone(Allocator &);	// clone internal state
	size_t outputSize(bool, size_t);

private:
	DigestObject	&mDigest;
};

#endif	/* _CRYPTKIT_DIGEST_CONTEXT_H_ */
