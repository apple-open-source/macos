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
#ifndef _H_TRANSFER
#define _H_TRANSFER

#include <Security/streams.h>
#include <Security/ip++.h>
#include "protocol.h"
#include "target.h"
#include "parameters.h"
#include "observer.h"


using namespace IPPlusPlus;


namespace Security {
namespace Network {


class Protocol;


//
// A Transfer is a single transaction with a target. It usually performs
// a data transfer (upload or download), though it could also be some
// administrative action such as creating or deleting (remote) directories.
//
class Transfer : public ParameterPointer {
    friend class Manager;
    friend class Connection;
public:
    typedef Protocol::Operation Operation;
    
    Transfer(Protocol &proto, const Target &tgt, Operation op, IPPort defaultPort = 0);
    virtual ~Transfer();
    
    Protocol &protocol;
    const Target target;
    
    enum State {
        cold,							// queued
        warm,							// (not yet used)
        active,							// in progress
        frozen,							// (not yet used)
        finished,						// successfully finished
        failed							// failed
    };
    
    enum ResultClass {
        success,						// seems to have worked
        localFailure,					// local error
        networkFailure,					// failure talking to remote partner
        remoteFailure,					// failure reported by remote partner
        authorizationFailure,			// remote reject our authorization
        abortedFailure,					// transfer was aborted intentionally
        unclassifiedFailure				// something else went wrong
    };
    
    State state() const				{ return mState; }
    Operation operation() const		{ return mOperation; }
    
    // valid only if state() is finished or failed
    virtual ResultClass resultClass() const;	// classify outcome
    
    // call these ONLY if state() == failed
    virtual OSStatus errorStatus() const;		// OSStatus short form of error condition
    virtual string errorDescription() const;	// string form of error condition
    
    template <class Conn>
    Conn &connectionAs() const
    { assert(mConnection); return *safe_cast<Conn *>(mConnection); }
    
    bool isDocked() const				{ return mConnection; }

    Sink &sink() const					{ assert(mSink); return *mSink; }
    Source &source() const				{ assert(mSource); return *mSource; }
    void sink(Sink &snk)				{ assert(!mSink); mSink = &snk; }
    void source(Source &src)			{ assert(!mSource); mSource = &src; }
    bool hasSink() const				{ return mSink; }
    bool hasSource() const				{ return mSource; }
    
    // get/set the Observer. Observer is initially inherited from Manager
    Observer *observer() const			{ return mObserver; }
    void observer(Observer *ob)			{ mObserver = ob; }
    
    // get/set connection reuse feature
    bool shareConnections() const		{ return mShareConnections; }
    void shareConnections(bool share)	{ mShareConnections = share; }
    
    // return our hostTarget or that of the proxy server, if any
    const HostTarget &proxyHostTarget() const
    { return protocol.isProxy() ? protocol.proxyHost() : target; }

    // last resort OSStatus to return for failure, if nothing better is known
    static const OSStatus defaultOSStatusError = -30785;	//@@@ not a good choice, but what?
        
protected:
    virtual void start() = 0;			// engage!
    virtual void abort();				// abort while running

    void restart();    
    void observe(Observer::Events events, const void *info = NULL);
    
    void setError(const char *s, OSStatus err = defaultOSStatusError)
    { if (s) mErrorStatus = err; mErrorDescription = s; }
    
    void finish();
    void fail();

private:
    State mState;						// current state
    Operation mOperation;				// operation type
    Connection *mConnection;			// docked connection (NULL if none)
    Observer *mObserver;				// observer (NULL if none)
    Source *mSource;					// origin data source (NULL if N/A)
    Sink *mSink;						// destination data sink (NULL if N/A)
    bool mShareConnections;				// participate in Connection pool (reuse)
    
    OSStatus mErrorStatus;				// OSStatus to return by default
    string mErrorDescription;			// error string to return by default
};


}	// end namespace Network
}	// end namespace Security


#endif /* _H_TRANSFER */
