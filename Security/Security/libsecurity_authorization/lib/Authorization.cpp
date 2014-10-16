/*
 * Copyright (c) 2000-2004,2011-2014 Apple Inc. All Rights Reserved.
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
// Authorization.cpp
//
// This file is the unified implementation of the Authorization and AuthSession APIs.
//
#include <stdint.h>
#include <Security/AuthSession.h>
#include <Security/AuthorizationPriv.h>
#include <security_utilities/ccaudit.h>
#include <security_cdsa_utilities/cssmbridge.h>
#include <Security/SecBase.h>
#include <security_utilities/logging.h>

//
// This no longer talks to securityd; it is a kernel function.
//
OSStatus SessionGetInfo(SecuritySessionId requestedSession,
    SecuritySessionId *sessionId,
    SessionAttributeBits *attributes)
{
    BEGIN_API
	CommonCriteria::AuditInfo session;
	if (requestedSession == callerSecuritySession)
		session.get();
	else
		session.get(requestedSession);
	if (sessionId)
		*sessionId = session.sessionId();
	if (attributes)
        *attributes = (SessionAttributeBits)session.flags();
    END_API(CSSM)
}


//
// Create a new session.
// This no longer talks to securityd; it is a kernel function.
// Securityd will pick up the new session when we next talk to it.
//
OSStatus SessionCreate(SessionCreationFlags flags,
    SessionAttributeBits attributes)
{
    BEGIN_API

	// we don't support the session creation flags anymore
	if (flags)
		Syslog::warning("SessionCreate flags=0x%lx unsupported (ignored)", (unsigned long)flags);
	CommonCriteria::AuditInfo session;
	session.create(attributes);
        
	// retrieve the (new) session id and set it into the process environment
	session.get();
	char idString[80];
	snprintf(idString, sizeof(idString), "%x", session.sessionId());
	setenv("SECURITYSESSIONID", idString, 1);

    END_API(CSSM)
}


//
// Get and set the distinguished uid (optionally) associated with the session.
//
OSStatus SessionSetDistinguishedUser(SecuritySessionId session, uid_t user)
{
	BEGIN_API
	CommonCriteria::AuditInfo session;
	session.get();
	session.ai_auid = user;
	session.set();
	END_API(CSSM)
}


OSStatus SessionGetDistinguishedUser(SecuritySessionId session, uid_t *user)
{
    BEGIN_API
	CommonCriteria::AuditInfo session;
	session.get();
	Required(user) = session.uid();
    END_API(CSSM)
}

//OSStatus _SessionSetUserPreferences(SecuritySessionId session);
//
//static
//void SessionUserPreferencesChanged(CFNotificationCenterRef center, void *observer, CFStringRef name, const void *object, CFDictionaryRef userInfo)
//{
//#warning "The cast will loose some information"
//	_SessionSetUserPreferences((SecuritySessionId)uintptr_t(observer));
//}
//
//OSStatus _SessionSetUserPreferences(SecuritySessionId session)
//{
//    BEGIN_API
//	CFStringRef appleLanguagesStr = CFSTR("AppleLanguages");
//	CFStringRef controlTintStr = CFSTR("AppleAquaColorVariant");
//	CFStringRef keyboardUIModeStr = CFSTR("AppleKeyboardUIMode");
//	CFStringRef textDirectionStr = CFSTR("AppleTextDirection");
//	CFStringRef hitoolboxAppIDStr = CFSTR("com.apple.HIToolbox");
//	CFNotificationCenterRef center = CFNotificationCenterGetDistributedCenter();
//
//	CFRef<CFMutableDictionaryRef> userPrefsDict(CFDictionaryCreateMutable(NULL, 10, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks));
//	CFRef<CFMutableDictionaryRef> globalPrefsDict(CFDictionaryCreateMutable(NULL, 10, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks));
//	
//	if (!userPrefsDict || !globalPrefsDict)
//		return errSessionValueNotSet;
//	
//	CFRef<CFArrayRef> appleLanguagesArray(static_cast<CFArrayRef>(CFPreferencesCopyAppValue(appleLanguagesStr, kCFPreferencesCurrentApplication)));
//	if (appleLanguagesArray)
//		CFDictionarySetValue(globalPrefsDict, appleLanguagesStr, appleLanguagesArray);
//	
//	CFRef<CFNumberRef> controlTintNumber(static_cast<CFNumberRef>(CFPreferencesCopyAppValue(controlTintStr, kCFPreferencesCurrentApplication)));
//	if (controlTintNumber)
//		CFDictionarySetValue(globalPrefsDict, controlTintStr, controlTintNumber);
//
//	CFRef<CFNumberRef> keyboardUIModeNumber(static_cast<CFNumberRef>(CFPreferencesCopyAppValue(keyboardUIModeStr, kCFPreferencesCurrentApplication)));
//	if (keyboardUIModeNumber)
//		CFDictionarySetValue(globalPrefsDict, keyboardUIModeStr, keyboardUIModeNumber);
//
//	CFRef<CFNumberRef> textDirectionNumber(static_cast<CFNumberRef>(CFPreferencesCopyAppValue(textDirectionStr, kCFPreferencesCurrentApplication)));
//	if (textDirectionNumber)
//		CFDictionarySetValue(globalPrefsDict, textDirectionStr, textDirectionNumber);
//	
//	if (CFDictionaryGetCount(globalPrefsDict) > 0)
//		CFDictionarySetValue(userPrefsDict, kCFPreferencesAnyApplication, globalPrefsDict);
//
//	CFPreferencesSynchronize(hitoolboxAppIDStr, kCFPreferencesCurrentUser, 
//			kCFPreferencesCurrentHost);
//	CFRef<CFDictionaryRef> hitoolboxPrefsDict(static_cast<CFDictionaryRef>(CFPreferencesCopyMultiple(NULL, hitoolboxAppIDStr, kCFPreferencesCurrentUser, kCFPreferencesCurrentHost)));
//	if (hitoolboxPrefsDict) {
//		CFDictionarySetValue(userPrefsDict, hitoolboxAppIDStr, hitoolboxPrefsDict);
//		CFNotificationCenterPostNotification(center, CFSTR("com.apple.securityagent.InputPrefsChanged"), CFSTR("com.apple.loginwindow"), hitoolboxPrefsDict, true);
//	}
//	
//	CFRef<CFDataRef> userPrefsData(CFPropertyListCreateXMLData(NULL, userPrefsDict));
//	if (!userPrefsData)
//		return errSessionValueNotSet;
//	server().setSessionUserPrefs(session, (uint32_t)CFDataGetLength(userPrefsData), CFDataGetBytePtr(userPrefsData));
//
//    END_API(CSSM)
//}

OSStatus SessionSetUserPreferences(SecuritySessionId session)
{
//	OSStatus status = _SessionSetUserPreferences(session);
//	if (errSecSuccess == status) {
//		CFNotificationCenterRef center = CFNotificationCenterGetDistributedCenter();
//		// We've succeeded in setting up a static set of prefs, now set up 
//		CFNotificationCenterAddObserver(center, (void*)session, SessionUserPreferencesChanged, CFSTR("com.apple.Carbon.TISNotifySelectedKeyboardInputSourceChanged"), NULL, CFNotificationSuspensionBehaviorDeliverImmediately);
//		CFNotificationCenterAddObserver(center, (void*)session, SessionUserPreferencesChanged, CFSTR("com.apple.Carbon.TISNotifyEnabledKeyboardInputSourcesChanged"), NULL, CFNotificationSuspensionBehaviorDeliverImmediately);
//	}
//	return status;
    return errSecSuccess;
}
