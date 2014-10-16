/*
 * Copyright (c) 2000-2004,2008 Apple Inc. All Rights Reserved.
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


//
// key - representation of securityd key objects
//
#ifndef _H_KEY
#define _H_KEY

#include "structure.h"
#include "database.h"
#include "acls.h"
#include <security_cdsa_utilities/u32handleobject.h>
#include <security_cdsa_client/keyclient.h>


class Database;


//
// A Key object represents a cryptographic key known to securityd and accessed by clients
// through securityd key references (KeyHandles). A Key may be raw or reference inside securityd,
// but from outside it is always a reference key, and we hide (as best we can) the details of
// its local storage and nature.
//
// Key is a very abstract class; it defines the minimal interface that all actual securityd
// keys must provide. Actual Key subclasses are produced by (subclasses of) Databases, which
// act as Key factories. Most Database subclasses will define Key class hierarchies to track
// their internal structure, but from out here, all we know is that Databases yield Key objects
// when asked nicely, and those subclass objects implement the Key protocol.
//
// A Key can be used by multiple Connections, even at the same time. It is possible for multiple
// Key objects to represent the same underlying cryptographic secret, so don't assume a 1-1 mapping.
//
// Key is completely agnostic as to how the key is stored or maintained.
// For all you know, it might be a virtual artifact of the Key subclass.
//
// All Key subclasses support ACLs. However, different subclasses may host
// their SecurityServerAcls at different levels of the object mesh. Don't assume.
//
// Key::attribute is there for a reason. If you want to check attributes,
// use it rather than returnKey - it may be much, much faster.
//
class Key : public Database::Subsidiary, public AclSource {
public:
	Key(Database &db);
	
	virtual const CssmData &canonicalDigest() = 0;
	
	Database &database() const { return referent<Database>(); }
	
	virtual CSSM_KEYATTR_FLAGS attributes() = 0;
	bool attribute(CSSM_KEYATTR_FLAGS f) { return attributes() & f; }
	
	virtual void returnKey(U32HandleObject::Handle &h, CssmKey::Header &hdr) = 0;
};


#endif //_H_KEY
