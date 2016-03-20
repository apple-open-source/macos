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


//
// tempdatabase - temporary (scratch) storage for keys
//
// A TempDatabase locally manages keys using the AppleCSP while providing
// no persistent storage. Keys live until they are no longer referenced in
// client space, at which point they are destroyed.
//
#ifndef _H_TEMPDATABASE
#define _H_TEMPDATABASE

#include "localdatabase.h"


//
// A TempDatabase is simply a container of (a subclass of) LocalKey.
// When it dies, all its contents irretrievably vanish. There is no DbCommon
// or global object; each TempDatabase is completely distinct.
// Database ACLs are not (currently) supported on TempDatabases.
//
class TempDatabase : public LocalDatabase {
public:
	TempDatabase(Process &proc);

	const char *dbName() const;
    uint32 dbVersion();
	bool transient() const;
	
	RefPointer<Key> makeKey(const CssmKey &newKey, uint32 moreAttributes,
		const AclEntryPrototype *owner);
	
	void generateKey(const Context &context,
		 const AccessCredentials *cred, 
		 const AclEntryPrototype *owner, uint32 usage, 
		 uint32 attrs, RefPointer<Key> &newKey);
	
protected:
	void getSecurePassphrase(const Context &context, string &passphrase);
	void makeSecurePassphraseKey(const Context &context, const AccessCredentials *cred, 
								 const AclEntryPrototype *owner, uint32 usage, 
								 uint32 attrs, RefPointer<Key> &newKey);
};

#endif //_H_TEMPDATABASE
