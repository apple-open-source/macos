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
#ifndef _H_MACHSERVER
#define _H_MACHSERVER

#include <Security/mach++.h>
#include <Security/timeflow.h>
#include <Security/threading.h>
#include <Security/globalizer.h>
#include <Security/cssmalloc.h>
#include <Security/tqueue.h>


namespace Security {
namespace MachPlusPlus {


extern "C" {
	void cdsa_mach_notify_dead_name(mach_port_t, mach_port_name_t port);
	void cdsa_mach_notify_port_destroyed(mach_port_t, mach_port_name_t port);
	void cdsa_mach_notify_port_deleted(mach_port_t, mach_port_name_t port);
	void cdsa_mach_notify_send_once(mach_port_t);
	void cdsa_mach_notify_no_senders(mach_port_t);	// legacy
};


//
// Mach server object
//
class MachServer {
	class LoadThread; friend class LoadThread;
	
protected:
	struct Allocation {
		void *addr;
		CssmAllocator *allocator;
		Allocation(void *p, CssmAllocator &alloc) : addr(p), allocator(&alloc) { }
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
    MachServer(const char *name);
	MachServer(const char *name, const Bootstrap &bootstrap);
	virtual ~MachServer();
	
	void run(size_t maxSize = 4096, mach_msg_options_t options = 0);
	
	Time::Interval timeout() const { return workerTimeout; }
	void timeout(Time::Interval t) { workerTimeout = t; }
	uint32 maxThreads() const	{ return maxWorkerCount; }
	void maxThreads(uint32 n)	{ maxWorkerCount = n; }

	// the currently active server in this thread (there can only be one)
	static MachServer &active()
	{ assert(perThread().server); return *perThread().server; }
	
	// request dead-port notification if this port dies (override notifyDeadName)
	virtual void notifyIfDead(Port port) const;

	// register (CssmAllocator-derived) memory to be released after reply is sent
	void releaseWhenDone(CssmAllocator &alloc, void *memory);
	
	// call if you realize that your server method will take a long time
	void longTermActivity();

public:
	class Timer : private ScheduleQueue<Time::Absolute>::Event {
		friend class MachServer;
	protected:
		virtual ~Timer() { }
	
	public:
		virtual void action() = 0;
		
		Time::Absolute when() const	{ return Event::when(); }
		bool scheduled() const		{ return Event::scheduled(); }
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
	virtual void notifySendOnce();

	// don't mess with this unless you know what you're doing
    Bootstrap bootstrap;			// bootstrap port we registered with
	ReceivePort mServerPort;		// port to receive requests
    PortSet mPortSet;				// joint receiver port set
	
	size_t mMaxSize;				// maximum message size
	mach_msg_options_t mMsgOptions;	// kernel call options
    
    typedef set<Handler *> HandlerSet;
    HandlerSet mHandlers;			// subsidiary message port handlers

protected:	
	void releaseDeferredAllocations();
	
protected:
	class LoadThread : public Thread {
	public:
		LoadThread(MachServer &srv) : server(srv) { }
		
		MachServer &server;
		
		void action();		// code implementation
	};
	
	Mutex managerLock;		// lock for thread-global management info below
	set<Thread *> workers;	// threads running for this server
	uint32 workerCount;		// number of worker threads (including primary)
	uint32 maxWorkerCount;	// administrative limit to workerCount
	uint32 highestWorkerCount; // high water mark for workerCount
	uint32 idleCount;		// number of threads waiting for work
	Time::Interval workerTimeout; // seconds of idle time before a worker retires
	Time::Absolute nextCheckTime; // next time to check for excess threads
	uint32 leastIdleWorkers; // max(idleCount) since last checkpoint
	ScheduleQueue<Time::Absolute> timers;

	void addThread(Thread *thread); // add thread to worker pool
	void removeThread(Thread *thread); // remove thread from worker pool
	bool processTimer();	// handle one due timer object, if any

private:
	static boolean_t handler(mach_msg_header_t *in, mach_msg_header_t *out);
    void setup(const char *name);
	void runServerThread(bool doTimeout = false);
	
	friend void cdsa_mach_notify_dead_name(mach_port_t, mach_port_name_t port);
	friend void cdsa_mach_notify_port_destroyed(mach_port_t, mach_port_name_t port);
	friend void cdsa_mach_notify_port_deleted(mach_port_t, mach_port_name_t port);
	friend void cdsa_mach_notify_send_once(mach_port_t);
};


} // end namespace MachPlusPlus

} // end namespace Security

#endif //_H_MACHSERVER
