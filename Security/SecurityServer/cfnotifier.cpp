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
// cfnotifier - quick & dirty code to send keychain lock notification
//
#include "cfnotifier.h"
#include <Security/cfutilities.h>
#include <Security/debugging.h>

#include "session.h"

using namespace Security;
using namespace Security::MachPlusPlus;


#define notificationName	CFSTR("com.apple.securitycore.kcevent")
#define eventTypeKey		CFSTR("type")
#define pidKey				CFSTR("pid")
#define keychainKey		CFSTR("keychain")
#define itekey			CFSTR("item")
#define keyGUID			CFSTR("GUID")
#define keySubserviceId	CFSTR("SubserviceId")
#define keySubserviceType	CFSTR("SubserviceType")
#define keyDbName		CFSTR("DbName")
#define keyDbLocation		CFSTR("DbLocation")
#define keyActive			CFSTR("Active")
#define keyMajorVersion	CFSTR("MajorVersion")
#define keyMinorVersion	CFSTR("MinorVersion")
#define defaultDLDbListKey	 CFSTR("DLDBSearchList")
#define defaultDomain		CFSTR("com.apple.securitycore")


//
// Event codes
//
enum {
    lockedEvent                = 1,	/* a keychain was locked */
    unlockedEvent              = 2,	/* a keychain was unlocked */
	passphraseChangedEvent	   = 6	/* a keychain password was (possibly) changed */
};


//
// Local functions
//
static CFDictionaryRef makeDictionary(const DLDbIdentifier &db);


//
// Main methods
//
void KeychainNotifier::lock(const DLDbIdentifier &db)
{ notify(db, lockedEvent); }

void KeychainNotifier::unlock(const DLDbIdentifier &db)
{ notify(db, unlockedEvent); }

void KeychainNotifier::passphraseChanged(const DLDbIdentifier &db)
{ notify(db, passphraseChangedEvent); }


//
// Lock and unlock notifications
//
void KeychainNotifier::notify(const DLDbIdentifier &db, int event)
{
    CFRef<CFMutableDictionaryRef> mutableDict(::CFDictionaryCreateMutable(
        kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks,
        &kCFTypeDictionaryValueCallBacks));
    if (!mutableDict)
        throw std::bad_alloc();
    
    SInt32 theEvent = event;
    CFRef<CFNumberRef> theEventData(::CFNumberCreate( kCFAllocatorDefault,
        kCFNumberSInt32Type, &theEvent));
    if (!theEventData)
        throw std::bad_alloc();
    ::CFDictionarySetValue( mutableDict, eventTypeKey, theEventData );
    
    CFRef<CFDictionaryRef> dict = makeDictionary(db);
    if (!dict)
        throw std::bad_alloc();
    ::CFDictionarySetValue(mutableDict, keychainKey, dict);
    
    for (Session::Iterator it = Session::begin(); it != Session::end(); it++) {
        StBootstrap bootSwitch(it->second->bootstrapPort());
        IFDEBUG(debug("cfnotify", "send event %d for database %s to session %p",
            event, db.dbName(), it->second));
        ::CFNotificationCenterPostNotification(CFNotificationCenterGetDistributedCenter(),
            notificationName, NULL, mutableDict, false);
    }
}

static CFDictionaryRef makeDictionary(const DLDbIdentifier &db)
{
	CFRef<CFMutableDictionaryRef> aDict(CFDictionaryCreateMutable(kCFAllocatorDefault,0,
		&kCFTypeDictionaryKeyCallBacks,&kCFTypeDictionaryValueCallBacks));
    if (!aDict)
        throw std::bad_alloc();

    // Put SUBSERVICE_UID in dictionary
    char buffer[Guid::stringRepLength+1];
    const CssmSubserviceUid& ssuid=db.ssuid();
    const Guid &theGuid = Guid::overlay(ssuid.Guid);
    CFRef<CFStringRef> stringGuid(::CFStringCreateWithCString(kCFAllocatorDefault,
            theGuid.toString(buffer),kCFStringEncodingMacRoman));
    if (stringGuid)
        ::CFDictionarySetValue(aDict,keyGUID,stringGuid);
        
    if (ssuid.SubserviceId!=0)
    {
        CFRef<CFNumberRef> subserviceId(::CFNumberCreate(kCFAllocatorDefault,kCFNumberSInt32Type,&ssuid.SubserviceId));
        if (subserviceId)
            ::CFDictionarySetValue(aDict,keySubserviceId,subserviceId);
    }
    if (ssuid.SubserviceType!=0)
    {
        CFRef<CFNumberRef> subserviceType(CFNumberCreate(kCFAllocatorDefault,kCFNumberSInt32Type,&ssuid.SubserviceType));
        if (subserviceType)
            ::CFDictionarySetValue(aDict,keySubserviceType,subserviceType);
    }
    if (ssuid.Version.Major!=0 && ssuid.Version.Minor!=0)
    {
        CFRef<CFNumberRef> majorVersion(::CFNumberCreate(kCFAllocatorDefault,kCFNumberSInt32Type,&ssuid.Version.Major));
        if (majorVersion)
            ::CFDictionarySetValue(aDict,keyMajorVersion,majorVersion);
        CFRef<CFNumberRef> minorVersion(::CFNumberCreate(kCFAllocatorDefault,kCFNumberSInt32Type,&ssuid.Version.Minor));
        if (minorVersion)
            ::CFDictionarySetValue(aDict,keyMinorVersion,minorVersion);
    }
    
    // Put DbName in dictionary
	const char *dbName=db.dbName();
    if (dbName)
    {
        CFRef<CFStringRef> theDbName(::CFStringCreateWithCString(kCFAllocatorDefault,dbName,kCFStringEncodingMacRoman));
        ::CFDictionarySetValue(aDict,keyDbName,theDbName);
    }
    // Put DbLocation in dictionary
	const CSSM_NET_ADDRESS *dbLocation=db.dbLocation();
    if (dbLocation!=NULL && dbLocation->AddressType!=CSSM_ADDR_NONE)
    {
        CFRef<CFDataRef> theData(::CFDataCreate(kCFAllocatorDefault,dbLocation->Address.Data,dbLocation->Address.Length));
        if (theData)
            ::CFDictionarySetValue(aDict,keyDbLocation,theData);
    }

    ::CFRetain(aDict);
	return aDict;
}

