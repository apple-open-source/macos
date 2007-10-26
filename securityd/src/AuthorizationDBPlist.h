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
 *  AuthorizationDBPlist.h
 *  Security
 *
 */
#ifndef _H_AUTHORIZATIONDBPLIST
#define _H_AUTHORIZATIONDBPLIST  1

#include <CoreFoundation/CoreFoundation.h>
#include <security_utilities/cfutilities.h>

#include <security_cdsa_utilities/AuthorizationData.h>
#include "AuthorizationRule.h"

class AuthorizationDBPlist; // @@@ the ordering sucks here, maybe engine should include all these and other should only include it

namespace Authorization
{

class AuthorizationDBPlist /* : public AuthorizationDB */
{
public:
	AuthorizationDBPlist(const char *configFile);
	
	void sync(CFAbsoluteTime now);
	bool validateRule(string inRightName, CFDictionaryRef inRightDefinition) const;
	CFDictionaryRef getRuleDefinition(string &key);
	
	bool existRule(string &ruleName) const;
	Rule getRule(const AuthItemRef &inRight) const;
	
	void setRule(const char *inRightName, CFDictionaryRef inRuleDefinition);
	void removeRule(const char *inRightName);

protected:
	void load();
	void save();
	
private:
	string mFileName;
	
private:
	enum { kTypeRight, kTypeRule };
	void parseConfig(CFDictionaryRef config);
	static void parseRule(const void *key, const void *value, void *context);
	void addRight(CFStringRef key, CFDictionaryRef definition);

	CFAbsoluteTime mLastChecked;
	struct timespec mRulesFileMtimespec;

	map<string,Rule> mRules;
	CFRef<CFDictionaryRef> mConfig;
	CFRef<CFMutableDictionaryRef> mConfigRights;
	CFRef<CFMutableDictionaryRef> mConfigRules;
	
    mutable Mutex mLock; // rule map lock
	mutable Mutex mReadWriteLock; // file operation lock
};

}; /* namespace Authorization */
	
#endif /* ! _H_AUTHORIZATIONDBPLIST */
