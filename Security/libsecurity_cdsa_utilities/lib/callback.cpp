/*
 * Copyright (c) 2000-2001,2003-2004,2006 Apple Computer, Inc. All Rights Reserved.
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
// Encapsulate the callback mechanism of CSSM.
//
#include <security_cdsa_utilities/callback.h>


//
// Invoke a callback
//
void ModuleCallback::operator () (CSSM_MODULE_EVENT event,
                                  const Guid &guid, uint32 subId,
                                  CSSM_SERVICE_TYPE serviceType) const
{
    try
    {
        if (mCallback)
            if (CSSM_RETURN err = mCallback(&guid, mContext, subId, serviceType, event))
                CssmError::throwMe(err);
    }
    catch (...)
    {
    }
}


//
// Manage Callback sets.
// THREADS: Caller is ensuring single-thread access on these calls.
//
void ModuleCallbackSet::insert(const ModuleCallback &newCallback)
{
    callbacks.insert(CallbackMap::value_type(newCallback, new CountingMutex));
}

void ModuleCallbackSet::erase(const ModuleCallback &oldCallback)
{
    CallbackMap::iterator it = callbacks.find(oldCallback);
    if (it == callbacks.end())	// not registered; fail
        CssmError::throwMe(CSSMERR_CSSM_INVALID_ADDIN_HANDLE);
    CountingMutex *counter = it->second;
    {
        StLock<Mutex> _(*counter);
        if (!counter->isIdle()) // callbacks are scheduled against this
            CssmError::throwMe(CSSM_ERRCODE_FUNCTION_FAILED);	// @#module is busy
    }
    // counter is zero (idle), and we hold the entry lock (via our caller)
    delete counter;
    callbacks.erase(it);
}


//
// Invoke an entire callback set.
// THREADS: Caller is ensuring  single-thread access on these calls.
//
void ModuleCallbackSet::operator () (CSSM_MODULE_EVENT event,
                                     const Guid &guid, uint32 subId,
                                     CSSM_SERVICE_TYPE serviceType) const
{
    if (callbacks.empty())	// nothing to do; quick exit
        return;

#if _USE_THREADS == _USE_NO_THREADS || defined(SYNCHRONOUS_CALLBACKS)
    // no threading model supported - we HAVE to do this right here
    // note that the user better not re-enter CSSM too much,
    // or we might deadlock...
    for (CallbackMap::const_iterator it = callbacks.begin();
         it != callbacks.end(); it++) {
        it->first(event, guid, subId, serviceType);
    }
#else // real threads available
    // lock down all callback elements - still protected by global lock (via caller)
    for (CallbackMap::iterator it = callbacks.begin();
         it != callbacks.end(); it++)
        it->second->enter();

    // get out of this thread - now!
    (new Runner(callbacks, event, guid, subId, serviceType))->run();
#endif
}

void ModuleCallbackSet::Runner::action()
{
    //
    // NOTE WELL: Our callbacks map shares (pointed-to) values with the ModuleCallbackSet
    // we were created from. Some of these values may be dangling pointers since they have
    // been destroyed by other threads, but only *after* we are done with them, since
    // we must call exit() on them before they become eligible for destruction.
    // In all cases, it is the responsibility of other threads to destroy those mutexi.
    //
    // @@@ Could also fan out to multiple callback threads in parallel.
    for (CallbackMap::iterator it = callbacks.begin();
         it != callbacks.end(); it++) {
        //@@@ safety vs. convenience - recheck
        it->first(event, guid, subserviceId, serviceType);
        it->second->exit();
    }
}
