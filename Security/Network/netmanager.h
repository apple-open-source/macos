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
// manager - network protocol core manager class
//
#ifndef _H_NETMANAGER
#define _H_NETMANAGER

#include <Security/ip++.h>
#include <Security/timeflow.h>
#include <Security/tqueue.h>
#include "xfercore.h"
#include "connectionpool.h"
#include "target.h"
#include "parameters.h"
#include "observer.h"
#include <set>
#include <map>


using namespace IPPlusPlus;


namespace Security {
namespace Network {


class Protocol;
class Transfer;
class Connection;


//
// A Manager object represents the top-level operations controller.
// You would usually only have one per process, though you *can*
// have more than one - they would not interact at all, and each
// Protocol, Transfer, etc. object could only belong to one of them.
//
class Manager : public ParameterPointer {
public:
    Manager();
    virtual ~Manager();
    
public:
    void add(Transfer *xfer);
    void remove(Transfer *xfer);
    void start(Transfer *xfer);
    void abort(Transfer *xfer);
    
    Observer *observer() const			{ return mObserver; }
    void observer(Observer *ob)			{ mObserver = ob; }
    
public:	// meant for just Transfer and Connection
    void done(Transfer *xfer);
    
    void addIO(TransferEngine::Client *client);
    void removeIO(TransferEngine::Client *client);
    
public:	// meant just for Connection
    template <class ProtoConnection>
    ProtoConnection *findConnection(const HostTarget &host)
    { return safe_cast<ProtoConnection *>(pickConnection(host)); }

    void retainConnection(Connection *connection);
    void closeConnection(Connection *connection);
    
public:
    void step();							// one small step for URLkind...
    void run();								// run until no more work
    void run(Time::Absolute upTo);			// run until some future time
    
public:
    bool reuseConnections() const	{ return mRetainConnections; }
    void reuseConnections(bool retain);		// global connection reuse override
    void flushConnections();				// clear connection cache (expensive)

public:
	class Timer : private ScheduleQueue<Time::Absolute>::Event {
        friend class Manager;
	protected:
		virtual ~Timer() { }
        
		Time::Absolute when() const	{ return Event::when(); }
		bool scheduled() const		{ return Event::scheduled(); }
	
	public:
		virtual void action() = 0;
	};
	
	virtual void setTimer(Timer *timer, Time::Absolute when);
	void setTimer(Timer *timer, Time::Interval offset)
	{ setTimer(timer, Time::now() + offset); }

	virtual void clearTimer(Timer *timer);
    
protected:
    virtual void runTimers();				// run ready timers
    virtual void prepare();					// setup for engine

private:
    void doStep();							// internal operative step
    Connection *pickConnection(const HostTarget &host);

private:
    typedef map<string, Protocol *> ProtoMap;
    ProtoMap mProtocols;					// map of registered protocols
    
private:
    typedef set<Transfer *> TransferSet;
    TransferSet mTransfers;					// set of active transfers (prelim)
    uint32 mActiveTransfers;				// number of active transfers
    
private:
    TransferEngine mEngine;					// transfer core engine
    ConnectionPool mConnections;			// pool of retained (live) Connections
    set<Connection *> mMorgue;				// Connections we should destroy
    bool mRetainConnections;				// global connection-reuse enable
    Observer *mObserver;					// default observer (NULL if none)
    
	ScheduleQueue<Time::Absolute> mTimers;	// timer queue
};


}	// end namespace Network
}	// end namespace Security


#endif /* _H_NETMANAGER */
