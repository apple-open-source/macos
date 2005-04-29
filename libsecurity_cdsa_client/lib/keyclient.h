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
#ifndef _H_CDSA_CLIENT_KEYCLIENT
#define _H_CDSA_CLIENT_KEYCLIENT  1

#include <security_cdsa_client/aclclient.h>
#include <security_cdsa_client/cspclient.h>

namespace Security
{

namespace CssmClient
{

//
// Key
//
class KeyImpl : public ObjectImpl, public AclBearer, public CssmKey
{
public:
	KeyImpl(const CSP &csp);
	KeyImpl(const CSP &csp, const CSSM_KEY &key, bool copy = false);
	KeyImpl(const CSP &csp, const CSSM_DATA &keyData);
	virtual ~KeyImpl();
	
	CSP csp() const { return parent<CSP>(); }
	void deleteKey(const CSSM_ACCESS_CREDENTIALS *cred);
    
    CssmKeySize sizeInBits() const;

	// Acl manipulation
	void getAcl(AutoAclEntryInfoList &aclInfos, const char *selectionTag = NULL) const;
	void changeAcl(const CSSM_ACL_EDIT &aclEdit,
		const CSSM_ACCESS_CREDENTIALS *accessCred);

	// Acl owner manipulation
	void getOwner(AutoAclOwnerPrototype &owner) const;
	void changeOwner(const CSSM_ACL_OWNER_PROTOTYPE &newOwner,
		const CSSM_ACCESS_CREDENTIALS *accessCred = NULL);

	// Call this after completing the CSSM API call after having called Key::makeNewKey()
	void activate();

protected:
	void deactivate(); 
};

class Key : public Object
{
public:
	typedef KeyImpl Impl;
	explicit Key(Impl *impl) : Object(impl) {}
	
	Key() : Object(NULL) {}
	Key(const CSP &csp, const CSSM_KEY &key, bool copy = false)	: Object(new Impl(csp, key, copy)) {}
	Key(const CSP &csp, const CSSM_DATA &keyData)	: Object(new Impl(csp, keyData)) {}

	// Creates an inactive key, client must call activate() after this.
	Key(const CSP &csp) : Object(new Impl(csp)) {}

	Impl *operator ->() const			{ return (*this) ? &impl<Impl>() : NULL; }
	Impl &operator *() const			{ return impl<Impl>(); }

	// Conversion operators to CssmKey baseclass.
	operator const CssmKey * () const	{ return (*this) ? &(**this) : NULL; }
	operator const CssmKey & () const	{ return **this; }
	
	// a few shortcuts to make life easier
	CssmKey::Header &header() const		{ return (*this)->header(); }

	// Creates an inactive key, client must call activate() after this.
	CssmKey *makeNewKey(const CSP &csp)	{ (*this) = Key(csp); return &(**this); }
    
    // inquiries
    CssmKeySize sizeInBits() const		{ return (*this)->sizeInBits(); }
};


struct KeySpec {
	CSSM_KEYUSE usage;
	CSSM_KEYATTR_FLAGS attributes;
	const CssmData *label;
	//add rc context
	
	KeySpec(CSSM_KEYUSE u, CSSM_KEYATTR_FLAGS a) : usage(u), attributes(a), label(NULL) { }
	KeySpec(CSSM_KEYUSE u, CSSM_KEYATTR_FLAGS a, const CssmData &l) : usage(u), attributes(a), label(&l) { }
};

} // end namespace CssmClient

} // end namespace Security


#endif // _H_CDSA_CLIENT_KEYCLIENT
