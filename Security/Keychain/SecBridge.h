/*
 * Copyright (c) 2000-2002 Apple Computer, Inc. All Rights Reserved.
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

#ifndef _SECURITY_SECBRIDGE_H_
#define _SECURITY_SECBRIDGE_H_

#include <Security/Globals.h>
#include <Security/KCUtilities.h>
#include <Security/SecCFTypes.h>

using namespace KeychainCore;

//
// API boilerplate macros. These provide a frame for C++ code that is impermeable to exceptions.
// Usage:
//	BEGIN_API
//		... your C++ code here ...
//  END_API		// returns CSSM_RETURN on exception
//	END_API0	// returns nothing (void) on exception
//	END_API1(bad) // return (bad) on exception
//
#define BEGIN_SECAPI \
	try { \
		StLock<Mutex> _(globals().apiLock);
#define END_SECAPI \
	} \
	catch (const MacOSError &err) { return err.osStatus(); } \
	catch (const CssmCommonError &err) { return GetKeychainErrFromCSSMErr(err.cssmError())/*err.cssmError(CSSM_CSSM_BASE_ERROR)*/; } \
	catch (const std::bad_alloc &) { return memFullErr; } \
	catch (...) { return internalComponentErr; } \
    return noErr;
#define END_SECAPI0		} catch (...) { return; }
#define END_SECAPI1(bad)	} catch (...) { return bad; }

#endif /* !_SECURITY_SECBRIDGE_H_ */
