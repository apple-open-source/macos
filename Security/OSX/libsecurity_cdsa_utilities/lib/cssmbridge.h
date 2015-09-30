/*
 * Copyright (c) 2000-2001,2003-2004,2006,2011,2014 Apple Inc. All Rights Reserved.
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
// CSSM-style C/C++ bridge facilities
//
#ifndef _H_CSSMBRIDGE
#define _H_CSSMBRIDGE

#include <security_utilities/utilities.h>
#include <security_cdsa_utilities/cssmerrors.h>
#include <Security/cssm.h>


namespace Security {


//
// API boilerplate macros. These provide a frame for C++ code that is impermeable to exceptions.
// Usage:
//	BEGIN_API
//		... your C++ code here ...
//  END_API(base)	// returns CSSM_RETURN on exception; complete it to 'base' (DL, etc.) class;
//					// returns CSSM_OK on fall-through
//	END_API0		// completely ignores exceptions; falls through in all cases
//	END_API1(bad)	// return (bad) on exception; fall through on success
//
#define BEGIN_API	try {
#define END_API(base) 	} \
catch (const CommonError &err) { return CssmError::cssmError(err, CSSM_ ## base ## _BASE_ERROR); } \
catch (const std::bad_alloc &) { return CssmError::cssmError(CSSM_ERRCODE_MEMORY_ERROR, CSSM_ ## base ## _BASE_ERROR); } \
catch (...) { return CssmError::cssmError(CSSM_ERRCODE_INTERNAL_ERROR, CSSM_ ## base ## _BASE_ERROR); } \
    return CSSM_OK;
#define END_API0		} catch (...) { return; }
#define END_API1(bad)	} catch (...) { return bad; }


} // end namespace Security


#endif //_H_CSSMBRIDGE
