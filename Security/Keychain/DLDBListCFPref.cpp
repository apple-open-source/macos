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


/*
    DLDBListCFPref.cpp
*/

#include "DLDBListCFPref.h"
#include <Security/cssmapple.h>
#include <memory>
#include <pwd.h>

using namespace CssmClient;

static const double kDLDbListCFPrefRevertInterval = 30.0;

#define kKeyGUID CFSTR("GUID")
#define kKeySubserviceId CFSTR("SubserviceId")
#define kKeySubserviceType CFSTR("SubserviceType")
#define kKeyDbName CFSTR("DbName")
#define kKeyDbLocation CFSTR("DbLocation")
#define kKeyActive CFSTR("Active")
#define kKeyMajorVersion CFSTR("MajorVersion")
#define kKeyMinorVersion CFSTR("MinorVersion")
#define kDefaultDLDbListKey CFSTR("DLDBSearchList")
#define kDefaultDomain CFSTR("com.apple.security")


//-------------------------------------------------------------------------------------
//
//			Lists of DL/DBs, with CFPreferences backing store
//
//-------------------------------------------------------------------------------------

DLDbListCFPref::DLDbListCFPref(CFStringRef theDLDbListKey,CFStringRef prefsDomain) :
    mPrefsDomain(prefsDomain?prefsDomain:kDefaultDomain),mDLDbListKey(theDLDbListKey?theDLDbListKey:kDefaultDLDbListKey)
{
    loadOrCreate();
}

DLDbListCFPref::~DLDbListCFPref()
{
    save();
}

void DLDbListCFPref::loadOrCreate()
{
 
    CFRef<CFArrayRef> theArray(static_cast<CFArrayRef>(::CFPreferencesCopyValue(mDLDbListKey, mPrefsDomain, kCFPreferencesCurrentUser, kCFPreferencesAnyHost)));
	if (!theArray)
        return;

    if (::CFGetTypeID(theArray)!=::CFArrayGetTypeID())
    {
        ::CFPreferencesSetValue(mDLDbListKey, NULL, mPrefsDomain, kCFPreferencesCurrentUser, kCFPreferencesAnyHost);
		return;
    }
    
    CFIndex top=::CFArrayGetCount(theArray);
    // Each entry is a CFDictionary; peel it off & add it to the array
    for (CFIndex idx=0;idx<top;idx++)
    {
        CFDictionaryRef theDict=reinterpret_cast<CFDictionaryRef>(::CFArrayGetValueAtIndex(theArray,idx));
        DLDbIdentifier theDLDbIdentifier=cfDictionaryRefToDLDbIdentifier(theDict);
        push_back(theDLDbIdentifier);
    }
	
	
	mPrefsTimeStamp=CFAbsoluteTimeGetCurrent();


}

void DLDbListCFPref::save()
{
    if (!hasChanged())
        return;
    // Make a temporary CFArray with the contents of the vector
 	CFRef<CFMutableArrayRef> theArray(::CFArrayCreateMutable(kCFAllocatorDefault,size(),&kCFTypeArrayCallBacks));
    for (DLDbList::const_iterator ix=begin();ix!=end();ix++)
	{
		CFRef<CFDictionaryRef> aDict(dlDbIdentifierToCFDictionaryRef(*ix));
        ::CFArrayAppendValue(theArray,aDict);
	}

	::CFPreferencesSetValue(mDLDbListKey, theArray, mPrefsDomain, kCFPreferencesCurrentUser, kCFPreferencesAnyHost);
    ::CFPreferencesSynchronize(mPrefsDomain, kCFPreferencesCurrentUser, kCFPreferencesAnyHost);

    changed(false);
}

void DLDbListCFPref::clearDefaultKeychain()
{
	::CFPreferencesSetValue(mDLDbListKey, NULL, mPrefsDomain, kCFPreferencesCurrentUser, kCFPreferencesAnyHost);
    ::CFPreferencesSynchronize(mPrefsDomain, kCFPreferencesCurrentUser, kCFPreferencesAnyHost);
	
    changed(false);
}



//----------------------------------------------------------------------
//			Conversions
//----------------------------------------------------------------------

DLDbIdentifier DLDbListCFPref::cfDictionaryRefToDLDbIdentifier(CFDictionaryRef theDict)
{
    // We must get individual values from the dictionary and store in basic types

    // GUID
    CCFValue vGuid(::CFDictionaryGetValue(theDict,kKeyGUID));
    string guidStr=vGuid;
    const Guid guid(guidStr.c_str());

    //CSSM_VERSION
	CSSM_VERSION theVersion={0,};
    CCFValue vMajor(::CFDictionaryGetValue(theDict,kKeyMajorVersion));
	theVersion.Major = vMajor;
    CCFValue vMinor(::CFDictionaryGetValue(theDict,kKeyMinorVersion));
	theVersion.Minor = vMinor;

    //subserviceId
    CCFValue vSsid(::CFDictionaryGetValue(theDict,kKeySubserviceId));
    uint32 subserviceId=sint32(vSsid);

    //CSSM_SERVICE_TYPE
    CSSM_SERVICE_TYPE subserviceType=CSSM_SERVICE_DL;
    CCFValue vSsType(::CFDictionaryGetValue(theDict,kKeySubserviceType));
    subserviceType=vSsType;
    
    // Get DbName from dictionary
    CCFValue vDbName(::CFDictionaryGetValue(theDict,kKeyDbName));
    string dbName=vDbName;
    
    // jch Get DbLocation from dictionary
	CssmNetAddress *dbLocation=NULL;
    
    // Create a local CssmSubserviceUid
    CssmSubserviceUid ssuid(guid,&theVersion,subserviceId,subserviceType);
        
    return DLDbIdentifier(ssuid,ExpandTildesInPath(dbName).c_str(),dbLocation);
}

string DLDbListCFPref::HomeDir()
{
    const char *home = getenv("HOME");
    if (!home)
    {
        // If $HOME is unset get the current users home directory from the passwd file.
        struct passwd *pw = getpwuid(getuid());
        if (pw)
            home = pw->pw_dir;
    }
    return home ? home : "";
}

string DLDbListCFPref::ExpandTildesInPath(const string &inPath)
{
    if ((short)inPath.find("~/",0,2) == 0)
        return HomeDir() + inPath.substr(1);
    else
        return inPath;
}

string DLDbListCFPref::StripPathStuff(const string &inPath)
{
    if (inPath.find("/private/automount/Network/",0,27) == 0)
        return inPath.substr(18);
    if (inPath.find("/automount/Network/",0,19) == 0)
        return inPath.substr(10);
    if (inPath.find("/private/Network/",0,17) == 0)
        return inPath.substr(8);
    return inPath;
}

string DLDbListCFPref::AbbreviatedPath(const string &inPath)
{
    string path = StripPathStuff(inPath);
    string home = StripPathStuff(HomeDir() + "/");
    size_t homeLen = home.length();

    if (homeLen > 1 && path.find(home.c_str(), 0, homeLen) == 0)
        return "~" + path.substr(homeLen - 1);
    else
        return path;
}

CFDictionaryRef DLDbListCFPref::dlDbIdentifierToCFDictionaryRef(const DLDbIdentifier& dldbIdentifier)
{
	CFRef<CFMutableDictionaryRef> aDict(CFDictionaryCreateMutable(kCFAllocatorDefault,0,
		&kCFTypeDictionaryKeyCallBacks,&kCFTypeDictionaryValueCallBacks));
    if (!aDict)
        throw ::std::bad_alloc();

    // Put SUBSERVICE_UID in dictionary
    char buffer[Guid::stringRepLength+1];
    const CssmSubserviceUid& ssuid=dldbIdentifier.ssuid();
    const Guid &theGuid = Guid::overlay(ssuid.Guid);
    CFRef<CFStringRef> stringGuid(::CFStringCreateWithCString(kCFAllocatorDefault,
            theGuid.toString(buffer),kCFStringEncodingMacRoman));
    if (stringGuid)
        ::CFDictionarySetValue(aDict,kKeyGUID,stringGuid);
        
    if (ssuid.SubserviceId!=0)
    {
        CFRef<CFNumberRef> subserviceId(::CFNumberCreate(kCFAllocatorDefault,kCFNumberSInt32Type,&ssuid.SubserviceId));
        if (subserviceId)
            ::CFDictionarySetValue(aDict,kKeySubserviceId,subserviceId);
    }
    if (ssuid.SubserviceType!=0)
    {
        CFRef<CFNumberRef> subserviceType(CFNumberCreate(kCFAllocatorDefault,kCFNumberSInt32Type,&ssuid.SubserviceType));
        if (subserviceType)
            ::CFDictionarySetValue(aDict,kKeySubserviceType,subserviceType);
    }
    if (ssuid.Version.Major!=0 && ssuid.Version.Minor!=0)
    {
        CFRef<CFNumberRef> majorVersion(::CFNumberCreate(kCFAllocatorDefault,kCFNumberSInt32Type,&ssuid.Version.Major));
        if (majorVersion)
            ::CFDictionarySetValue(aDict,kKeyMajorVersion,majorVersion);
        CFRef<CFNumberRef> minorVersion(::CFNumberCreate(kCFAllocatorDefault,kCFNumberSInt32Type,&ssuid.Version.Minor));
        if (minorVersion)
            ::CFDictionarySetValue(aDict,kKeyMinorVersion,minorVersion);
    }
    
    // Put DbName in dictionary
	const char *dbName=dldbIdentifier.dbName();
    if (dbName)
    {
        CFRef<CFStringRef> theDbName(::CFStringCreateWithCString(kCFAllocatorDefault,AbbreviatedPath(dbName).c_str(),kCFStringEncodingMacRoman));
        ::CFDictionarySetValue(aDict,kKeyDbName,theDbName);
    }
    // Put DbLocation in dictionary
	const CSSM_NET_ADDRESS *dbLocation=dldbIdentifier.dbLocation();
    if (dbLocation!=NULL && dbLocation->AddressType!=CSSM_ADDR_NONE)
    {
        CFRef<CFDataRef> theData(::CFDataCreate(kCFAllocatorDefault,dbLocation->Address.Data,dbLocation->Address.Length));
        if (theData)
            ::CFDictionarySetValue(aDict,kKeyDbLocation,theData);
    }

    ::CFRetain(aDict);
	return aDict;
}
bool DLDbListCFPref::revert(bool force)
{ 

	// if the prefs have not been refreshed in the last 5 seconds force a reload
	if (force || CFAbsoluteTimeGetCurrent() - mPrefsTimeStamp > kDLDbListCFPrefRevertInterval)
	{
		clear();
		::CFPreferencesSynchronize(mPrefsDomain, kCFPreferencesCurrentUser, kCFPreferencesAnyHost);
	    loadOrCreate();
		return true;  // @@@ Be smarter about when something *really* changed
	}

	return false;
}

