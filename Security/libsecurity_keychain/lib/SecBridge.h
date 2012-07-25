/*
 * Copyright (c) 2000-2004 Apple Computer, Inc. All Rights Reserved.
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

#ifndef _SECURITY_SECBRIDGE_H_
#define _SECURITY_SECBRIDGE_H_

#include <security_keychain/Globals.h>
#include <security_keychain/SecCFTypes.h>
#include "SecBasePriv.h"
#include <Security/SecKeychainPriv.h>
#include <security_keychain/KCUtilities.h>
#include <security_cdsa_utilities/cssmbridge.h>

using namespace KeychainCore;

//
// API boilerplate macros. These provide a frame for C++ code that is impermeable to exceptions.
// Usage:
//	BEGIN_API
//		... your C++ code here ...
//  END_API		// returns CSSM_RETURN on exception
//	END_API0	// returns nothing (void) on exception
//	END_API1(bad) // return (bad) on exception
//	END_API2(name) // like END_API, with API name as debug scope for printing function result
//	END_API3(name, bad) // like END_API1, with API name as debug scope for printing function result
//
#define BEGIN_SECAPI \
    OSStatus __secapiresult = noErr; \
	try {
#define END_SECAPI }\
	catch (const MacOSError &err) { __secapiresult=err.osStatus(); } \
	catch (const CommonError &err) { __secapiresult=SecKeychainErrFromOSStatus(err.osStatus()); } \
	catch (const std::bad_alloc &) { __secapiresult=memFullErr; } \
	catch (...) { __secapiresult=internalComponentErr; } \
    return __secapiresult;
#define END_SECAPI1(BAD_RETURN_VAL) \
	} \
	catch (...) \
	{ \
		__secapiresult=BAD_RETURN_VAL; \
	} \
	return __secapiresult;
#define END_SECAPI1(BAD_RETURN_VAL) }\
    catch (...) { __secapiresult=BAD_RETURN_VAL; } \
    return __secapiresult;
#define END_SECAPI0 }\
    catch (...) { return; }

#endif /* !_SECURITY_SECBRIDGE_H_ */
