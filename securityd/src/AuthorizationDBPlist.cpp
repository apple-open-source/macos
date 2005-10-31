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


namespace Authorization
{

AuthorizationDBPlist::AuthorizationDBPlist(const char *configFile) : mFileName(configFile), mLastChecked(DBL_MIN)
{
	memset(&mRulesFileMtimespec, 0, sizeof(mRulesFileMtimespec));
}

void AuthorizationDBPlist::sync(CFAbsoluteTime now)
{
	if (mRules.empty())
		load(now);
	else
	{
		// Don't do anything if we checked the timestamp less than 5 seconds ago
		if (mLastChecked > now - 5.0)
			return;
	
		struct stat st;
		if (stat(mFileName.c_str(), &st))
		{
			Syslog::error("Stating rules file \"%s\": %s", mFileName.c_str(), strerror(errno));
			/* @@@ No rules file found, use defaults: admin group for everything. */
			//UnixError::throwMe(errno);
		}
		else
		{
			// @@@ Make sure this is the right way to compare 2 struct timespec thingies
			// Technically we should check st_dev and st_ino as well since if either of those change
			// we are looking at a different file too.
			if (memcmp(&st.st_mtimespec, &mRulesFileMtimespec, sizeof(mRulesFileMtimespec)))
				load(now);
		}
	}

	mLastChecked = now;
}

void AuthorizationDBPlist::save() const
{
	if (!mConfig)
		return;

	StLock<Mutex> _(mReadWriteLock);
	
	int fd = -1;
	string tempFile = mFileName + ",";
	
	for (;;)
	{
		fd = open(tempFile.c_str(), O_WRONLY|O_CREAT|O_EXCL, 0644);
		if (fd == -1)
		{
			if (errno == EEXIST)
			{
				unlink(tempFile.c_str());
				continue;
			}
			if (errno == EINTR)
				continue;
			else
				break;
		}
		else
			break;
	}
			
	if (fd == -1)
	{
		Syslog::error("Saving rules file \"%s\": %s", tempFile.c_str(), strerror(errno));
		return;
	}

	// convert config to plist
	CFDataRef configXML = CFPropertyListCreateXMLData(NULL, mConfig);
	
	if (!configXML)
		return;

	// write out data
	SInt32 configSize = CFDataGetLength(configXML);
	size_t bytesWritten = write(fd, CFDataGetBytePtr(configXML), configSize);
	CFRelease(configXML);
	
	if (bytesWritten != uint32_t(configSize))
	{
		if (bytesWritten == static_cast<size_t>(-1))
			Syslog::error("Writing rules file \"%s\": %s", tempFile.c_str(), strerror(errno));
		else
			Syslog::error("Could only write %lu out of %ld bytes from rules file \"%s\"",
						bytesWritten, configSize, tempFile.c_str());

		close(fd);
		unlink(tempFile.c_str());
	}
	else
	{
		close(fd);
		if (rename(tempFile.c_str(), mFileName.c_str()))
			unlink(tempFile.c_str());
	}
	return;
}

void AuthorizationDBPlist::load(CFTimeInterval now)
{
	StLock<Mutex> _(mReadWriteLock);
	
	int fd = open(mFileName.c_str(), O_RDONLY, 0);
	if (fd == -1)
	{
		Syslog::error("Opening rules file \"%s\": %s", mFileName.c_str(), strerror(errno));
		return;
	}

	struct stat st;
	if (fstat(fd, &st))
	{
		int error = errno;
		close(fd);
		UnixError::throwMe(error);
	}
		

	mRulesFileMtimespec = st.st_mtimespec;

	off_t fileSize = st.st_size;

	CFMutableDataRef xmlData = CFDataCreateMutable(NULL, fileSize);
	CFDataSetLength(xmlData, fileSize);
	void *buffer = CFDataGetMutableBytePtr(xmlData);
	size_t bytesRead = read(fd, buffer, fileSize);
	if (bytesRead != fileSize)
	{
		if (bytesRead == static_cast<size_t>(-1))
		{
			Syslog::error("Reading rules file \"%s\": %s", mFileName.c_str(), strerror(errno));
			CFRelease(xmlData);
			return;
		}

		Syslog::error("Could only read %ul out of %ul bytes from rules file \"%s\"",
						bytesRead, fileSize, mFileName.c_str());
		CFRelease(xmlData);
		return;
	}

	CFStringRef errorString;
	CFDictionaryRef configPlist = reinterpret_cast<CFDictionaryRef>(CFPropertyListCreateFromXMLData(NULL, xmlData, kCFPropertyListMutableContainersAndLeaves, &errorString));
	
	if (!configPlist)
	{
		char buffer[512];
		const char *error = CFStringGetCStringPtr(errorString, kCFStringEncodingUTF8);
		if (error == NULL)
		{
			if (CFStringGetCString(errorString, buffer, 512, kCFStringEncodingUTF8))
				error = buffer;
		}

		Syslog::error("Parsing rules file \"%s\": %s", mFileName.c_str(), error);
		if (errorString)
			CFRelease(errorString);
		
		CFRelease(xmlData);
		return;
	}

	if (CFGetTypeID(configPlist) != CFDictionaryGetTypeID())
	{

		Syslog::error("Rules file \"%s\": is not a dictionary", mFileName.c_str());

		CFRelease(xmlData);
		CFRelease(configPlist);
		return;
	}

	{
		StLock<Mutex> _(mLock);
		parseConfig(configPlist);
		mLastChecked = now;
	}
	CFRelease(xmlData);
	CFRelease(configPlist);

	close(fd);
}



void
AuthorizationDBPlist::parseConfig(CFDictionaryRef config)
{
	// grab items from top-level dictionary that we care about
	CFStringRef rightsKey = CFSTR("rights");
	CFStringRef rulesKey = CFSTR("rules");
	CFMutableDictionaryRef newRights = NULL;
	CFMutableDictionaryRef newRules = NULL;

	if (!config)
		MacOSError::throwMe(errAuthorizationInternal); // XXX/cs invalid rule file

	if (CFDictionaryContainsKey(config, rulesKey))
	{
		newRules = reinterpret_cast<CFMutableDictionaryRef>(const_cast<void*>(CFDictionaryGetValue(config, rulesKey)));
	}

	if (CFDictionaryContainsKey(config, rightsKey))
	{
		newRights = reinterpret_cast<CFMutableDictionaryRef>(const_cast<void*>(CFDictionaryGetValue(config, rightsKey)));
	}
	
	if (newRules 
		&& newRights 
		&& (CFDictionaryGetTypeID() == CFGetTypeID(newRules)) 
		&& (CFDictionaryGetTypeID() == CFGetTypeID(newRights)))
	{
		mConfig = config;
		mConfigRights = static_cast<CFMutableDictionaryRef>(newRights);
		mConfigRules = static_cast<CFMutableDictionaryRef>(newRules);
		mRules.clear();
		try
		{
			CFDictionaryApplyFunction(newRights, parseRule, this);
		}
		catch (...)
		{
			MacOSError::throwMe(errAuthorizationInternal); // XXX/cs invalid rule file
		}
	}
	else
		MacOSError::throwMe(errAuthorizationInternal); // XXX/cs invalid rule file
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
	try {
		Rule newRule(inRightName, inRightDefinition, mConfigRules);
		if (newRule->name() == inRightName)
			return true;
	} 
	catch (...)
	{
		secdebug("authrule", "invalid definition for rule %s.\n", inRightName.c_str());
	}
	return false;
}

CFDictionaryRef
AuthorizationDBPlist::getRuleDefinition(string &key)
{
	CFStringRef cfKey = makeCFString(key);
    StLock<Mutex> _(mLock);
	if (CFDictionaryContainsKey(mConfigRights, cfKey))
	{
		CFDictionaryRef definition = reinterpret_cast<CFMutableDictionaryRef>(const_cast<void*>(CFDictionaryGetValue(mConfigRights, cfKey)));
		CFRelease(cfKey);
		return CFDictionaryCreateCopy(NULL, definition);
	}
	else
	{
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
	
	if (mRules.empty())
		return Rule();

	for (;;)
	{
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
	if (!inRuleDefinition || !mConfigRights)
		MacOSError::throwMe(errAuthorizationDenied); // errInvalidRule

	CFRef<CFStringRef> keyRef(CFStringCreateWithCString(NULL, inRightName, kCFStringEncodingASCII));
	if (!keyRef)
		return;
		
	StLock<Mutex> _(mLock);

	CFDictionarySetValue(mConfigRights, keyRef, inRuleDefinition);
	// release modification lock here already?
	save();
	mLastChecked = 0.0;
}

void
AuthorizationDBPlist::removeRule(const char *inRightName)
{
	if (!mConfigRights)
		MacOSError::throwMe(errAuthorizationDenied);
			
	CFRef<CFStringRef> keyRef(CFStringCreateWithCString(NULL, inRightName, kCFStringEncodingASCII));
	if (!keyRef)
		return;

	StLock<Mutex> _(mLock);

	if (CFDictionaryContainsKey(mConfigRights, keyRef))
	{
		CFDictionaryRemoveValue(mConfigRights, keyRef);
		// release modification lock here already?
		save();
		mLastChecked = 0.0;
	}
}


} // end namespace Authorization
