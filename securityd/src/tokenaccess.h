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
// tokenaccess - access management to a TokenDatabase's Token's TokenDaemon's tokend
//
#ifndef _H_TOKENACCESS
#define _H_TOKENACCESS

#include "tokendatabase.h"
#include "tokenkey.h"
#include "server.h"


//
// Turn a Key into a TokenKey, when we know that it's that
//
inline TokenKey &myKey(Key &key)
{
	return safer_cast<TokenKey &>(key);
}


//
// The common access/retry/management framework for calls that go to the actual daemon.
//
class Access : public Token::Access {
public:
	Access(Token &token) : Token::Access(token), mIteration(0)
	{ Server::active().longTermActivity(); }
	template <class Whatever>
	Access(Token &token, Whatever &it) : Token::Access(token)
	{ add(it); Server::active().longTermActivity(); }
	
	void operator () (const CssmError &err);
	using Token::Access::operator ();

	void add(TokenAcl &acl)		{ mAcls.insert(&acl); }
	void add(TokenAcl *acl)		{ if (acl) mAcls.insert(acl); }
	void add(AclSource &src)	{ add(dynamic_cast<TokenAcl&>(src.acl())); }
	void add(AclSource *src)	{ if (src) add(*src); }
	void add(Key &key)			{ mAcls.insert(&myKey(key)); }

private:
	set<TokenAcl *> mAcls;		// TokenAcl subclasses to clear on retry
	unsigned int mIteration;	// iteration count (try, retry, give up)
};


//
// A nice little macro bracket to apply it.
// You must declare an Access called 'access' before doing
//	TRY
//		some actions
//		GUARD(a call to tokend)
//	DONE
//
#define TRY			for (;;) {
#define GUARD		try {
#define DONE		return; \
	} catch (const CssmError &error) { \
		access(error); \
	} }


#endif //_H_TOKENACCESS
