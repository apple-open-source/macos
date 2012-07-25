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
// DigestContext.cpp 
//
#include "DigestContext.h"
#include <AppleCSPUtils.h>

/* 
 * Just field the expected/required calls from CSPFullPluginSession,
 * and dispatch them to mDigest.
 */
void DigestContext::init(const Context &context, bool)
{
	mDigest.digestInit();
}

void DigestContext::update(const CssmData &data)
{
	mDigest.digestUpdate((const uint8 *)data.data(), data.length());
}

void DigestContext::final(CssmData &data)
{
	data.length(mDigest.digestSizeInBytes());
	mDigest.digestFinal((uint8 *)data.data());
}

CSPFullPluginSession::CSPContext *DigestContext::clone(Allocator &)
{
	/* first clone the low-level digest object */
	DigestObject *newDigest = mDigest.digestClone();
	
	/* now construct a new context */
	return new DigestContext(session(), *newDigest);
}

size_t DigestContext::outputSize(bool, size_t) 
{
	return mDigest.digestSizeInBytes();
}

