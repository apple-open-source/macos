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
// key - representation of SecurityServer key objects
//
#ifndef _H_KEY
#define _H_KEY

#include "securityserver.h"
#include "acls.h"
#include <Security/utilities.h>
#include <Security/handleobject.h>


class Database;


//
// A Key object represents a CSSM_KEY known to the SecurityServer.
// We give each Key a handle that allows our clients to access it, while we use
// the Key's ACL to control such accesses.
// A Key can be used by multiple Connections. Whether more than one Key can represent
// the same actual key object is up to the CSP we use, so let's be tolerant about that.
//
// A note on key attributes: We keep two sets of attribute bits. The internal bits are used
// when talking to our CSP; the external bits are used when negotiating with our client(s).
// The difference is the bits in managedAttributes, which relate to persistent key storage
// and are not digestible by our CSP. The internal attributes are kept in mKey. The external
// ones are kept in mAttributes, and are a superset of the internal ones.
//
class Key : public HandleObject, public SecurityServerAcl {
public:
	//Key(Database *db, const CssmKey &newKey, uint32 usage, uint32 attrs,
	//	const AclEntryPrototype *owner = NULL);
	//Key(Database *db, const CssmKey &newKey, const AclEntryPrototype *owner = NULL);
	Key(Database &db, const KeyBlob *blob);
	Key(Database *db, const CssmKey &newKey, uint32 moreAttributes,
		const AclEntryPrototype *owner = NULL);
	virtual ~Key();
    
    Database *database() const { return mDatabase; }
    bool hasDatabase() const { return mDatabase != NULL; }
	
    // yield the decoded internal key -- internal attributes
	operator CssmKey &()		{ return keyValue(); }
	size_t length()				{ return keyValue().length(); }
	void *data()				{ return keyValue().data(); }
    
    // yield the approximate external key header -- external attributes
    void returnKey(Handle &h, CssmKey::Header &hdr);
    
    // we can also yield an encoded KeyBlob *if* we belong to a database	
	KeyBlob *blob();
    
    // calculate the UID value for this key (if possible)
    KeyUID &uid();
    
    // ACL state management hooks
	void instantiateAcl();
	void noticeAclChange();
    const Database *relatedDatabase() const;
    
    // key attributes that should not be passed on to the CSP
    static const uint32 managedAttributes = KeyBlob::managedAttributes;

private:
	void setup(const CssmKey &newKey, uint32 attrs);
    void decode();
    CssmKey::Header &keyHeader();
	CssmKey &keyValue();

private:
	CssmKey mKey;			// clear form CssmKey (attributes modified)
    CSSM_KEYATTR_FLAGS mAttributes; // full attributes (external form)
	bool mValidKey;			// CssmKey form is valid

	Database *mDatabase;	// the database we belong to, NULL if independent

	KeyBlob *mBlob;			// key blob encoded by mDatabase
	bool mValidBlob;		// mBlob is valid key encoding
    
    KeyUID mUID;			// cached UID
    bool mValidUID;			// UID has been calculated
};


#endif //_H_KEY
