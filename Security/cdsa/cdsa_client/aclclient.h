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
// keyclient 
//
#ifndef _H_CDSA_CLIENT_ACLCLIENT
#define _H_CDSA_CLIENT_ACLCLIENT  1

#include <Security/cssmaclpod.h>
#include <Security/cssmcred.h>

namespace Security
{

namespace CssmClient
{

class CSP;

//
// AclClient -- abstract interface implemented by objects that can manipulate their acls
//
class AclClient
{
public:	
	// Acl manipulation
	virtual void getAcl(const char *selectionTag, AutoAclEntryInfoList &aclInfos) const = 0;
	virtual void changeAcl(const CSSM_ACCESS_CREDENTIALS *accessCred,
						   const CSSM_ACL_EDIT &aclEdit) = 0;

	// Acl owner manipulation
	virtual void getOwner(AutoAclOwnerPrototype &owner) const = 0;
	virtual void changeOwner(const CSSM_ACCESS_CREDENTIALS *accessCred,
							 const CSSM_ACL_OWNER_PROTOTYPE &newOwner) = 0;

#if 0
	// Create a random owner
	static void makeRandomOwner(CSP &csp, AutoAclOwnerPrototype &owner, AutoCredentials &cred);
	void setOwnerAndAcl(const AutoCredentials &cred, const AutoAclOwnerPrototype &newOwner,
						uint32 numEntries, const CSSM_ACL_ENTRY_INFO *entries);
#endif
};


} // end namespace CssmClient

} // end namespace Security

#endif // _H_CDSA_CLIENT_ACLCLIENT
