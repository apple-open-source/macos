/*
 * Copyright (c) 2004,2006 Apple Computer, Inc. All Rights Reserved.
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
// tokenaccess - access management to a TokenDatabase's Token's TokenDaemon's tokend
//
#include "tokenaccess.h"


//
// Process an exception thrown (presumably) by a TokenDaemon interface call.
//
void Access::operator () (const CssmError &err)
{
	if (++mIteration > 2) {
		secinfo("tokendb", "retry failed; aborting operation");
		throw;
	}
	
	//@@@ hack until tokend returns RESET
	if (err.error == -1) {
		secinfo("tokendb", "TEMP HACK (error -1) action - reset and retry");
		token.resetAcls();
		return;
	}
	
	if (CSSM_ERR_IS_CONVERTIBLE(err.error))
		switch (CSSM_ERRCODE(err.error)) {
		case CSSM_ERRCODE_OPERATION_AUTH_DENIED:
		case CSSM_ERRCODE_OBJECT_USE_AUTH_DENIED:
			// @@@ do something more focused here, but for now...
			secinfo("tokendb", "tokend denies auth; we're punting for now");
			throw;
		case CSSM_ERRCODE_DEVICE_RESET:
			secinfo("tokendb", "tokend signals reset; clearing and retrying");
			token.resetAcls();
			return;	// induce retry
		}
	// all others are non-recoverable
	secinfo("tokendb", "non-recoverable error in Access(): %d", err.error);
	throw;
}
