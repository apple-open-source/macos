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
// https-protocol - SSL based HTTP.
//
#include "https-protocol.h"
#include "netparameters.h"



namespace Security {
namespace Network {

//
// Construct the protocol object
//
SecureHTTPProtocol::SecureHTTPProtocol(Manager &mgr) : HTTPProtocol(mgr, "https")
{
}


//
// Names and identifiers
//
const char *SecureHTTPProtocol::name() const
{
    return "http/ssl";
}


//
// Create a Transfer object for our protocol
//
SecureHTTPProtocol::SecureHTTPTransfer *SecureHTTPProtocol::makeTransfer(const Target &target,
    Operation operation)
{
    return new SecureHTTPTransfer(*this, target, operation, defaultHttpsPort);
}


//
// Construct an HTTPConnection object
//
SecureHTTPProtocol::SecureHTTPConnection::SecureHTTPConnection(Protocol &proto,
    const HostTarget &hostTarget)
    : HTTPConnection(proto, hostTarget),
    SecureTransport<Socket>(static_cast<TCPClientSocket &>(*this)),	// (CC pitfall)
    deferStartSSL(false), sslActive(false)

{
    // HTTPConnection already set up everything for talking to the server
    deferSendRequest = true;	// interrupt HTTP state machine after connecting state
    sslState = sslConnecting;
}

SecureHTTPProtocol::SecureHTTPConnection::~SecureHTTPConnection()
{
}

void SecureHTTPProtocol::SecureHTTPConnection::sslRequest()
{
    switch (sslState) {
    case sslConnecting:		// new connection - wait for TL ready
        break;
    case sslConnected:		// already set; go ahead HTTP
        transfer().startRequest();
        break;
    default:
        assert(false);		// huh?
    }
}

void SecureHTTPProtocol::SecureHTTPConnection::startSSL()
{
    assert(sslState == sslStartup);

    // from now on, perform I/O through the SSL layer
    sslActive = true;

    // switch initially to raw input mode. Note that no input bytes will actually
    // be delivered by our modified read() until SSL handshake is complete.
    mode(rawInput);
    
    // configure the SSL session
    allowsExpiredCerts(getv<bool>(kNetworkHttpAcceptExpiredCerts, false));
    allowsUnknownRoots(getv<bool>(kNetworkHttpAcceptUnknownRoots, false));
	peerId(peerAddress());
	
    // start SSL handshake
    SSL::open();
    assert(SSL::state() == kSSLHandshake);	// there is no chance that we could already be done
    sslState = sslHandshaking;
}


//
// Validate a connection retrieved from the cache
//
bool SecureHTTPProtocol::SecureHTTPConnection::validate()
{
    return HTTPConnection::validate() && SSL::state() == kSSLConnected;
}


//
// Our state transit method controls only the initial SSL handshake.
// Think of it as a "prefix" to the HTTP protocol state engine. Once the handshake
// is complete, we hand off further state management to the HTTP machine.
//
void SecureHTTPProtocol::SecureHTTPConnection::transit(Event event, 
    char *input, size_t inputLength)
{
    try {
        switch (sslState) {
        case sslConnecting:
            HTTPConnection::transit(event, input, inputLength);
            if (HTTPConnection::state == idle) { // transport level ready
                sslState = sslStartup;
                if (!deferStartSSL)
                    startSSL();
            }
            return;
        case sslHandshaking:
            assert(event == inputAvailable);
            SecureTransport<Socket>::open();	// advance handshake protocol
            switch (SSL::state()) {
            case kSSLHandshake:					// not yet done
                return;
            case kSSLConnected:					// ready for HTTP
                sslState = sslConnected;
                transfer().startRequest();
                return;
            default:
                assert(false);	// can't happen - would have thrown
            }
        case sslConnected:
            return HTTPConnection::transit(event, input, inputLength);
        default:
            assert(false);						// huh?
        }

        // if SSL fails, we have to abandon the Connection
    } catch (const CssmCommonError &err) {
        setError("SSL failed", err.osStatus());
        throw;
    } catch (...) {
        setError("SSL failed");
        throw;
    }
}


//
// The I/O layer for SecureHTTPConnection objects.
//
size_t SecureHTTPProtocol::SecureHTTPConnection::read(void *data, size_t length)
{
    return sslActive ? SSL::read(data, length) : Socket::read(data, length);
}

size_t SecureHTTPProtocol::SecureHTTPConnection::write(const void *data, size_t length)
{
    return sslActive ? SSL::write(data, length) : Socket::write(data, length);
}

bool SecureHTTPProtocol::SecureHTTPConnection::atEnd() const
{
    return sslActive ? SSL::atEnd() : Socket::atEnd();
}


//
// HTTPS Transfer objects.
//
SecureHTTPProtocol::SecureHTTPTransfer::SecureHTTPTransfer(Protocol &proto,
    const Target &tgt, Operation operation, IPPort defPort) 
    : HTTPTransfer(proto, tgt, operation, defPort)
{
}

void SecureHTTPProtocol::SecureHTTPTransfer::start()
{
    SecureHTTPConnection *connection =
        protocol.manager.findConnection<SecureHTTPConnection>(target);
    if (connection == NULL)
        connection = new SecureHTTPConnection(protocol, target);
    connection->dock(this);
    connection->sslRequest();
}


}	// end namespace Network
}	// end namespace Security
