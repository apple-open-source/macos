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
// reader - token reader objects
//
#ifndef _H_READER
#define _H_READER

#include "structure.h"
#include "token.h"
#include "tokencache.h"
#include <security_utilities/pcsc++.h>


//
// A Reader object represents a token (card) reader device attached to the
// system.
//
class Reader : public PerGlobal {
public:
	Reader(TokenCache &cache, const PCSC::ReaderState &state);	// PCSC managed
	Reader(TokenCache &cache, const std::string &name);			// software
	~Reader();
	
	enum Type {
		pcsc,				// represents PCSC-managed reader
		software			// software (virtual) reader,
	};
	Type type() const { return mType; }
	bool isType(Type type) const;
	
	TokenCache &cache;
	
	void kill();
	
	string name() const { return mName; }
	string printName() const { return mPrintName; }
	const PCSC::ReaderState &pcscState() const { return mState; }

	void insertToken(TokenDaemon *tokend);
	void update(const PCSC::ReaderState &state);
	void removeToken();
	
	IFDUMP(void dumpNode());
	
protected:
	
private:
	Type mType;
	string mName;			// PCSC reader name
	string mPrintName;		// human readable name of reader
	PCSC::ReaderState mState; // name field not valid (use mName)
	Token *mToken;			// token inserted here (also in references)
};


#endif //_H_READER
