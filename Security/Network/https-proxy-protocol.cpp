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
// https-proxy - CONNECT style transparent proxy connection to SSL host.
//
// This is the CONNECT method of an ordinary (proxying) HTTP server.
// Once it switches a connection to transparent proxying, there's no way to get out
// again. Hence, our Connection objects belong to the remote host, not the proxy.
//
#include "https-proxy-protocol.h"
#include "netparameters.h"


namespace Security {
namespace Network {


//
// Construct the protocol object
//
ConnectHTTPProtocol::ConnectHTTPProtocol(Manager &mgr, const HostTarget &proxy)
    : SecureHTTPProtocol(mgr), host(proxy.defaultPort(defaultHttpPort))
{
}


//
// Create a Transfer object for our protocol
//
ConnectHTTPProtocol::ConnectHTTPTransfer *ConnectHTTPProtocol::makeTransfer(const Target &target,
    Operation operation)
{
    return new ConnectHTTPTransfer(*this, target, operation, defaultHttpsPort);
}


//
// Construct an HTTPConnection object
//
ConnectHTTPProtocol::ConnectHTTPConnection::ConnectHTTPConnection(Protocol &proto,
    const HostTarget &hostTarget)
    : SecureHTTPConnection(proto, hostTarget)

{
    // SecureHTTPConnection already set up everything for talking to the server
    connectState = connectConnecting;
    deferStartSSL = true;	// tell parent protocol to break on connect-complete
}

ConnectHTTPProtocol::ConnectHTTPConnection::~ConnectHTTPConnection()
{
}


//
// Start a connection request
//
void ConnectHTTPProtocol::ConnectHTTPConnection::connectRequest()
{
    switch (connectState) {
    case connectConnecting:
        return;		// still waiting for TCP
    case connectStartup:
        {
            const HostTarget &host = target().host;
            flushOutput(false); // hold output
            printfe("CONNECT %s:%d HTTP/1.1",
                host.host().name().c_str(), target().host.port());
            hostHeader();
            authorizationHeader("Proxy-Authorization", hostTarget,
                kNetworkGenericProxyUsername, kNetworkGenericProxyPassword);
            printfe("");		// end of headers
            flushOutput();		// flush accumulated output
            mode(lineInput);
            connectState = connectPrimaryResponse;
        }
        break;
    case connectReady:			// already set; go ahead next layer (https)
        sslRequest();
        break;
    default:
        assert(false);			// huh?
    }
}


//
// Our state transit method controls only the initial SSL handshake.
// Think of it as a "prefix" to the HTTP protocol state engine. Once the handshake
// is complete, we hand off further state management to the HTTP machine.
//
void ConnectHTTPProtocol::ConnectHTTPConnection::transit(Event event, 
    char *input, size_t inputLength)
{
    if (event == endOfInput && connectState != connectReady)
        UnixError::throwMe(ECONNRESET);	// @@@ diagnostic?
    
    switch (connectState) {
    case connectConnecting:
        SecureHTTPConnection::transit(event, input, inputLength);
        if (SecureHTTPConnection::sslState == sslStartup) { // transport level ready
            connectState = connectStartup;
            connectRequest();
        }
        return;
    case connectPrimaryResponse:
        {
            // sketchily read proxy's primary response
            int major, minor, code;
            if (sscanf(input, "HTTP/%d.%d %u", &major, &minor, &code) != 3) {
                fail(input);	// malformed response header
            }
            if (major != 1 || minor < 0 || minor > 1)
                fail(input);
            switch (code) {
            case 200:		// okay, proceed
                connectState = connectReadHeaders;
                break;
            default:		// this didn't work
                transfer().httpResponse() = input;	// won't have a better one
                fail(input);
            }
        }
        break;
    case connectReadHeaders:
        {
            if (inputLength) {
                headers().add(input);
            } else {
                // end of proxy headers: start SSL now
                connectState = connectReady;
                try {
                    startSSL();
                } catch (CssmCommonError &err) {
                    setError("SSL failed", err.osStatus());
                    throw;
                } catch (...) {
                    setError("SSL failed");
                    throw;
                }
            }
        }
        break;
    case connectReady:
        return SecureHTTPConnection::transit(event, input, inputLength);
    default:
        assert(false);						// huh?
    }
}


//
// HTTPS Transfer objects.
//
ConnectHTTPProtocol::ConnectHTTPTransfer::ConnectHTTPTransfer(Protocol &proto,
    const Target &tgt, Operation operation, IPPort defPort) 
    : SecureHTTPTransfer(proto, tgt, operation, defPort)
{
}

void ConnectHTTPProtocol::ConnectHTTPTransfer::start()
{
    ConnectHTTPConnection *connection =
        protocol.manager.findConnection<ConnectHTTPConnection>(target);
    if (connection == NULL)
        connection = new ConnectHTTPConnection(protocol, target);
    connection->dock(this);
    connection->connectRequest();
}


//
// Even though this is formally a proxy protocol, we should not use
// proxy headers, since the proxy is transparent and the remote system
// expects a direct request.
// 
bool ConnectHTTPProtocol::ConnectHTTPTransfer::useProxyHeaders() const
{
    return false;
}


//
// We are a proxy protocol
//
bool ConnectHTTPProtocol::isProxy() const
{ return true; }

const HostTarget &ConnectHTTPProtocol::proxyHost() const
{ return host; }


}	// end namespace Network
}	// end namespace Security
