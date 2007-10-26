/*
 * Copyright (c) 2005 Apple Computer, Inc. All Rights Reserved.
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

/*
 * TrustSettingsUtils.h - Utility routines for TrustSettings module
 */
 
#ifndef	_TRUST_SETTINGS_UTILS_H_
#define _TRUST_SETTINGS_UTILS_H_

#include <security_keychain/TrustSettings.h>
#include <security_keychain/SecTrustSettingsPriv.h>
#include <security_utilities/alloc.h>
#include <string>
#include <CoreFoundation/CoreFoundation.h>
#include <CoreServices/../Frameworks/CarbonCore.framework/Headers/MacErrors.h>

#define CFRELEASE(cf)		if(cf) { CFRelease(cf); }

#define TS_REQUIRED(arg)	if(arg == NULL) { return paramErr; }

namespace Security
{

namespace KeychainCore
{

/* Read entire file. */
int tsReadFile(
	const char		*fileName,
	Allocator		&alloc,
	CSSM_DATA		&fileData);		// mallocd via alloc and RETURNED

} /* end namespace KeychainCore */

} /* end namespace Security */

#endif	/* _TRUST_SETTINGS_UTILS_H_ */
