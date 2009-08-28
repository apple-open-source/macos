/*
 * Copyright (c) 2000-2001,2008 Apple Inc. All Rights Reserved.
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
#ifndef _H_STRUCTURE
#define _H_STRUCTURE

#include <security_utilities/refcount.h>
#include <security_utilities/mach++.h>
#include <security_cdsa_utilities/u32handleobject.h>
#include <map>
#include "dtrace.h"

using MachPlusPlus::Port;


//
// Track a per-process real world object
//
template <class Base, class Glob> class Node;
class PerConnection;
class PerProcess;
class PerSession;
class PerGlobal;


//
// A generic core node of the object mesh.
// Repeat after me: "Everything that matters is a Node."
//
// This contains the mesh links (as smart pointers to NodeCores).
// The 'parent' is the next-more-global related object in the mesh, if any;
//  nodes with the same parent "belong together" at the more global layer.
//  For example, processes have their sessions as parents.
// The 'referent' is an object at the *same* globality layer that controls
//  the lifetime of this node. For example, a Database has its Process as
//  its referent.
// Both parent and referent are optional (can be NULL).
// The references set is a partial referent back-link. All NodeCores listed
//  in a node's References have this node as a referent, but the set is
//  selective (not necessarily complete). The References set propagates the
//  'kill' operation up the referents chain; thus being included in a node's
//  References means that a kill() on the referent will (recursively) kill
//  all references, too.
//
// Do not inherit directly from NodeCore; use Node<> (below).
//
class NodeCore : public RefCount, public Mutex {
	template <class Base, class Glob> friend class Node;
public:
#if !defined(DEBUGDUMP) // (see below if DEBUGDUMP)
	NodeCore() : Mutex(Mutex::recursive) { }
#endif
	virtual ~NodeCore();

	void addReference(NodeCore &p);
	void removeReference(NodeCore &p);

	// reference set operations
	template <class Sub>
	void allReferences(void (Sub::*func)());
	template <class Sub, class Value>
	RefPointer<Sub> findFirst(Value (Sub::*func)() const, Value compare);
	void clearReferences();

	virtual void kill();				// kill all references and self
	virtual void kill(NodeCore &ref);	// kill ref from my references()

	// for STL ordering (so we can have sets of RefPointers of NodeCores)
	bool operator < (const NodeCore &other) const
	{ return this < &other; }
	
protected:
	void parent(NodeCore &p);			// set parent
	void referent(NodeCore &r);			// set referent
	void clearReferent();				// clear referent
	
	bool hasParent() const { return mParent; }
	bool hasReferent() const { return mReferent; }

private:
	RefPointer<NodeCore> mParent;
	RefPointer<NodeCore> mReferent;
	typedef set<RefPointer<NodeCore> > ReferenceSet;
	ReferenceSet mReferences;
	
	IFDEBUG(bool hasReference(NodeCore &p));
	
#if defined(DEBUGDUMP)
public: // dump support
	NodeCore();						// dump-only constructor (registers node)

	virtual void dumpNode();		// node description (partial line)
	virtual void dump();			// dumpNode() + references + NL
	static void dumpAll();			// dump all nodes

	static Mutex mCoreLock;			// lock for mCoreNodes
	static set<NodeCore *> mCoreNodes; // (debug) set of all known nodes
#endif //DEBUGDUMP
};


//
// Call a member on each reference of a Node<> object.
// The object lock is held throughout, and we keep a RefPointer to each object
// as it's being processed. Thus it's safe for a reference to self-unlink in
// the object mesh; it'll get destroyed after its method returns.
//
template <class Sub>
void NodeCore::allReferences(void (Sub::*func)())
{
	StLock<Mutex> _(*this);
	for (ReferenceSet::const_iterator it = mReferences.begin(); it != mReferences.end();)
		if (RefPointer<Sub> sub = dynamic_cast<Sub *>((it++)->get()))
			(sub->*func)();
}


//
// Find a reference of a Node<> object that satisfies a simple "method returns value"
// condition. There is no defined order of the scan, so if the condition is not unique,
// any one reference may be returned. If none are found, we return NULL.
// This returns a RefPointer: lifetime of the returned instance (if any) is ensured even
// if it is asynchronously removed from the references set.
//
template <class Sub, class Value>
RefPointer<Sub> NodeCore::findFirst(Value (Sub::*func)() const, Value compare)
{
	StLock<Mutex> _(*this);
	for (ReferenceSet::const_iterator it = mReferences.begin(); it != mReferences.end(); it++)
		if (Sub *sub = dynamic_cast<Sub *>(it->get()))
			if ((sub->*func)() == compare)
				return sub;
	return NULL;
}


//
// A typed node of the object mesh.
// This adds type-safe accessors and modifiers to NodeCore.
//
template <class Base, class Glob>
class Node : public NodeCore {
protected:
	// type-safer versions of node mesh setters
	void parent(Glob &p)				{ NodeCore::parent(p); }
	void referent(Base &r)				{ NodeCore::referent(r); }
	
public:
	template <class T>
	T& parent() const
	{ assert(mParent); return safer_cast<T &>(*mParent); }
	
	template <class T>
	T& referent() const
	{ assert(mReferent); return safer_cast<T &>(*mReferent); }
	
public:
	void addReference(Base &p)			{ NodeCore::addReference(p); }
	void removeReference(Base &p)		{ NodeCore::removeReference(p); }
};


//
// Connection (client thread) layer nodes
//
class PerConnection : public Node<PerConnection, PerProcess> {
public:
};


//
// Process (client process) layer nodes
//
class PerProcess : public U32HandleObject, public Node<PerProcess, PerSession> {
public:	
};


//
// Session (client-side session) layer nodes
//
class PerSession : public Node<PerSession, PerGlobal> {
public:
};


//
// Global (per-system) layer nodes
//
class PerGlobal : public Node<PerGlobal, PerGlobal> {
public:
};


//
// A map from mach port names to (refcounted) pointers-to-somethings
//
template <class Node>
class PortMap : public Mutex, public std::map<Port, RefPointer<Node> > {
	typedef std::map<Port, RefPointer<Node> > _Map;
public:
	bool contains(mach_port_t port) const   { return this->find(port) != this->end(); }
	Node *getOpt(mach_port_t port) const
	{
		typename _Map::const_iterator it = this->find(port);
		return (it == this->end()) ? NULL : it->second;
	}
	
	Node *get(mach_port_t port) const
	{
		typename _Map::const_iterator it = this->find(port);
		assert(it != this->end());
		return it->second;
	}
	
	Node *get(mach_port_t port, OSStatus error) const
	{
		typename _Map::const_iterator it = this->find(port);
		if (it == this->end())
			MacOSError::throwMe(error);
		return it->second;
	}
	
	void dump();
};

template <class Node>
void PortMap<Node>::dump()
{
	for (typename _Map::const_iterator it = this->begin(); it != this->end(); it++)
		it->second->dump();
}


#endif //_H_STRUCTURE
