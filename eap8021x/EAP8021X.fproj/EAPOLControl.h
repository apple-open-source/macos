
/*
 * Copyright (c) 2002-2009 Apple Inc. All rights reserved.
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
#include <CoreFoundation/CFDictionary.h>
#include <CoreFoundation/CFString.h>
#include <TargetConditionals.h>
#include <EAP8021X/EAPOLControlTypes.h>

/*
 * Function: EAPOLControlKeyCreate
 * Returns:
 *   The DynamicStore key to use to retrieve information for the given
 *   interface, and/or use for notification for a specific interface.
 */
CFStringRef
EAPOLControlKeyCreate(const char * interface_name);

/*
 * Function: EAPOLControlAnyInterfaceKeyCreate
 * Returns:
 *   The DynamicStore key to use to register for notifications on 
 *   any interface's EAPOL information changing.
 */
CFStringRef
EAPOLControlAnyInterfaceKeyCreate(void);

/*
 * Function: EAPOLControlKeyCopyInterface
 * Returns:
 *   The interface associated with the given EAPOL interface key,
 *   NULL if the key is invalid.
 */
CFStringRef
EAPOLControlKeyCopyInterface(CFStringRef key);


/*
 * The config dictionary has the following format:
 *   kEAPOLControlEAPClientConfiguration    EAP client properties
 *   kEAPOLControlUniqueIdentifier	    unique string for session (optional)
 *   kEAPOLControlLogLevel		    log level (optional)
 *
 * See also <EAP8021X/EAPOLControlTypes.h>.
 */

/*
 * Functions: EAPOLControlStart
 * Purpose:
 *   Start an 802.1X authentication session on the given
 *   interface.
 */
int
EAPOLControlStart(const char * interface_name, CFDictionaryRef config);

int
EAPOLControlUpdate(const char * interface_name, CFDictionaryRef config);

int
EAPOLControlStop(const char * interface_name);

int
EAPOLControlRetry(const char * interface_name);

/*
 * Function: EAPOLControlProvideUserInput
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
			     CFDictionaryRef user_input);
int
EAPOLControlCopyStateAndStatus(const char * interface_name, 
			       EAPOLControlState * state,
			       CFDictionaryRef * status_dict_p);

/*
 * Function: EAPOLControlSetLogLevel
 * Purpose:
 *   Set the log level.  If (level >= 0), logging is enabled,
 *   otherwise logging is disabled.
 */
int
EAPOLControlSetLogLevel(const char * interface_name, int32_t level);

#if ! TARGET_OS_EMBEDDED
/*
 * Functions: EAPOLControlStartSystem
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
EAPOLControlStartSystem(const char * interface_name, CFDictionaryRef options);

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
					 CFDictionaryRef * config_p);
#endif /* ! TARGET_OS_EMBEDDED */

#endif _EAP8021X_EAPOLCONTROL_H
