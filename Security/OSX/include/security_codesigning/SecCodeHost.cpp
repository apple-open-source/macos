/*
 * Copyright (c) 2006-2007,2011 Apple Inc. All Rights Reserved.
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
// SecCodeHost - Host Code API
//
#include "cs.h"
#include "SecCodeHost.h"
#include <security_utilities/cfutilities.h>
#include <security_utilities/globalizer.h>
#include <securityd_client/ssclient.h>

using namespace CodeSigning;


OSStatus SecHostCreateGuest(SecGuestRef host,
	uint32_t status, CFURLRef path, CFDictionaryRef attributes,
	SecCSFlags flags, SecGuestRef *newGuest)
{
	BEGIN_CSAPI
	
	MacOSError::throwMe(errSecCSNotSupported);
	
	END_CSAPI
}

OSStatus SecHostRemoveGuest(SecGuestRef host, SecGuestRef guest, SecCSFlags flags)
{
	BEGIN_CSAPI

	MacOSError::throwMe(errSecCSNotSupported);

	END_CSAPI
}

OSStatus SecHostSelectGuest(SecGuestRef guestRef, SecCSFlags flags)
{
	BEGIN_CSAPI

	MacOSError::throwMe(errSecCSNotSupported);

	END_CSAPI
}


OSStatus SecHostSelectedGuest(SecCSFlags flags, SecGuestRef *guestRef)
{
	BEGIN_CSAPI
	
	MacOSError::throwMe(errSecCSNotSupported);

	END_CSAPI
}

OSStatus SecHostSetGuestStatus(SecGuestRef guestRef,
	uint32_t status, CFDictionaryRef attributes,
	SecCSFlags flags)
{
	BEGIN_CSAPI

	MacOSError::throwMe(errSecCSNotSupported);

	END_CSAPI
}

OSStatus SecHostSetHostingPort(mach_port_t hostingPort, SecCSFlags flags)
{
	BEGIN_CSAPI

	MacOSError::throwMe(errSecCSNotSupported);

	END_CSAPI
}
