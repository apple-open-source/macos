/*
 * Copyright (c) 2000-2001,2004,2008 Apple Inc. All Rights Reserved.
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
// localkey - Key objects that store a local CSSM key object
//
#ifndef _H_LOCALKEY
#define _H_LOCALKEY

#include "key.h"
#include <security_cdsa_client/keyclient.h>


class LocalDatabase;


//
// A LocalKey object represents a CssmKey known to securityd. This subclass of Key is the
// parent of all Key objects that rely on local storage of the raw key matter. Cryptographic
// operations are performed by a local CSP within securityd's address space.
//
// LocalKeys are paired with LocalDatabases; LocalKey subclasses must be produced by, and must
// belong to, subclasses of LocalDatabase.
//
// LocalKeys implement their ACLs with a local evaluation machine that does not rely on an outside
// agent for evaluation. It is still possible for different subclasses of LocalDatabase to host
// their ObjectAcl instances at different globality layers.
//
// Since the local CSP refuses to deal with storage-related key attributes, we split the keys's
// CSSM_KEY_ATTRBITS into two parts:
//  (*) The KeyHeader.attributes() contain attributes as seen by the local CSP.
//  (*) The local mAttributes member contains attributes as seen by the client.
// The two are related by a simple formula: take the external attributes, remove the global-storage
// bits, add the EXTRACTABLE bit (so securityd itself can get at the key matter), and use that in
// the CssmKey. The reverse transition is done on the way out. A local subclass of KeySpec is used
// to make this more consistent. Just follow the pattern.
//
class LocalKey : public Key {
public:
	LocalKey(Database &db, const CssmKey &newKey, uint32 moreAttributes);
	virtual ~LocalKey();
    
	LocalDatabase &database() const;
	
    // yield the decoded internal key -- internal attributes
	CssmClient::Key key()		{ return keyValue(); }
	const CssmKey &cssmKey()	{ return keyValue(); }
	operator CssmClient::Key ()	{ return keyValue(); }
	operator const CssmKey &()	{ return keyValue(); }
    operator const CSSM_KEY & () { return keyValue(); }
    
    // yield the approximate external key header -- external attributes
    void returnKey(U32HandleObject::Handle &h, CssmKey::Header &hdr);
	
	// generate the canonical key digest
	const CssmData &canonicalDigest();
    
	CSSM_KEYATTR_FLAGS attributes();
	
public:
    // key attributes that should not be passed on to the CSP
    static const CSSM_KEYATTR_FLAGS managedAttributes = KeyBlob::managedAttributes;
	// these attributes are "forced on" in internal keys (but not always in external attributes)
	static const CSSM_KEYATTR_FLAGS forcedAttributes = KeyBlob::forcedAttributes;
	// these attributes are internally generated, and invalid on input
	static const CSSM_KEYATTR_FLAGS generatedAttributes =
		CSSM_KEYATTR_ALWAYS_SENSITIVE | CSSM_KEYATTR_NEVER_EXTRACTABLE;
	
	// a version of KeySpec that self-checks and masks for CSP operation
	class KeySpec : public CssmClient::KeySpec {
	public:
		KeySpec(CSSM_KEYUSE usage, CSSM_KEYATTR_FLAGS attrs);
		KeySpec(CSSM_KEYUSE usage, CSSM_KEYATTR_FLAGS attrs, const CssmData &label);
	};
	
private:
	void setup(const CssmKey &newKey, CSSM_KEYATTR_FLAGS attrs);
	CssmClient::Key keyValue();
	
protected:
	LocalKey(Database &db, CSSM_KEYATTR_FLAGS attributes);
	void setOwner(const AclEntryPrototype *owner);
	
	virtual void getKey();				// decode into mKey or throw
	virtual void getHeader(CssmKey::Header &hdr); // get header (only) without mKey

protected:
	bool mValidKey;			// CssmKey form is valid
	CssmClient::Key mKey;	// clear form CssmKey (attributes modified)

    CSSM_KEYATTR_FLAGS mAttributes; // full attributes (external form)
	CssmAutoData mDigest;	// computed key digest (cached)
};


#endif //_H_LOCALKEY
