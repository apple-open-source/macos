/*
 * Copyright (c) 2000-2004 Apple Computer, Inc. All Rights Reserved.
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
#include <security_utilities/globalizer.h>
#include <security_utilities/debugging.h>
#include <cstdlib>
#include <stdexcept>

//
// The Error class thrown if Nexus operations fail
//
GlobalNexus::Error::~Error() throw()
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
//	(b) Pointers acquired from new have at least their LSB zero (are at
//      least two-byte aligned).
// It seems like it's been a while since anyone made a machine/runtime that
// violated either of those. But you have been warned.
//
void *ModuleNexusCommon::create(void *(*make)())
{
    sync++;		// keep mutex alive if needed
  retry:
    void *initialPointer = Atomic<void *>::load(pointer);	// latch pointer
    if (!initialPointer || (uintptr_t(initialPointer) & 0x1)) {
        Mutex *mutex;
        if (initialPointer == 0) {
            mutex = new Mutex;
            mutex->lock();
			if (!Atomic<void *>::casb(0, (void *)(uintptr_t(mutex) | 0x1), pointer)) {
                // somebody beat us to the lead - back off
                mutex->unlock();
                delete mutex;
                goto retry;
            }
            // we have the ball
            try {
                void *singleton = make();
                pointer = singleton;
                // we need a write barrier here, but the mutex->unlock below provides it for free
            } catch (...) {
				secdebug("nexus", "ModuleNexus %p construction failed", this);
                mutex->unlock();
                if (--sync == 0) {
                    delete mutex;
                    pointer = 0;
                }
                throw;
            }
        } else {
            mutex = reinterpret_cast<Mutex *>(uintptr_t(initialPointer) & ~0x1);
            mutex->lock();	// we'll wait here
        }
        mutex->unlock();
        //@@@ retry if not resolved -- or fail here (with "object can't be built")
        if (--sync == 0)
            delete mutex;
    }
    return pointer;
}


// thread nexus static globals
ModuleNexus<Mutex> ThreadNexusBase::mInstanceLock;

// Thread nexus globals
ModuleNexus<RetentionSet> ThreadNexusBase::mInstances;

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
			throw std::runtime_error("environment communication failed");
		if (mStore == store.get())		// we won the race...
			store.release();			// ... so keep the store
	} else
		if (sscanf(env, "*%p", &mStore) != 1)
			throw std::runtime_error("environment communication failed");
}
