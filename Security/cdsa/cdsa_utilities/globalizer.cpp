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
// globalizer - multiscope globalization services.
//
// This is a tentative, partial implementation.
// Status:
//	module scope: constructs, optional cleanup
//	thread scope: constructs, optional cleanup
//	process scope: not implemented (obsolete implementation, unused)
//	system scope: not implemented (probably never will)
//
// @@@ Assumption: {bool,T*} atomic unless PTHREAD_STRICT
//
#ifdef __MWERKS__
#define _CPP_GLOBALIZER
#endif
#include <Security/globalizer.h>
#include <cstdlib>


//
// The Error class thrown if Nexus operations fail
//
GlobalNexus::Error::~Error()
{
}


//
// The long (and possibly contentious) path of ModuleNexus()
//
// Briefly, the trick here is to go through a three-stage sequence
// to lazily construct a unique singleton object, no matter how many
// threads all of a sudden decide they need it.
// State sequence:
// State 0: pointer == 0, not initialized, idle
// State 1: pointer == mutexp | 0x1, where mutexp points to a Mutex
//  used to serialize construction of the singleton object
// State 2: pointer == &singleton, and we're done
//
// TAKE NOTE:
// This code is optimized with a particular issue in mind: when placed
// into static storage (as ModuleNexi are wont to), it should not require
// dynamic initialization. This is important because our code is, in effect,
// linked into just about every program in the system. The price we pay
// for this coolness is
//  (a) This won't work *except* in static storage (not on stack or heap)
//  (b) We slightly fracture portability (see below)
// This has been considered Worth It, at least for now. Before you throw
// up and throw this code out, please try to figure out whether you know
// the Whole Story. Thank you.
//
// WARNING:
// This code makes the following non-portable assumptions:
//  (a) NULL == 0 (binary representation of NULL pointer is zero value)
//	Pointers acquired from new have at least their LSB zero (are at
//  (b) least two-byte aligned).
// It seems like it's been a while since anyone made a machine/runtime that
// violated either of those. But you have been warned.
//
#if defined(_HAVE_ATOMIC_OPERATIONS)

AtomicWord ModuleNexusCommon::create(void *(*make)())
{
    sync++;		// keep mutex alive if needed
  retry:
    AtomicWord initialPointer = pointer;	// latch pointer
    if (!initialPointer || (initialPointer & 0x1)) {
        Mutex *mutex;
        if (initialPointer == 0) {
            mutex = new Mutex(false);	// don't bother debugging this one
            mutex->lock();
            if (atomicStore(pointer, AtomicWord(mutex) | 0x1, 0) != 0) {
                // somebody beat us to the lead - back off
                mutex->unlock();
                delete mutex;
                goto retry;
            }
            // we have the ball
            try {
                void *singleton = make();
                pointer = AtomicWord(singleton);
                // we need a write barrier here, but the mutex->unlock below provides it for free
                debug("nexus", "ModuleNexus %p constructed object 0x%x", this, pointer);
            } catch (...) {
				debug("nexus", "ModuleNexus %p construction failed", this);
                mutex->unlock();
                sync--;
                //@@@ set up for retry here?
                throw;
            }
        } else {
            mutex = reinterpret_cast<Mutex *>(pointer & ~0x1);
            mutex->lock();	// we'll wait here
        }
        mutex->unlock();
        //@@@ retry if not resolved -- or fail here (with "object can't be built")
        if (--sync == 0)
            delete mutex;
    }
    return pointer;
}

#endif //_HAVE_ATOMIC_OPERATIONS


//
// Process nexus operation
//
ProcessNexusBase::ProcessNexusBase(const char *identifier)
{
	const char *env = getenv(identifier);
	if (env == NULL) {	// perhaps we're first...
		auto_ptr<Store> store(new Store);
		char form[2*sizeof(Store *) + 2];
		sprintf(form, "*%p", &store);
		setenv(identifier, form, 0);	// do NOT overwrite...
		env = getenv(identifier);		// ... and refetch to resolve races
		if (sscanf(env, "*%p", &mStore) != 1)
			CssmError::throwMe(CSSM_ERRCODE_INTERNAL_ERROR /*"environment communication failed" */);
		if (mStore == store.get())		// we won the race...
			store.release();			// ... so keep the store
	} else
		if (sscanf(env, "*%p", &mStore) != 1)
			CssmError::throwMe(CSSM_ERRCODE_INTERNAL_ERROR /*"environment communication failed"*/);
}
