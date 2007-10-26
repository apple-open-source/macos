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

#include "credential.h"
#include <pwd.h>
#include <syslog.h>

#include <Security/checkpw.h>
extern "C" int checkpw_internal( const struct passwd *pw, const char* password );
#include "server.h"

namespace Authorization {

// default credential: invalid for everything, needed as a default session credential
CredentialImpl::CredentialImpl() : mUid(0), mShared(false), mName(""), mRealname(""), mCreationTime(CFAbsoluteTimeGetCurrent()), mValid(false), mRight(false)
{
}

// only for testing whether this credential is usable
CredentialImpl::CredentialImpl(const uid_t uid, const string &username, const string &realname, bool shared) : mUid(uid), mShared(shared), mName(username), mRealname(realname), mCreationTime(CFAbsoluteTimeGetCurrent()), mValid(true), mRight(false)
{
}

CredentialImpl::CredentialImpl(const string &username, const string &password, bool shared) : mShared(shared), mName(username), mCreationTime(CFAbsoluteTimeGetCurrent()), mValid(false), mRight(false)
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
        mRealname = pw->pw_gecos;

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

CredentialImpl::CredentialImpl(const string &right, bool shared) : mUid(-2), mShared(shared), mName(right), mCreationTime(CFAbsoluteTimeGetCurrent()), mValid(true), mRight(true)
{
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

        return mUid < other.mUid;
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


