/*
 * Copyright (c) 2003 Apple Computer, Inc. All Rights Reserved.
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
	return Server::connection().process.byteFlipped();
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
