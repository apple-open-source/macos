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
// context - manage CSSM (cryptographic) contexts every which way
//
#ifndef _H_CSSMCONTEXT
#define _H_CSSMCONTEXT

#include "cssmint.h"
#include "cspattachment.h"
#include <Security/context.h>

#ifdef _CPP_CSSMCONTEXT
# pragma export on
#endif


//
// A HandleContext adds handle semantics to the Context object.
// Note that not every Context is a HandleContext - the Contexts we hand
// to our API customers for fondling are not. Also note that a HandleContext
// not a PODWrapper.
// HandleContext has an allocation method taking a CssmAllocator. To destroy
// a HandleObject, call HandleObject::destroy(the-context, the-allocator).
// You are responsible for picking the same allocator used on construction.
//
// THREADS: HandleContexts are assumed to have single-thread use. That means that
// operations on HandleContexts are NOT interlocked automatically; two users of
// the same context must do any arbitration themselves. A HandleContext is howerver
// safely interlocked against other objects, in particular its CSPAttachment.
// The upshot is that you're safe using a HandleContext unless someone else is trying
// to use the same context in parallel.
//
class HandleContext : public HandleObject, public Context {
public:
    HandleContext(CSPAttachment &attach,
                  CSSM_CONTEXT_TYPE type,
                  CSSM_ALGORITHMS algorithmId)
    : Context(type, algorithmId), attachment(attach), extent(0) { }
    virtual ~HandleContext();

    CSPAttachment &attachment;
	
	using Context::find;	// guard against HandleObjec::find

    void mergeAttributes(const CSSM_CONTEXT_ATTRIBUTE *attributes, uint32 count);
    CSSM_RETURN validateChange(CSSM_CONTEXT_EVENT event);

    void *operator new (size_t size, CssmAllocator &alloc) throw(std::bad_alloc)
    { return alloc.malloc(size); }
    void operator delete (void *addr, size_t, CssmAllocator &alloc) throw()
    { return alloc.free(addr); }
    static void destroy(HandleContext *context, CssmAllocator &alloc) throw()
    { context->~HandleContext(); alloc.free(context); }

    class Maker;	// deluxe builder

#if __GNUC__ > 2
private:
    void operator delete (void *addr) throw() { assert(0); }
#endif

protected:
    // Locking protocol, courtesy of HandleObject.
    // This locks the underlying attachment.
    void lock();
    bool tryLock();

private:
    void *extent;			// extra storage extent in use
};

inline HandleContext &enterContext(CSSM_CC_HANDLE h)
{
    return findHandleAndLock<HandleContext>(h, CSSM_ERRCODE_INVALID_CONTEXT_HANDLE);
}


//
// A Maker is a deluxe wrapper around Builder. It creates whole HandleContext
// objects in one swell foop, handling object locking, construction, error
// recovery, and all that jazz. A Maker cannot create plain Context objects.
//
class HandleContext::Maker : public Context::Builder {
public:
    Maker(CSSM_CSP_HANDLE handle) 
		: Context::Builder(findHandleAndLock<CSPAttachment>(handle, CSSM_ERRCODE_INVALID_CSP_HANDLE)),
		attachment(static_cast<CSPAttachment &>(allocator)), // order dependency(!)
		locker(attachment, true)
	{ attachment.finishEnter(); }

    CSPAttachment &attachment;

    CSSM_CC_HANDLE operator () (CSSM_CONTEXT_TYPE type,
                                CSSM_ALGORITHMS algorithm);
								
private:
	StLock<CountingMutex, &CountingMutex::enter, &CountingMutex::exit> locker;
};

#endif //_H_CSSMCONTEXT
