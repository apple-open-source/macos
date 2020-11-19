/*
 * Copyright (c) 2002-2019 Apple Inc. All rights reserved.
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

#ifndef _EAP8021X_EAPOLCONTROL_H
#define _EAP8021X_EAPOLCONTROL_H

#include <TargetConditionals.h>
#include <os/availability.h>
#include <CoreFoundation/CFDictionary.h>
#include <CoreFoundation/CFString.h>
#include <EAP8021X/EAPOLControlTypes.h>
#if TARGET_OS_OSX
#include <EAP8021X/EAPOLClientConfiguration.h>
#else /* TARGET_OS_OSX */
typedef struct __EAPOLClientItemID * EAPOLClientItemIDRef;
#endif /* TARGET_OS_OSX */

/*
 * Function: EAPOLControlKeyCreate
 * Returns:
 *   The DynamicStore key to use to retrieve information for the given
 *   interface, and/or use for notification for a specific interface.
 */
CFStringRef
EAPOLControlKeyCreate(const char * interface_name) API_AVAILABLE(macos(10.10), ios(8.0), watchos(5.0), tvos(9.0));

/*
 * Function: EAPOLControlAnyInterfaceKeyCreate
 * Returns:
 *   The DynamicStore key to use to register for notifications on 
 *   any interface's EAPOL information changing.
 */
CFStringRef
EAPOLControlAnyInterfaceKeyCreate(void) API_AVAILABLE(macos(10.10), ios(8.0), watchos(5.0), tvos(9.0));

/*
 * Function: EAPOLControlKeyCopyInterface
 * Returns:
 *   The interface associated with the given EAPOL interface key,
 *   NULL if the key is invalid.
 */
CFStringRef
EAPOLControlKeyCopyInterface(CFStringRef key) API_AVAILABLE(macos(10.10), ios(8.0), watchos(5.0), tvos(9.0));


/*
 * Function: EAPOLControlStart
 *
 * Purpose:
 *   Start an authentication session on the specified interface.
 *
 *   The config dictionary has the following format:
 *     kEAPOLControlEAPClientConfiguration  authentication properties
 *     kEAPOLControlUniqueIdentifier	    unique string for session (optional)
 *     kEAPOLControlLogLevel		    log level (optional)
 *
 *   See also <EAP8021X/EAPOLControlTypes.h>.
 *
 * Returns:
 *   0 if successful, non-zero errno value otherwise.
 * Note:
 *   On the Mac, EAPOLControlStartWithClientItemID() is the preferred way to
 *   start the authentication session.   This function remains for
 *   backwards compatibility.
 */
int
EAPOLControlStart(const char * interface_name, CFDictionaryRef config) API_AVAILABLE(macos(10.10), ios(8.0), watchos(5.0), tvos(9.0));

/*
 * Function: EAPOLControlStartWithClientItemID
 *
 * Purpose:
 *   Start an authentication session over the specified interface using
 *   the specified EAPOLClientItemIDRef.
 *   
 *   The 'auth_info' dictionary may be NULL.   If it is not NULL, it may
 *   contain any of the following properties:
 *     kEAPClientPropUserName
 *     kEAPClientPropUserPassword
 *     kEAPClientPropTLSIdentityHandle
 *     kEAPClientPropSaveCredentialsOnSuccessfulAuthentication
 *   If properties not in this list are specified in 'auth_info", EINVAL
 *   is returned.
 *   
 * Returns:
 *   0 if successful, non-zero errno value otherwise.
 */
int
EAPOLControlStartWithClientItemID(const char * if_name,
				  EAPOLClientItemIDRef itemID,
				  CFDictionaryRef auth_info) API_AVAILABLE(macos(10.10)) API_UNAVAILABLE(ios, watchos, tvos);

/*
 * Function: EAPOLControlUpdate
 *
 * Purpose:
 *   Update the configuration used by the authentication session on
 *   the specified interface.
 *
 * Returns:
 *   0 if successful, non-zero errno value otherwise.
 */
int
EAPOLControlUpdate(const char * interface_name, CFDictionaryRef config) API_AVAILABLE(macos(10.10), ios(8.0), watchos(5.0), tvos(9.0));

/*
 * Function: EAPOLControlStop
 *
 * Purpose:
 *    Terminate the authentication session over the specified interface.
 * Returns:
 *   0 if successful, non-zero errno value otherwise.
 */
int
EAPOLControlStop(const char * interface_name) API_AVAILABLE(macos(10.10), ios(8.0), watchos(5.0), tvos(9.0));

/*
 * Function: EAPOLControlRetry
 *
 * Purpose:
 *    Forces the authentication client to send an EAPOL Start
 *    packet which usually has the effect of triggering a re-authentication
 *
 * Returns:
 *   0 if successful, non-zero errno value otherwise.
 *
 * Note:
 *   This function should not be called by production code, it's there for
 *   testing/debugging.
 */
int
EAPOLControlRetry(const char * interface_name) API_AVAILABLE(macos(10.10), ios(8.0), watchos(5.0), tvos(9.0));

/*
 * Function: EAPOLControlProvideUserInput
 *
 * Purpose:
 *   Tell the EAP client that the user has provided input and/or changed
 *   something in the environment that would allow the authentication to
 *   continue e.g. modified trust settings.
 *
 * Arguments:
 *   interface_name	name of the BSD interface
 *   user_input		If ! NULL, contains keys/values
 *			corresponding to the additional user input.
 *                      The keys/values are merged into the
 *                      client configuration dictionary.
 *
 *                      If NULL, simply means that the user has changed
 *       		something in the environment, and the EAP client
 *			should try to continue the authentication.
 */
int
EAPOLControlProvideUserInput(const char * interface_name,
			     CFDictionaryRef user_input) API_AVAILABLE(macos(10.10), ios(8.0), watchos(5.0), tvos(9.0));

/* 
 * Function: EAPOLControlCopyStateAndStatus
 *
 * Purpose:
 *   Return the current EAPOLControl state (Idle, Starting, Running, Stopping)
 *   and the current authentication status.
 *
 * Returns:
 *   0 if successful, *status_dict_p may be non-NULL,
 *   non-zero errno value otherwise.
 */
int
EAPOLControlCopyStateAndStatus(const char * interface_name, 
			       EAPOLControlState * state,
			       CFDictionaryRef * status_dict_p) API_AVAILABLE(macos(10.10), ios(8.0), watchos(5.0), tvos(9.0));

/*
 * Function: EAPOLControlSetLogLevel
 * Purpose:
 *   Deprecated.
 */
int
EAPOLControlSetLogLevel(const char * interface_name, int32_t level) API_AVAILABLE(macos(10.10), ios(8.0), watchos(5.0), tvos(9.0));

/*
 * Function: EAPOLControlDidUserCancel
 *
 * Purpose:
 *    Returns whether the user cancelled the last authentication session.
 *    This helps to avoid bothering the user with multiple dialogues
 *    when they've already been informed and are aware of what's happenened.
 *    EAPOLMonitor uses it to avoid prompting the user again if the user
 *    cancels the authentication over Ethernet.
 */
Boolean
EAPOLControlDidUserCancel(const char * if_name) API_AVAILABLE(macos(10.10)) API_UNAVAILABLE(ios, watchos, tvos);

/*
 * Function: EAPOLControlStartSystem
 * Purpose:
 *   If a System Mode configuration exists on the given interface, start it.
 *   This function is used to resume a System mode authentication session
 *   after calling EAPOLControlStop() when System mode was active on the
 *   interace.
 *
 * Note:
 *   Currently the 'options' parameter is not used, pass NULL.
 */
int
EAPOLControlStartSystem(const char * interface_name, CFDictionaryRef options) API_AVAILABLE(macos(10.10)) API_UNAVAILABLE(ios, watchos, tvos);

/*
 * Function: EAPOLControlStartSystemWithClientItemID
 * Purpose:
 *   Start an authentication session running in System Mode over the
 *   specified interface using the specified EAPOLClientItemIDRef.
 *   
 * Returns:
 *   0 if successful, non-zero errno value otherwise.
 */
int
EAPOLControlStartSystemWithClientItemID(const char * interface_name,
					EAPOLClientItemIDRef itemID) API_AVAILABLE(macos(10.10)) API_UNAVAILABLE(ios, watchos, tvos);

/*
 * Function: EAPOLControlCopyLoginWindowConfiguration
 * Purpose:
 *   If LoginWindow mode is activated during this login session, returns the
 *   configuration that was used.  This value is cleared when the user logs out.
 *
 * Returns:
 *   0 and non-NULL CFDictionaryRef value in *config_p on success,
 *   non-zero on failure
 */
int
EAPOLControlCopyLoginWindowConfiguration(const char * interface_name,
					 CFDictionaryRef * config_p) API_AVAILABLE(macos(10.10)) API_UNAVAILABLE(ios, watchos, tvos);
/*
 * Function: EAPOLControlCopyLoginWindowClientItemID
 *
 * Purpose:
 *   If LoginWindow mode is activated during this login session, returns the
 *   EAPOLClientItemIDRef corresponding to the profile that was used.  The
 *   information is cleared when the user logs out.
 *
 * Returns:
 *   0 and non-NULL EAPOLClientItemIDRef on success,
 *   non-zero errno on failure.
 */
int
EAPOLControlCopyLoginWindowClientItemID(const char * if_name,
					EAPOLClientItemIDRef * itemID_p) API_AVAILABLE(macos(10.10)) API_UNAVAILABLE(ios, watchos, tvos);

/*
 * Function: EAPOLControlSetUserAutoConnectEnabled
 * Purpose:
 *    Enable/disable connecting automatically to 802.1X ethernet networks
 *    for the logged-in user.
 *
 *    If "enable" is TRUE, connection attempts will be made automatically.
 *    If "enable" is FALSE, no connection attempts will be made automatically.
 */
void
EAPOLControlSetUserAutoConnectEnabled(Boolean enable) API_AVAILABLE(macos(10.10)) API_UNAVAILABLE(ios, watchos, tvos);

/*
 * Function: EAPOLControlIsUserAutoConnectEnabled
 * Purpose:
 *   Return whether the system will auto-connect to 802.1X to ethernet
 *   networks for the logged-in user.
 *
 * Returns:
 *   TRUE if auto-connect is enabled, FALSE otherwise.
 */
Boolean
EAPOLControlIsUserAutoConnectEnabled(void) API_AVAILABLE(macos(10.10)) API_UNAVAILABLE(ios, watchos, tvos);

#endif /* _EAP8021X_EAPOLCONTROL_H */
