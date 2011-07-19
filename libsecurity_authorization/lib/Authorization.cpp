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


//
// Authorization.cpp
//
// This file is the unified implementation of the Authorization and AuthSession APIs.
//
#include <stdint.h>
#include <Security/Authorization.h>
#include <Security/AuthorizationPriv.h>
#include <Security/AuthorizationDB.h>
#include <Security/AuthorizationTagsPriv.h>
#include <Security/AuthSession.h>
#include <security_utilities/mach++.h>
#include <security_utilities/globalizer.h>
#include <security_utilities/alloc.h>
#include <security_utilities/cfutilities.h>
#include <security_cdsa_utilities/cssmbridge.h>
#include <security_cdsa_utilities/AuthorizationWalkers.h>
#include <security_utilities/ccaudit.h>
#include <securityd_client/ssclient.h>
#include <CoreFoundation/CFPreferences.h>
#include <Carbon/../Frameworks/HIToolbox.framework/Headers/TextInputSources.h>

#include <security_utilities/logging.h>

using namespace SecurityServer;
using namespace MachPlusPlus;


//
// Shared cached client object
//
class AuthClient : public SecurityServer::ClientSession {
public:
	AuthClient()
	: SecurityServer::ClientSession(Allocator::standard(), Allocator::standard())
	{ }
};

static ModuleNexus<AuthClient> server;


//
// Create an Authorization
//
OSStatus AuthorizationCreate(const AuthorizationRights *rights,
	const AuthorizationEnvironment *environment,
	AuthorizationFlags flags,
	AuthorizationRef *authorization)
{
	BEGIN_API
	AuthorizationBlob result;
	server().authCreate(rights, environment, flags, result);
	if (authorization)
	{
		*authorization = 
			(AuthorizationRef) new(server().returnAllocator) AuthorizationBlob(result);
	}
	else
	{
		// If no authorizationRef is desired free the one we just created.
		server().authRelease(result, flags);
	}
	END_API(CSSM)
}


//
// Free an authorization reference
//
OSStatus AuthorizationFree(AuthorizationRef authorization, AuthorizationFlags flags)
{
	BEGIN_API
	AuthorizationBlob *auth = (AuthorizationBlob *)authorization;
	server().authRelease(Required(auth, errAuthorizationInvalidRef), flags);
	server().returnAllocator.free(auth);
	END_API(CSSM)
}


//
// Augment and/or interrogate an authorization
//
OSStatus AuthorizationCopyRights(AuthorizationRef authorization,
	const AuthorizationRights *rights,
	const AuthorizationEnvironment *environment,
	AuthorizationFlags flags,
	AuthorizationRights **authorizedRights)
{
	BEGIN_API
	AuthorizationBlob *auth = (AuthorizationBlob *)authorization;
	server().authCopyRights(Required(auth, errAuthorizationInvalidRef),
		rights, environment, flags, authorizedRights);
	END_API(CSSM)
}


//
// Augment and/or interrogate an authorization asynchronously
//
void AuthorizationCopyRightsAsync(AuthorizationRef authorization,
	const AuthorizationRights *rights,
	const AuthorizationEnvironment *environment,
	AuthorizationFlags flags,
	AuthorizationAsyncCallback callbackBlock)
{
	__block AuthorizationRights *blockAuthorizedRights = NULL;
	
	dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
		OSStatus status = AuthorizationCopyRights(authorization, rights, environment, flags, &blockAuthorizedRights);
		callbackBlock(status, blockAuthorizedRights);
	});
}


//
// Retrieve side-band information from an authorization
//
OSStatus AuthorizationCopyInfo(AuthorizationRef authorization, 
	AuthorizationString tag,
	AuthorizationItemSet **info)
{
	BEGIN_API
	AuthorizationBlob *auth = (AuthorizationBlob *)authorization;
	server().authCopyInfo(Required(auth, errAuthorizationInvalidRef),
		tag, Required(info));
	END_API(CSSM)
}


//
// Externalize and internalize authorizations
//
OSStatus AuthorizationMakeExternalForm(AuthorizationRef authorization,
	AuthorizationExternalForm *extForm)
{
	BEGIN_API
	AuthorizationBlob *auth = (AuthorizationBlob *)authorization;
	server().authExternalize(Required(auth, errAuthorizationInvalidRef), *extForm);
	END_API(CSSM)
}

OSStatus AuthorizationCreateFromExternalForm(const AuthorizationExternalForm *extForm,
	AuthorizationRef *authorization)
{
	BEGIN_API
	AuthorizationBlob result;
	server().authInternalize(*extForm, result);
	Required(authorization, errAuthorizationInvalidRef) =
		(AuthorizationRef) new(server().returnAllocator) AuthorizationBlob(result);

	END_API(CSSM)
}


//
// Free an ItemSet structure returned from an API call. This is a local operation.
// Since we allocate returned ItemSets as compact blobs, this is just a simple
// free() call.
//
OSStatus AuthorizationFreeItemSet(AuthorizationItemSet *set)
{
	BEGIN_API
	server().returnAllocator.free(set);
	return errAuthorizationSuccess;
	END_API(CSSM)
}


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
        *attributes = session.flags();
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
		Syslog::warning("SessionCreate flags=0x%x unsupported (ignored)", flags);
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

OSStatus _SessionSetUserPreferences(SecuritySessionId session);

void SessionUserPreferencesChanged(CFNotificationCenterRef center, void *observer, CFStringRef name, const void *object, CFDictionaryRef userInfo)
{
	_SessionSetUserPreferences(uintptr_t(observer));
}

OSStatus _SessionSetUserPreferences(SecuritySessionId session)
{
    BEGIN_API
	CFStringRef appleLanguagesStr = CFSTR("AppleLanguages");
	CFStringRef controlTintStr = CFSTR("AppleAquaColorVariant");
	CFStringRef keyboardUIModeStr = CFSTR("AppleKeyboardUIMode");
	CFStringRef textDirectionStr = CFSTR("AppleTextDirection");
	CFStringRef hitoolboxAppIDStr = CFSTR("com.apple.HIToolbox");
	CFNotificationCenterRef center = CFNotificationCenterGetDistributedCenter();

	CFRef<CFMutableDictionaryRef> userPrefsDict(CFDictionaryCreateMutable(NULL, 10, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks));
	CFRef<CFMutableDictionaryRef> globalPrefsDict(CFDictionaryCreateMutable(NULL, 10, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks));
	
	if (!userPrefsDict || !globalPrefsDict)
		return errSessionValueNotSet;
	
	CFRef<CFArrayRef> appleLanguagesArray(static_cast<CFArrayRef>(CFPreferencesCopyAppValue(appleLanguagesStr, kCFPreferencesCurrentApplication)));
	if (appleLanguagesArray)
		CFDictionarySetValue(globalPrefsDict, appleLanguagesStr, appleLanguagesArray);
	
	CFRef<CFNumberRef> controlTintNumber(static_cast<CFNumberRef>(CFPreferencesCopyAppValue(controlTintStr, kCFPreferencesCurrentApplication)));
	if (controlTintNumber)
		CFDictionarySetValue(globalPrefsDict, controlTintStr, controlTintNumber);

	CFRef<CFNumberRef> keyboardUIModeNumber(static_cast<CFNumberRef>(CFPreferencesCopyAppValue(keyboardUIModeStr, kCFPreferencesCurrentApplication)));
	if (keyboardUIModeNumber)
		CFDictionarySetValue(globalPrefsDict, keyboardUIModeStr, keyboardUIModeNumber);

	CFRef<CFNumberRef> textDirectionNumber(static_cast<CFNumberRef>(CFPreferencesCopyAppValue(textDirectionStr, kCFPreferencesCurrentApplication)));
	if (textDirectionNumber)
		CFDictionarySetValue(globalPrefsDict, textDirectionStr, textDirectionNumber);
	
	if (CFDictionaryGetCount(globalPrefsDict) > 0)
		CFDictionarySetValue(userPrefsDict, kCFPreferencesAnyApplication, globalPrefsDict);

	CFPreferencesSynchronize(hitoolboxAppIDStr, kCFPreferencesCurrentUser, 
			kCFPreferencesCurrentHost);
	CFRef<CFDictionaryRef> hitoolboxPrefsDict(static_cast<CFDictionaryRef>(CFPreferencesCopyMultiple(NULL, hitoolboxAppIDStr, kCFPreferencesCurrentUser, kCFPreferencesCurrentHost)));
	if (hitoolboxPrefsDict) {
		CFDictionarySetValue(userPrefsDict, hitoolboxAppIDStr, hitoolboxPrefsDict);
		CFNotificationCenterPostNotification(center, CFSTR("com.apple.securityagent.InputPrefsChanged"), CFSTR("com.apple.loginwindow"), hitoolboxPrefsDict, true);
	}
	
	CFRef<CFDataRef> userPrefsData(CFPropertyListCreateXMLData(NULL, userPrefsDict));
	if (!userPrefsData)
		return errSessionValueNotSet;
	server().setSessionUserPrefs(session, CFDataGetLength(userPrefsData), CFDataGetBytePtr(userPrefsData));

    END_API(CSSM)
}

OSStatus SessionSetUserPreferences(SecuritySessionId session)
{
	OSStatus status = _SessionSetUserPreferences(session);
	if (noErr == status) {
		CFNotificationCenterRef center = CFNotificationCenterGetDistributedCenter();
		// We've succeeded in setting up a static set of prefs, now set up 
		CFNotificationCenterAddObserver(center, (void*)session, SessionUserPreferencesChanged, CFSTR("com.apple.Carbon.TISNotifySelectedKeyboardInputSourceChanged"), NULL, CFNotificationSuspensionBehaviorDeliverImmediately);
		CFNotificationCenterAddObserver(center, (void*)session, SessionUserPreferencesChanged, CFSTR("com.apple.Carbon.TISNotifyEnabledKeyboardInputSourcesChanged"), NULL, CFNotificationSuspensionBehaviorDeliverImmediately);
	}
	return status;
}


//
// Modify Authorization rules
//

// 
// AuthorizationRightGet 
// 
OSStatus AuthorizationRightGet(const char *rightName, CFDictionaryRef *rightDefinition)
{
	BEGIN_API;
	Required(rightName);
	CssmDataContainer definition(server().returnAllocator);

	server().authorizationdbGet(rightName, definition, server().returnAllocator);
	// convert rightDefinition to dictionary
	
	if (rightDefinition)
	{
		CFRef<CFDataRef> data(CFDataCreate(NULL, static_cast<UInt8 *>(definition.data()), definition.length()));
		if (!data)
			CssmError::throwMe(errAuthorizationInternal);
			
		CFRef<CFDictionaryRef> rightDict(static_cast<CFDictionaryRef>(CFPropertyListCreateFromXMLData(NULL, data, kCFPropertyListImmutable, NULL)));
		if (!rightDict 
			|| CFGetTypeID(rightDict) != CFDictionaryGetTypeID()) 
				CssmError::throwMe(errAuthorizationInternal);

		CFRetain(rightDict);
		*rightDefinition = rightDict;
	}

	END_API(CSSM);
}

//
// AuthorizationRightSet
//
OSStatus AuthorizationRightSet(AuthorizationRef authRef, 
	const char *rightName, CFTypeRef rightDefinition, 
	CFStringRef descriptionKey, CFBundleRef bundle, CFStringRef tableName)
{
	BEGIN_API;
	Required(rightName);
	AuthorizationBlob *auth = (AuthorizationBlob *)authRef;

	CFRef<CFMutableDictionaryRef> rightDefinitionDict;
	if (rightDefinition && (CFGetTypeID(rightDefinition) == CFStringGetTypeID()))
	{
		rightDefinitionDict = CFDictionaryCreateMutable(NULL, 10, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
		if (!rightDefinitionDict)
				CssmError::throwMe(errAuthorizationInternal);
		CFDictionarySetValue(rightDefinitionDict, CFSTR(kAuthorizationRightRule), rightDefinition);
	}
	else
		if (rightDefinition && (CFGetTypeID(rightDefinition) == CFDictionaryGetTypeID()))
		{
			rightDefinitionDict = CFDictionaryCreateMutableCopy(NULL, 0, static_cast<CFDictionaryRef>(rightDefinition));
			if (!rightDefinitionDict)
				CssmError::throwMe(errAuthorizationInternal);
		}
		else
			CssmError::throwMe(errAuthorizationDenied);

	if (rightDefinitionDict)
		CFRelease(rightDefinitionDict); // we just assigned things that were already retained
	
	if (descriptionKey)
	{
		CFRef<CFMutableDictionaryRef> localizedDescriptions(CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks));
		
		if (!localizedDescriptions)
			CssmError::throwMe(errAuthorizationInternal);
	
		// assigning to perform a retain on either
		CFRef<CFBundleRef> clientBundle; clientBundle = bundle ? bundle : CFBundleGetMainBundle(); 
		
		// looks like a list of CFStrings: English us_en etc.
		CFRef<CFArrayRef> localizations(CFBundleCopyBundleLocalizations(clientBundle));
	
		if (localizations)
		{
			// for every CFString in localizations do
			CFIndex locIndex, allLocs = CFArrayGetCount(localizations);
			for (locIndex = 0; locIndex < allLocs; locIndex++)
			{
				CFStringRef oneLocalization = static_cast<CFStringRef>(CFArrayGetValueAtIndex(localizations, locIndex));
				
				if (!oneLocalization)
					continue;
		
				// @@@ no way to get "Localized" and "strings" as constants?
				CFRef<CFURLRef> locURL(CFBundleCopyResourceURLForLocalization(clientBundle, tableName ? tableName :  CFSTR("Localizable"), CFSTR("strings"), NULL /*subDirName*/, oneLocalization));
				
				if (!locURL)
					continue;
				
				CFDataRef tableData = NULL;
				SInt32 errCode;
				CFStringRef errStr;
				CFPropertyListRef stringTable;
		
				CFURLCreateDataAndPropertiesFromResource(CFGetAllocator(clientBundle), locURL, &tableData, NULL, NULL, &errCode);
				
				if (errCode)
				{
					if (NULL != tableData) {
						CFRelease(tableData);
					}
					continue;
				}
		
				stringTable = CFPropertyListCreateFromXMLData(CFGetAllocator(clientBundle), tableData, kCFPropertyListImmutable, &errStr);
				if (errStr != NULL) {
					CFRelease(errStr);
					errStr = NULL;
				}
				CFRelease(tableData);
				
				CFStringRef value = static_cast<CFStringRef>(CFDictionaryGetValue(static_cast<CFDictionaryRef>(stringTable), descriptionKey));
				if (value == NULL || CFEqual(value, CFSTR(""))) {
					CFRelease(stringTable);
					continue;
				} else {
					// oneLocalization/value into our dictionary 
					CFDictionarySetValue(localizedDescriptions, oneLocalization, value);
					CFRelease(stringTable);
				}
			}
		}

		// add the description as the default localization into the dictionary
		CFDictionarySetValue(localizedDescriptions, CFSTR(""), descriptionKey);
		
		// stuff localization table into rule definition
		CFDictionarySetValue(rightDefinitionDict, CFSTR(kAuthorizationRuleParameterDefaultPrompt), localizedDescriptions);

	}
	
	// serialize cfdictionary with data into rightDefinitionXML
	CFRef<CFDataRef> rightDefinitionXML(CFPropertyListCreateXMLData(NULL, rightDefinitionDict));

	server().authorizationdbSet(Required(auth), rightName, CFDataGetLength(rightDefinitionXML), CFDataGetBytePtr(rightDefinitionXML));
		
    END_API(CSSM);
}

//
// AuthorizationRightRemove
//
OSStatus AuthorizationRightRemove(AuthorizationRef authRef, const char *rightName)
{
	BEGIN_API;
	Required(rightName);
	AuthorizationBlob *auth = (AuthorizationBlob *)authRef;
	server().authorizationdbRemove(Required(auth), rightName);
	END_API(CSSM);
}

