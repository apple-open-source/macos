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
// http-protocol - HTTP protocol objects
//
// HTTP Transfers succeed (state() == finished) if the HTTP protocol was successfully
// observed. This means that even 300/400/500 type results are "successful" as far
// as state() is concerned. ResultClass() will attempt to classify both successful and
// unsuccessful outcomes, and errorDescription() is the primary HTTP response line
// (HTTP/1.n ccc some-string). HTTP Transfers fail (state() == failed) only if they can't
// talk to the server, or a protocol violation (or unimplemented feature) is detected.
// Deal with it.
//
// Note that the protected flag deferSendRequest allows the state sequencer to be
// interrupted at the idle stage (before an HTTP request is sent over the virtual wire).
// This is used by the https protocol driver to "wedge in" the SSL negotiation. Not very
// elegant, but it works.
//
// This implementation of the http protocol includes http proxy operation. As a result,
// it is very important to distinguish the various HostTargets and Targets involved:
//	Connection::hostTarget is the host we're talking to - it could be a proxy.
//	Transfer::target.host is the host we're trying to reach.
// From the HTTPConnection's point of view:
//	hostTarget may be a proxy or the destination
//	target().host is always the host we're trying to reach
// If we're not in proxy mode, these two are usually the same (caveat tester).
//
#include "http-protocol.h"
#include "netparameters.h"


namespace Security {
namespace Network {


//
// Construct the protocol object
//
HTTPProtocol::HTTPProtocol(Manager &mgr, const char *scheme) : Protocol(mgr, scheme)
{
}


//
// Create a Transfer object for our protocol
//
HTTPProtocol::HTTPTransfer *HTTPProtocol::makeTransfer(const Target &target, Operation operation)
{
    return new HTTPTransfer(*this, target, operation, defaultHttpPort);
}


//
// Construct an HTTPConnection object
//
HTTPProtocol::HTTPConnection::HTTPConnection(Protocol &proto,
    const HostTarget &hostTarget)
    : TCPConnection(proto, hostTarget),
    state(errorState), deferSendRequest(false)
{
    const HostTarget &host = proxyHostTarget();
    connect(host.host(), host.port());
    state = connecting;
}


//
// Start a request/response transaction on this Connection. This puts out all the
// HTTP request headers in one fell swoop (but not any request body).
// The Connection must be in idle state.
//
void HTTPProtocol::HTTPConnection::request(const char *operation)
{
    mOperation = operation;
    if (state == idle)	// already waiting for request
        sendRequest();
}

void HTTPProtocol::HTTPConnection::sendRequest()
{
    assert(state == idle);

    flushOutput(false);	// hold output until we're done
    const Target &target = this->target();
    if (transfer().useProxyHeaders()) {
        printfe("%s %s HTTP/1.1",
            mOperation.c_str(), target.urlForm().c_str());
        authorizationHeader("Proxy-Authorization", hostTarget,
            kNetworkGenericProxyUsername, kNetworkGenericProxyPassword);
    } else {
        printfe("%s %s HTTP/1.1", mOperation.c_str(), target.path.c_str());
    }
    hostHeader();
    authorizationHeader("Authorization", target,
        kNetworkGenericUsername, kNetworkGenericPassword);
    printfe("User-Agent: %s",
        getv<string>(kNetworkHttpUserAgent, "MacNetwork/1.0 (Macintosh)").c_str());
        
    // if restarting, add a Range header
    if (int restartOffset = getv<int>(kNetworkRestartPosition, 0)) {
        printfe("Range: bytes=%d-", restartOffset);
    }
    
    // add other headers set by caller, if any
    {
        string otherHeaders;
        if (get(kNetworkHttpMoreHeaders, otherHeaders)) {
            // launder and rinse - don't let the caller screw up the HTTP header structure
            static const char lineEndings[] = "\r\n";
            const char *p = otherHeaders.c_str();
            while (const char *q = strpbrk(p, lineEndings)) {
                if (q > p)
                    printfe("%.*s", q - p, p);
                p = q + strspn(q, lineEndings);
            }
            // now send any last (unterminated) line
            if (*p)
                printfe("%s", p);
        }
    }

    // add fields used for upstream transfer, if any, and initiate
    if (transfer().hasSource()) {
        Source &source = transfer().source();
        size_t size = source.getSize();
        if (size == Source::unknownSize) {
            //@@@ try to use Transfer-encoding: chunked -- for now, just use EOF delimiting
        } else {
            printfe("Content-length: %ld", size);
        }
        printfe("");					// end of headers
        mode(source);					// initiate autoWrite mode
    } else {
        printfe("");					// end of headers, no data
    }

    flushOutput();						// release pent-up output
    mode(lineInput);					// line input mode
    state = primaryResponse;			// prime the state machine
}

void HTTPProtocol::HTTPConnection::hostHeader()
{
    const HostTarget &host = target().host;
    if (host.port())
        printfe("Host: %s:%d", host.host().name().c_str(), host.port());
    else
        printfe("Host: %s", host.host().name().c_str());
}

void HTTPProtocol::HTTPConnection::authorizationHeader(const char *headerName,
    const HostTarget &host,
    ParameterSource::Key userKey, ParameterSource::Key passKey)
{
    string username = host.haveUserPass() ? host.username() : getv<string>(userKey);
    string password = host.haveUserPass() ? host.password() : getv<string>(passKey);
    //@@@ only "Basic" authentication supported for now
    if (!username.empty()) {
        //@@@ ad-hoc Base64 encoding. Replace with suitable stream encoder when available
        static const char alphabet[] = 
            "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
        string token = username + ":" + password;
        char *buffer = new char[4 * token.length() / 3 + 2];	// just enough
        const char *src = token.c_str(), *end = src + token.length();
        char *outp = buffer;
        while (src < end) {
            uint32 binary = src[0] << 16;
            if (src+1 < end)
                binary |= src[1] << 8 | src[2];
            *outp++ = alphabet[(binary >> 18) & 0x3F];
            *outp++ = alphabet[(binary >> 12) & 0x3F];
            *outp++ = (src+1 < end) ? alphabet[(binary >> 6) & 0x3F] : '=';
            *outp++ = (src+2 < end) ? alphabet[binary & 0x3F] : '=';
            src += 3;
        }
        *outp = '\0';
        printfe("%s: Basic %s", headerName, buffer);
        delete[] buffer;
    }
}


//
// This is the master state transit machine for HTTP.
//
void HTTPProtocol::HTTPConnection::transit(Event event, char *input, size_t length)
{
    switch (event) {
    case autoWriteDone:	// ingore: it's asynchronous to our state machine
        return;
    case endOfInput:	// most of the time, this is a protocol error, so filter it out now
        switch (state) {
        case idle:
        case readWholeBody:	// expected
            break;
        case primaryResponse: // Connection failed; restart it
            return restart();
        default:		// unexpected; fail
            UnixError::throwMe(ECONNRESET);	// @@@ diagnostic?
        }
        break;
    case connectionDone: // TCP connection complete or failed
        {
            assert(state == connecting);
            int error = length;
            observe(Observer::connectEvent, &error);
            if (error) {	// retry
                connect();
            } else {		// connection good
                state = idle;
                if (!deferSendRequest) {	// (subclass wants to wedge in)
                    mode(lineInput);
                    sendRequest();
                }
            }
        }
        return;
    default:
        break;
    }

    switch (state) {
    case primaryResponse:
        {
            assert(mode() == lineInput);
            observe(Observer::protocolReceive, input);
            transfer().httpResponse() = input;	// remember response for caller
            // --> HTTP/major.minor status reason-phrase
            int reasonPos;
            if (sscanf(input, "HTTP/%d.%d %u %n",
                    &httpVersionMajor, &httpVersionMinor,
                    &transfer().httpResponseCode(), &reasonPos) != 3) {
                // malformed response header
                fail(Transfer::remoteFailure);
            }
            
            if (httpVersionMajor != 1)	// wrong major protocol Version
                fail(Transfer::remoteFailure);
            if (httpVersionMinor < 0 || httpVersionMinor > 1)
                fail(Transfer::remoteFailure);
 
            // notify the URLAccess emulation that we have the result code
            observe (Observer::resultCodeReady);
            
           // okay, we grok the version. We'll proceed for now reading headers etc.
            state = readHeaders;
            
            // we got input from the server, so this Connection is now confirmed good
            restarting(false);
            break;
        }
    case readHeaders:
        {
            assert(mode() == lineInput);
            if (length) {		// another header
                headers().add(input);
                observe(Observer::protocolReceive, input);
            } else {			// end of headers
                // we are now handling the transition from response headers to response body
                observe(Observer::protocolReceive, "** END OF HEADER **");
                
                // Transfer-Encoding overrides Content-Length as per RFC2616 p.34
                if (const char *encoding = headers().find("Transfer-Encoding")) {
                    if (!strcasecmp(encoding, "chunked")) {
                        // eat input in chunks
                        state = chunkHeader;
                        // mode remains lineInput
                        break;
                    } else if (!strcasecmp(encoding, "identity")) {
                        // allowed and ignored
                    } else {
                        // unrecognized transfer-encoding
                        fail(Transfer::remoteFailure);
                    }
                }
                // no transfer-encoding (or transfer-encoding: identity): big gulp mode
                state = readWholeBody;
                if (const char *lengthArg = headers().find("Content-Length")) {
                    size_t length = strtol(lengthArg, NULL, 10);
                    sink().setSize(length);
                    if (length > 0)
                        mode(sink(), length);
                    else	// null body, already done
                        finish();
                } else {	// read until EOI
                    mode(sink());
                }
            }
            break;
        }
    case chunkHeader:
        {
            assert(mode() == lineInput);
            // line should be (just) a hex number, sans "0x" prefix or spaces. Be strict
            char *endOfMatch;
            size_t chunkLength = strtol(input, &endOfMatch, 0x10);
            if (length == 0 || endOfMatch == input) // no valid number
                fail(Transfer::remoteFailure);
            if (chunkLength) {
                debug("http", "reading chunk of %ld bytes", chunkLength);
                mode(sink(), chunkLength);
                state = chunkDownload;
            } else {
                debug("http", "final chunk marker");
                state = chunkTrailer;
                observe(Observer::protocolReceive, "** END OF DATA **");
            }
            break;
        }
    case chunkGap:
        {
            assert(mode() == lineInput);
            state = chunkHeader;
            break;
        }
    case chunkTrailer:
        {
            assert(mode() == lineInput);
            if (input[0] == '\0') {	// end of trailer
                finish();
            } else {
                headers().add(input);
                observe(Observer::protocolReceive, input);
            }
            break;
        }
    case chunkDownload:
        {
            assert(event == autoReadDone);
            state = chunkGap;
            mode(lineInput);
            break;
        }
    case readWholeBody:
        {
            assert(event == autoReadDone || event == endOfInput);
            finish();
            break;
        }
    case idle:
        {
            // the only asynchronous event in idle mode is a connection drop
            debug("http",
                "%p event %d while idle; destroying connection", this, event);
            abort();
            state = dead;
        }
        break;
    default:
        assert(false);
    }
}

void HTTPProtocol::HTTPConnection::transitError(const CssmCommonError &error)
{
    // note that fail(const char * [, OSStatus]) has already called setError
    fail(true);		// fail transfer and throw out connection
}


void HTTPProtocol::HTTPConnection::finish()
{
    flushInput();			// clear excess garbage input (resynchronize)
    chooseRetain();			// shall we keep the Connection?
    mode(lineInput);		// ensure valid input mode
    state = idle;			// idle state
    Connection::finish();	// finish this transfer
}


void HTTPProtocol::HTTPConnection::fail(bool forceDrop)
{
    if (forceDrop)
        retain(false);		// drop the Connection
    else
        chooseRetain();		// perhaps keep it
    Connection::fail();		// fail this transfer
}


bool HTTPProtocol::HTTPConnection::validate()
{
    assert(state == idle);
    tickle();	// may change state
    return state == idle;
}


void HTTPProtocol::HTTPConnection::chooseRetain()
{
    // figure out whether to stay alive
    retain(strcasecmp(headers().find("Connection", "Keep"), "Close"));
    //@@@ need to handle the HTTP/1.0 case    
}


//
// Transfer objects
//
HTTPProtocol::HTTPTransfer::HTTPTransfer(Protocol &proto,
    const Target &tgt, Operation operation, IPPort defaultPort)
    : Transfer(proto, tgt, operation, defaultPort),
      mResultClass(unclassifiedFailure)
{
}

void HTTPProtocol::HTTPTransfer::start()
{
    // HTTP servers can serve both proxy requests and direct requests,
    // and can be pooled based on that fact. Use proxy==target here.
    const HostTarget &host = proxyHostTarget();
    HTTPConnection *connection = protocol.manager.findConnection<HTTPConnection>(host);
    if (connection == NULL)
        connection = new HTTPConnection(protocol, host);
    connection->dock(this);
    startRequest();
}

void HTTPProtocol::HTTPTransfer::abort()
{
    setError("aborted");
    connectionAs<HTTPConnection>().abort();
}

void HTTPProtocol::HTTPConnection::abort()
{
    close();
    fail(true);
}


//
// This lower-level request startup function can be called directly by children.
//
void HTTPProtocol::HTTPTransfer::startRequest()
{
    const char *defaultForm;
    switch (operation()) {
    case Protocol::upload:		defaultForm = "PUT"; break;
    case Protocol::transaction:	defaultForm = "POST"; break;
    default:					defaultForm = "GET"; break;
    }
    connectionAs<HTTPConnection>().request(getv<string>(kNetworkHttpCommand, defaultForm).c_str());
}


//
// Determine whether we should use the proxy form of HTTP headers.
// By default, this is true iff we are used by a proxy Protocol.
// However, children may override this determination.
//
bool HTTPProtocol::HTTPTransfer::useProxyHeaders() const
{
    return protocol.isProxy();
}

Transfer::ResultClass HTTPProtocol::HTTPTransfer::resultClass() const
{
    switch (state()) {
    case failed:
        return mResultClass;
    case finished:
        {
            if (mResultClass != unclassifiedFailure)
                return mResultClass;	// preclassified
            unsigned int code = httpResponseCode();
            if (code == 401 || code == 407 || code == 305)	// auth or proxy auth required
                return authorizationFailure;
            else if (code / 100 == 3)			// redirect (interpreted as success)
                return success;
            else if (code / 100 == 2)			// success codes
                return success;
            else	// when in doubt, blame the remote end :-)
                return remoteFailure;
        }
    default:
        assert(false);
        return localFailure;
    }
}


void HTTPProtocol::HTTPTransfer::fail(ResultClass why, OSStatus how)
{
    mResultClass = why;
    Error::throwMe(how);
}


//
// Manage the HTTP version of a HeaderMap
//
void HTTPProtocol::HTTPHeaderMap::merge(string key, string &old, string newValue)
{
    // duplicates must be CSV type; concatenate (RFC 2616; section 4.2)
    old = old + ", " + newValue;
}


}	// end namespace Network
}	// end namespace Security
