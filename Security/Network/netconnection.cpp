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
// connection - a (potentially) persistent access path to a (possibly :-) remote entity
//
#include "netconnection.h"
#include "protocol.h"
#include "netmanager.h"
#include "transfer.h"


namespace Security {
namespace Network {


//
// Create a Connection object for a particular Protocol and HostTarget.
// Note that these two arguments are potentially unrelated; in general,
// you can't assume that &proto == &host.protocol().
//
Connection::Connection(Protocol &proto, const HostTarget &host)
    : protocol(proto), hostTarget(host), mTransfer(NULL), mRetainMe(false), mRestarting(false)
{
    secdebug("netconn", 
        "connection %p created for %s", this, hostTarget.urlForm().c_str());
}


//
// Destroy a Connection, assuming it's idle.
//
Connection::~Connection()
{
    assert(!isDocked());
    secdebug("netconn", "connection %p destroyed", this);
}


//
// Dock the Connection to a Transfer.
//
void Connection::dock(Transfer *xfer)
{
    assert(!isDocked());
    assert(!xfer->isDocked());
    mTransfer = xfer;
    xfer->mConnection = this;
    secdebug("netconn", "connection %p docked xfer %p", this, xfer);
}


//
// Undock the Connection from its currently docked Transfer.
// The mRetainMe flag determines what happens next: we either
// submit ourselves to our Manager for retention, or for cleanup.
//
void Connection::undock()
{
    // paranoia first
    assert(isDocked());
    assert(mTransfer->mConnection == this);
    
    // will we be kept?
    bool retain = mRetainMe && mTransfer->shareConnections();
    
    // physically sever our relationship with the Transfer
    secdebug("netconn", "connection %p undocking xfer %p", this, mTransfer);
    mTransfer->mConnection = NULL;
    mTransfer = NULL;

    // submit ourselves to the manager for retention
    if (retain)
        protocol.manager.retainConnection(this);
    else
        protocol.manager.closeConnection(this);
}


//
// Forwarders for finish/fail
//
void Connection::finish()
{
    assert(isDocked());
    mTransfer->finish();
}

void Connection::fail()
{
    if (isDocked()) {
        // fail the transfer we're docked to, which will undock us and dispose of us
        mTransfer->fail();
    } else {
        // we failed while in limbo. Self-dispose
        retain(false);
        protocol.manager.closeConnection(this);
    }
}


//
// Drop the current Connection and re-execute start()
//
void Connection::restart()
{
    if (mRestarting) {
        Transfer *transfer = mTransfer;
        secdebug("netconn", "%p restarting xfer %p", this, transfer);
        
        // throw outselves out
        retain(false);
        undock();
        
        // restart the transfer
        transfer->start();
    } else {
        // restart request on Connection that's not marked restarting.
        // Presumably a real error, and we assume error indications have already
        // been set (in the Transfer) by the caller as desired.
        fail();
    }
}


//
// The default implementation of validate() does nothing and succeeds.
//
bool Connection::validate()
{
    return true;
}


//
// The file descriptor of a TCPConnection is itself (as a TCPClientSocket)
//
int TCPConnection::fileDesc() const
{
    return *this;
}


//
// The TCPConnection destructor will remove any remaining I/O hook
//
TCPConnection::~TCPConnection()
{
    close();
}

void TCPConnection::close()
{
    if (isOpen()) {
        protocol.manager.removeIO(this);
        TCPClientSocket::close();
    }
}


//
// Asynchronous connect processing for TCPClient subclasses.
// The full call sets up data and initiates the first connect attempt; the second
// form needs to be called on failure notification to (re)try other addresses.
//
void TCPConnection::connect(const Host &host, IPPort port)
{
    mAddressCandidates = host.addresses();
    mPort = port;
    nextCandidate();
    protocol.manager.addIO(this);
    mode(connecting);
}

void TCPConnection::connect()
{
    if (mAddressCandidates.empty()) {
        // out of candidates. This connection attempt is failing
        UnixError::throwMe(EHOSTUNREACH);
    }

    close();
    nextCandidate();
    protocol.manager.addIO(this);
    mode(connecting);
}

void TCPConnection::nextCandidate()
{
    // pull the next address from the candidate set
    std::set<IPAddress>::iterator it = mAddressCandidates.begin();
    IPAddress addr = *it;
    mAddressCandidates.erase(it);
    
    open(addr, mPort, O_NONBLOCK);
}


}	// end namespace Network
}	// end namespace Security
