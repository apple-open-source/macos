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
#ifndef _H_TRANSWALKERS
#define _H_TRANSWALKERS

#include <Security/AuthorizationWalkers.h>
#include "flippers.h"
#include "server.h"
#include <Security/context.h>
	
using LowLevelMemoryUtilities::increment;
using LowLevelMemoryUtilities::difference;


//
// Should we flip data?
// This looks at the current client's process information (a thread-global state)
// to determine flip status. Valid (only) within BEGIN_IPC/END_IPC brackets.
//
bool flipClient();


//
// A CheckingReconstituteWalker is a variant of an ordinary ReconstituteWalker
// that checks object pointers and sizes against the incoming block limits.
// It throws an exception if incoming data has pointers outside the incoming block.
// This avoids trouble inside of the SecurityServer caused (by bug or malice)
// from someone spoofing the client access side.
//
class CheckingReconstituteWalker {
private:
	void check(void *addr, size_t size)
	{
		if (addr < mBase || increment(addr, size) > mLimit)
			CssmError::throwMe(CSSM_ERRCODE_INVALID_POINTER);
	}

public:
    CheckingReconstituteWalker(void *ptr, void *base, size_t size, bool flip);
	
	template <class T>
	void operator () (T &obj, size_t size = sizeof(T))
	{
		check(increment(&obj, -mOffset), size);
		if (mFlip)
			Flippers::flip(obj);
	}

    template <class T>
    void operator () (T * &addr, size_t size = sizeof(T))
    {
		DEBUGWALK("checkreconst:ptr");
        if (addr) {
			// process the pointer
			void *p = addr;
			blob(p, size);
			addr = reinterpret_cast<T *>(p);

			// now flip the contents
			if (mFlip)
				Flippers::flip(*addr);
		}
    }
	
	template <class T>
	void blob(T * &addr, size_t size)
	{
		DEBUGWALK("checkreconst:blob");
		if (addr) {
			// flip the address (the pointer itself)
			if (mFlip) {
				secdebug("flippers", "flipping %s@%p", Debug::typeName(addr).c_str(), addr);
				Flippers::flip(addr);
			}
			
			// check the address against the transmitted bounds
			check(addr, size);
			
			// relocate it
            addr = increment<T>(addr, mOffset);
		}
    }
	
    static const bool needsRelinking = true;
    static const bool needsSize = false;
    
private:
	void *mBase;			// old base address
	void *mLimit;			// old last byte address + 1
    off_t mOffset;			// relocation offset
	bool mFlip;				// apply byte order flipping
};


//
// Process an incoming (IPC) data blob of type T.
// This relocates pointers to fit in the local address space,
// and fixes byte order issues as needed.
//
template <class T>
void relocate(T *obj, T *base, size_t size)
{
    if (obj) {
		if (base == NULL)	// invalid, could confuse walkers
			CssmError::throwMe(CSSM_ERRCODE_INVALID_POINTER);
        CheckingReconstituteWalker relocator(obj, base, size,
			Server::connection().process.byteFlipped());
        walk(relocator, base);
    }
}


//
// Special handling for incoming CSSM contexts.
//
void relocate(Context &context, void *base, Context::Attr *attrs, uint32 attrSize);


//
// A FlipWalker is a walker operator that collects its direct invocations
// into a set of memory objects. These objects can then collectively be
// byte-flipped (exactly once :-) at the flick of a function.
//
class FlipWalker {
private:
	struct FlipBase {
		virtual ~FlipBase() { }
		virtual void flip() const = 0;
	};

	template <class T>
	struct FlipRef : public FlipBase {
		T &obj;
		FlipRef(T &s) : obj(s)		{ }
		void flip() const		
{ secdebug("outflip", "%p flip/ref %s@%p", this, Debug::typeName(obj).c_str(), &obj);
{ Flippers::flip(obj); }
}
	};

	template <class T>
	struct FlipPtr : public FlipBase {
		T * &obj;
		FlipPtr(T * &s) : obj(s)	{ }
		void flip() const			
{ secdebug("outflip", "%p flip/ptr %s@%p(%p)", this, Debug::typeName(obj).c_str(), &obj, obj);
{ Flippers::flip(*obj); Flippers::flip(obj); }
}
};

	template <class T>
	struct FlipBlob : public FlipBase {
		T * &obj;
		FlipBlob(T * &s) : obj(s)	{ }
		void flip() const			
{ secdebug("outflip", "%p flip/blob %s@%p(%p)", this, Debug::typeName(obj).c_str(), &obj, obj);
{ Flippers::flip(obj); }
}
	};
	
	struct Flipper {
		FlipBase *impl;
		Flipper(FlipBase *p) : impl(p)	{ }
		bool operator < (const Flipper &other) const
			{ return impl < other.impl; }
	};
	
public:
	~FlipWalker();
	void doFlips(bool active = true);
	
	template <class T>
	void operator () (T &obj, size_t = sizeof(T))
	{ mFlips.insert(new FlipRef<T>(obj)); }
	
	template <class T>
	T *operator () (T * &addr, size_t size = sizeof(T))
	{ mFlips.insert(new FlipPtr<T>(addr)); return addr; }
	
	template <class T>
	void blob(T * &addr, size_t size)
	{ mFlips.insert(new FlipBlob<T>(addr)); }
	
	static const bool needsRelinking = true;
	static const bool needsSize = true;
	
private:
	set<Flipper> mFlips;
};


//
// A raw flip, conditioned on the client's flip state
//
template <class T>
void flip(T &addr)
{
	if (flipClient()) {
		secdebug("flippers", "raw flipping %s", Debug::typeName(addr).c_str());
		Flippers::flip(addr);
	}
}

template <class T>
void flips(T *value, T ** &addr, T ** &base)
{
	*addr = *base = value;
	if (flipClient()) {
		FlipWalker w;		// collector
		walk(w, value);		// collect all flippings needed
		w.doFlips();		// execute flips (flips value but leaves addr alone)
		Flippers::flip(*base); // flip base (so it arrives right side up)
	}
}


#endif //_H_TRANSWALKERS
