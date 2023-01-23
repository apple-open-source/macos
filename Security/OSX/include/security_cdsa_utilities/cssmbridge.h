/*
 * Copyright (c) 2000-2001,2003-2004,2006,2011,2014,2021 Apple Inc. All Rights Reserved.
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
#define BEGIN_API \
	CSSM_RETURN __attribute__((unused)) __retval = CSSM_OK;	  \
	bool __countlegacyapi = countLegacyAPIEnabledForThread(); \
	static dispatch_once_t countToken; \
	countLegacyAPI(&countToken, __FUNCTION__); \
	setCountLegacyAPIEnabledForThread(false); \
	try {

#define BEGIN_API_NO_METRICS \
	CSSM_RETURN __attribute__((unused)) __retval = CSSM_OK;	\
	try {

#define END_API(base)	} \
	catch (const CommonError &err) { __retval = CssmError::cssmError(err, CSSM_ ## base ## _BASE_ERROR); } \
	catch (const std::bad_alloc &) { __retval = CssmError::cssmError(CSSM_ERRCODE_MEMORY_ERROR, CSSM_ ## base ## _BASE_ERROR); } \
	catch (...) { __retval = CssmError::cssmError(CSSM_ERRCODE_INTERNAL_ERROR, CSSM_ ## base ## _BASE_ERROR); } \
	setCountLegacyAPIEnabledForThread(__countlegacyapi); \
	return __retval;
#define END_API0		} \
	catch (...) {} \
	setCountLegacyAPIEnabledForThread(__countlegacyapi); \
	return;
#define END_API1(bad)	} \
	catch (...) { __retval = bad; } \
	setCountLegacyAPIEnabledForThread(__countlegacyapi); \
	return __retval;

#define END_API_NO_METRICS(base)	} \
	catch (const CommonError &err) { __retval = CssmError::cssmError(err, CSSM_ ## base ## _BASE_ERROR); } \
	catch (const std::bad_alloc &) { __retval = CssmError::cssmError(CSSM_ERRCODE_MEMORY_ERROR, CSSM_ ## base ## _BASE_ERROR); } \
	catch (...) { __retval = CssmError::cssmError(CSSM_ERRCODE_INTERNAL_ERROR, CSSM_ ## base ## _BASE_ERROR); } \
	return __retval;
#define END_API0_NO_METRICS		} \
	catch (...) {} \
	return;
#define END_API1_NO_METRICS(bad)	} \
	catch (...) {} \
	return bad;

} // end namespace Security


#endif //_H_CSSMBRIDGE
