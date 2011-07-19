/*
 *  Copyright (c) 2003-2004 Apple Computer, Inc. All Rights Reserved.
 *
 *  @APPLE_LICENSE_HEADER_START@
 *  
 *  This file contains Original Code and/or Modifications of Original Code
 *  as defined in and that are subject to the Apple Public Source License
 *  Version 2.0 (the 'License'). You may not use this file except in
 *  compliance with the License. Please obtain a copy of the License at
 *  http://www.opensource.apple.com/apsl/ and read it before using this
 *  file.
 *  
 *  The Original Code and all software distributed under the License are
 *  distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 *  EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 *  INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 *  FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 *  Please see the License for the specific language governing rights and
 *  limitations under the License.
 *  
 *  @APPLE_LICENSE_HEADER_END@
 *
 *  AuthorizationDBPlist.cpp
 *  Security
 *
 */

#include "AuthorizationDBPlist.h"
#include <security_utilities/logging.h>
#include <System/sys/fsctl.h>

// mLock is held when the database is changed
// mReadWriteLock is held when the file on disk is changed
// during load(), save() and parseConfig() mLock is assumed

namespace Authorization {

AuthorizationDBPlist::AuthorizationDBPlist(const char *configFile) : 
    mFileName(configFile), mLastChecked(DBL_MIN)
{
	memset(&mRulesFileMtimespec, 0, sizeof(mRulesFileMtimespec));
}

void AuthorizationDBPlist::sync(CFAbsoluteTime now)
{
	if (mRules.empty()) {
		StLock<Mutex> _(mLock);
		load();
	} else {
		// Don't do anything if we checked the timestamp less than 5 seconds ago
		if (mLastChecked > now - 5.0) {
			secdebug("authdb", "no sync: last reload %.0f + 5 > %.0f", 
				mLastChecked, now);
			return;
		}
		
		{
			struct stat st;
			{
				StLock<Mutex> _(mReadWriteLock);
				if (stat(mFileName.c_str(), &st)) {
					Syslog::error("Stating rules file \"%s\": %s", mFileName.c_str(), 
						strerror(errno));
					return;
				}
			}

			if (memcmp(&st.st_mtimespec, &mRulesFileMtimespec, sizeof(mRulesFileMtimespec))) {
				StLock<Mutex> _(mLock);
				load();
			}
		}
	}
}

void AuthorizationDBPlist::save()
{
	if (!mConfig)
		return;

	StLock<Mutex> _(mReadWriteLock);

    secdebug("authdb", "policy db changed, saving to disk.");
	int fd = -1;
	string tempFile = mFileName + ",";
	
	for (;;) {
		fd = open(tempFile.c_str(), O_WRONLY|O_CREAT|O_EXCL, 0644);
		if (fd == -1) {
			if (errno == EEXIST) {
				unlink(tempFile.c_str());
				continue;
			}
			if (errno == EINTR)
				continue;
			else
				break;
		} else
			break;
	}
			
	if (fd == -1) {
		Syslog::error("Saving rules file \"%s\": %s", tempFile.c_str(), 
                strerror(errno));
		return;
	}

	CFDataRef configXML = CFPropertyListCreateXMLData(NULL, mConfig);
	if (!configXML)
		return;

	CFIndex configSize = CFDataGetLength(configXML);
	ssize_t bytesWritten = write(fd, CFDataGetBytePtr(configXML), configSize);
	CFRelease(configXML);
	
	if (bytesWritten != configSize) {
		if (bytesWritten == -1)
			Syslog::error("Problem writing rules file \"%s\": (errno=%s)", 
                    tempFile.c_str(), strerror(errno));
		else
			Syslog::error("Problem writing rules file \"%s\": "
                "only wrote %lu out of %ld bytes",
				tempFile.c_str(), bytesWritten, configSize);

		close(fd);
		unlink(tempFile.c_str());
	}
	else
	{
		if (-1 == fcntl(fd, F_FULLFSYNC, NULL))
			fsync(fd);

		close(fd);
		int fd2 = open (mFileName.c_str(), O_RDONLY);
		if (rename(tempFile.c_str(), mFileName.c_str()))
		{
			close(fd2);
			unlink(tempFile.c_str());
		}
		else
		{
			/* force a sync to flush the journal */
			int flags = FSCTL_SYNC_WAIT|FSCTL_SYNC_FULLSYNC;
			ffsctl(fd2, FSCTL_SYNC_VOLUME, &flags, sizeof(flags));
			close(fd2);
			mLastChecked = CFAbsoluteTimeGetCurrent(); // we have the copy that's on disk now, so don't go loading it right away
		}
	}
}

void AuthorizationDBPlist::load()
{
	StLock<Mutex> _(mReadWriteLock);
	CFDictionaryRef configPlist;

    secdebug("authdb", "(re)loading policy db from disk.");    
	int fd = open(mFileName.c_str(), O_RDONLY, 0);
	if (fd == -1) {
		Syslog::error("Problem opening rules file \"%s\": %s", 
                mFileName.c_str(), strerror(errno));
		return;
	}

	struct stat st;
	if (fstat(fd, &st)) {
		int error = errno;
		close(fd);
		UnixError::throwMe(error);
	}

	mRulesFileMtimespec = st.st_mtimespec;
	off_t fileSize = st.st_size;
	CFMutableDataRef xmlData = CFDataCreateMutable(NULL, fileSize);
	CFDataSetLength(xmlData, fileSize);
	void *buffer = CFDataGetMutableBytePtr(xmlData);
	ssize_t bytesRead = read(fd, buffer, fileSize);
	if (bytesRead != fileSize) {
		if (bytesRead == -1) {
			Syslog::error("Problem reading rules file \"%s\": %s", 
                    mFileName.c_str(), strerror(errno));
			goto cleanup;
		}
		Syslog::error("Problem reading rules file \"%s\": "
                "only read %ul out of %ul bytes",
				bytesRead, fileSize, mFileName.c_str());
		goto cleanup;
	}

	CFStringRef errorString;
	configPlist = reinterpret_cast<CFDictionaryRef>(CFPropertyListCreateFromXMLData(NULL, xmlData, kCFPropertyListMutableContainersAndLeaves, &errorString));
	
	if (!configPlist) {
		char buffer[512];
		const char *error = CFStringGetCStringPtr(errorString, 
                kCFStringEncodingUTF8);
		if (error == NULL) {
			if (CFStringGetCString(errorString, buffer, 512, 
                        kCFStringEncodingUTF8))
				error = buffer;
		}

		Syslog::error("Parsing rules file \"%s\": %s", 
                mFileName.c_str(), error);
		if (errorString)
			CFRelease(errorString);
		
		goto cleanup;
	}

	if (CFGetTypeID(configPlist) != CFDictionaryGetTypeID()) {

		Syslog::error("Rules file \"%s\": is not a dictionary", 
                mFileName.c_str());

		goto cleanup;
	}

	parseConfig(configPlist);

cleanup:
	if (xmlData)
		CFRelease(xmlData);
	if (configPlist)
		CFRelease(configPlist);

	close(fd);

	// If all went well, we have the copy that's on disk now, so don't go loading it right away
	mLastChecked = CFAbsoluteTimeGetCurrent();
}

void AuthorizationDBPlist::parseConfig(CFDictionaryRef config)
{
	CFStringRef rightsKey = CFSTR("rights");
	CFStringRef rulesKey = CFSTR("rules");
	CFMutableDictionaryRef newRights = NULL;
	CFMutableDictionaryRef newRules = NULL;

	if (!config)
	{
		Syslog::alert("Failed to parse config, no config");
		MacOSError::throwMe(errAuthorizationInternal); 
	}

	if (CFDictionaryContainsKey(config, rulesKey))
		newRules = reinterpret_cast<CFMutableDictionaryRef>(const_cast<void*>(CFDictionaryGetValue(config, rulesKey)));

	if (CFDictionaryContainsKey(config, rightsKey))
		newRights = reinterpret_cast<CFMutableDictionaryRef>(const_cast<void*>(CFDictionaryGetValue(config, rightsKey)));
	
	if (newRules && newRights 
		&& (CFDictionaryGetTypeID() == CFGetTypeID(newRules)) 
		&& (CFDictionaryGetTypeID() == CFGetTypeID(newRights))) 
    {
        mConfigRights = static_cast<CFMutableDictionaryRef>(newRights);
        mConfigRules = static_cast<CFMutableDictionaryRef>(newRules);
		mRules.clear();
		try {
			CFDictionaryApplyFunction(newRights, parseRule, this);
		} catch (...) {
			Syslog::alert("Failed to parse config and apply dictionary function");
			MacOSError::throwMe(errAuthorizationInternal); // XXX/cs invalid rule file
		}
		mConfig = config;
	}
	else 
	{
		Syslog::alert("Failed to parse config, invalid rule file");
		MacOSError::throwMe(errAuthorizationInternal); // XXX/cs invalid rule file
	}
}

void AuthorizationDBPlist::parseRule(const void *key, const void *value, void *context)
{
	static_cast<AuthorizationDBPlist*>(context)->addRight(static_cast<CFStringRef>(key), static_cast<CFDictionaryRef>(value));
}

void AuthorizationDBPlist::addRight(CFStringRef key, CFDictionaryRef definition)
{
	string keyString = cfString(key);
	mRules[keyString] = Rule(keyString, definition, mConfigRules);
}

bool
AuthorizationDBPlist::validateRule(string inRightName, CFDictionaryRef inRightDefinition) const
{
    if (!mConfigRules ||
        0 == CFDictionaryGetCount(mConfigRules)) {
        Syslog::error("No rule definitions!");
        MacOSError::throwMe(errAuthorizationInternal);
    }
	try {
		Rule newRule(inRightName, inRightDefinition, mConfigRules);
		if (newRule->name() == inRightName)
			return true;
	} catch (...) {
		secdebug("authrule", "invalid definition for rule %s.\n", 
                inRightName.c_str());
	}
	return false;
}

CFDictionaryRef
AuthorizationDBPlist::getRuleDefinition(string &key)
{
    if (!mConfigRights ||
        0 == CFDictionaryGetCount(mConfigRights)) {
        Syslog::error("No rule definitions!");
        MacOSError::throwMe(errAuthorizationInternal);
    }
	CFStringRef cfKey = makeCFString(key);
    StLock<Mutex> _(mLock);
	if (CFDictionaryContainsKey(mConfigRights, cfKey)) {
		CFDictionaryRef definition = reinterpret_cast<CFMutableDictionaryRef>(const_cast<void*>(CFDictionaryGetValue(mConfigRights, cfKey)));
		CFRelease(cfKey);
		return CFDictionaryCreateCopy(NULL, definition);
	} else {
		CFRelease(cfKey);
		return NULL;
	}
}

bool
AuthorizationDBPlist::existRule(string &ruleName) const
{
	AuthItemRef candidateRule(ruleName.c_str());
	string ruleForCandidate = getRule(candidateRule)->name();
	// same name or covered by wildcard right -> modification.
	if ( (ruleName == ruleForCandidate) ||
		 (*(ruleForCandidate.rbegin()) == '.') )
		return true;

	return false;
}

Rule
AuthorizationDBPlist::getRule(const AuthItemRef &inRight) const
{
	string key(inRight->name());
    // Lock the rulemap
    StLock<Mutex> _(mLock);
	
    secdebug("authdb", "looking up rule %s.", inRight->name());
	if (mRules.empty())
		return Rule();

	for (;;) {
		map<string,Rule>::const_iterator rule = mRules.find(key);
		
		if (rule != mRules.end())
			return (*rule).second;
		
		// no default rule
		assert (key.size());
		
		// any reduction of a combination of two chars is futile
		if (key.size() > 2) {
			// find last dot with exception of possible dot at end
			string::size_type index = key.rfind('.', key.size() - 2);
			// cut right after found dot, or make it match default rule
			key = key.substr(0, index == string::npos ? 0 : index + 1);
		} else
			key.erase();
	}
}

void
AuthorizationDBPlist::setRule(const char *inRightName, CFDictionaryRef inRuleDefinition)
{
	// if mConfig is now a reasonable guard
	if (!inRuleDefinition || !mConfigRights)
	{
		Syslog::alert("Failed to set rule, no definition or rights");
		MacOSError::throwMe(errAuthorizationDenied);    // ???/gh  errAuthorizationInternal instead?
	}

	CFRef<CFStringRef> keyRef(CFStringCreateWithCString(NULL, inRightName, 
                kCFStringEncodingASCII));
	if (!keyRef)
		return;
		
	{
		StLock<Mutex> _(mLock);
		secdebug("authdb", "setting up rule %s.", inRightName);
		CFDictionarySetValue(mConfigRights, keyRef, inRuleDefinition);
		save();
		parseConfig(mConfig);
	}
}

void
AuthorizationDBPlist::removeRule(const char *inRightName)
{
	// if mConfig is now a reasonable guard
	if (!mConfigRights)
	{
		Syslog::alert("Failed to remove rule, no rights");
		MacOSError::throwMe(errAuthorizationDenied);    // ???/gh  errAuthorizationInternal instead?
	}
			
	CFRef<CFStringRef> keyRef(CFStringCreateWithCString(NULL, inRightName, 
                kCFStringEncodingASCII));
	if (!keyRef)
		return;

	{
		StLock<Mutex> _(mLock);
		secdebug("authdb", "removing rule %s.", inRightName);
		CFDictionaryRemoveValue(mConfigRights, keyRef);
		save();
		parseConfig(mConfig);
	}
}


} // end namespace Authorization
