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
// acls - SecurityServer ACL implementation
//
#include "acls.h"
#include "connection.h"
#include "server.h"
#include "SecurityAgentClient.h"
#include <Security/acl_any.h>
#include <Security/acl_password.h>
#include <Security/acl_threshold.h>


//
// SecurityServerAcl is virtual
//
SecurityServerAcl::~SecurityServerAcl()
{ }


//
// Each SecurityServerAcl type must provide some indication of a database
// it is associated with. The default, naturally, is "none".
//
const Database *SecurityServerAcl::relatedDatabase() const
{ return NULL; }


//
// Provide environmental information to get/change-ACL calls.
// Also make them virtual so our children can override them.
//
void SecurityServerAcl::cssmGetAcl(const char *tag, uint32 &count, AclEntryInfo * &acls)
{
	instantiateAcl();
	return ObjectAcl::cssmGetAcl(tag, count, acls);
}

void SecurityServerAcl::cssmGetOwner(AclOwnerPrototype &owner)
{
	instantiateAcl();
	return ObjectAcl::cssmGetOwner(owner);
}

void SecurityServerAcl::cssmChangeAcl(const AclEdit &edit, const AccessCredentials *cred)
{
	instantiateAcl();
	SecurityServerEnvironment env(*this);
	ObjectAcl::cssmChangeAcl(edit, cred, &env);
	noticeAclChange();
}

void SecurityServerAcl::cssmChangeOwner(const AclOwnerPrototype &newOwner,
	const AccessCredentials *cred)
{
	instantiateAcl();
	SecurityServerEnvironment env(*this);
	ObjectAcl::cssmChangeOwner(newOwner, cred, &env);
	noticeAclChange();
}


//
// Modified validate() methods to connect all the conduits...
//
void SecurityServerAcl::validate(AclAuthorization auth, const AccessCredentials *cred) const
{
    SecurityServerEnvironment env(*this);
    ObjectAcl::validate(auth, cred, &env);
}

void SecurityServerAcl::validate(AclAuthorization auth, const Context &context) const
{
	validate(auth,
		context.get<AccessCredentials>(CSSM_ATTRIBUTE_ACCESS_CREDENTIALS));
}


//
// This function decodes the "special passphrase samples" that provide passphrases
// to the SecurityServer through ACL sample blocks. Essentially, it trolls a credentials
// structure's samples for the special markers, resolves anything that contains
// passphrases outright (and returns true), or returns false if the normal interactive
// procedures are to be followed.
// (This doesn't strongly belong to the SecurityServerAcl class, but doesn't really have
// a better home elsewhere.)
//
bool SecurityServerAcl::getBatchPassphrase(const AccessCredentials *cred,
	CSSM_SAMPLE_TYPE neededSampleType, CssmOwnedData &passphrase)
{
    if (cred) {
		// check all top-level samples
        const SampleGroup &samples = cred->samples();
        for (uint32 n = 0; n < samples.length(); n++) {
            TypedList sample = samples[n];
            if (!sample.isProper())
                CssmError::throwMe(CSSM_ERRCODE_INVALID_SAMPLE_VALUE);
            if (sample.type() == neededSampleType) {
                sample.snip();
                if (!sample.isProper())
                    CssmError::throwMe(CSSM_ERRCODE_INVALID_SAMPLE_VALUE);
                switch (sample.type()) {
                case CSSM_SAMPLE_TYPE_KEYCHAIN_PROMPT:
                    return false;
                case CSSM_SAMPLE_TYPE_PASSWORD:
					if (sample.length() != 2)
						CssmError::throwMe(CSSM_ERRCODE_INVALID_SAMPLE_VALUE);
					passphrase = sample[1];
                    return true;
                default:
                    CssmError::throwMe(CSSM_ERRCODE_INVALID_SAMPLE_VALUE);
                }
            }
        }
    }
	return false;
}


//
// Implement our environment object
//
uid_t SecurityServerEnvironment::getuid() const
{
    return Server::connection().process.uid();
}

gid_t SecurityServerEnvironment::getgid() const
{
    return Server::connection().process.gid();
}

pid_t SecurityServerEnvironment::getpid() const
{
    return Server::connection().process.pid();
}

bool SecurityServerEnvironment::verifyCodeSignature(const CodeSigning::Signature *signature)
{
	return Server::connection().process.verifyCodeSignature(signature);
}
