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
#include "netmanager.h"
#include "protocol.h"
#include "transfer.h"
#include "netconnection.h"
#include "neterror.h"


namespace Security {
namespace Network {


Manager::Manager() : mActiveTransfers(0), mRetainConnections(true), mObserver(NULL)
{
}

Manager::~Manager()
{
    //@@@ cleanup, s'il vous plait:
    //@@@ abort transfers and destroy them
    //@@@ notify any observers
    //@@@ destroy protocol objects
}


//
// Add a new Transfer to this Manager.
// This does not start it; it'll just sit around until started.
//
void Manager::add(Transfer *xfer)
{
    assert(xfer->state() == Transfer::cold);
    mTransfers.insert(xfer);
    xfer->mState = Transfer::warm;
}


//
// Remove a Transfer from this Manager.
// You can remove a pre-active Transfer, or one that has finished or failed.
// You can't remove an active Transfer - abort it first.
//
void Manager::remove(Transfer *xfer)
{
    assert(mTransfers.find(xfer) != mTransfers.end());	// is ours
    assert(xfer->state() != Transfer::active);
    mTransfers.erase(xfer);
}


//
// Start a Transfer. It must already have been added, and must be in a pre-active state.
//
void Manager::start(Transfer *xfer)
{
    assert(mTransfers.find(xfer) != mTransfers.end());	// is ours
    assert(xfer->state() == Transfer::warm);
    try {
        xfer->start();
        xfer->mState = Transfer::active;
        xfer->observe(Observer::transferStarting);
        mActiveTransfers++;
        secdebug("netmanager", "%ld active transfers", mActiveTransfers);
    } catch (...) {
        xfer->mState = Transfer::failed;
        secdebug("netmanager", "Transfer %p failed to start", xfer);
        throw;
    }
}


//
// Abort a Transfer.
// If it is active, try to make it stop as soon as it's safe. This may return while
// the Transfer's state is still active; it will eventually switch to failed unless it
// happened to succeed before we got to it (in which case it'll be finished).
// You can't abort a Transfer that isn't active.
//@@@ Phasing problem? Perhaps aborting non-active Transfers should be
//@@@ allowed (and ignored or flagged).
//
void Manager::abort(Transfer *xfer)
{
    assert(mTransfers.find(xfer) != mTransfers.end());	// is ours
    switch (xfer->state()) {
    case Transfer::active:
        try {
            secdebug("netmanager", "xfer %p request abort", xfer);
            xfer->abort();
        } catch (...) {
            secdebug("netmanager", "xfer %p failed to abort; forcing the issue", xfer);
            xfer->Transfer::abort();
        }
        break;
    case Transfer::finished:
    case Transfer::failed:
        // no longer running; ignore cancel request
        secdebug("netmanager", "xfer %p abort ignored (already done)", xfer);
        break;
    default:
        assert(false);		// mustn't call in this state
    }
}


//
// Do bookkeeping for a Transfer that wants to be done.
// This method can only be called from a Transfer that belongs
// to this Manager and was started.
//
void Manager::done(Transfer *xfer)
{
    assert(mTransfers.find(xfer) != mTransfers.end());	// is ours
    assert(xfer->state() == Transfer::finished || xfer->state() == Transfer::failed);
    assert(mActiveTransfers > 0);
    mActiveTransfers--;
    secdebug("netmanager", "%ld active transfers", mActiveTransfers);
}


//
// Manage engine clients on behalf of active Transfers.
//@@@ Currently the API doesn't specify which Transfer these belong to.
//@@@ Perhaps it should.
//
void Manager::addIO(TransferEngine::Client *client)
{
    mEngine.add(client);
}

void Manager::removeIO(TransferEngine::Client *client)
{
    mEngine.remove(client);
}


//
// Manage Connections on behalf of Transfers (and perhaps Protocols)
//
void Manager::retainConnection(Connection *connection)
{
    if (mRetainConnections)
        mConnections.retain(connection);
    else
        closeConnection(connection);
}

void Manager::closeConnection(Connection *connection)
{
    mConnections.remove(connection);
    mMorgue.insert(connection);
}


//
// Try to find a live retained Connection for a HostTarget and return it.
//
Connection *Manager::pickConnection(const HostTarget &host)
{
    while (Connection *connection = mConnections.get(host)) {
        if (connection->validate()) {
            connection->restarting(true);	// mark restarting
            return connection;				// good to go
        }
        // if validate returned false, the connection has self-destructed (so ignore it)
        secdebug("netmanager", "%p connection %p failed to validate",
            this, connection);
    }
    return NULL;	// no joy, caller must make a new one
}


//
// Handle the global Connection cache
//
void Manager::reuseConnections(bool retain)
{
    mRetainConnections = retain;
}


void Manager::flushConnections()
{
    mConnections.purge();
}


//
// Timer management
//
void Manager::setTimer(Timer *timer, Time::Absolute when)
{
	mTimers.schedule(timer, when); 
}
	
void Manager::clearTimer(Timer *timer)
{
	if (timer->scheduled())
		mTimers.unschedule(timer); 
}


void Manager::runTimers()
{
    while (Timer *top = static_cast<Timer *>(mTimers.pop(Time::now()))) {
        secdebug("netmanager", "%p timer %p executing at %.3f",
            this, top, Time::now().internalForm());
        try {
            top->action();
            secdebug("machsrvtime", "%p timer %p done", this, top);
        } catch (...) {
            secdebug("machsrvtime",
                "%p server timer %p failed with exception", this, top);
        }
    }
}


//
// Perform a (small) incremental operations step.
//
void Manager::step()
{
    prepare();
    if (!mEngine.isEmpty()) {
        secdebug("mgrstep", "operations step");
        mEngine();
    }
}


//
// Run in this thread until a particular time (or until no more Transfers are active).
//
void Manager::run(Time::Absolute stopTime)
{
    secdebug("netmanager",
		"starting run with %ld active transfers", mActiveTransfers);
    while (mActiveTransfers > 0) {
        prepare();
        Time::Absolute limit = mTimers.empty() ? stopTime : min(stopTime, mTimers.next());
        mEngine(limit - Time::now());
        if (Time::now() > stopTime)
            break;
    }
    secdebug("netmanager", "ending run");
}

void Manager::run()
{
    run(Time::heatDeath());
}


//
// Internal stepper
//
void Manager::prepare()
{
    // clear the morgue
    if (!mMorgue.empty()) {
        secdebug("netmanager",
            "clearing morgue of %ld connections", mMorgue.size());
        for (set<Connection *>::iterator it = mMorgue.begin(); it != mMorgue.end(); it++)
            delete *it;
        mMorgue.erase(mMorgue.begin(), mMorgue.end());
    }
    
    // run pending timers
    runTimers();
}


}	// end namespace Network
}	// end namespace Security
