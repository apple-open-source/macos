/*
 * Copyright (c) 2000-2004,2007 Apple Inc. All Rights Reserved.
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
#ifndef _H_MACHSERVER
#define _H_MACHSERVER

#include <security_utilities/mach++.h>
#include <security_utilities/timeflow.h>
#include <security_utilities/threading.h>
#include <security_utilities/globalizer.h>
#include <security_utilities/alloc.h>
#include <security_utilities/tqueue.h>
#include <set>

namespace Security {
namespace MachPlusPlus {


extern "C" {
	void cdsa_mach_notify_dead_name(mach_port_t, mach_port_name_t port);
	void cdsa_mach_notify_port_destroyed(mach_port_t, mach_port_name_t port);
	void cdsa_mach_notify_port_deleted(mach_port_t, mach_port_name_t port);
	void cdsa_mach_notify_send_once(mach_port_t);
	void cdsa_mach_notify_no_senders(mach_port_t, mach_port_mscount_t);
};


//
// Mach server object
//
class MachServer {
protected:
	class LoadThread; friend class LoadThread;
	
	struct Allocation {
		void *addr;
		Allocator *allocator;
		Allocation(void *p, Allocator &alloc) : addr(p), allocator(&alloc) { }
		bool operator < (const Allocation &other) const
		{ return addr < other.addr || (addr == other.addr && allocator < other.allocator); }
	};
    
protected:
    struct PerThread {
        MachServer *server;
        set<Allocation> deferredAllocations;

        PerThread() : server(NULL) { }
    };
    static ModuleNexus< ThreadNexus<PerThread> > thread;
    static PerThread &perThread()	{ return thread()(); }
    
public:
	MachServer();
    MachServer(const char *name);
	MachServer(const char *name, const Bootstrap &bootstrap);
	virtual ~MachServer();
	
	void run(size_t maxSize = 4096, mach_msg_options_t options = 0);
	
	Time::Interval timeout() const { return workerTimeout; }
	void timeout(Time::Interval t)	{ workerTimeout = t; }
	UInt32 maxThreads() const		{ return maxWorkerCount; }
	void maxThreads(UInt32 n)		{ maxWorkerCount = n; }
	bool floatingThread() const		{ return useFloatingThread; }
	void floatingThread(bool t)		{ useFloatingThread = t; }
	
	Port primaryServicePort() const	{ return mServerPort; }
	
	// listen on additional ports (dispatching to the main handler)
	void add(Port receiver);
	void remove(Port receiver);

	// the currently active server in this thread (there can only be one)
	static MachServer &active()
	{ assert(perThread().server); return *perThread().server; }
	
	// request port status notifications (override virtual methods below to receive)
	virtual void notifyIfDead(Port port, bool doNotify = true) const;
	virtual void notifyIfUnused(Port port, bool doNotify = true) const;

	// register (Allocator-derived) memory to be released after reply is sent
	void releaseWhenDone(Allocator &alloc, void *memory);
	
	// call if you realize that your server method will take a long time
	void longTermActivity();

public:
	class Timer : private ScheduleQueue<Time::Absolute>::Event {
		friend class MachServer;
	protected:
		Timer(bool longTerm = false) { mLongTerm = longTerm; }
		virtual ~Timer();

		bool longTerm() const		{ return mLongTerm; }
		void longTerm(bool lt)		{ mLongTerm = lt; }
	
	public:
		virtual void action() = 0;
		
		Time::Absolute when() const	{ return Event::when(); }
		bool scheduled() const		{ return Event::scheduled(); }
		
		// lifetime management hooks (default does nothing)
		virtual void select();
		virtual void unselect();
	
	private:
		bool mLongTerm;				// long-term activity (count as worker thread)
	};
	
	virtual void setTimer(Timer *timer, Time::Absolute when);
	void setTimer(Timer *timer, Time::Interval offset)
	{ setTimer(timer, Time::now() + offset); }

	virtual void clearTimer(Timer *timer);

public:
    class Handler {
    public:
        Handler(mach_port_t p) : mPort(p) { }
        Handler() : mPort(MACH_PORT_NULL) { }
		virtual ~Handler();
        
        mach_port_t port() const	{ return mPort; }
        
        virtual boolean_t handle(mach_msg_header_t *in, mach_msg_header_t *out) = 0;
        
    protected:
        void port(mach_port_t p)	{ assert(mPort == MACH_PORT_NULL); mPort = p; }

    private:
        mach_port_t mPort;
    };
    
    class NoReplyHandler : public Handler {
    public:
        virtual boolean_t handle(mach_msg_header_t *in) = 0;

    private:
        boolean_t handle(mach_msg_header_t *in, mach_msg_header_t *out);
    };
    
    void add(Handler &handler);
    void remove(Handler &handler);
    
protected:
	// your server dispatch function
	virtual boolean_t handle(mach_msg_header_t *in, mach_msg_header_t *out) = 0;
	
	// override these to receive Mach-style port notifications about your clients
	virtual void notifyDeadName(Port port);
	virtual void notifyPortDeleted(Port port);
	virtual void notifyPortDestroyed(Port port);
	virtual void notifySendOnce(Port port);
	virtual void notifyNoSenders(Port port, mach_port_mscount_t);
	
	// this will be called if the server wants a new thread but has hit its limit
	virtual void threadLimitReached(UInt32 limit);
	
	// this gets called every time the server finishes an action (any action)
	virtual void eventDone();

	// don't mess with this unless you know what you're doing
    Bootstrap bootstrap;			// bootstrap port we registered with
	ReceivePort mServerPort;		// registered/primary server port
    PortSet mPortSet;				// joint receiver port set
	
	size_t mMaxSize;				// maximum message size
	mach_msg_options_t mMsgOptions;	// kernel call options
    
    typedef set<Handler *> HandlerSet;
    HandlerSet mHandlers;			// subsidiary message port handlers

protected:	
	void releaseDeferredAllocations();

protected:
	void busy();
	void idle();
	void ensureReadyThread();

protected:
	class LoadThread : public Thread {
	public:
		LoadThread(MachServer &srv) : server(srv) { }
		
		MachServer &server;
		
		void action();		// code implementation
	};
	
	Mutex managerLock;		// lock for thread-global management info below
	set<Thread *> workers;	// threads running for this server
	UInt32 workerCount;		// number of worker threads (including primary)
	UInt32 maxWorkerCount;	// administrative limit to workerCount
	bool useFloatingThread;	// keep a "floating" idle thread (instead of using longTermActivity)
	
	UInt32 highestWorkerCount; // high water mark for workerCount
	UInt32 idleCount;		// number of threads waiting for work
	Time::Interval workerTimeout; // seconds of idle time before a worker retires
	Time::Absolute nextCheckTime; // next time to check for excess threads
	UInt32 leastIdleWorkers; // max(idleCount) since last checkpoint
	ScheduleQueue<Time::Absolute> timers;

	void addThread(Thread *thread); // add thread to worker pool
	void removeThread(Thread *thread); // remove thread from worker pool
	bool processTimer();	// handle one due timer object, if any (return true if there was one)

private:
	static boolean_t handler(mach_msg_header_t *in, mach_msg_header_t *out);
    void setup(const char *name);
	void runServerThread(bool doTimeout = false);
	
	friend void cdsa_mach_notify_dead_name(mach_port_t, mach_port_name_t port);
	friend void cdsa_mach_notify_port_destroyed(mach_port_t, mach_port_name_t port);
	friend void cdsa_mach_notify_port_deleted(mach_port_t, mach_port_name_t port);
	friend void cdsa_mach_notify_send_once(mach_port_t);
	friend void cdsa_mach_notify_no_senders(mach_port_t, mach_port_mscount_t);
};


} // end namespace MachPlusPlus
} // end namespace Security

#endif //_H_MACHSERVER
