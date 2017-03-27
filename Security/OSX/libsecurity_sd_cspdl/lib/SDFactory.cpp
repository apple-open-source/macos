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
 */


//
// SDFactory -- The factory for Security Server context objects
//
#include "SDFactory.h"

#include "SDContext.h"


//
// SDFactory -- The factory for Security Server context objects
//
bool SDFactory::setup(SDCSPSession &session, CSPFullPluginSession::CSPContext * &cspCtx,
					  const Context &context, bool encoding)
{
	if (cspCtx)
		return false;	// not ours or already set

	switch (context.type())
	{
	case CSSM_ALGCLASS_SIGNATURE:
		cspCtx = new SDSignatureContext(session);
		return true;
	case CSSM_ALGCLASS_MAC:
		cspCtx = new SDMACContext(session);
		return true;
	case CSSM_ALGCLASS_DIGEST:
		cspCtx = new SDDigestContext(session);
		return true;
	case CSSM_ALGCLASS_SYMMETRIC:
	case CSSM_ALGCLASS_ASYMMETRIC:
		cspCtx = new SDCryptContext(session); // @@@ Could also be wrap/unwrap
		return true;
	case CSSM_ALGCLASS_RANDOMGEN:
		cspCtx = new SDRandomContext(session); // @@@ Should go.
		return true;
	}

	return false;

#if 0
	/* FIXME - qualify by ALGCLASS as well to avoid MAC */
	switch (context.algorithm()) {
	case CSSM_ALGID_MD5:
		cspCtx = new MD5Context(session);
		return true;
	case CSSM_ALGID_SHA1:
		cspCtx = new SHA1Context(session);
		return true;
	}
	return false;

    if (ctx)
        CssmError::throwMe(CSSM_ERRCODE_INTERNAL_ERROR);	// won't support re-definition
    switch (context.algorithm()) {
        case CSSM_ALGID_ROTTY_ROT_16:
            ctx = new SDContext(16);
            return true;
        case CSSM_ALGID_ROTTY_ROT_37:
            ctx = new SDContext(37);
            return true;
    }
#endif
}
