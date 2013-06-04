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


/*
    DLDBListCFPref.cpp
*/

#include "DLDBListCFPref.h"
#include <Security/cssmapple.h>
#include <security_utilities/debugging.h>
#include <security_utilities/utilities.h>
#include <memory>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <pwd.h>
#include <sys/param.h>
#include <copyfile.h>
#include <xpc/private.h>
#include <syslog.h>
#include <sandbox.h>

dispatch_once_t AppSandboxChecked;
xpc_object_t KeychainHomeFromXPC;

using namespace CssmClient;

static const double kDLDbListCFPrefRevertInterval = 30.0;

// normal debug calls, which get stubbed out for deployment builds

#define kKeyGUID CFSTR("GUID")
#define kKeySubserviceId CFSTR("SubserviceId")
#define kKeySubserviceType CFSTR("SubserviceType")
#define kKeyDbName CFSTR("DbName")
#define kKeyDbLocation CFSTR("DbLocation")
#define kKeyActive CFSTR("Active")
#define kKeyMajorVersion CFSTR("MajorVersion")
#define kKeyMinorVersion CFSTR("MinorVersion")
#define kDefaultDLDbListKey CFSTR("DLDBSearchList")
#define kDefaultKeychainKey CFSTR("DefaultKeychain")
#define kLoginKeychainKey CFSTR("LoginKeychain")
#define kUserDefaultPath "~/Library/Preferences/com.apple.security.plist"
#define kSystemDefaultPath "/Library/Preferences/com.apple.security.plist"
#define kCommonDefaultPath "/Library/Preferences/com.apple.security-common.plist"
#define kLoginKeychainPathPrefix "~/Library/Keychains/"
#define kUserLoginKeychainPath "~/Library/Keychains/login.keychain"
#define kSystemLoginKeychainPath "/Library/Keychains/System.keychain"


// A utility class for managing password database lookups

const time_t kPasswordCacheExpire = 30; // number of seconds cached password db info is valid

PasswordDBLookup::PasswordDBLookup () : mValid (false), mCurrent (0), mTime (0)
{
}

void PasswordDBLookup::lookupInfoOnUID (uid_t uid)
{
    time_t currentTime = time (NULL);
    
    if (!mValid || uid != mCurrent || currentTime - mTime >= kPasswordCacheExpire)
    {
        struct passwd* pw = getpwuid(uid);
		if (pw == NULL)
		{
			UnixError::throwMe (EPERM);
		}
		
        mDirectory = pw->pw_dir;
        mName = pw->pw_name;
        mValid = true;
        mCurrent = uid;
        mTime = currentTime;

        secdebug("secpref", "uid=%d caching home=%s", uid, pw->pw_dir);

        endpwent();
    }
}

PasswordDBLookup *DLDbListCFPref::mPdbLookup = NULL;

//-------------------------------------------------------------------------------------
//
//			Lists of DL/DBs, with CFPreferences backing store
//
//-------------------------------------------------------------------------------------

DLDbListCFPref::DLDbListCFPref(SecPreferencesDomain domain) : mDomain(domain), mPropertyList(NULL), mChanged(false),
    mSearchListSet(false), mDefaultDLDbIdentifierSet(false), mLoginDLDbIdentifierSet(false)
{
    secdebug("secpref", "New DLDbListCFPref %p for domain %d", this, domain);
	loadPropertyList(true);
}

void DLDbListCFPref::set(SecPreferencesDomain domain)
{
	save();

	mDomain = domain;

    secdebug("secpref", "DLDbListCFPref %p domain set to %d", this, domain);

	if (loadPropertyList(true))
        resetCachedValues();
}

DLDbListCFPref::~DLDbListCFPref()
{
    save();

	if (mPropertyList)
		CFRelease(mPropertyList);
}

void
DLDbListCFPref::forceUserSearchListReread()
{
	// set mPrefsTimeStamp so that it will "expire" the next time loadPropertyList is called
	mPrefsTimeStamp = CFAbsoluteTimeGetCurrent() - kDLDbListCFPrefRevertInterval;
}

bool
DLDbListCFPref::loadPropertyList(bool force)
{
    string prefsPath;
	
	switch (mDomain)
    {
	case kSecPreferencesDomainUser:
		prefsPath = ExpandTildesInPath(kUserDefaultPath);
		break;
	case kSecPreferencesDomainSystem:
		prefsPath = kSystemDefaultPath;
		break;
	case kSecPreferencesDomainCommon:
		prefsPath = kCommonDefaultPath;
		break;
	default:
		MacOSError::throwMe(errSecInvalidPrefsDomain);
	}

	secdebug("secpref", "force=%s prefsPath=%s", force ? "true" : "false",
		prefsPath.c_str());

	CFAbsoluteTime now = CFAbsoluteTimeGetCurrent();

    // If for some reason the prefs file path has changed, blow away the old plist and force an update
    if (mPrefsPath != prefsPath)
    {
        mPrefsPath = prefsPath;
        if (mPropertyList)
        {
            CFRelease(mPropertyList);
            mPropertyList = NULL;
        }

		mPrefsTimeStamp = now;
    }
	else if (!force)
	{
		if (now - mPrefsTimeStamp < kDLDbListCFPrefRevertInterval)
			return false;

		mPrefsTimeStamp = now;
	}

	struct stat st;
	if (stat(mPrefsPath.c_str(), &st))
	{
		if (errno == ENOENT)
		{
			if (mPropertyList)
			{
				if (CFDictionaryGetCount(mPropertyList) == 0)
					return false;
				CFRelease(mPropertyList);
			}

			mPropertyList = CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
			return true;
		}
	}
	else
	{
		if (mPropertyList)
		{
			if (mTimespec.tv_sec == st.st_mtimespec.tv_sec
				&& mTimespec.tv_nsec == st.st_mtimespec.tv_nsec)
				return false;
		}

		mTimespec = st.st_mtimespec;
	}

	CFMutableDictionaryRef thePropertyList = NULL;
	CFMutableDataRef xmlData = NULL;
	CFStringRef errorString = NULL;
	int fd = -1;

	do
	{
		fd = open(mPrefsPath.c_str(), O_RDONLY, 0);
		if (fd < 0)
			break;

		off_t theSize = lseek(fd, 0, SEEK_END);
		if (theSize <= 0)
			break;

		if (lseek(fd, 0, SEEK_SET))
			break;

		xmlData = CFDataCreateMutable(NULL, CFIndex(theSize));
		if (!xmlData)
			break;
		CFDataSetLength(xmlData, CFIndex(theSize));
		void *buffer = reinterpret_cast<void *>(CFDataGetMutableBytePtr(xmlData));
		if (!buffer)
			break;
		ssize_t bytesRead = read(fd, buffer, theSize);
		if (bytesRead != theSize)
			break;

		thePropertyList = CFMutableDictionaryRef(CFPropertyListCreateFromXMLData(NULL, xmlData, kCFPropertyListMutableContainers, &errorString));
		if (!thePropertyList)
			break;

		if (CFGetTypeID(thePropertyList) != CFDictionaryGetTypeID())
		{
			CFRelease(thePropertyList);
			thePropertyList = NULL;
			break;
		}
	} while (0);

	if (fd >= 0)
		close(fd);
	if (xmlData)
		CFRelease(xmlData);
	if (errorString)
		CFRelease(errorString);

	if (!thePropertyList)
	{
		thePropertyList = CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
	}

	if (mPropertyList)
	{
		if (CFEqual(mPropertyList, thePropertyList))
		{
            // The new property list is the same as the old one, so nothing has changed.
			CFRelease(thePropertyList);
			return false;
		}
		CFRelease(mPropertyList);
	}

	mPropertyList = thePropertyList;
	return true;
}

void
DLDbListCFPref::writePropertyList()
{
	if (!mPropertyList || CFDictionaryGetCount(mPropertyList) == 0)
	{
		// There is nothing in the mPropertyList dictionary,
		// so we don't need a prefs file.
		unlink(mPrefsPath.c_str());
	}
	else
	{
		if(testAndFixPropertyList()) 
			return;
		
		CFDataRef xmlData = CFPropertyListCreateXMLData(NULL, mPropertyList);
		if (!xmlData)
			return; // Bad out of memory or something evil happened let's act like CF and do nothing.
			
		// The prefs file should at least be made readable by user/group/other and writable by the owner.
		// Change from euid to ruid if needed for the duration of the new prefs file creat.
		
		mode_t mode = 0666;
		changeIdentity(UNPRIV);
		int fd = open(mPrefsPath.c_str(), O_WRONLY|O_CREAT|O_TRUNC, mode);
		changeIdentity(PRIV);
		if (fd >= 0)
		{
			const void *buffer = CFDataGetBytePtr(xmlData);
			size_t toWrite = CFDataGetLength(xmlData);
			/* ssize_t bytesWritten = */ write(fd, buffer, toWrite);
			// Emulate CFPreferences by not checking for any errors.
	
			fsync(fd);
			struct stat st;
			if (!fstat(fd, &st))
				mTimespec = st.st_mtimespec;

			close(fd);
		}

		CFRelease(xmlData);
	}

	mPrefsTimeStamp = CFAbsoluteTimeGetCurrent();
}

// This function can clean up some problems caused by setuid clients.  We've had instances where the
// Keychain search list has become owned by root, but is still able to be re-written by the user because
// of the permissions on the directory above.  We'll take advantage of that fact to recreate the file with
// the correct ownership by copying it.

int
DLDbListCFPref::testAndFixPropertyList()
{
	char *prefsPath = (char *)mPrefsPath.c_str();
	
	int fd1, fd2, retval;
	struct stat stbuf;

	if((fd1 = open(prefsPath, O_RDONLY)) < 0) {
		if (errno == ENOENT) return 0; // Doesn't exist - the default case
		else return -1;
	}
	
	if((retval = fstat(fd1, &stbuf)) == -1) return -1;
		
	if(stbuf.st_uid != getuid()) {
		char tempfile[MAXPATHLEN+1];

		snprintf(tempfile, MAXPATHLEN, "%s.XXXXX", prefsPath);
		mktemp(tempfile);
		changeIdentity(UNPRIV);
		if((fd2 = open(tempfile, O_RDWR | O_CREAT | O_EXCL, 0666)) < 0) {
			retval = -1;
		} else {
			copyfile_state_t s = copyfile_state_alloc();
			retval = fcopyfile(fd1, fd2, s, COPYFILE_DATA);
			copyfile_state_free(s);
			if(!retval) retval = ::unlink(prefsPath);
			if(!retval) retval = ::rename(tempfile, prefsPath);
		}
		changeIdentity(PRIV);
		close(fd2);
	}
	close(fd1);
	return retval;
}

// Encapsulated process uid/gid change routine.
void 
DLDbListCFPref::changeIdentity(ID_Direction toPriv)
{
	if(toPriv == UNPRIV) {
		savedEUID = geteuid();
		savedEGID = getegid();
		if(savedEGID != getgid()) setegid(getgid());
		if(savedEUID != getuid()) seteuid(getuid());
	} else {
		if(savedEUID != getuid()) seteuid(savedEUID);
		if(savedEGID != getgid()) setegid(savedEGID);
	}
}

void
DLDbListCFPref::resetCachedValues()
{
	// Unset the login and default Keychain.
	mLoginDLDbIdentifier = mDefaultDLDbIdentifier = DLDbIdentifier();

	// Clear the searchList.
	mSearchList.clear();

	changed(false);

    // Note that none of our cached values are valid
    mSearchListSet = mDefaultDLDbIdentifierSet = mLoginDLDbIdentifierSet = false;

	mPrefsTimeStamp = CFAbsoluteTimeGetCurrent();
}

void DLDbListCFPref::save()
{
    if (!hasChanged())
        return;

	// Resync from disc to make sure we don't clobber anyone elses changes.
	// @@@ This is probably already done by the next layer up so we don't
	// really need to do it here again.
	loadPropertyList(true);

    // Do the searchList first since it might end up invoking defaultDLDbIdentifier() which can set
    // mLoginDLDbIdentifierSet and mDefaultDLDbIdentifierSet to true.
    if (mSearchListSet)
    {
        // Make a temporary CFArray with the contents of the vector
        if (mSearchList.size() == 1 && mSearchList[0] == defaultDLDbIdentifier() && mSearchList[0] == LoginDLDbIdentifier())
        {
            // The only element in the search list is the default keychain, which is a
            // post Jaguar style login keychain, so omit the entry from the prefs file.
            CFDictionaryRemoveValue(mPropertyList, kDefaultDLDbListKey);
        }
        else
        {
            CFMutableArrayRef searchArray = CFArrayCreateMutable(kCFAllocatorDefault, mSearchList.size(), &kCFTypeArrayCallBacks);
            for (DLDbList::const_iterator ix=mSearchList.begin();ix!=mSearchList.end();ix++)
            {
                CFDictionaryRef aDict = dlDbIdentifierToCFDictionaryRef(*ix);
                CFArrayAppendValue(searchArray, aDict);
                CFRelease(aDict);
            }
    
            CFDictionarySetValue(mPropertyList, kDefaultDLDbListKey, searchArray);
            CFRelease(searchArray);
        }
    }

    if (mLoginDLDbIdentifierSet)
    {
        // Make a temporary CFArray with the login keychain
        CFArrayRef loginArray = NULL;
        if (!mLoginDLDbIdentifier)
        {
            loginArray = CFArrayCreate(kCFAllocatorDefault, NULL, 0, &kCFTypeArrayCallBacks);
        }
        else if (!(mLoginDLDbIdentifier == LoginDLDbIdentifier()))
        {
            CFDictionaryRef aDict = dlDbIdentifierToCFDictionaryRef(mLoginDLDbIdentifier);
            const void *value = reinterpret_cast<const void *>(aDict);
            loginArray = CFArrayCreate(kCFAllocatorDefault, &value, 1, &kCFTypeArrayCallBacks);
            CFRelease(aDict);
        }
    
        if (loginArray)
        {
            CFDictionarySetValue(mPropertyList, kLoginKeychainKey, loginArray);
            CFRelease(loginArray);
        }
        else
            CFDictionaryRemoveValue(mPropertyList, kLoginKeychainKey);
    }

    if (mDefaultDLDbIdentifierSet)
    {
        // Make a temporary CFArray with the default keychain
        CFArrayRef defaultArray = NULL;
        if (!mDefaultDLDbIdentifier)
        {
            defaultArray = CFArrayCreate(kCFAllocatorDefault, NULL, 0, &kCFTypeArrayCallBacks);
        }
        else if (!(mDefaultDLDbIdentifier == LoginDLDbIdentifier()))
        {
            CFDictionaryRef aDict = dlDbIdentifierToCFDictionaryRef(mDefaultDLDbIdentifier);
            const void *value = reinterpret_cast<const void *>(aDict);
            defaultArray = CFArrayCreate(kCFAllocatorDefault, &value, 1, &kCFTypeArrayCallBacks);
            CFRelease(aDict);
        }
    
        if (defaultArray)
        {
            CFDictionarySetValue(mPropertyList, kDefaultKeychainKey, defaultArray);
            CFRelease(defaultArray);
        }
        else
            CFDictionaryRemoveValue(mPropertyList, kDefaultKeychainKey);
    }

	writePropertyList();
    changed(false);
}


//----------------------------------------------------------------------
//			Conversions
//----------------------------------------------------------------------

DLDbIdentifier DLDbListCFPref::LoginDLDbIdentifier()
{
	CSSM_VERSION theVersion={};
    CssmSubserviceUid ssuid(gGuidAppleCSPDL,&theVersion,0,CSSM_SERVICE_DL|CSSM_SERVICE_CSP);
	CssmNetAddress *dbLocation=NULL;

	switch (mDomain) {
	case kSecPreferencesDomainUser:
		return DLDbIdentifier(ssuid, ExpandTildesInPath(kUserLoginKeychainPath).c_str(), dbLocation);
	default:
		assert(false);
	case kSecPreferencesDomainSystem:
	case kSecPreferencesDomainCommon:
		return DLDbIdentifier(ssuid, kSystemLoginKeychainPath, dbLocation);
	}
}

DLDbIdentifier DLDbListCFPref::JaguarLoginDLDbIdentifier()
{
	CSSM_VERSION theVersion={};
    CssmSubserviceUid ssuid(gGuidAppleCSPDL,&theVersion,0,CSSM_SERVICE_DL|CSSM_SERVICE_CSP);
	CssmNetAddress *dbLocation=NULL;

	switch (mDomain) {
	case kSecPreferencesDomainUser:
    {
        string basepath = ExpandTildesInPath(kLoginKeychainPathPrefix) + getPwInfo(kUsername);
        return DLDbIdentifier(ssuid,basepath.c_str(),dbLocation);
    }
	case kSecPreferencesDomainSystem:
	case kSecPreferencesDomainCommon:
		return DLDbIdentifier(ssuid, kSystemLoginKeychainPath, dbLocation);
	default:
		assert(false);
		return DLDbIdentifier();
	}
}

DLDbIdentifier DLDbListCFPref::makeDLDbIdentifier (const CSSM_GUID &guid, const CSSM_VERSION &version,
												   uint32 subserviceId, CSSM_SERVICE_TYPE subserviceType,
												   const char* dbName, CSSM_NET_ADDRESS *dbLocation)
{
	CssmSubserviceUid ssuid (guid, &version, subserviceId, subserviceType);
	return DLDbIdentifier (ssuid, ExpandTildesInPath (dbName).c_str (), dbLocation);
}

DLDbIdentifier DLDbListCFPref::cfDictionaryRefToDLDbIdentifier(CFDictionaryRef theDict)
{
    // We must get individual values from the dictionary and store in basic types
	if (CFGetTypeID(theDict) != CFDictionaryGetTypeID())
		throw std::logic_error("wrong type in property list");

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
    
	return makeDLDbIdentifier (guid, theVersion, subserviceId, subserviceType, dbName.c_str (), dbLocation);
}

void DLDbListCFPref::clearPWInfo ()
{
    if (mPdbLookup != NULL)
    {
        delete mPdbLookup;
        mPdbLookup = NULL;
    }
}

string DLDbListCFPref::getPwInfo(PwInfoType type)
{
    const char *value;
    switch (type)
    {
    case kHomeDir:
		if (KeychainHomeFromXPC) {
			value = xpc_string_get_string_ptr(KeychainHomeFromXPC);
		} else {
			value = getenv("HOME");
		}
        if (value)
            return value;
        break;
    case kUsername:
        value = getenv("USER");
        if (value)
            return value;
        break;
    }

	// Get our effective uid
	uid_t uid = geteuid();
	// If we are setuid root use the real uid instead
	if (!uid) uid = getuid();

    // get the password entries
    if (mPdbLookup == NULL)
    {
        mPdbLookup = new PasswordDBLookup ();
    }
    
    mPdbLookup->lookupInfoOnUID (uid);
    
    string result;
    switch (type)
    {
    case kHomeDir:
        result = mPdbLookup->getDirectory ();
        break;
    case kUsername:
        result = mPdbLookup->getName ();
        break;
    }

	return result;
}

static void check_app_sandbox()
{
	if (!_xpc_runtime_is_app_sandboxed()) {
		// We are not in a sandbox, no work to do here
		return;
	}
	
	extern xpc_object_t xpc_create_with_format(const char * format, ...);
	xpc_connection_t con = xpc_connection_create("com.apple.security.XPCKeychainSandboxCheck", NULL);
    xpc_connection_set_event_handler(con, ^(xpc_object_t event) {
        xpc_type_t xtype = xpc_get_type(event);
        if (XPC_TYPE_ERROR == xtype) {
            syslog(LOG_ERR, "Keychain sandbox connection error: %s\n", xpc_dictionary_get_string(event, XPC_ERROR_KEY_DESCRIPTION));
        } else {
            syslog(LOG_ERR, "Keychain sandbox unexpected connection event %p\n", event);
        }
    });
    xpc_connection_resume(con);
    
    xpc_object_t message = xpc_create_with_format("{op: GrantKeychainPaths}");
	xpc_object_t reply = xpc_connection_send_message_with_reply_sync(con, message);
	xpc_type_t xtype = xpc_get_type(reply);
	if (XPC_TYPE_DICTIONARY == xtype) {
#if 0
		// This is useful for debugging.
		char *debug = xpc_copy_description(reply);
		syslog(LOG_ERR, "DEBUG (KCsandbox) %s\n", debug);
		free(debug);
#endif
		
		xpc_object_t extensions_array = xpc_dictionary_get_value(reply, "extensions");
		xpc_array_apply(extensions_array, ^(size_t index, xpc_object_t extension) {
			char pbuf[MAXPATHLEN];
			char *path = pbuf;
			int status = sandbox_consume_fs_extension(xpc_string_get_string_ptr(extension), &path);
			if (status) {
				syslog(LOG_ERR, "Keychain sandbox consume extension error: s=%d p=%s %m\n", status, path);
			}
			return (bool)true;
		});
		
		KeychainHomeFromXPC = xpc_dictionary_get_value(reply, "keychain-home");
		xpc_retain(KeychainHomeFromXPC);
		xpc_release(con);
	} else if (XPC_TYPE_ERROR == xtype) {
		syslog(LOG_ERR, "Keychain sandbox message error: %s\n", xpc_dictionary_get_string(reply, XPC_ERROR_KEY_DESCRIPTION));
	} else {
		syslog(LOG_ERR, "Keychain sandbox unexpected message reply type %p\n", xtype);
	}
    xpc_release(message);
	xpc_release(reply);
}



string DLDbListCFPref::ExpandTildesInPath(const string &inPath)
{
	dispatch_once(&AppSandboxChecked, ^{
		check_app_sandbox();
	});
	
	if ((short)inPath.find("~/",0,2) == 0)
        return getPwInfo(kHomeDir) + inPath.substr(1);
    else
        return inPath;
}

string DLDbListCFPref::StripPathStuff(const string &inPath)
{
    if (inPath.find("/private/var/automount/Network/",0,31) == 0)
        return inPath.substr(22);
    if (inPath.find("/private/automount/Servers/",0,27) == 0)
        return "/Network" + inPath.substr(18);
    if (inPath.find("/automount/Servers/",0,19) == 0)
        return "/Network" + inPath.substr(10);
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
    string home = StripPathStuff(getPwInfo(kHomeDir) + "/");
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
        CFRef<CFStringRef> theDbName(::CFStringCreateWithCString(kCFAllocatorDefault,AbbreviatedPath(dbName).c_str(),kCFStringEncodingUTF8));
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
	// If the prefs have not been refreshed in the last kDLDbListCFPrefRevertInterval
	// seconds or we are asked to force a reload, then reload.
	if (!loadPropertyList(force))
		return false;

	resetCachedValues();
	return true;
}

void
DLDbListCFPref::add(const DLDbIdentifier &dldbIdentifier)
{
	// convert the location specified in dldbIdentifier to a standard form
	// make a canonical form of the database name
	std::string canon = ExpandTildesInPath(AbbreviatedPath(dldbIdentifier.dbName()).c_str());

	DLDbIdentifier localIdentifier  (dldbIdentifier.ssuid(), canon.c_str(), dldbIdentifier.dbLocation ());
	
	if (member(localIdentifier))
		return;

    mSearchList.push_back(localIdentifier);
    changed(true);
}

void
DLDbListCFPref::remove(const DLDbIdentifier &dldbIdentifier)
{
    // Make sure mSearchList is set
    searchList();
    for (vector<DLDbIdentifier>::iterator ix = mSearchList.begin(); ix != mSearchList.end(); ++ix)
	{
		if (*ix==dldbIdentifier)		// found in list
		{
			mSearchList.erase(ix);
			changed(true);
			break;
		}
	}
}

void
DLDbListCFPref::rename(const DLDbIdentifier &oldId, const DLDbIdentifier &newId)
{
    // Make sure mSearchList is set
    searchList();
    for (vector<DLDbIdentifier>::iterator ix = mSearchList.begin();
		ix != mSearchList.end(); ++ix)
	{
		if (*ix==oldId)
		{
			// replace oldId with newId
			*ix = newId;
			changed(true);
		}
		else if (*ix==newId)
		{
			// remove newId except where we just inserted it
			mSearchList.erase(ix);
			changed(true);
		}
	}
}

bool
DLDbListCFPref::member(const DLDbIdentifier &dldbIdentifier)
{
    if (dldbIdentifier.IsImplEmpty())
    {
        return false;
    }
    
    for (vector<DLDbIdentifier>::const_iterator ix = searchList().begin(); ix != mSearchList.end(); ++ix)
	{
        if (ix->mImpl == NULL)
        {
            continue;
        }
        
		// compare the dldbIdentifiers based on the full, real path to the keychain
		if (ix->ssuid() == dldbIdentifier.ssuid())
		{
			char localPath[PATH_MAX],
				 inPath[PATH_MAX];
			
			// try to resolve these down to a canonical form
			const char* localPathPtr = cached_realpath(ix->dbName(), localPath);
			const char* inPathPtr = cached_realpath(dldbIdentifier.dbName(), inPath);

			// if either of the paths didn't resolve for some reason, use the originals
			if (localPathPtr == NULL)
			{
				localPathPtr = ix->dbName();
			}
			
			if (inPathPtr == NULL)
			{
				inPathPtr = dldbIdentifier.dbName();
			}
			
			if (strcmp(localPathPtr, inPathPtr) == 0)
			{
				return true;
			}
		}
	}

	return false;
}

const vector<DLDbIdentifier> &
DLDbListCFPref::searchList()
{
    if (!mSearchListSet)
    {
        CFArrayRef searchList = reinterpret_cast<CFArrayRef>(CFDictionaryGetValue(mPropertyList, kDefaultDLDbListKey));
        if (searchList && CFGetTypeID(searchList) != CFArrayGetTypeID())
            searchList = NULL;

        if (searchList)
        {
            CFIndex top = CFArrayGetCount(searchList);
            // Each entry is a CFDictionary; peel it off & add it to the array
            for (CFIndex idx = 0; idx < top; ++idx)
            {
                CFDictionaryRef theDict = reinterpret_cast<CFDictionaryRef>(CFArrayGetValueAtIndex(searchList, idx));
                try
                {
                    mSearchList.push_back(cfDictionaryRefToDLDbIdentifier(theDict));
                }
                catch (...)
                {
                    // Drop stuff that doesn't parse on the floor.
                }
            }
    
            // If there were entries specified, but they were invalid revert to using the
            // default keychain in the searchlist.
            if (top > 0 && mSearchList.size() == 0)
                searchList = NULL;
        }

        // The default when no search list is specified is to only search the
        // default keychain.
        if (!searchList && static_cast<bool>(defaultDLDbIdentifier()))
            mSearchList.push_back(mDefaultDLDbIdentifier);

        mSearchListSet = true;
    }

	return mSearchList;
}

void
DLDbListCFPref::searchList(const vector<DLDbIdentifier> &searchList)
{
	vector<DLDbIdentifier> newList(searchList);
	mSearchList.swap(newList);
    mSearchListSet = true;
    changed(true);
}

void
DLDbListCFPref::defaultDLDbIdentifier(const DLDbIdentifier &dlDbIdentifier)
{
	if (!(defaultDLDbIdentifier() == dlDbIdentifier))
	{
		mDefaultDLDbIdentifier = dlDbIdentifier;
		changed(true);
	}
}

const DLDbIdentifier &
DLDbListCFPref::defaultDLDbIdentifier()
{
	
    if (!mDefaultDLDbIdentifierSet)
    {
        CFArrayRef defaultArray = reinterpret_cast<CFArrayRef>(CFDictionaryGetValue(mPropertyList, kDefaultKeychainKey));
        if (defaultArray && CFGetTypeID(defaultArray) != CFArrayGetTypeID())
            defaultArray = NULL;

        if (defaultArray && CFArrayGetCount(defaultArray) > 0)
        {
            CFDictionaryRef defaultDict = reinterpret_cast<CFDictionaryRef>(CFArrayGetValueAtIndex(defaultArray, 0));
            try
            {
                secdebug("secpref", "getting default DLDbIdentifier from defaultDict");
                mDefaultDLDbIdentifier = cfDictionaryRefToDLDbIdentifier(defaultDict);
                secdebug("secpref", "now we think the default keychain is %s", (mDefaultDLDbIdentifier) ? mDefaultDLDbIdentifier.dbName() : "<NULL>");
            }
            catch (...)
            {
                // If defaultArray doesn't parse fall back on the default way of getting the default keychain
                defaultArray = NULL;
            }
        }
    
        if (!defaultArray)
        {
			
            // If the Panther style login keychain actually exists we use that otherwise no
            // default is set.
            mDefaultDLDbIdentifier = loginDLDbIdentifier();
            secdebug("secpref", "now we think the default keychain is: %s", (mDefaultDLDbIdentifier) ? mDefaultDLDbIdentifier.dbName() : 
			"Name doesn't exist");
			
			
			//"Name doesn't exist");

            struct stat st;
            int st_result = stat(mDefaultDLDbIdentifier.dbName(), &st);
			
			
            if (st_result)
            {
				secdebug("secpref", "stat(%s) -> %d", mDefaultDLDbIdentifier.dbName(), st_result);
                mDefaultDLDbIdentifier  = DLDbIdentifier(); // initialize a NULL keychain
                secdebug("secpref", "after DLDbIdentifier(), we think the default keychain is %s", static_cast<bool>(mDefaultDLDbIdentifier) ? mDefaultDLDbIdentifier.dbName() : "<NULL>");
            }
        }
		
        mDefaultDLDbIdentifierSet = true;
    }
	
	
	return mDefaultDLDbIdentifier;
}

void
DLDbListCFPref::loginDLDbIdentifier(const DLDbIdentifier &dlDbIdentifier)
{
	if (!(loginDLDbIdentifier() == dlDbIdentifier))
	{
		mLoginDLDbIdentifier = dlDbIdentifier;
		changed(true);
	}
}

const DLDbIdentifier &
DLDbListCFPref::loginDLDbIdentifier()
{
    if (!mLoginDLDbIdentifierSet)
    {
        CFArrayRef loginArray = reinterpret_cast<CFArrayRef>(CFDictionaryGetValue(mPropertyList, kLoginKeychainKey));
        if (loginArray && CFGetTypeID(loginArray) != CFArrayGetTypeID())
            loginArray = NULL;

        if (loginArray && CFArrayGetCount(loginArray) > 0)
        {
            CFDictionaryRef loginDict = reinterpret_cast<CFDictionaryRef>(CFArrayGetValueAtIndex(loginArray, 0));
            try
            {
                secdebug("secpref", "Getting login DLDbIdentifier from loginDict");
                mLoginDLDbIdentifier = cfDictionaryRefToDLDbIdentifier(loginDict);
                secdebug("secpref", "we think the login keychain is %s", static_cast<bool>(mLoginDLDbIdentifier) ? mLoginDLDbIdentifier.dbName() : "<NULL>");
            }
            catch (...)
            {
                // If loginArray doesn't parse fall back on the default way of getting the login keychain.
                loginArray = NULL;
            }
        }
    
        if (!loginArray)
        {
			mLoginDLDbIdentifier = LoginDLDbIdentifier();
			secdebug("secpref", "after LoginDLDbIdentifier(), we think the login keychain is %s", static_cast<bool>(mLoginDLDbIdentifier) ? mLoginDLDbIdentifier.dbName() : "<NULL>");
        }

        mLoginDLDbIdentifierSet = true;
    }

	return mLoginDLDbIdentifier;
}
