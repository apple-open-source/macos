/*
 * Copyright (c) 2000-2004,2009 Apple Inc. All Rights Reserved.
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

#ifndef _H_CREDENTIAL
#define _H_CREDENTIAL

#include <security_utilities/refcount.h>
#include <CoreFoundation/CFDate.h>
#include <set>

namespace Authorization {
    
    // There should be an abstract base class for Credential so we can have 
    // different kinds, e.g., those associated with smart-card auth, or those
    // not requiring authentication as such at all.  (<rdar://problem/6556724>)

/* Credentials are less than comparable so they can be put in sets or maps. */
class CredentialImpl : public RefCount
{
public:
		CredentialImpl();
        CredentialImpl(const uid_t uid, const string &username, const string &realname, bool shared);
        CredentialImpl(const string &username, const string &password, bool shared);
		CredentialImpl(const string &right, bool shared);
        ~CredentialImpl();

        bool operator < (const CredentialImpl &other) const;

        // Returns true if this credential should be shared.
        bool isShared() const;

        // Merge with other
        void merge(const CredentialImpl &other);

        // The time at which this credential was obtained.
        CFAbsoluteTime creationTime() const;

        // Return true iff this credential is valid.
        bool isValid() const;

        // Make this credential invalid.
        void invalidate();

        // We could make Rule a friend but instead we just expose this for now
        inline const uid_t uid() const { return mUid; }
        inline const string& name() const { return mName; }
        inline const string& realname() const { return mRealName; }
        inline const bool isRight() const { return mRight; }
    
private:
        bool mShared;       // credential is shared
        bool mRight;            // is least-privilege credential


        // Fields below are not used by less-than operator

        // The user that provided his password.
        uid_t mUid;
        string mName;
        string mRealName;

        CFAbsoluteTime mCreationTime;
        bool mValid;
};

/* Credentials are less than comparable so they can be put in sets or maps. */
class Credential : public RefPointer<CredentialImpl>
{
public:
        Credential();
        Credential(CredentialImpl *impl);
        Credential(const uid_t uid, const string &username, const string &realname, bool shared);
        Credential(const string &username, const string &password, bool shared);
		Credential(const string &right, bool shared);		
        ~Credential();

        bool operator < (const Credential &other) const;
};

typedef set<Credential> CredentialSet;

} // namespace Authorization

#endif // _H_CREDENTIAL
