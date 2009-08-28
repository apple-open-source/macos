/*
 * Copyright (c) 2000-2007 Apple Inc. All Rights Reserved.
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
// machserver - C++ shell for writing Mach 3 servers
//
#include "machserver.h"
#include <servers/bootstrap.h>
#include <mach/kern_return.h>
#include <mach/message.h>
#include <mach/mig_errors.h>
#include "mach_notify.h"
#include <security_utilities/debugging.h>

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
MachServer::MachServer()
{ setup("(anonymous)"); }

MachServer::MachServer(const char *name)
	: mServerPort(name, bootstrap)
{ setup(name); }

MachServer::MachServer(const char *name, const Bootstrap &boot)
	: bootstrap(boot), mServerPort(name, bootstrap)
{ setup(name); }

void MachServer::setup(const char *name)
{
	workerTimeout = 60 * 2;	// 2 minutes default timeout
	maxWorkerCount = 100;	// sanity check limit
	useFloatingThread = false; // tight thread management
    
    mPortSet += mServerPort;
}

MachServer::~MachServer()
{
	// The ReceivePort members will clean themselves up.
	// The bootstrap server will clear us from its map when our receive port dies.
}


//
// Add and remove extra listening ports.
// Messages directed to those ports are dispatched through the main handler.
// To get automatic call-out to another handler, use the Handler class.
//
void MachServer::add(Port receiver)
{
	SECURITY_MACHSERVER_PORT_ADD(receiver);
	mPortSet += receiver;
}

void MachServer::remove(Port receiver)
{
	SECURITY_MACHSERVER_PORT_REMOVE(receiver);
	mPortSet -= receiver;
}


//
// Register for mach port notifications
//
void MachServer::notifyIfDead(Port port, bool doNotify) const
{
	if (doNotify)
		port.requestNotify(mServerPort);
	else
		port.cancelNotify();
}

void MachServer::notifyIfUnused(Port port, bool doNotify) const
{
	if (doNotify)
		port.requestNotify(port, MACH_NOTIFY_NO_SENDERS, true);
	else
		port.cancelNotify(MACH_NOTIFY_NO_SENDERS);
}


//
// Initiate service.
// This call will take control of the current thread and use it to service
// incoming requests. The thread will not be released until an error happens, which
// will cause an exception to be thrown. In other words, this never returns normally.
// We may also be creating additional threads to service concurrent requests
// as appropriate.
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
	SECURITY_MACHSERVER_START_THREAD(false);
	runServerThread(false);
	SECURITY_MACHSERVER_END_THREAD(false);
	
	// primary server thread exited somehow (not currently possible)
	assert(false);
}


//
// This is the core of a server thread at work. It takes over the thread until
// (a) an error occurs, throwing an exception
// (b) low-load timeout happens, causing a normal return (doTimeout only)
// This code was once based on mach_msg_server.c, but it is getting harder to notice
// the lingering resemblance.
//
extern "C" boolean_t cdsa_notify_server(mach_msg_header_t *in, mach_msg_header_t *out);

void MachServer::runServerThread(bool doTimeout)
{
	// allocate request/reply buffers
    Message bufRequest(mMaxSize);
    Message bufReply(mMaxSize);
	
	// all exits from runServerThread are through exceptions
	try {
		// register as a worker thread
		perThread().server = this;

		for (;;) {
			// progress hook
			eventDone();
			
			// process all pending timers
			while (processTimer()) ;
		
			// check for worker idle timeout
			{	StLock<Mutex> _(managerLock);
				// record idle thread low-water mark in scan interval
				if (idleCount < leastIdleWorkers)
					leastIdleWorkers = idleCount;
				
				// perform self-timeout processing
				if (doTimeout) {
					if (workerCount > maxWorkerCount)	// someone reduced maxWorkerCount recently...
						break;							// ... so release this thread immediately
					Time::Absolute rightNow = Time::now();
					if (rightNow >= nextCheckTime) {	// reaping period complete; process
						UInt32 idlers = leastIdleWorkers;
						SECURITY_MACHSERVER_REAP(workerCount, idlers);
						nextCheckTime = rightNow + workerTimeout;
						leastIdleWorkers = INT_MAX;
						if (idlers > 1)					// multiple idle threads throughout measuring interval...
							break;						// ... so release this thread now
					}
				}
			}
            
            // release deferred-release memory
            releaseDeferredAllocations();
			
			// determine next timeout (if any)
            bool indefinite = false;
			Time::Interval timeout = workerTimeout;
			{	StLock<Mutex> _(managerLock);
				if (timers.empty()) {
					indefinite = !doTimeout;
				} else {
					timeout = max(Time::Interval(0), timers.next() - Time::now());
					if (doTimeout && workerTimeout < timeout)
						timeout = workerTimeout;
                }
			}
			if (SECURITY_MACHSERVER_RECEIVE_ENABLED())
				SECURITY_MACHSERVER_RECEIVE(indefinite ? 0 : timeout.seconds());
			
			// receive next IPC request (or wait for timeout)
			mach_msg_return_t mr = indefinite ?
				mach_msg_overwrite(bufRequest,
					MACH_RCV_MSG | mMsgOptions,
					0, mMaxSize, mPortSet,
					MACH_MSG_TIMEOUT_NONE, MACH_PORT_NULL,
					(mach_msg_header_t *) 0, 0)
                    :
				mach_msg_overwrite(bufRequest,
					MACH_RCV_MSG | MACH_RCV_TIMEOUT | MACH_RCV_INTERRUPT | mMsgOptions,
					0, mMaxSize, mPortSet,
					mach_msg_timeout_t(timeout.mSeconds()), MACH_PORT_NULL,
					(mach_msg_header_t *) 0, 0);
					
			switch (mr) {
			case MACH_MSG_SUCCESS:
				// process received request message below
				break;
			default:
				SECURITY_MACHSERVER_RECEIVE_ERROR(mr);
				continue;
			}
			
			// process received message
			if (bufRequest.msgId() >= MACH_NOTIFY_FIRST &&
				bufRequest.msgId() <= MACH_NOTIFY_LAST) {
				// mach kernel notification message
				// we assume this is quick, so no thread arbitration here
				cdsa_notify_server(bufRequest, bufReply);
			} else {
				// normal request message
				StLock<MachServer, &MachServer::busy, &MachServer::idle> _(*this);
				SECURITY_MACHSERVER_BEGIN(bufRequest.localPort(), bufRequest.msgId());
				
				// try subsidiary handlers first
				bool handled = false;
				for (HandlerSet::const_iterator it = mHandlers.begin();
						it != mHandlers.end(); it++)
					if (bufRequest.localPort() == (*it)->port()) {
						(*it)->handle(bufRequest, bufReply);
						handled = true;
					}
				if (!handled) {
					// unclaimed, send to main handler
                    handle(bufRequest, bufReply);
                }

				SECURITY_MACHSERVER_END();
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
			mr = mach_msg_overwrite(bufReply,
                          (MACH_MSGH_BITS_REMOTE(bufReply.bits()) ==
                                                MACH_MSG_TYPE_MOVE_SEND_ONCE) ?
                          MACH_SEND_MSG | mMsgOptions :
                          MACH_SEND_MSG | MACH_SEND_TIMEOUT | mMsgOptions,
                          bufReply.length(), 0, MACH_PORT_NULL,
                          0, MACH_PORT_NULL, NULL, 0);
			switch (mr) {
			case MACH_MSG_SUCCESS:
				break;
			default:
				SECURITY_MACHSERVER_SEND_ERROR(mr, bufReply.remotePort());
				bufReply.destroy();
				break;
			}
        }
		perThread().server = NULL;
		
	} catch (...) {
		perThread().server = NULL;
		throw;
	}
}


//
// Manage subsidiary port handlers
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
// Abstract auxiliary message handlers
//
MachServer::Handler::~Handler()
{ /* virtual */ }


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
void MachServer::releaseWhenDone(Allocator &alloc, void *memory)
{
    if (memory) {
        set<Allocation> &releaseSet = perThread().deferredAllocations;
        assert(releaseSet.find(Allocation(memory, alloc)) == releaseSet.end());
		SECURITY_MACHSERVER_ALLOC_REGISTER(memory, &alloc);
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
		SECURITY_MACHSERVER_ALLOC_RELEASE(it->addr, it->allocator);
		it->allocator->free(it->addr);
    }
	releaseSet.erase(releaseSet.begin(), releaseSet.end());
}


//
// The handler function calls this if it realizes that it might be blocked
// (or doing something that takes a long time). We respond by ensuring that
// at least one more thread is ready to serve requests.
// Calls the threadLimitReached callback in the server object if the thread
// limit has been exceeded and a needed new thread was not created.
//
void MachServer::longTermActivity()
{
	if (!useFloatingThread) {
		StLock<Mutex> _(managerLock);
		ensureReadyThread();
	}
}

void MachServer::busy()
{
	StLock<Mutex> _(managerLock);
	idleCount--;
	if (useFloatingThread)
		ensureReadyThread();
}

void MachServer::idle()
{
	StLock<Mutex> _(managerLock);
	idleCount++;
}


void MachServer::ensureReadyThread()
{
	if (idleCount == 0) {
		if (workerCount >= maxWorkerCount) {
			this->threadLimitReached(workerCount);	// call remedial handler
		}
		if (workerCount < maxWorkerCount) { // threadLimit() may have raised maxWorkerCount
			(new LoadThread(*this))->run();
		}
	}
}


//
// The callback hook for our subclasses.
// The default does nothing, thereby denying further thread creation.
// You could do something like maxThreads(limit+1) here to grant an variance;
// or throw an exception to avoid possible deadlocks (this would abort the current
// request but not otherwise impact the server's operation).
//
void MachServer::threadLimitReached(UInt32 limit)
{
}


//
// What our (non-primary) load threads do
//
void MachServer::LoadThread::action()
{
	//@@@ race condition?! can server exit before helpers thread gets here?
	
	// register the worker thread and go
	server.addThread(this);
	try {
		SECURITY_MACHSERVER_START_THREAD(true);
		server.runServerThread(true);
		SECURITY_MACHSERVER_END_THREAD(false);
	} catch (...) {
		// fell out of server loop by error. Let the thread go quietly
		SECURITY_MACHSERVER_END_THREAD(true);
	}
	server.removeThread(this);
}


//
// Thread accounting
//
void MachServer::addThread(Thread *thread)
{
	StLock<Mutex> _(managerLock);
	workerCount++;
	idleCount++;
	workers.insert(thread);
}

void MachServer::removeThread(Thread *thread)
{
	StLock<Mutex> _(managerLock);
	workerCount--;
	idleCount--;
	workers.erase(thread);
}


//
// Timer management
//
MachServer::Timer::~Timer()
{ }

void MachServer::Timer::select()
{ }

void MachServer::Timer::unselect()
{ }

bool MachServer::processTimer()
{
	Timer *top;
	{	StLock<Mutex> _(managerLock);	// could have multiple threads trying this
		if (!(top = static_cast<Timer *>(timers.pop(Time::now()))))
			return false;				// nothing (more) to be done now
	}	// drop lock; work has been retrieved
	try {
		SECURITY_MACHSERVER_TIMER_START(top, top->longTerm(), Time::now().internalForm());
		StLock<MachServer::Timer,
			&MachServer::Timer::select, &MachServer::Timer::unselect> _t(*top);
		if (top->longTerm()) {
			StLock<MachServer, &MachServer::busy, &MachServer::idle> _(*this);
			top->action();
		} else {
			top->action();
		}
		SECURITY_MACHSERVER_TIMER_END(false);
	} catch (...) {
		SECURITY_MACHSERVER_TIMER_END(true);
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
{
	try {
		MachServer::active().notifyDeadName(port);
	} catch (...) {
	}
}

void MachServer::notifyDeadName(Port) { }

void cdsa_mach_notify_port_deleted(mach_port_t, mach_port_name_t port)
{
	try {
		MachServer::active().notifyPortDeleted(port);
	} catch (...) {
	}
}

void MachServer::notifyPortDeleted(Port) { }

void cdsa_mach_notify_port_destroyed(mach_port_t, mach_port_name_t port)
{
	try {
		MachServer::active().notifyPortDestroyed(port);
	} catch (...) {
	}
}

void MachServer::notifyPortDestroyed(Port) { }

void cdsa_mach_notify_send_once(mach_port_t port)
{
	try {
		MachServer::active().notifySendOnce(port);
	} catch (...) {
	}
}

void MachServer::notifySendOnce(Port) { }

void cdsa_mach_notify_no_senders(mach_port_t port, mach_port_mscount_t count)
{
	try {
		MachServer::active().notifyNoSenders(port, count);
	} catch (...) {
	}
}

void MachServer::notifyNoSenders(Port, mach_port_mscount_t) { }

void MachServer::eventDone() { }


} // end namespace MachPlusPlus

} // end namespace Security
