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
// machserver - C++ shell for writing Mach 3 servers
//
#include "machserver.h"
#include <servers/bootstrap.h>
#include <mach/kern_return.h>
#include <mach/message.h>
#include <mach/mig_errors.h>
#include "mach_notify.h"
#include <Security/debugging.h>

#if defined(USECFCURRENTTIME)
# include <CoreFoundation/CFDate.h>
#else
# include <sys/time.h>
#endif

namespace Security {
namespace MachPlusPlus {


//
// Global per-thread information
//
ModuleNexus< ThreadNexus<MachServer::PerThread> > MachServer::thread;


//
// Create a server object.
// The resulting object is not "active", and any number of server objects
// can be in this "prepared" state at the same time.
//
MachServer::MachServer(const char *name)
: mServerPort(name, bootstrap)
{ setup(name); }

MachServer::MachServer(const char *name, const Bootstrap &boot)
: bootstrap(boot), mServerPort(name, bootstrap)
{ setup(name); }

void MachServer::setup(const char *name)
{
	debug("machsrv", "%p preparing service for \"%s\"", this, name);
	workerTimeout = 60 * 2;	// 2 minutes default timeout
	maxWorkerCount = 100;	// sanity check limit
    
    mPortSet += mServerPort;
}

MachServer::~MachServer()
{
	// The ReceivePort members will clean themselves up.
	// The bootstrap server will clear us from its map when our receive port dies.
	debug("machsrv", "%p destroyed", this);
}


//
// Utility access
//
void MachServer::notifyIfDead(Port port) const
{
	port.requestNotify(mServerPort, MACH_NOTIFY_DEAD_NAME, true);
}


//
// Initiate service.
// This call will take control of the current thread and use it to service
// incoming requests. The thread will not be released until an error happens.
// We may also be creating additional threads to service concurrent requests
// as appropriate.
// @@@ Additional threads are not being reaped at this point.
// @@@ Msg-errors in additional threads are not acted upon.
//
void MachServer::run(size_t maxSize, mach_msg_options_t options)
{
	// establish server-global (thread-shared) parameters
	mMaxSize = maxSize;
	mMsgOptions = options;
	
	// establish the thread pool state
	// (don't need managerLock since we're the only thread as of yet)
	idleCount = workerCount = 1;
	nextCheckTime = Time::now() + workerTimeout;
	leastIdleWorkers = 1;
	highestWorkerCount = 1;
	
	// run server loop in initial (immortal) thread
	runServerThread(false);
	
	// primary server thread exited somehow (not currently possible)
	assert(false);
}


//
// This is the core of a server thread at work. It takes over the thread until
// something makes it exit normally. Then it returns. Errors cause exceptions.
// This code is loosely based on mach_msg_server.c, but is drifting away for
// various reasons of flexibility and resilience.
//
extern "C" boolean_t cdsa_notify_server(mach_msg_header_t *in, mach_msg_header_t *out);

void MachServer::runServerThread(bool doTimeout)
{
	// allocate request/reply buffers
    Message bufRequest(mMaxSize);
    Message bufReply(mMaxSize);
	
	// all exits from runServerThread are through exceptions or "goto exit"
	try {
		// register as a worker thread
		debug("machsrv", "%p starting service on port %d", this, int(mServerPort));
		perThread().server = this;

		for (;;) {
			// process all pending timers
			while (processTimer()) ;
		
			// check for worker idle timeout
			{	StLock<Mutex> _(managerLock);
				// record idle thread low-water mark in scan interval
				if (idleCount < leastIdleWorkers)
					leastIdleWorkers = idleCount;
				
				// perform self-timeout processing
				if (doTimeout) {
					if (workerCount > maxWorkerCount) {
						debug("machsrv", "%p too many threads; reaping immediately", this);
						break;
					}
					Time::Absolute rightNow = Time::now();
					if (rightNow >= nextCheckTime) {	// reaping period complete; process
						uint32 idlers = leastIdleWorkers;
						debug("machsrv", "%p end of reaping period: %ld (min) idle of %ld total",
							this, idlers, workerCount);
						nextCheckTime = rightNow + workerTimeout;
						leastIdleWorkers = INT_MAX;
						if (idlers > 1)
							break;
					}
				}
			}
            
            // release deferred-release memory
            releaseDeferredAllocations();
			
			// determine next timeout, or zero for infinity
            bool indefinite = false;
			Time::Interval timeout;
			{	StLock<Mutex> _(managerLock);
				if (timers.empty()) {
                    if (doTimeout)
                        timeout = workerTimeout;
                    else
                        indefinite = true;
				} else {
					timeout = doTimeout 
						? min(workerTimeout, timers.next() - Time::now()) 
						: timers.next() - Time::now();
                }
			}
			
			// receive next IPC request (or wait for timeout)
			switch (mach_msg_return_t mr = indefinite ?
				mach_msg_overwrite_trap(bufRequest,
					MACH_RCV_MSG | mMsgOptions,
					0, mMaxSize, mPortSet,
					MACH_MSG_TIMEOUT_NONE, MACH_PORT_NULL,
					(mach_msg_header_t *) 0, 0)
                    :
				mach_msg_overwrite_trap(bufRequest,
					MACH_RCV_MSG | MACH_RCV_TIMEOUT | mMsgOptions,
					0, mMaxSize, mPortSet,
					mach_msg_timeout_t(timeout.mSeconds()), MACH_PORT_NULL,
					(mach_msg_header_t *) 0, 0)) {
			case MACH_MSG_SUCCESS:
				// process received request message below
				break;
			case MACH_RCV_TIMED_OUT:
				// back to top for time-related processing
				continue;
			case MACH_RCV_TOO_LARGE:
				// the kernel destroyed the request
				continue;
            case MACH_RCV_INTERRUPTED:
                // receive interrupted, try again
                continue;
			default:
				Error::throwMe(mr);
			}
			
			// process received message
			if (bufRequest.msgId() >= MACH_NOTIFY_FIRST &&
				bufRequest.msgId() <= MACH_NOTIFY_LAST) {
				// mach kernel notification message
				// we assume this is quick, so no thread arbitration here
				cdsa_notify_server(bufRequest, bufReply);
			} else {
				// normal request message
				{ StLock<Mutex> _(managerLock); idleCount--; }
				debug("machsrvreq",
                    "servicing port %d request id=%d",
                    bufRequest.localPort().port(), bufRequest.msgId());
                if (bufRequest.localPort() == mServerPort) {	// primary
                    handle(bufRequest, bufReply);
                } else {
                    for (HandlerSet::const_iterator it = mHandlers.begin();
                            it != mHandlers.end(); it++)
                        if (bufRequest.localPort() == (*it)->port())
                            (*it)->handle(bufRequest, bufReply);
                }
				debug("machsrvreq", "request complete");
				{ StLock<Mutex> _(managerLock); idleCount++; }
			}

			// process reply generated by handler
            if (!(bufReply.bits() & MACH_MSGH_BITS_COMPLEX) &&
                bufReply.returnCode() != KERN_SUCCESS) {
                    if (bufReply.returnCode() == MIG_NO_REPLY)
						continue;
                    // don't destroy the reply port right, so we can send an error message
                    bufRequest.remotePort(MACH_PORT_NULL);
                    mach_msg_destroy(bufRequest);
            }

            if (bufReply.remotePort() == MACH_PORT_NULL) {
                // no reply port, so destroy the reply
                if (bufReply.bits() & MACH_MSGH_BITS_COMPLEX)
                    bufReply.destroy();
                continue;
            }

            /*
             *  We don't want to block indefinitely because the client
             *  isn't receiving messages from the reply port.
             *  If we have a send-once right for the reply port, then
             *  this isn't a concern because the send won't block.
             *  If we have a send right, we need to use MACH_SEND_TIMEOUT.
             *  To avoid falling off the kernel's fast RPC path unnecessarily,
             *  we only supply MACH_SEND_TIMEOUT when absolutely necessary.
             */
			switch (mach_msg_return_t mr = mach_msg_overwrite_trap(bufReply,
                          (MACH_MSGH_BITS_REMOTE(bufReply.bits()) ==
                                                MACH_MSG_TYPE_MOVE_SEND_ONCE) ?
                          MACH_SEND_MSG | mMsgOptions :
                          MACH_SEND_MSG | MACH_SEND_TIMEOUT | mMsgOptions,
                          bufReply.length(), 0, MACH_PORT_NULL,
                          0, MACH_PORT_NULL, NULL, 0)) {
			case MACH_MSG_SUCCESS:
				break;
			case MACH_SEND_INVALID_DEST:
			case MACH_SEND_TIMED_OUT:
				/* the reply can't be delivered, so destroy it */
				mach_msg_destroy(bufRequest);
				break;
			default:
				Error::throwMe(mr);
			}
        }
		perThread().server = NULL;
		debug("machsrv", "%p ending service on port %d", this, int(mServerPort));
		
	} catch (...) {
		perThread().server = NULL;
		debug("machsrv", "%p aborted by exception (port %d)", this, int(mServerPort));
		throw;
	}
}


//
// Manage subsidiary ports
//
void MachServer::add(Handler &handler)
{
    assert(mHandlers.find(&handler) == mHandlers.end());
    assert(handler.port() != MACH_PORT_NULL);
    mHandlers.insert(&handler);
    mPortSet += handler.port();
}

void MachServer::remove(Handler &handler)
{
    assert(mHandlers.find(&handler) != mHandlers.end());
    mHandlers.erase(&handler);
    mPortSet -= handler.port();
}


//
// Implement a Handler that sends no reply
//
boolean_t MachServer::NoReplyHandler::handle(mach_msg_header_t *in, mach_msg_header_t *out)
{
    // set up reply message to be valid (enough) and read "do not send reply"
    out->msgh_bits = 0;
    out->msgh_remote_port = MACH_PORT_NULL;
    out->msgh_size = sizeof(mig_reply_error_t);
    ((mig_reply_error_t *)out)->RetCode = MIG_NO_REPLY;
    
    // call input-only handler
    return handle(in);
}


//
// Register a memory block for deferred release.
//
void MachServer::releaseWhenDone(CssmAllocator &alloc, void *memory)
{
    if (memory) {
        set<Allocation> &releaseSet = perThread().deferredAllocations;
        assert(releaseSet.find(Allocation(memory, alloc)) == releaseSet.end());
        debug("machsrvmem", "%p register %p for release with %p",
            this, memory, &alloc);
        releaseSet.insert(Allocation(memory, alloc));
    }
}


//
// Run through the accumulated deferred allocations and release them.
// This is done automatically on every pass through the server loop;
// it must be called by subclasses that implement their loop in some
// other way.
// @@@X Needs to be thread local
//
void MachServer::releaseDeferredAllocations()
{
    set<Allocation> &releaseSet = perThread().deferredAllocations;
	for (set<Allocation>::iterator it = releaseSet.begin(); it != releaseSet.end(); it++) {
        debug("machsrvmem", "%p release %p with %p", this, it->addr, it->allocator);
		it->allocator->free(it->addr);
    }
	releaseSet.erase(releaseSet.begin(), releaseSet.end());
}


//
// The handler function calls this if it realizes that it might be blocked
// (or doing something that takes a long time). We respond by ensuring that
// at least one more thread is ready to serve requests.
//
void MachServer::longTermActivity()
{
	StLock<Mutex> _(managerLock);
	if (idleCount == 0 && workerCount < maxWorkerCount) {
		// spawn a new thread of activity that shares in the server main loop
		(new LoadThread(*this))->run();
	}
}

void MachServer::LoadThread::action()
{
	//@@@ race condition?! can server exit before helpers thread gets here?
	
	// register the worker thread and go
	server.addThread(this);
	try {
		server.runServerThread(true);
	} catch (...) {
		// fell out of server loop by error. Let the thread go quietly
	}
	server.removeThread(this);
}

void MachServer::addThread(Thread *thread)
{
	StLock<Mutex> _(managerLock);
	workerCount++;
	idleCount++;
	debug("machsrv", "%p adding worker thread (%ld workers, %ld idle)",
		this, workerCount, idleCount);
	workers.insert(thread);
}

void MachServer::removeThread(Thread *thread)
{
	StLock<Mutex> _(managerLock);
	workerCount--;
	idleCount--;
	debug("machsrv", "%p removing worker thread (%ld workers, %ld idle)",
		this, workerCount, idleCount);
	workers.erase(thread);
}


//
// Timer management
//
bool MachServer::processTimer()
{
	Timer *top;
	{	StLock<Mutex> _(managerLock);	// could have multiple threads trying this
		if (!(top = static_cast<Timer *>(timers.pop(Time::now()))))
			return false;				// nothing (more) to be done now
	}	// drop lock; work has been retrieved
	debug("machsrvtime", "%p timer %p executing at %.3f",
        this, top, Time::now().internalForm());
	try {
		top->action();
		debug("machsrvtime", "%p timer %p done", this, top);
	} catch (...) {
		debug("machsrvtime", "%p server timer %p failed with exception", this, top);
	}
	return true;
}

void MachServer::setTimer(Timer *timer, Time::Absolute when)
{
	StLock<Mutex> _(managerLock);
	timers.schedule(timer, when); 
}
	
void MachServer::clearTimer(Timer *timer)
{
	StLock<Mutex> _(managerLock); 
	if (timer->scheduled())
		timers.unschedule(timer); 
}


//
// Notification hooks and shims. Defaults do nothing.
//
void cdsa_mach_notify_dead_name(mach_port_t, mach_port_name_t port)
{ MachServer::active().notifyDeadName(port); }

void MachServer::notifyDeadName(Port) { }

void cdsa_mach_notify_port_deleted(mach_port_t, mach_port_name_t port)
{ MachServer::active().notifyPortDeleted(port); }

void MachServer::notifyPortDeleted(Port) { }

void cdsa_mach_notify_port_destroyed(mach_port_t, mach_port_name_t port)
{ MachServer::active().notifyPortDestroyed(port); }

void MachServer::notifyPortDestroyed(Port) { }

void cdsa_mach_notify_send_once(mach_port_t)
{ MachServer::active().notifySendOnce(); }

void MachServer::notifySendOnce() { }

void cdsa_mach_notify_no_senders(mach_port_t)
{ /* legacy handler - not used by system */ }


} // end namespace MachPlusPlus

} // end namespace Security
