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
 *  AuthorizationEngine.cpp
 *  Authorization
 *
 *  Created by Michael Brouwer on Thu Oct 12 2000.
 *  Copyright (c) 2000 Apple Computer Inc. All rights reserved.
 *
 */

#include "AuthorizationEngine.h"

#include "server.h"
#include "authority.h"

#include <Security/AuthorizationTags.h>
#include <Security/logging.h>
#include <Security/debugging.h>

#include <CoreFoundation/CFData.h>
#include <CoreFoundation/CFNumber.h>
#include <CoreFoundation/CFPropertyList.h>

#include <errno.h>
#include <fcntl.h>
#include <float.h>
#include <unistd.h>
#include <grp.h>
#include <pwd.h>

// for longname lookup
#include <netinfo/ni.h>
// private header (lu_utils.h from lookup project)
extern "C" {
int lookupd_query(ni_proplist *l, ni_proplist ***out);
ni_proplist *lookupd_make_query(char *cat, char *fmt, ...);
int _lu_running(void);
}

using namespace Authorization;

//
// Errors to be thrown
//
Error::Error(int err) : error(err)
{
}

const char *Error::what() const
{ return "Authorization error"; }

CSSM_RETURN Error::cssmError() const
{ return error; }	// @@@ eventually...

OSStatus Error::osStatus() const
{ return error; }

void Error::throwMe(int err) { throw Error(err); }


//
// CredentialImpl class
//

// only for testing whether this credential is usable
CredentialImpl::CredentialImpl(const string &username, const uid_t uid, const gid_t gid, bool shared) :
mUsername(username), mShared(shared), mUid(uid), mGid(gid), mCreationTime(CFAbsoluteTimeGetCurrent()), mValid(true)
{
}

// credential with validity based on username/password combination.
CredentialImpl::CredentialImpl(const string &username, const string &password, bool shared) :
mShared(shared), mCreationTime(CFAbsoluteTimeGetCurrent()), mValid(false)
{
	// try short name first
	const char *user = username.c_str();
	struct passwd *pw = getpwnam(user);

	do
	{
		if ( !pw && _lu_running() ) {
			// try lookup query to find passed username as a long name (realname in NI-speak)
			ni_proplist **out = NULL;
			// query "user" records.  "k" specifies position of keys in varargs
			ni_proplist *in = lookupd_make_query("user", "kv", "realname", user);
			if (!in) break;

			int results = lookupd_query(in, &out);
			ni_proplist_free(in);
			if (!out) break;

			// Find the first, if any, name value in returned records, getpwnam, and dispose of them
			for (int i=0; i<results; ++i) {
				ni_proplist *nipl = out[i];
				for (unsigned int j=0; !pw && j< nipl->ni_proplist_len; j++) {
					if ( !strcmp(nipl->ni_proplist_val[j].nip_name, "name") &&
						(nipl->ni_proplist_val[j].nip_val.ni_namelist_len > 0) )
							pw = getpwnam( *(nipl->ni_proplist_val[j].nip_val.ni_namelist_val) );
				}
				ni_proplist_free(nipl);
			}
			free(out);
		}

		if (!pw)
		{
			debug("autheval", "user %s not found, creating invalid credential", user);
			break;
		}

		if (pw->pw_passwd != NULL && pw->pw_passwd[0])
		{
			const char *passwd = password.c_str();
			if (strcmp(crypt(passwd, pw->pw_passwd), pw->pw_passwd))
			{
				debug("autheval", "password for user %s is invalid, creating invalid credential", user);
				break;
			}
		}

		debug("autheval", "password for user %s is ok, creating%s credential",
			user, mShared ? " shared" : "");

		mUsername = string ( pw->pw_name );
		mUid = pw->pw_uid;
		mGid = pw->pw_gid;
		mValid = true;
	}
	while (0);
	
	if (pw)
		endpwent();
}


CredentialImpl::~CredentialImpl()
{
}

bool
CredentialImpl::operator < (const CredentialImpl &other) const
{
	if (!mShared && other.mShared)
		return true;
	if (!other.mShared && mShared)
		return false;

	return mUsername < other.mUsername;
}

// Returns true if this CredentialImpl should be shared.
bool
CredentialImpl::isShared() const
{
	return mShared;
}

// Merge with other
void
CredentialImpl::merge(const CredentialImpl &other)
{
	assert(mUsername == other.mUsername);

	if (other.mValid && (!mValid || mCreationTime < other.mCreationTime))
	{
		mCreationTime = other.mCreationTime;
		mUid = other.mUid;
		mGid = other.mGid;
		mValid = true;
	}
}

// The time at which this credential was obtained.
CFAbsoluteTime
CredentialImpl::creationTime() const
{
	return mCreationTime;
}

// Return true iff this credential is valid.
bool
CredentialImpl::isValid() const
{
	return mValid;
}

void
CredentialImpl::invalidate()
{
	mValid = false;
}

//
// Credential class
//
Credential::Credential() :
RefPointer<CredentialImpl>(NULL)
{
}

Credential::Credential(CredentialImpl *impl) :
RefPointer<CredentialImpl>(impl)
{
}

Credential::Credential(const string &username, const uid_t uid, const gid_t gid, bool shared) :
RefPointer<CredentialImpl>(new CredentialImpl(username, uid, gid, shared))
{
}

Credential::Credential(const string &username, const string &password, bool shared) :
RefPointer<CredentialImpl>(new CredentialImpl(username, password, shared))
{
}

Credential::~Credential()
{
}

bool
Credential::operator < (const Credential &other) const
{
	if (!*this)
		return other;

	if (!other)
		return false;

	return (**this) < (*other);
}


//
// Right class
//
Right &
Right::overlay(AuthorizationItem &item)
{
	return static_cast<Right &>(item);
}

Right *
Right::overlay(AuthorizationItem *item)
{
	return static_cast<Right *>(item);
}

Right::Right()
{
	name = "";
	valueLength = 0;
	value = NULL;
	flags = 0;
}

Right::Right(AuthorizationString inName, size_t inValueLength, const void *inValue)
{
	name = inName;
	valueLength = inValueLength;
	value = const_cast<void *>(inValue);
}

Right::~Right()
{
}

bool
Right::operator < (const Right &other) const
{
	return strcmp(name, other.name) < 0;
}


//
// RightSet class
//
const AuthorizationRights RightSet::gEmptyRights = { 0, NULL };

RightSet::RightSet(const AuthorizationRights *rights) :
mRights(const_cast<AuthorizationRights *>(rights ? rights : &gEmptyRights))
{
}

RightSet::RightSet(const RightSet &other)
{
	mRights = other.mRights;
}

RightSet::~RightSet()
{
}

RightSet::const_reference
RightSet::back() const
{
	// @@@ Should this if empty::throwMe()?
	return static_cast<const_reference>(mRights->items[size() - 1]);
}


//
// MutableRightSet class
//
MutableRightSet::MutableRightSet(size_t count, const Right &element) :
mCapacity(count)
{
	mRights = new AuthorizationRights();
	mRights->items = reinterpret_cast<pointer>(malloc(sizeof(Right) * mCapacity));
	if (!mRights->items)
	{
		delete mRights;
		throw std::bad_alloc();
	}

	mRights->count = count;
	for (size_type ix = 0; ix < count; ++ix)
		mRights->items[ix] = element;
}

MutableRightSet::MutableRightSet(const RightSet &other)
{
	size_type count = other.size();
	mCapacity = count;
	mRights = new AuthorizationRights();

	mRights->items = reinterpret_cast<pointer>(malloc(sizeof(Right) * mCapacity));
	if (!mRights->items)
	{
		delete mRights;
		throw std::bad_alloc();
	}

	mRights->count = count;
	for (size_type ix = 0; ix < count; ++ix)
		mRights->items[ix] = other.mRights->items[ix];
}

MutableRightSet::~MutableRightSet()
{
	free(mRights->items);
	delete mRights;
}

MutableRightSet &
MutableRightSet::operator = (const RightSet &other)
{
	size_type count = other.size();
	if (capacity() < count)
		grow(count);

	mRights->count = count;
	for (size_type ix = 0; ix < count; ++ix)
		mRights->items[ix] = other.mRights->items[ix];

	return *this;
}

void
MutableRightSet::swap(MutableRightSet &other)
{
	AuthorizationRights *rights = mRights;
	size_t capacity = mCapacity;
	mRights = other.mRights;
	mCapacity = other.mCapacity;
	other.mRights = rights;
	other.mCapacity = capacity;
}

MutableRightSet::reference
MutableRightSet::back()
{
	// @@@ Should this if empty::throwMe()?
	return static_cast<reference>(mRights->items[size() - 1]);
}

void
MutableRightSet::push_back(const_reference right)
{
	if (size() >= capacity())
		grow(capacity() + 1);

	mRights->items[mRights->count] = right;
	mRights->count++;
}

void
MutableRightSet::pop_back()
{
	// @@@ Should this if empty::throwMe()?
	if (!empty())
		mRights->count--;
}

void
MutableRightSet::grow(size_type min_capacity)
{
	size_type newCapacity = mCapacity * mCapacity;
	if (newCapacity < min_capacity)
		newCapacity = min_capacity;

	void *newItems = realloc(mRights->items, sizeof(*mRights->items) * newCapacity);
	if (!newItems)
		throw std::bad_alloc();

	mRights->items = reinterpret_cast<pointer>(newItems);
	mCapacity = newCapacity;
}


//
// Rule class
//
CFStringRef Rule::kUserInGroupID = CFSTR("group");
CFStringRef Rule::kTimeoutID = CFSTR("timeout");
CFStringRef Rule::kSharedID = CFSTR("shared");
CFStringRef Rule::kAllowRootID = CFSTR("allow-root");
CFStringRef Rule::kDenyID = CFSTR("deny");
CFStringRef Rule::kAllowID = CFSTR("allow");


Rule::Rule() :
mType(kUserInGroup), mGroupName("admin"), mMaxCredentialAge(300.0), mShared(true), mAllowRoot(false)
{
	// @@@ Default rule is shared admin group with 5 minute timeout
}

Rule::Rule(CFTypeRef cfRule)
{
	// @@@ This code is ugly.  Serves me right for using CF.
	if (CFGetTypeID(cfRule) == CFStringGetTypeID())
	{
		CFStringRef tag = reinterpret_cast<CFStringRef>(cfRule);
		if (CFEqual(kAllowID, tag))
		{
			debug("authrule", "rule always allow");
			mType = kAllow;
		}
		else if (CFEqual(kDenyID, tag))
		{
			debug("authrule", "rule always deny");
			mType = kDeny;
		}
		else
			Error::throwMe();
	}
	else if (CFGetTypeID(cfRule) == CFDictionaryGetTypeID())
	{
		mType = kUserInGroup;
		CFDictionaryRef dict = reinterpret_cast<CFDictionaryRef>(cfRule);
		CFTypeRef groupTag = CFDictionaryGetValue(dict, kUserInGroupID);
		if (!groupTag || CFGetTypeID(groupTag) != CFStringGetTypeID())
			Error::throwMe();

		CFStringRef group = reinterpret_cast<CFStringRef>(groupTag);
		char buffer[512];
		const char *ptr = CFStringGetCStringPtr(group, kCFStringEncodingUTF8);
		if (ptr == NULL)
		{
			if (CFStringGetCString(group, buffer, 512, kCFStringEncodingUTF8))
				ptr = buffer;
			else
				Error::throwMe();
		}

		mGroupName = string(ptr);

		mMaxCredentialAge = DBL_MAX;
		CFTypeRef timeoutTag = CFDictionaryGetValue(dict, kTimeoutID);
		if (timeoutTag)
		{
			if (CFGetTypeID(timeoutTag) != CFNumberGetTypeID())
				Error::throwMe();
			CFNumberGetValue(reinterpret_cast<CFNumberRef>(timeoutTag), kCFNumberDoubleType, &mMaxCredentialAge);
		}

		CFTypeRef sharedTag = CFDictionaryGetValue(dict, kSharedID);
		mShared = false;
		if (sharedTag)
		{
			if (CFGetTypeID(sharedTag) != CFBooleanGetTypeID())
				Error::throwMe();
			mShared = CFBooleanGetValue(reinterpret_cast<CFBooleanRef>(sharedTag));
		}

		CFTypeRef allowRootTag = CFDictionaryGetValue(dict, kAllowRootID);
		mAllowRoot = false;
		if (allowRootTag)
		{
			if (CFGetTypeID(allowRootTag) != CFBooleanGetTypeID())
				Error::throwMe();
			mAllowRoot = CFBooleanGetValue(reinterpret_cast<CFBooleanRef>(allowRootTag));
		}
		debug("authrule", "rule user in group \"%s\" timeout %g%s%s",
			mGroupName.c_str(), mMaxCredentialAge, mShared ? " shared" : "",
			mAllowRoot ? " allow-root" : "");
	}
}

Rule::Rule(const Rule &other) :
mType(other.mType),
mGroupName(other.mGroupName),
mMaxCredentialAge(other.mMaxCredentialAge),
mShared(other.mShared),
mAllowRoot(other.mAllowRoot)
{
}

Rule &
Rule::operator = (const Rule &other)
{
	mType = other.mType;
	mGroupName = other.mGroupName;
	mMaxCredentialAge = other.mMaxCredentialAge;
	mShared = other.mShared;
	mAllowRoot = other.mAllowRoot;
	return *this;
}

Rule::~Rule()
{
}

OSStatus
Rule::evaluate(const Right &inRight,
    const AuthorizationEnvironment *environment, AuthorizationFlags flags,
	CFAbsoluteTime now, const CredentialSet *inCredentials, CredentialSet &credentials,
	const AuthorizationToken &auth)
{
	switch (mType)
	{
	case kAllow:
		debug("autheval", "rule is always allow");
		return errAuthorizationSuccess;
	case kDeny:
		debug("autheval", "rule is always deny");
		return errAuthorizationDenied;
	case kUserInGroup:
		debug("autheval", "rule is user in group");
		break;
	default:
		Error::throwMe();
	}

	// If we got here, this is a kUserInGroup type rule, let's start looking for a
	// credential that is satisfactory

	// Zeroth -- Here is an extra special saucy ugly hack to allow authorizations
	// created by a proccess running as root to automatically get a right.
	if (mAllowRoot && auth.creatorUid() == 0)
	{
		debug("autheval", "creator of authorization has uid == 0 granting right %s",
			inRight.rightName());
		return errAuthorizationSuccess;
	}

	// First -- go though the credentials we either already used or obtained during this authorize operation.
	for (CredentialSet::const_iterator it = credentials.begin(); it != credentials.end(); ++it)
	{
		OSStatus status = evaluate(inRight, environment, now, *it, true);
		if (status != errAuthorizationDenied)
			return status;
	}

	// Second -- go though the credentials passed in to this authorize operation by the state management layer.
	if (inCredentials)
	{
		for (CredentialSet::const_iterator it = inCredentials->begin(); it != inCredentials->end(); ++it)
		{
			OSStatus status = evaluate(inRight, environment, now, *it, false);
			if (status == errAuthorizationSuccess)
			{
				// Add the credential we used to the output set.
				// @@@ Deal with potential credential merges.
				credentials.insert(*it);
				return status;
			}
			else if (status != errAuthorizationDenied)
				return status;
		}
	}

	// Finally -- We didn't find the credential in our passed in credential lists.  Obtain a new credential if
	// our flags let us do so.
	if (!(flags & kAuthorizationFlagExtendRights))
		return errAuthorizationDenied;

	if (!(flags & kAuthorizationFlagInteractionAllowed))
		return errAuthorizationInteractionNotAllowed;

	QueryAuthorizeByGroup query;

	string usernamehint;
	// @@@ This should really be the loginname of the proccess that originally created the AuthorizationRef.
	// For now we get the pw_name of the user with the uid of the calling process.
	uid_t uid = query.uid();
	if (uid)
	{
		struct passwd *pw = getpwuid(uid);
		if (pw != NULL)
		{
			// avoid hinting a locked account (ie. root)
			if ( (pw->pw_passwd == NULL) ||
				 strcmp(pw->pw_passwd, "*") ) {
				// Check if username will authorize the request and set username to
				// be used as a hint to the user if so
				if (evaluate(inRight, environment, now, Credential(pw->pw_name, pw->pw_uid, pw->pw_gid, mShared), true) == errAuthorizationSuccess) {
						// user long name as hint
						usernamehint = string( pw->pw_gecos );
						// minus other gecos crud
						size_t comma = usernamehint.find(',');
						if (comma)
							usernamehint = usernamehint.substr(0, comma);
						// or fallback to short username
						if (usernamehint.size() == 0)
							usernamehint = string( pw->pw_name );
				} //fi
			} //fi
			endpwent();
		}
	}

	Credential newCredential;
	// @@@ Keep the default reason the same, so the agent only gets userNotInGroup or invalidPassphrase
	SecurityAgent::Reason reason = SecurityAgent::userNotInGroup;
	// @@@ Hardcoded 3 tries to avoid infinite loops.
	for (int tryCount = 0; tryCount < 3; ++tryCount)
	{
		// Obtain a new credential.  Anything but success is considered an error.
		OSStatus status = obtainCredential(query, inRight, environment, usernamehint.c_str(), newCredential, reason);
		if (status)
            return status;

		// Now we have successfully obtained a credential we need to make sure it authorizes the requested right
		if (!newCredential->isValid())
			reason = SecurityAgent::invalidPassphrase;
		else {
			status = evaluate(inRight, environment, now, newCredential, true);
			if (status == errAuthorizationSuccess)
			{
				// Add the new credential we obtained to the output set.
				// @@@ Deal with potential credential merges.
				credentials.insert(newCredential);
				query.done();
				return errAuthorizationSuccess;
			}
			else if (status != errAuthorizationDenied)
				return status;
			reason = SecurityAgent::userNotInGroup;
		}
	}

	query.cancel(SecurityAgent::tooManyTries);
	return errAuthorizationDenied;
}

// Return errAuthorizationSuccess if this rule allows access based on the specified credential,
// return errAuthorizationDenied otherwise.
OSStatus
Rule::evaluate(const Right &inRight, const AuthorizationEnvironment *environment, CFAbsoluteTime now,
	const Credential &credential, bool ignoreShared)
{
	assert(mType == kUserInGroup);

	// Get the username from the credential
	const char *user = credential->username().c_str();

	// If the credential is not valid or it's age is more than the allowed maximum age
	// for a credential, deny.
	if (!credential->isValid())
	{
		debug("autheval", "credential for user %s is invalid, denying right %s", user, inRight.rightName());
		return errAuthorizationDenied;
	}

	if (now - credential->creationTime() > mMaxCredentialAge)
	{
		debug("autheval", "credential for user %s has expired, denying right %s", user, inRight.rightName());
		return errAuthorizationDenied;
	}

	if (!ignoreShared && !mShared && credential->isShared())
	{
		debug("autheval", "shared credential for user %s cannot be used, denying right %s", user, inRight.rightName());
		return errAuthorizationDenied;
	}

	// A root (uid == 0) user can do anything
	if (credential->uid() == 0)
	{
		debug("autheval", "user %s has uid 0, granting right %s", user, inRight.rightName());
		return errAuthorizationSuccess;
	}

	const char *groupname = mGroupName.c_str();
	struct group *gr = getgrnam(groupname);
	if (!gr)
		return errAuthorizationDenied;

	// Is this the default group of this user?
	// <grp.h> declares gr_gid int, as opposed to advertised (getgrent(3)) gid_t
	if (credential->gid() == gr->gr_gid)
	{
		debug("autheval", "user %s has group %s(%d) as default group, granting right %s",
			user, groupname, gr->gr_gid, inRight.rightName());
		endgrent();
		return errAuthorizationSuccess;
	}

	for (char **group = gr->gr_mem; *group; ++group)
	{
		if (!strcmp(*group, user))
		{
			debug("autheval", "user %s is a member of group %s, granting right %s",
				user, groupname, inRight.rightName());
			endgrent();
			return errAuthorizationSuccess;
		}
	}

	debug("autheval", "user %s is not a member of group %s, denying right %s",
		user, groupname, inRight.rightName());
	endgrent();
	return errAuthorizationDenied;
}

OSStatus
Rule::obtainCredential(QueryAuthorizeByGroup &query, const Right &inRight, 
    const AuthorizationEnvironment *environment, const char *usernameHint, Credential &outCredential, SecurityAgent::Reason reason)
{
	char nameBuffer[SecurityAgent::maxUsernameLength];
	char passphraseBuffer[SecurityAgent::maxPassphraseLength];
	OSStatus status = errAuthorizationDenied;

	try {
        if (query(mGroupName.c_str(), usernameHint, nameBuffer, passphraseBuffer, reason))
            status = noErr;
	} catch (const CssmCommonError &err) {
		status = err.osStatus();
	} catch (...) {
		status = errAuthorizationInternal;
	}
	if (status == CSSM_ERRCODE_USER_CANCELED)
	{
		debug("auth", "canceled obtaining credential for user in group %s", mGroupName.c_str());
		return errAuthorizationCanceled;
	}
	if (status == CSSM_ERRCODE_NO_USER_INTERACTION)
	{
		debug("auth", "user interaction not possible obtaining credential for user in group %s", mGroupName.c_str());
		return errAuthorizationInteractionNotAllowed;
	}

	if (status != noErr)
	{
		debug("auth", "failed obtaining credential for user in group %s", mGroupName.c_str());
		return status;
	}

	debug("auth", "obtained credential for user %s", nameBuffer);

	string username(nameBuffer);
	string password(passphraseBuffer);
	outCredential = Credential(username, password, mShared);
	return errAuthorizationSuccess;
}


//
// Engine class
//
Engine::Engine(const char *configFile) :
mLastChecked(DBL_MIN)
{
	mRulesFileName = new char[strlen(configFile) + 1];
	strcpy(mRulesFileName, configFile);
	memset(&mRulesFileMtimespec, 0, sizeof(mRulesFileMtimespec));
}

Engine::~Engine()
{
	delete[] mRulesFileName;
}

void
Engine::updateRules(CFAbsoluteTime now)
{
	if (mRules.empty())
		readRules();
	else
	{
		// Don't do anything if we checked the timestamp less than 5 seconds ago
		if (mLastChecked > now - 5.0)
			return;
	
		struct stat st;
		if (stat(mRulesFileName, &st))
		{
			Syslog::error("Stating rules file \"%s\": %s", mRulesFileName, strerror(errno));
			/* @@@ No rules file found, use defaults: admin group for everything. */
			//UnixError::throwMe(errno);
		}
		else
		{
			// @@@ Make sure this is the right way to compare 2 struct timespec thingies
			// Technically we should check st_dev and st_ino as well since if either of those change
			// we are looking at a different file too.
			if (memcmp(&st.st_mtimespec, &mRulesFileMtimespec, sizeof(mRulesFileMtimespec)))
				readRules();
		}
	}

	mLastChecked = now;
}

void
Engine::readRules()
{
	// Make an entry in the mRules map that matches every right to the default Rule.
	mRules.clear();
	mRules.insert(RuleMap::value_type(string(), Rule()));

	int fd = open(mRulesFileName, O_RDONLY, 0);
	if (fd == -1)
	{
		Syslog::error("Opening rules file \"%s\": %s", mRulesFileName, strerror(errno));
		return;
	}

	try
	{
		struct stat st;
		if (fstat(fd, &st))
			UnixError::throwMe(errno);

		mRulesFileMtimespec = st.st_mtimespec;

		off_t fileSize = st.st_size;

        CFRef<CFMutableDataRef> xmlData(CFDataCreateMutable(NULL, fileSize));
        CFDataSetLength(xmlData, fileSize);
		void *buffer = CFDataGetMutableBytePtr(xmlData);
		size_t bytesRead = read(fd, buffer, fileSize);
		if (bytesRead != fileSize)
		{
			if (bytesRead == static_cast<size_t>(-1))
			{
				Syslog::error("Reading rules file \"%s\": %s", mRulesFileName, strerror(errno));
				return;
			}

			Syslog::error("Could only read %ul out of %ul bytes from rules file \"%s\"",
						  bytesRead, fileSize, mRulesFileName);
			return;
		}

		CFStringRef errorString;
        CFRef<CFDictionaryRef> newRoot(reinterpret_cast<CFDictionaryRef>
			(CFPropertyListCreateFromXMLData(NULL, xmlData, kCFPropertyListImmutable, &errorString)));
		if (!newRoot)
		{
			char buffer[512];
			const char *error = CFStringGetCStringPtr(errorString, kCFStringEncodingUTF8);
			if (error == NULL)
			{
				if (CFStringGetCString(errorString, buffer, 512, kCFStringEncodingUTF8))
					error = buffer;
			}

			Syslog::error("Parsing rules file \"%s\": %s", mRulesFileName, error);
			return;
		}

		if (CFGetTypeID(newRoot) != CFDictionaryGetTypeID())
		{
			Syslog::error("Rules file \"%s\": is not a dictionary", mRulesFileName);
			return;
		}

		parseRules(newRoot);
	}
	catch(...)
	{
		close(fd);
	}

	close(fd);
}

void
Engine::parseRules(CFDictionaryRef rules)
{
	CFDictionaryApplyFunction(rules, parseRuleCallback, this);
}

void
Engine::parseRuleCallback(const void *key, const void *value, void *context)
{
	Engine *engine = reinterpret_cast<Engine *>(context);
	if (CFGetTypeID(key) != CFStringGetTypeID())
		return;

	CFStringRef right = reinterpret_cast<CFStringRef>(key);
	engine->parseRule(right, reinterpret_cast<CFTypeRef>(value));
}

void
Engine::parseRule(CFStringRef cfRight, CFTypeRef cfRule)
{
	char buffer[512];
	const char *ptr = CFStringGetCStringPtr(cfRight, kCFStringEncodingUTF8);
	if (ptr == NULL)
	{
		if (CFStringGetCString(cfRight, buffer, 512, kCFStringEncodingUTF8))
			ptr = buffer;
	}

	string right(ptr);
	try
	{
		mRules[right] = Rule(cfRule);
		debug("authrule", "added rule for right \"%s\"", right.c_str());
	}
	catch (...)
	{
		Syslog::error("Rules file \"%s\" right \"%s\": rule is invalid", mRulesFileName, ptr);
	}
}


/*!
	@function AuthorizationEngine::getRule

	Look up the Rule for a given right.

	@param inRight (input) the right for which we want a rule.

	@results The Rule for right
*/
Rule
Engine::getRule(const Right &inRight) const
{
	string key(inRight.rightName());
	for (;;)
	{
		RuleMap::const_iterator it = mRules.find(key);
		if (it != mRules.end())
		{
			debug("authrule", "right \"%s\" using right expression \"%s\"", inRight.rightName(), key.c_str());
			return it->second;
		}

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

/*!
	@function AuthorizationEngine::authorize

	@@@.

	@param inRights (input) List of rights being requested for authorization.
	@param environment (optional/input) Environment containing information to be used during evaluation.
	@param flags (input) Optional flags @@@ see AuthorizationCreate for a description.
	@param inCredentials (input) Credentials already held by the caller.
	@param outCredentials (output/optional) Credentials obtained, used or refreshed during this call to authorize the requested rights.
	@param outRights (output/optional) Subset of inRights which were actually authorized.

	@results Returns errAuthorizationSuccess if all rights requested are authorized, or if the kAuthorizationFlagPartialRights flag was specified.  Might return other status values like errAuthorizationDenied, errAuthorizationCanceled or errAuthorizationInteractionNotAllowed 
*/
OSStatus
Engine::authorize(const RightSet &inRights, const AuthorizationEnvironment *environment,
	AuthorizationFlags flags, const CredentialSet *inCredentials, CredentialSet *outCredentials,
	MutableRightSet *outRights, const AuthorizationToken &auth)
{
	CredentialSet credentials;
	MutableRightSet rights;
	OSStatus status = errAuthorizationSuccess;

	// Get current time of day.
	CFAbsoluteTime now = CFAbsoluteTimeGetCurrent();

	// Update rules from database if needed
	updateRules(now);

	// Check if a credential was passed into the environment and we were asked to extend the rights
	if (environment && (flags & kAuthorizationFlagExtendRights))
	{
		const AuthorizationItem *username = NULL, *password = NULL;
		bool shared = false;
		for (UInt32 ix = 0; ix < environment->count; ++ix)
		{
			const AuthorizationItem &item = environment->items[ix];
			if (!strcmp(item.name, kAuthorizationEnvironmentUsername))
				username = &item;
			if (!strcmp(item.name, kAuthorizationEnvironmentPassword))
				password = &item;
			if (!strcmp(item.name, kAuthorizationEnvironmentShared))
				shared = true;
		}

		if (username && password)
		{
			// Let's create a credential from the passed in username and password.
			Credential newCredential(string(reinterpret_cast<const char *>(username->value), username->valueLength),
				string(reinterpret_cast<const char *>(password->value), password->valueLength), shared);
			// If it's valid insert it into the credentials list.  Normally this is
			// only done if it actually authorizes a requested right, but for this
			// special case (environment) we do it even when no rights are being requested.
			if (newCredential->isValid())
				credentials.insert(newCredential);
		}
	}

	RightSet::const_iterator end = inRights.end();
	for (RightSet::const_iterator it = inRights.begin(); it != end; ++it)
	{
		// Get the rule for each right we are trying to obtain.
		OSStatus result = getRule(*it).evaluate(*it, environment, flags, now, 
            inCredentials, credentials, auth);
		if (result == errAuthorizationSuccess)
			rights.push_back(*it);
		else if (result == errAuthorizationDenied || result == errAuthorizationInteractionNotAllowed)
		{
			if (!(flags & kAuthorizationFlagPartialRights))
			{
				status = result;
				break;
			}
		}
        else if (result == errAuthorizationCanceled)
        {
            status = result;
            break;
        }
		else
		{
			Syslog::error("Engine::authorize: Rule::evaluate returned %ld returning errAuthorizationInternal", result);
			status = errAuthorizationInternal;
			break;
		}
	}

	if (outCredentials)
		outCredentials->swap(credentials);
	if (outRights)
		outRights->swap(rights);

	return status;
}
