/*
 *  AuthorizationDBPlist.h
 *  Security
 *
 *  Created by Conrad Sauerwald on Tue Mar 18 2003.
 *  Copyright (c) 2003 Apple Computer, Inc. All rights reserved.
 *
 */
#ifndef _H_AUTHORIZATIONDBPLIST
#define _H_AUTHORIZATIONDBPLIST  1

#include <CoreFoundation/CoreFoundation.h>
#include "AuthorizationData.h"
#include "AuthorizationRule.h"

namespace Authorization
{

class AuthorizationDBPlist /* : public AuthorizationDB */
{
public:
	AuthorizationDBPlist(const char *configFile = "/etc/authorization");
	//~AuthorizationDBPlist();
	
	void sync(CFAbsoluteTime now);
	bool validateRule(string inRightName, CFDictionaryRef inRightDefinition) const;
	CFDictionaryRef getRuleDefinition(string &key);
	
	bool existRule(string &ruleName) const;
	Rule getRule(const AuthItemRef &inRight) const;
	
	void setRule(const char *inRightName, CFDictionaryRef inRuleDefinition);
	void removeRule(const char *inRightName);

protected:
	void load(CFTimeInterval now);
	void save() const;
	
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
