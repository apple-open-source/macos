/*
 * Copyright (c) 2004,2007-2008 Apple Inc. All Rights Reserved.
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
#include "reader.h"


//
// Construct a Reader
// This does not commence state tracking; call update to start up the reader.
//
Reader::Reader(TokenCache &tc, const PCSC::ReaderState &state)
	: cache(tc), mType(pcsc), mToken(NULL)
{
	mName = state.name();	// remember separate copy of name
	mPrintName = mName;		//@@@ how to make this readable? Use IOKit information?
	secdebug("reader", "%p (%s) new PCSC reader", this, name().c_str());
}

Reader::Reader(TokenCache &tc, const string &identifier)
	: cache(tc), mType(software), mToken(NULL)
{
	mName = identifier;
	mPrintName = mName;
	secdebug("reader", "%p (%s) new software reader", this, name().c_str());
}

Reader::~Reader()
{
	secdebug("reader", "%p (%s) destroyed", this, name().c_str());
}


//
// Type qualification. None matches anything.
//
bool Reader::isType(Type reqType) const
{
	return reqType == this->type();
}


//
// Killing a reader forcibly removes its Token, if any
//
void Reader::kill()
{
	if (mToken)
		removeToken();
	NodeCore::kill();
}


//
// State transition matrix for a reader, based on PCSC state changes
//
void Reader::update(const PCSC::ReaderState &state)
{
	// set new state
	unsigned long oldState = mState.state();
	mState = state;
	mState.name(mName.c_str());		// (fix name pointer, unchanged)
	
	try {
		if (state.state(SCARD_STATE_UNAVAILABLE)) {
			// reader is unusable (probably being removed)
			secdebug("reader", "%p (%s) unavailable (0x%lx)",
				this, name().c_str(), state.state());
			if (mToken)
				removeToken();
		} else if (state.state(SCARD_STATE_EMPTY)) {
			// reader is empty (no token present)
			secdebug("reader", "%p (%s) empty (0x%lx)",
				this, name().c_str(), state.state());
			if (mToken)
				removeToken();
		} else if (state.state(SCARD_STATE_PRESENT)) {
			// reader has a token inserted
			secdebug("reader", "%p (%s) card present (0x%lx)",
				this, name().c_str(), state.state());
			//@@@ is this hack worth it (with notifications in)??
			if (mToken && CssmData(state) != CssmData(pcscState()))
				removeToken();  // incomplete but better than nothing
			//@@@ or should we call some verify-still-the-same function of tokend?
			//@@@ (I think pcsc will return an error if the card changed?)
			if (!mToken)
				insertToken(NULL);
		} else {
			secdebug("reader", "%p (%s) unexpected state change (0x%lx to 0x%lx)",
				this, name().c_str(), oldState, state.state());
		}
	} catch (...) {
		secdebug("reader", "state update exception (ignored)");
	}
}


void Reader::insertToken(TokenDaemon *tokend)
{
	RefPointer<Token> token = new Token();
	token->insert(*this, tokend);
	mToken = token;
	addReference(*token);
	secdebug("reader", "%p (%s) inserted token %p",
		this, name().c_str(), mToken);
}


void Reader::removeToken()
{
	secdebug("reader", "%p (%s) removing token %p",
		this, name().c_str(), mToken);
	assert(mToken);
	mToken->remove();
	removeReference(*mToken);
	mToken = NULL;
}


//
// Debug dump support
//
#if defined(DEBUGDUMP)

void Reader::dumpNode()
{
	PerGlobal::dumpNode();
	Debug::dump(" [%s] state=0x%lx", name().c_str(), mState.state());
}

#endif //DEBUGDUMP
