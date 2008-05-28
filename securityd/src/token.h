/*
 * Copyright (c) 2004,2007 Apple Inc. All Rights Reserved.
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
// token - internal representation of a (single distinct) hardware token
//
#ifndef _H_TOKEN
#define _H_TOKEN

#include "structure.h"
#include "tokencache.h"
#include "tokenacl.h"
#include "tokend.h"
#include <security_utilities/pcsc++.h>
#include <securityd_client/ssnotify.h>

class Reader;
class TokenDbCommon;


//
// Token is the global-scope object representing a smartcard token.
// It also acts as the global-scope database object for the TokenDatabase representing
// its content, and carries the ObjectAcls for objects on the token.
//
class Token : public PerGlobal, public virtual TokenAcl, public FaultRelay {
public:
	class Access; friend class Access;
	
public:
	Token();
	~Token();
	
	::Reader &reader() const;
	TokenDaemon &tokend();
	GenericHandle tokenHandle() const;
	uint32 subservice() const { return mSubservice; }
	std::string printName() const { return mPrintName; }
	TokenCache::Token &cache() const { return *mCache; }
	
	void insert(::Reader &slot, RefPointer<TokenDaemon> tokend);
	void remove();
	
	void notify(NotificationEvent event);
	void fault(bool async);
	
	void kill();
	
	IFDUMP(void dumpNode());
	
	static RefPointer<Token> find(uint32 ssid);
	
	void getAcl(const char *tag, uint32 &count, AclEntryInfo *&acls);
	ResetGeneration resetGeneration() const;
	bool resetGeneration(ResetGeneration rg) const { return rg == resetGeneration(); }
	void resetAcls();
	
public:
	// SecurityServerAcl and TokenAcl personalities
	AclKind aclKind() const;
	Token &token();		// myself
	
	// FaultRelay personality
	void relayFault(bool async);
	
public:
	class Access {
	public:
		Access(Token &token);
		~Access();

		Token &token;
		
		TokenDaemon &tokend() const { return *mTokend; }
		TokenDaemon &operator () () const { return tokend(); }
		
	private:
		RefPointer<TokenDaemon> mTokend;
	};

public:
	// keep track of TokenDbCommons for reset processing
	// (this interface is for TokenDbCommon only)
	void addCommon(TokenDbCommon &dbc);
	void removeCommon(TokenDbCommon &dbc);
	
private:
	RefPointer<TokenDaemon> chooseTokend();

private:
	bool mFaulted;			// fault state flag
	RefPointer<TokenDaemon> mTokend; // the (one) tokend that runs the card
	RefPointer<TokenCache::Token> mCache;  // token cache reference
	std::string mPrintName;	// print name of token
	
	Guid mGuid;				// our CSP/DL's Guid
	uint32 mSubservice;		// dynamic subservice of gGuidAppleSdCSPDL
	PCSC::ReaderState mState; // reader state as of insertion
	
	TokenDaemon::Score mScore; // score of winning tokend

private:
	typedef map<uint32, Token *> SSIDMap;
	static SSIDMap mSubservices;
	static Mutex mSSIDLock;

	typedef set<TokenDbCommon *> CommonSet;
	CommonSet mCommons;
	ResetGeneration mResetLevel;
};


#endif //_H_TOKEN
