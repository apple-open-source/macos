/*
 * Copyright (c) 2010 Apple Inc. All rights reserved.
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

#ifndef _EAP8021X_EAPOLCONTROLPRIVATE_H
#define _EAP8021X_EAPOLCONTROLPRIVATE_H

/* 
 * Modification History
 *
 * September 3, 2010	Dieter Siegmund (dieter@apple.com)
 * - created
 */

/*
 * EAPOLControlPrivate.h
 * - EAPOLControl private definitions
 */

#include <CoreFoundation/CFString.h>
#include <CoreFoundation/CFDictionary.h>
#include <TargetConditionals.h>

#if ! TARGET_OS_EMBEDDED

extern const CFStringRef	kEAPOLAutoDetectSecondsSinceLastPacket; /* CFNumber */
extern const CFStringRef	kEAPOLAutoDetectAuthenticatorMACAddress; /* CFDataRef */

/*
 * Key: kEAPOLControlAutoDetectInformationNotifyKey
 * Purpose:
 *   SCDynamicStore notify key posted by the 802.1X auto-detection code
 *   whenever the auto-detect information has changed.
 */
extern const CFStringRef	kEAPOLControlAutoDetectInformationNotifyKey;

/*
 * Function: EAPOLControlCopyAutoDetectInformation
 *
 * Purpose:
 *   Returns a dictionary of (key, value) pairs.  The key is the interface
 *   name, the value is the number of seconds since having received the last
 *   802.1X packet.
 *
 * Returns:
 *   0 if successful, and *info_p contains a non-NULL CFDictionaryRef that
 *   must be released
 *   non-zero errno value otherwise, and *info_p is set to NULL.
 */
int
EAPOLControlCopyAutoDetectInformation(CFDictionaryRef * info_p);

extern const CFStringRef
kEAPOLControlStartOptionManagerName; /* CFStringRef */

extern const CFStringRef
kEAPOLControlStartOptionAuthenticationInfo; /* CFDictionary */

/*
 * Function: EAPOLControlStartWithOptions
 *
 * Purpose:
 *    Start an authentication session with the provided options, which may
 *    be NULL.
 */
int
EAPOLControlStartWithOptions(const char * if_name,
			     EAPOLClientItemIDRef itemID,
			     CFDictionaryRef options);

/*
 * Function: EAPOLControlCopyItemIDForAuthenticator
 * Purpose:
 *   Return the binding for the current user for the specified
 *   Authenticator.
 */
EAPOLClientItemIDRef
EAPOLControlCopyItemIDForAuthenticator(CFDataRef authenticator);

/*
 * Function: EAPOLControlSetItemIDForAuthenticator
 * Purpose:
 *   Set the binding for the current user for the specified
 *   Authenticator.
 *  
 *   Supplying an 'itemID' with value NULL clears the binding.
 */
void
EAPOLControlSetItemIDForAuthenticator(CFDataRef authenticator,
				      EAPOLClientItemIDRef itemID);


/*
 * Function: EAPOLControlIsUserAutoConnectVerboseEnabled
 * Purpose:
 *   Get whether verbose mode is enabled.
 */
Boolean
EAPOLControlIsUserAutoConnectVerboseEnabled(void);

/*
 * Function: EAPOLControlSetUserAutoConnectVerboseEnabled
 * Purpose:
 *   Set whether verbose mode is enabled.
 */
void
EAPOLControlSetUserAutoConnectVerboseEnabled(Boolean enable);

/*
 * Const: kEAPOLControlUserSettingsNotifyKey
 * Purpose:
 *   Notification key used with BSD notify(3) to let know whether user
 *   EAPOLControl settings have been modified.
 */
extern const char * kEAPOLControlUserSettingsNotifyKey;


#endif /* ! TARGET_OS_EMBEDDED */

#endif /* _EAP8021X_EAPOLCONTROLPRIVATE_H */
