/*
 * Copyright (c) 2000-2001 Apple Computer, Inc. All Rights Reserved.
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
// structure - structural framework for securityd objects
//
#include "structure.h"


//
// NodeCore always has a destructor (because it's virtual),
// but its dump support is conditionally included.
//
NodeCore::~NodeCore()
{
#if defined(DEBUGDUMP)
	StLock<Mutex> _(mCoreLock);
	mCoreNodes.erase(this);
#endif //DEBUGDUMP
}


//
// Basic object mesh maintainance
//
void NodeCore::parent(NodeCore &p)
{
	StLock<Mutex> _(*this);
	mParent = &p;
}

void NodeCore::referent(NodeCore &r)
{
	StLock<Mutex> _(*this);
	assert(!mReferent);
	mReferent = &r;
}
	
void NodeCore::clearReferent()
{
	StLock<Mutex> _(*this);
	if (mReferent)
		assert(!mReferent->hasReference(*this));
	mReferent = NULL;
}


void NodeCore::addReference(NodeCore &p)
{
	StLock<Mutex> _(*this);
	assert(p.mReferent == this);
	mReferences.insert(&p);
}

void NodeCore::removeReference(NodeCore &p)
{
	StLock<Mutex> _(*this);
	assert(hasReference(p));
	mReferences.erase(&p);
}

#if !defined(NDEBUG)

bool NodeCore::hasReference(NodeCore &p)
{
	assert(p.refCountForDebuggingOnly() > 0);
	return mReferences.find(&p) != mReferences.end();
}

#endif //NDEBUG


//
// ClearReferences clears the reference set but does not propagate
// anything; it is NOT recursive.
//
void NodeCore::clearReferences()
{
	StLock<Mutex> _(*this);
	secdebug("ssnode", "%p clearing all %d references",
		this, int(mReferences.size()));
	mReferences.erase(mReferences.begin(), mReferences.end());
}


//
// Kill should be overloaded by Nodes to implement any cleanup and release
// operations that should happen at LOGICAL death of the represented object.
// This is where you should release ports, close files, etc.
// This default behavior, which you MUST include in your override,
// propagates kills to all active references, recursively.
//
void NodeCore::kill()
{
	StLock<Mutex> _(*this);
	for (ReferenceSet::const_iterator it = mReferences.begin(); it != mReferences.end(); it++)
		(*it)->kill();
	clearReferences();
}


void NodeCore::kill(NodeCore &ref)
{
	StLock<Mutex> _(*this);
	assert(hasReference(ref));
	ref.kill();
	removeReference(ref);
}


//
// NodeCore-level support for state dumping.
// Call NodeCore::dumpAll() to debug-dump all nodes.
// Note that enabling DEBUGDUMP serializes all node creation/destruction
// operations, and thus may cause significant shifts in thread interactions.
//
#if defined(DEBUGDUMP)

// The (uncounted) set of all known NodeCores in existence, with protective lock
set<NodeCore *> NodeCore::mCoreNodes;
Mutex NodeCore::mCoreLock;

// add a new NodeCore to the known set
NodeCore::NodeCore()
	: Mutex(Mutex::recursive)
{
	StLock<Mutex> _(mCoreLock);
	mCoreNodes.insert(this);
}

// partial-line common dump text for any NodeCore
// override this to add text to your Node type's state dump output
void NodeCore::dumpNode()
{
 Debug::dump("%s@%p rc=%u", Debug::typeName(*this).c_str(), this, unsigned(refCountForDebuggingOnly()));
	if (mParent)
		Debug::dump(" parent=%p", mParent.get());
	if (mReferent)
		Debug::dump(" referent=%p", mReferent.get());
}

// full-line dump of a NodeCore
// override this to completely re-implement the dump format for your Node type
void NodeCore::dump()
{
 dumpNode();
	if (!mReferences.empty()) {
		Debug::dump(" {");
		for (ReferenceSet::const_iterator it = mReferences.begin(); it != mReferences.end(); it++) {
			Debug::dump(" %p", it->get());
			if ((*it)->mReferent != this)
				Debug::dump("!*INVALID*");
		}
		Debug::dump(" }");
	}
	Debug::dump("\n");
}

// dump all known nodes
void NodeCore::dumpAll()
{
 StLock<Mutex> _(mCoreLock);
	time_t now; time(&now);
	Debug::dump("\nNODE DUMP (%24.24s)\n", ctime(&now));
	for (set<NodeCore *>::const_iterator it = mCoreNodes.begin(); it != mCoreNodes.end(); it++)
		(*it)->dump();
	Debug::dump("END NODE DUMP\n\n");
}

#endif //DEBUGDUMP
