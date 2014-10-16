/*
 * Copyright (c) 2000-2004,2006-2007,2009,2012 Apple Inc. All Rights Reserved.
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

#include "credential.h"
#include <pwd.h>
#include <syslog.h>

#include <Security/checkpw.h>
extern "C" int checkpw_internal( const struct passwd *pw, const char* password );
#include "server.h"

namespace Authorization {

// default credential: invalid for everything, needed as a default session credential
CredentialImpl::CredentialImpl() : mShared(false), mRight(false), mUid(0), mName(""), mCreationTime(CFAbsoluteTimeGetCurrent()), mValid(false)
{
}

// only for testing whether this credential is usable
CredentialImpl::CredentialImpl(const uid_t uid, const string &username, const string &realname, bool shared) : mShared(shared), mRight(false), mUid(uid), mName(username), mRealName(realname), mCreationTime(CFAbsoluteTimeGetCurrent()), mValid(true)
{
}

CredentialImpl::CredentialImpl(const string &username, const string &password, bool shared) : mShared(shared), mRight(false), mName(username), mCreationTime(CFAbsoluteTimeGetCurrent()), mValid(false)
{
    Server::active().longTermActivity();
    const char *user = username.c_str();
    struct passwd *pw = getpwnam(user);

    do {
        if (!pw) {
			syslog(LOG_ERR, "getpwnam() failed for user %s, creating invalid credential", user);
            break;
        }

        mUid = pw->pw_uid;
        mName = pw->pw_name;
        mRealName = pw->pw_gecos;

        const char *passwd = password.c_str();
        int checkpw_status = checkpw_internal(pw, passwd);

        if (checkpw_status != CHECKPW_SUCCESS) {
            syslog(LOG_ERR, "checkpw() returned %d; failed to authenticate user %s (uid %lu).", checkpw_status, pw->pw_name, pw->pw_uid);
            break;
        }

		syslog(LOG_INFO, "checkpw() succeeded, creating%s credential for user %s", mShared ? " shared" : "", user);

        mValid = true;

        endpwent();
    } while (0);
}

// least-privilege
    // @@@  arguably we don't care about the UID any more and should not
    // require it in this ctor
CredentialImpl::CredentialImpl(const string &right, bool shared) : mShared(shared), mRight(true), mUid(-2), mName(right), mRealName(""), mCreationTime(CFAbsoluteTimeGetCurrent()), mValid(true)
{
}

CredentialImpl::~CredentialImpl()
{
}

bool
CredentialImpl::operator < (const CredentialImpl &other) const
{
    // all shared creds are placed into mSessionCreds
    // all non shared creds are placed into AuthorizationToken
    //
    // There are 2 types of credentials UID and Right
    // UID = Authenticated Identity
    // Right = Rights which were previously authenticated by a uid credential
    
    // Right Credentials are only used during kAuthorizationFlagLeastPrivileged 
    // operations and should not have a valid uid set    

    // this allows shared and none shared co-exist in the same container
    // used when processing multiple rights shared vs non-shared during evaluation 
    if (!mShared && other.mShared)
        return true;
    if (!other.mShared && mShared)
        return false;
    
    // this allows uids and rights co-exist in the same container
    // used when holding onto Rights inside of the AuthorizationToken
    if (mRight && !other.mRight)
        return true;
    if (!mRight && other.mRight)
        return false;
    
    // this is the actual comparision
    if (mRight) {
        return mName < other.mName;
    } else {
        return mUid < other.mUid;
    }
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
    // try to ensure that the credentials are the same type
    assert(mRight == other.mRight);
    if (mRight)
        assert(mName == other.mName);
    else 
        assert(mUid == other.mUid);

    if (other.mValid && (!mValid || mCreationTime < other.mCreationTime))
    {
        mCreationTime = other.mCreationTime;
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
RefPointer<CredentialImpl>(new CredentialImpl())
{
}

Credential::Credential(CredentialImpl *impl) :
RefPointer<CredentialImpl>(impl)
{
}

Credential::Credential(const uid_t uid, const string &username, const string &realname, bool shared) :
RefPointer<CredentialImpl>(new CredentialImpl(uid, username, realname, shared))
{
}

Credential::Credential(const string &username, const string &password, bool shared) : RefPointer<CredentialImpl>(new CredentialImpl(username, password, shared))
{
}

Credential::Credential(const string &right, bool shared) : RefPointer<CredentialImpl>(new CredentialImpl(right, shared))
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

} // end namespace Authorization


