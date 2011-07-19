/*
 * Copyright (c) 2010-2011 Apple Inc. All rights reserved.
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

#ifndef _EAP8021X_EAPOLCLIENTCONFIGURATIONPRIVATE_H
#define _EAP8021X_EAPOLCLIENTCONFIGURATIONPRIVATE_H

#include <EAP8021X/EAPOLClientConfiguration.h>
#include <CoreFoundation/CFRuntime.h>
#include <SystemConfiguration/SCPreferences.h>

/*
 * EAPOLClientConfigurationPrivate.h
 * - EAPOL client configuration private functions
 */

/* 
 * Modification History
 *
 * January 5, 2009	Dieter Siegmund (dieter@apple.com)
 * - created
 */

/**
 ** EAPOLClientItemID
 **/
CFStringRef
EAPOLClientItemIDGetProfileID(EAPOLClientItemIDRef itemID);

CFDataRef
EAPOLClientItemIDGetWLANSSID(EAPOLClientItemIDRef itemID);

EAPOLClientProfileRef
EAPOLClientItemIDGetProfile(EAPOLClientItemIDRef itemID);

/*
 * Function: EAPOLClientItemIDCopyDictionary
 *           EAPOLClientItemIDCreateWithDictionary
 * Purpose:
 *   EAPOLClientItemIDCopyDictionary() creates an externalized form of the
 *   EAPOLClientItemIDRef that can be passed (after serialization) to another
 *   process.   The other process turns it back into an EAPOLClientItemIDRef
 *   by calling EAPOLClientItemIDCreateWithDictionary().
 */
CFDictionaryRef
EAPOLClientItemIDCopyDictionary(EAPOLClientItemIDRef itemID);

EAPOLClientItemIDRef
EAPOLClientItemIDCreateWithDictionary(EAPOLClientConfigurationRef cfg,
				      CFDictionaryRef dict);


OSStatus
EAPOLClientSetACLForIdentity(SecIdentityRef identity);

#endif /* _EAP8021X_EAPOLCLIENTCONFIGURATIONPRIVATE_H */
