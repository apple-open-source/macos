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
// connection - a (potentially) persistent access path to a (possibly :-) remote entity.
//
// Connection objects are the bearers of actual network (or other) I/O. They are distinct
// from Transfers, which embody an individual activity for a particular client (and Target).
// To do its stuff, a Transfer "docks" with a Connection, uses its resources, then "undocks"
// and leaves the Connection free to dock again with another Transfer (or, eventually, expire).
//
// Access protocols that do not have persistent state connections (e.g. FILE) will not use
// Connections at all; there is no requirement for a Transfer to use a Connection for its work.
//
// Actual Connection objects are specialized per protocol; for example, you'd expect
// an HTTPTransfer to dock to an HTTPConnection. If you subclass an existing protocol,
// you *may* be able to get away with using its Connection objects - but more often you'd
// subclass them in turn.
//
#ifndef _H_NETCONNECTION
#define _H_NETCONNECTION

#include <Security/ip++.h>
#include <Security/hosts.h>
#include <Security/streams.h>
#include "protocol.h"
#include "target.h"
#include "parameters.h"
#include "transfer.h"
#include <set>


using namespace IPPlusPlus;


namespace Security {
namespace Network {


class Manager;
class Protocol;
class Target;


//
// A generic Connection represents a semi-persistent channel of access to something
// identified by a Target.
//
class Connection : public ParameterSource {
    friend class Transfer;
    friend class Manager;
    typedef Protocol::Operation Operation;
public:
    Connection(Protocol &proto, const HostTarget &spec);
    virtual ~Connection();
    
    Protocol &protocol;
    const HostTarget hostTarget;
    
    // dock status
    virtual void dock(Transfer *xfer);
    virtual void undock();
    bool isDocked() const		{ return mTransfer; }
    
    template <class XFer>
    XFer &transferAs() const	{ assert(mTransfer); return *safe_cast<XFer *>(mTransfer); }
    
    // manage persistence
    bool retain() const			{ return mRetainMe; }
    void retain(bool r)			{ mRetainMe = r; }
    
    // see if we're still alive (after perhaps a delay)
    virtual bool validate();
    
    // return our hostTarget or that of the proxy server, if any
    const HostTarget &proxyHostTarget() const
    { return protocol.isProxy() ? protocol.proxyHost() : hostTarget; }
    
protected:
    Sink &sink() const				{ assert(isDocked()); return mTransfer->sink(); }
    Source &source() const			{ assert(isDocked()); return mTransfer->source(); }
    const Target &target() const	{ assert(isDocked()); return mTransfer->target; }
    Operation operation() const		{ assert(isDocked()); return mTransfer->operation(); }
    
    ParameterSource *parameters()	{ assert(mTransfer); return mTransfer->parameters(); }
    bool getParams(Key key, Value &value) const
    { assert(mTransfer); return mTransfer->getParams(key, value); }
    void observe(Observer::Event event, const void *info = NULL) const
    { if (mTransfer) mTransfer->observe(event, info); }
    
    void setError(const char *s, OSStatus err = Transfer::defaultOSStatusError)
    { if (mTransfer) mTransfer->setError(s, err); }
    
    void finish();
    void fail();    
    
    virtual void restart();
    void restarting(bool rs)		{ mRestarting = rs; }
    bool restarting() const			{ return mRestarting; }
    
private:
    Transfer *mTransfer;				// currently docked transfer (NULL if idle)
    bool mRetainMe;						// want to be retained in connection pool
    bool mRestarting;					// restart allowed
};


//
// A Connection that is also a TransferAgent::Client.
// This is a common case (but it isn't always true).
//
class TCPConnection : public Connection,
        public TransferEngine::Client, public TCPClientSocket {
public:
    TCPConnection(Protocol &proto, const HostTarget &spec)
    : Connection(proto, spec) { }
    ~TCPConnection();
    
    // remove from I/O hooks and close
    void close();

    // manage asynchronous connection establishment
    void connect(const Host &host, IPPort port);
    void connect();

    int fileDesc() const;
    
private:
    std::set<IPAddress> mAddressCandidates;
    IPPort mPort;
    
    void nextCandidate();
};


}	// end namespace Network
}	// end namespace Security


#endif _H_NETCONNECTION
