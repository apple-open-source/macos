/*
 * Copyright (c) 2000-2001 Apple Computer, Inc. All Rights Reserved.
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


//
// Authorization.cpp
//
// This file is the unified implementation of the Authorization and AuthSession APIs.
//
#include <Security/Authorization.h>
#include <Security/AuthorizationDB.h>
#include <Security/AuthorizationPriv.h>
#include <Security/AuthSession.h>
#include "AuthorizationWalkers.h"
#include <Security/mach++.h>
#include <Security/globalizer.h>
#include <Security/cssmalloc.h>
#include <Security/ssclient.h>
#include <Security/ktracecodes.h>

using namespace SecurityServer;
using namespace MachPlusPlus;


//
// Shared cached client object
//
class AuthClient : public SecurityServer::ClientSession {
public:
	AuthClient()
	: SecurityServer::ClientSession(CssmAllocator::standard(), CssmAllocator::standard())
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
    Debug::trace(kSecTraceAuthorizationCreateStart);
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
	Debug::trace(kSecTraceAuthorizationCreateEnd);
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
    Debug::trace(kSecTraceAuthorizationCopyRightsStart);
	BEGIN_API
	AuthorizationBlob *auth = (AuthorizationBlob *)authorization;
	server().authCopyRights(Required(auth, errAuthorizationInvalidRef),
		rights, environment, flags, authorizedRights);
	Debug::trace(kSecTraceAuthorizationCopyRightsEnd);
	END_API(CSSM)
}


//
// Retrieve side-band information from an authorization
//
OSStatus AuthorizationCopyInfo(AuthorizationRef authorization, 
	AuthorizationString tag,
	AuthorizationItemSet **info)
{
    Debug::trace(kSecTraceAuthorizationCopyInfoStart);
	BEGIN_API
	AuthorizationBlob *auth = (AuthorizationBlob *)authorization;
	server().authCopyInfo(Required(auth, errAuthorizationInvalidRef),
		tag, Required(info));
	Debug::trace(kSecTraceAuthorizationCopyInfoEnd);
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
// Get session information
//
OSStatus SessionGetInfo(SecuritySessionId session,
    SecuritySessionId *sessionId,
    SessionAttributeBits *attributes)
{
    BEGIN_API
    SecuritySessionId sid = session;
    server().getSessionInfo(sid, *attributes);
    if (sessionId)
        *sessionId = sid;
    END_API(CSSM)
}


//
// Create a new session
//
OSStatus SessionCreate(SessionCreationFlags flags,
    SessionAttributeBits attributes)
{
    BEGIN_API
    
    // unless the (expert) caller has already done so, create a sub-bootstrap and set it
    // note that this is inherently thread-unfriendly; we can't do anything about that
    // (caller's responsibility)
    Bootstrap bootstrap;
    if (!(flags & sessionKeepCurrentBootstrap)) {
		TaskPort self;
		bootstrap = bootstrap.subset(TaskPort());
		self.bootstrap(bootstrap);
    }
    
    // now call the SecurityServer and tell it to initialize the (new) session
    server().setupSession(flags, attributes);
	
	// retrieve the (new) session id and set it into the process environment
	SecuritySessionId id = callerSecuritySession;
	SessionAttributeBits attrs;
	server().getSessionInfo(id, attrs);
	char idString[80];
	snprintf(idString, sizeof(idString), "%lx", id);
	setenv("SECURITYSESSIONID", idString, 1);

    END_API(CSSM)
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
		CFRef<CFBundleRef> clientBundle = bundle ? bundle : CFBundleGetMainBundle(); 
		
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
				
				CFDataRef tableData;
				SInt32 errCode;
				CFStringRef errStr;
				CFPropertyListRef stringTable;
		
				CFURLCreateDataAndPropertiesFromResource(CFGetAllocator(clientBundle), locURL, &tableData, NULL, NULL, &errCode);
				
				if (errCode)
				{
					CFRelease(tableData);
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
	CFDataRef rightDefinitionXML = CFPropertyListCreateXMLData(NULL, rightDefinitionDict);

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

