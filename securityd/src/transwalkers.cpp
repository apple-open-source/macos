/*
 * Copyright (c) 2003-2004 Apple Computer, Inc. All Rights Reserved.
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
// transwalkers - server side transition data walking support
//
// These are data walker operators for securely marshaling and unmarshaling
// data structures across IPC. They are also in charge of fixing byte order
// inconsistencies between server and clients.
//
#include <transwalkers.h>
	

using LowLevelMemoryUtilities::increment;
using LowLevelMemoryUtilities::difference;


bool flipClient()
{
	return Server::process().byteFlipped();
}


//
// CheckingRelocateWalkers
//
CheckingReconstituteWalker::CheckingReconstituteWalker(void *ptr, void *base, size_t size, bool flip)
	: mBase(base), mFlip(flip)
{
	if (mFlip)
		Flippers::flip(mBase);	// came in reversed; fix for base use
	mOffset = difference(ptr, mBase);
	mLimit = increment(mBase, size);
}


//
// Relocation support
//
void relocate(Context &context, void *base, Context::Attr *attrs, uint32 attrSize)
{
	flip(context);
	CheckingReconstituteWalker relocator(attrs, base, attrSize, flipClient());
	context.ContextAttributes = attrs;	// fix context->attr vector link
	for (uint32 n = 0; n < context.attributesInUse(); n++)
		walk(relocator, context[n]);
}


//
// Outbound flipping support
//
FlipWalker::~FlipWalker()
{
	for (set<Flipper>::const_iterator it = mFlips.begin(); it != mFlips.end(); it++)
		delete it->impl;
}

void FlipWalker::doFlips(bool active)
{
	if (active) {
		secdebug("flipwalkers", "starting outbound flips");
		for (set<Flipper>::const_iterator it = mFlips.begin(); it != mFlips.end(); it++)
			it->impl->flip();
		secdebug("flipwalkers", "outbound flips done");
	}
}


//
// Choose a Database from a choice of two sources, giving preference
// to persistent stores and to earlier sources.
//
Database *pickDb(Database *db1, Database *db2)
{
	// persistent db1 always wins
	if (db1 && !db1->transient())
		return db1;
	
	// persistent db2 is next choice
	if (db2 && !db2->transient())
		return db2;
	
	// pick any existing transient database
	if (db1)
		return db1;
	if (db2)
		return db2;
	
	// none at all. use the canonical transient store
	return Server::optionalDatabase(noDb);
}
