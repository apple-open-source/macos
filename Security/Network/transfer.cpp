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
// transfer - the embodiment of a single transfer transaction
//
#include "transfer.h"
#include "netmanager.h"
#include "netconnection.h"
#include "protocol.h"
#include "neterror.h"


namespace Security {
namespace Network {


//
// Construct and destroy Transfer objects
//
Transfer::Transfer(Protocol &proto, const Target &tgt, Operation op, IPPort defPort)
    : protocol(proto),
      target(tgt.host.defaultPort(defPort), tgt.path),
      mState(cold), mOperation(op), mConnection(NULL),
      mSource(NULL), mSink(NULL),
      mShareConnections(proto.manager.reuseConnections()),
      mErrorStatus(defaultOSStatusError)
{
    secdebug("netxfer", "%p created for protocol %p(%s) target %s operation %d",
        this, &proto, proto.name(), target.urlForm().c_str(), mOperation);

    parameters(protocol.manager);	// inherit environment from manager object
    mObserver = protocol.manager.observer();
}

Transfer::~Transfer()
{
    secdebug("netxfer", "transfer %p destroyed", this);
}


//
// Generic error management.
// These defaults do (almost) nothing useful; they should be overridden by
// each Protocol's Transfer object.
//
Transfer::ResultClass Transfer::resultClass() const
{
    switch (state()) {
    case failed:
        return unclassifiedFailure;
    case finished:
        return success;
    default:
        Error::throwMe();
    }
}

OSStatus Transfer::errorStatus() const
{
    assert(state() == failed);
    return mErrorStatus;
}

string Transfer::errorDescription() const
{
    assert(state() == failed);
    return mErrorDescription;
}


//
// Restart trampoline
//
void Transfer::restart()
{
    assert(mConnection);
    return mConnection->restart();
}


//
// Notify any observer
//
void Transfer::observe(Observer::Events events, const void *info)
{
    if (mObserver && mObserver->wants(events))
        mObserver->observe(events, this, info);
}


//
// Set yourself to be successfully done
//
void Transfer::finish()
{
    secdebug("xferengine", "transfer %p is finishing up", this);
    mState = finished;
    if (isDocked())
        mConnection->undock();
    protocol.manager.done(this);
    observe(Observer::transferComplete);
}


//
// Set yourself to have failed
//
void Transfer::fail()
{
    secdebug("xferengine", "transfer %p is failing", this);
    mState = failed;
    if (isDocked())
        mConnection->undock();
    protocol.manager.done(this);
    observe(Observer::transferFailed);
}


//
// This default implementation of abort() simply fails.
// This is not likely to be enough for most protocols.
//
void Transfer::abort()
{
    observe(Observer::aborting);
    if (isDocked())
        mConnection->retain(false);	// indeterminate state; don't keep it
    fail();
}


}	// end namespace Network
}	// end namespace Security
