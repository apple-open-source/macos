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
// ftp-protocol - FTP protocol objects
//
// Basic design notes:
// FTPConnection and FTPDataConnection are mildly incestuous. An FTPConnection
// *contains* an FTPDataConnection to manage its data channel during transfers.
// It could *be* an FTPDataConnection, but they are both TransferEngine::TCPClients,
// which would make coding awkward and mistake prone.
// During wrap-up of a transfer, the control and data channels must synchronize to
// make sure they're both done. (Note that 226/250 replies do NOT guarantee that all
// data has been received on the data path; network latency can hold back that data
// for an arbitrarily long time (modulo TCP timeouts). Synchronization is achieved in
// classic ping-pong fashion: FTPConnection calls FTPDataConnection::connectionDone()
// to signal that it's side is done. The data connection calls FTPConnection::finish once
// it knows they're both done (because FTPConnection told it about its side already).
//
// This version has support for simple FTP proxy operation, where the PASS argument
// is of the form user@remote-host. FTPProxyProtocol uses this support to implement
// FTP/FTP proxies.
//
// Limits on functionality:
// Only stream mode is supported.
// No EBCDIC support.
//
#include <CoreServices/../Frameworks/CarbonCore.framework/Headers/MacErrors.h>

#include "ftp-protocol.h"
#include "netparameters.h"

namespace Security {
namespace Network {


//
// Construct the protocol object
//
FTPProtocol::FTPProtocol(Manager &mgr) : Protocol(mgr, "ftp")
{
}


//
// Create a Transfer object for our protocol
//
FTPProtocol::FTPTransfer *FTPProtocol::makeTransfer(const Target &target, Operation operation)
{
    return new FTPTransfer(*this, target, operation);
}


//
// Construct an FTPConnection object
//
FTPProtocol::FTPConnection::FTPConnection(Protocol &proto, const HostTarget &hostTarget)
    : TCPConnection(proto, hostTarget), state(errorState), mImageMode(false),
      mDataPath(*this)
{
    const HostTarget &host = proxyHostTarget();
    connect(host.host(), host.port());
    state = loginInitial;
}


//
// Issue a request on the connection.
//
void FTPProtocol::FTPConnection::request(const char *path)
{
    assert(isDocked());
    mOperationPath = path;
    
    if (state == idle)		// already (idly) at command prompt, so...
        startCommand();					// ... start operation right now
}

void FTPProtocol::FTPConnection::startCommand()
{
    // notify any observer of the change in status
    observe(Observer::resourceFound);
    
    switch (operation()) {
    case makeDirectory:
        printfe("MKD %s", mOperationPath.c_str());
        state = directCommandSent;
        return;
    case removeDirectory:
        printfe("RMD %s", mOperationPath.c_str());
        state = directCommandSent;
        return;
    case removeFile:
        printfe("DELE %s", mOperationPath.c_str());
        state = directCommandSent;
        return;
    case genericCommand:
        printfe("%s", mOperationPath.c_str());
        state = directCommandSent;
        return;
    }
    
    // all other commands initiate data transfers. First, set appropriate mode
    bool wantImageMode;
    switch (operation()) {
    case downloadDirectory:
    case downloadListing:
        wantImageMode = false;
        break;
    case download:
    case upload:
        wantImageMode = getv<string>(kNetworkFtpTransferMode, "I") == "I";
        break;
    default:
        assert(false);
    }
    
    // adjust transfer mode if needed
    if (mImageMode != wantImageMode) {
        printfe("TYPE %s", wantImageMode ? "I" : "A");
        mImageMode = wantImageMode;		// a bit premature, but this shouldn't fail
        state = typeCommandSent;
        return;		// we'll be back here
    } 
    if (mPassive = getv<bool>(kNetworkFtpPassiveTransfers)) {
        // initiate passive mode download
        printfe("PASV");
        state = passiveSent;
    } else {
        // initiate "active mode" (default mode) download.
        // The cooking recipe for the host/port address is deliciously subtle. We obviously take
        // the receiver's bound port. But in most cases, its address at this stage (passive bound)
        // is ANY, and thus useless to the server. We pick the command connection's local
        // address for completion. However, in SOME cases mReceiver.localAddress() has
        // a meaningful value (SOCKS, for one), so we allow this to prevail if available.
        mReceiver.open();		// open receiver and bind
        FTPAddress addr(mReceiver.localAddress().defaults(localAddress()));
        printfe("PORT %u,%u,%u,%u,%u,%u",
            addr.h1, addr.h2, addr.h3, addr.h4, addr.p1, addr.p2);
        state = portSent;
    }
}


//
// Initiate a data transfer (any direction or form) as indicated by mOperation.
// mDataPath has already been set up.
//
void FTPProtocol::FTPConnection::startTransfer(bool restarted)
{
    if (!restarted)
        if (int restartOffset = getv<int>(kNetworkRestartPosition, 0)) {
            // restart requested - insert a REST command here
            printfe("REST %d", restartOffset);
            state = restartSent;
            return;
        }
    
    switch (operation()) {
    case download:
        printfe("RETR %s", mOperationPath.c_str());
        break;
    case downloadDirectory:
        printfe("NLST %s", mOperationPath.c_str());
        break;
    case downloadListing:
        printfe("LIST %s", mOperationPath.c_str());
        break;
    case upload:
        printfe("%s %s",
            getv<bool>(kNetworkFtpUniqueStores, false) ? "STOU" : "STOR",
            mOperationPath.c_str());
        break;
    default:
        assert(false);
    }
    state = transferSent;
}


//
// This is the master state transit machine for FTP.
//
void FTPProtocol::FTPConnection::transit(Event event, char *input, size_t length)
{
    if (!isDocked()) {	// not docked; event while in Connection cache
        abort();		// clean up
        return;
    }

    switch (event) {
    case connectionDone: // TCP connection complete or failed
        {
            int error = length;	// transmitted in the 'length' argument
            observe(Observer::connectEvent, &error);
            if (error)		// retry
                connect();
            else			// connection good
                mode(lineInput);
        }
        return;
    case inputAvailable:
        {
            restarting(false);	// valid input observed, commit to this Connection

            // interpret input as FTP protocol reply, handling continued responses
            observe(Observer::protocolReceive, input);
            if (replyContinuation(input))
                return;				// still continuing, keep reading
            InetReply reply(input); // parse this reply
            if (!reply.valid())		// don't know why, but we're dead
                fail(input);
            if (replyContinuation(reply))
                return;				// is continuation now
    
            // dispatch state machine
            switch (state) {
            case loginInitial:
                switch (reply) {
                case 220:
                    {
                        string username = getv<string>(kNetworkGenericUsername,
                            hostTarget.haveUserPass() ? hostTarget.username() : "anonymous");
                        if (transfer().protocol.isProxy()) {
                            char portPart[10];
                            sprintf(portPart, ":%d", transfer().target.host.port());
                            username += "@" + transfer().target.host.host().name() + portPart;
                        }
                        printfe("USER %s", username.c_str());
                        state = loginUserSent;
                        break;
                    }
                default:
                    fail(input);
                }
                break;
            case loginUserSent:
                switch (reply) {
                case 331:
                    {
                        string password = getv<string>(kNetworkGenericPassword,
                            hostTarget.haveUserPass() ? hostTarget.password() : "anonymous@nowhere.net");
                        printfe("PASS %s", password.c_str());
                        state = loginPassSent;
                        break;
                    }
                case 230:
                    startCommand();
                    break;
                default:
                    fail(input);
                }
                break;
            case loginPassSent:
                switch (reply) {
                case 230:
                    startCommand();
                    break;
                default:
                    fail(input);
                }
                break;
            case typeCommandSent:
                switch (reply) {
                case 200:
                    startCommand();
                    break;
                default:
                    fail(input);
                }
                break;
            case passiveSent:
                switch (reply) {
                case 227:
                    {
                        // reply text =~ Entering passive mode (h1,h2,h3,h4,p1,p2)
                        FTPAddress addr;
                        if (const char *p = strchr(reply.message(), '(')) {
                            if (sscanf(p, "(%u,%u,%u,%u,%u,%u)", 
                                &addr.h1, &addr.h2, &addr.h3, &addr.h4, &addr.p1, &addr.p2) != 6)
                                fail(input);
                        } else if (const char *p = strstr(reply.message(), "mode")) {
                            // RFC1123 says to be really nice to BROKEN FTP servers here
                            if (sscanf(p+4, "%u,%u,%u,%u,%u,%u",
                                &addr.h1, &addr.h2, &addr.h3, &addr.h4, &addr.p1, &addr.p2) != 6)
                                fail(input);
                        } else {
                            fail(input);
                            return;
                        }
                        mDataPath.open(addr);	//@@@ synchronous - move to state machine
                        startTransfer();
                    }
                    break;
                default:
                    fail(input);
                }
                break;
            case portSent:
                switch (reply) {
                case 200:	// PORT command successful
                    startTransfer();
                    break;
                default:
                    fail(input);
                }
                break;
            case restartSent:
                switch (reply) {
                case 350:	// Restarting at ...
                    startTransfer(true);	// now do the transfer command for real
                    break;
                default:
                    fail(input);
                }
                break;
            case transferSent:
                switch (reply) {
                case 150:
                case 125:
                    transfer().ftpResponse() = input;	// remember response for caller.
                    transfer().ftpResponseCode() = reply;
                    if (!mPassive)
                        mReceiver.receive(mDataPath);	// accept incoming connection and stop listening
                    observe(Observer::resultCodeReady, input);
                    
                    // engage the data path
                    switch (operation()) {
                    case download:
                    case downloadDirectory:
                    case downloadListing:
                        mDataPath.start(sink());
                        observe(Observer::downloading, input);
                        break;
                    case upload:
                        mDataPath.start(source());
                        observe(Observer::uploading, input);
                        break;
                    default:
                        assert(false);
                    }
                    state = transferInProgress;
                    break;
                default:	// download command failed
                    if (!mPassive)
                        mReceiver.close();
                    state = idle;
                    fail();
                    break;
                }
                break;
            case transferInProgress:
                switch (reply) {
                case 226:	// transfer complete
                    state = idle;	// idle command mode
                    retain(true);
                    mDataPath.connectionDone();
                    break;
                case 452:
                    mDataPath.close();
                    state = idle;
                    fail(input, dskFulErr);
                    break;
                default:	// transfer failed
                    // (ignore any error in mDataPath - prefer diagnostics from remote)
                    mDataPath.close();
                    state = idle;
                    fail(input);
                    break;
                }
                break;
                
            case directCommandSent:
                {
                    switch (reply.type()) {
                    case 2:
                        retain(true);
                        finish();
                        break;
                    default:
                        fail();
                        break;
                    }
                    state = idle;
                }
                break;
                
            default:
                assert(false);
            }
        }
        break;

    case endOfInput:
        return restart();			// try to restart, fail if we can't (or shouldn't)
    default:
        assert(false);
    }
}

void FTPProtocol::FTPConnection::transitError(const CssmCommonError &error)
{
    //@@@ need to do much better diagnostics here
    fail();		// fail transfer and discard connection
}


bool FTPProtocol::FTPConnection::validate()
{
    assert(state == idle);
    tickle();
    return state == idle;
}


//
// The data connection object
//
void FTPProtocol::FTPDataConnection::start(Sink &sink)
{
    debug("ftp", "data connection starts download");
    setup();
    mode(sink);
}

void FTPProtocol::FTPDataConnection::start(Source &source)
{
    debug("ftp", "data connection starts upload");
    setup();
    mode(source);
}

void FTPProtocol::FTPDataConnection::setup()
{
    connection.protocol.manager.addIO(this);
    mFailureStatus = noErr;			// okay so far
    mConnectionDone = false;		// connection side not ready yet
    mTransferDone = false;			// our side not ready net
}

int FTPProtocol::FTPDataConnection::fileDesc() const
{
    return *this;
}

void FTPProtocol::FTPDataConnection::transit(Event event, char *input, size_t length)
{
    assert(event == autoReadDone || event == autoWriteDone || event == endOfInput);
    debug("ftp", "data transfer complete");
    close();		// close data path
    finish();		// proceed with state protocol
}

void FTPProtocol::FTPDataConnection::transitError(const CssmCommonError &error)
{
    mFailureStatus = error.osStatus();
    close();		// close data path
    finish();		// proceed with state protocol
}

void FTPProtocol::FTPDataConnection::close()
{
    if (isOpen()) {
        connection.protocol.manager.removeIO(this);
        TCPClientSocket::close();
        mTransferDone = true;
    }
}

void FTPProtocol::FTPDataConnection::connectionDone()
{
    mConnectionDone = true;
    finish();
}

void FTPProtocol::FTPDataConnection::finish()
{
    if (mFailureStatus) {
        connection.fail("data transfer failed", mFailureStatus);
        connection.finish();
    } else if (mTransferDone && mConnectionDone) {
        connection.finish();
    } else if (mConnectionDone) {
        debug("ftp", "holding for data transfer completion");
    } else {
        debug("ftp", "holding for control message");
    }
}


//
// Transfer objects
//
FTPProtocol::FTPTransfer::FTPTransfer(Protocol &proto, const Target &tgt, Operation operation)
    : Transfer(proto, tgt, operation, defaultFtpPort)
{ }

void FTPProtocol::FTPTransfer::start()
{
    FTPConnection *connection = protocol.manager.findConnection<FTPConnection>(target);
    if (connection == NULL)
        connection = new FTPConnection(protocol, target);
                
    connection->dock(this);
    connection->request(target.path.c_str());
}

void FTPProtocol::FTPTransfer::abort()
{
    observe(Observer::aborting);
    setError("aborted");
    connectionAs<FTPConnection>().abort();
}

void FTPProtocol::FTPConnection::abort()
{
    close();
    mDataPath.close();
    fail();
}


Transfer::ResultClass FTPProtocol::FTPTransfer::resultClass() const
{
    switch (state()) {
    case failed:
        {
            InetReply reply(errorDescription().c_str());
            if (reply / 10 == 53)	// 53x - authentication failure
                return authorizationFailure;
            if (errorDescription() == "aborted")
                return abortedFailure;
            // when in doubt, blame the remote
            return remoteFailure;
        }
    case finished:
        return success;
    default:
        assert(false);
    }
}


//
// Translate the ideosyncratic text form of FTP's socket addresses to and from the real thing
//
FTPProtocol::FTPAddress::FTPAddress(const IPSockAddress &sockaddr)
{
    uint32 addr = sockaddr.address();
    h1 = addr >> 24;
    h2 = (addr >> 16) & 0xFF;
    h3 = (addr >> 8) & 0xFF;
    h4 = addr & 0xFF;
    p1 = sockaddr.port() >> 8;
    p2 = sockaddr.port() & 0xFF;
}

FTPProtocol::FTPAddress::operator IPSockAddress() const
{
    assert(!(h1 & ~0xff) & !(h2 & ~0xff) & !(h3 & ~0xff) & !(h4 & ~0xff)
        & !(p1 & ~0xff) & !(p2 & ~0xff));
    return IPSockAddress(IPAddress(h1 << 24 | h2 << 16 | h3 << 8 | h4), p1 << 8 | p2);
}


}	// end namespace Network
}	// end namespace Security
