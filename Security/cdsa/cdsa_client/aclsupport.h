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


//
// aclsupport.h - support for special Keychain style acls
//

#ifndef _ACLSUPPORT_H_
#define _ACLSUPPORT_H_

#include <Security/cssmdata.h>
#include <Security/threading.h>
#include <Security/cssmalloc.h>
#include <Security/refcount.h>
#include <Security/keyclient.h>
#include <vector>


namespace Security
{

namespace CssmClient
{

class TrustedApplicationImpl : public RefCount
{
public:
	TrustedApplicationImpl(const CssmData &signature, const CssmData &comment, bool enabled);
	TrustedApplicationImpl(const char *path, const CssmData &comment, bool enabled);

	const CssmData &signature() const;
	const CssmData &comment() const;
	bool enabled() const;
	void enabled(bool enabled);

	bool sameSignature(const char *path); // return true if object at path has same signature
	CssmAutoData calcSignature(const char *path); // generate a signature

private:
	CssmAutoData mSignature;
	CssmAutoData mComment;
	bool mEnabled;
};

class TrustedApplication : public RefPointer<TrustedApplicationImpl>
{
public:
	TrustedApplication();
	TrustedApplication(const CssmData &signature, const CssmData &comment, bool enabled = true);
	TrustedApplication(const char *path, const CssmData &comment, bool enabled = true);
};

class KeychainACL : public vector<TrustedApplication>
{
public:
	KeychainACL(const Key &key);
	void commit();

	void anyAllow(bool allow);
	bool anyAllow() const;

	void alwaysAskUser(bool allow);
	bool alwaysAskUser() const;
	bool isCustomACL() const;
    void label(const CssmData &label);

private:
	void initialize();
	Key mKey;
	bool mAnyAllow;
	bool mAlwaysAskUser;
	bool mIsCustomACL;
	CssmAutoData mLabel;

	CSSM_ACL_HANDLE mHandle;
};

}; // end namespace CssmClient

} // end namespace Security

#endif // _ACLSUPPORT_H_
