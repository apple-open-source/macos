/*
 * Copyright (c) 2004 Apple Computer, Inc. All Rights Reserved.
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
#ifndef _H_TOKENACL
#define _H_TOKENACL


//
// tokenacl - Token-based ACL implementation
//
#include "acls.h"
#include <security_cdsa_utilities/acl_preauth.h>

class Token;
class TokenDatabase;


//
// The Token version of a SecurityServerAcl.
//
class TokenAcl : public virtual SecurityServerAcl {
public:
	TokenAcl();
	
	typedef unsigned int ResetGeneration;

public:
	// implement SecurityServerAcl
	void getOwner(AclOwnerPrototype &owner);
	void getAcl(const char *tag, uint32 &count, AclEntryInfo *&acls);
    void changeAcl(const AclEdit &edit, const AccessCredentials *cred,
		Database *relatedDatabase);
	void changeOwner(const AclOwnerPrototype &newOwner, const AccessCredentials *cred,
		Database *relatedDatabase);

	void instantiateAcl();
	void changedAcl();

public:
	// required from our MDC
	virtual Token &token() = 0;
	virtual GenericHandle tokenHandle() const = 0;
	
protected:
	void invalidateAcl()	{ mLastReset = 0; }
	void pinChange(unsigned int pin, CSSM_ACL_HANDLE handle, TokenDatabase &database);
	
private:
	ResetGeneration mLastReset;
};


#endif //_H_TOKENACL
