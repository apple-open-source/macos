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
#ifndef _H_HTTP_PROTOCOL
#define _H_HTTP_PROTOCOL

#include "neterror.h"
#include "xfercore.h"
#include "protocol.h"
#include "transfer.h"
#include "netconnection.h"
#include <Security/ip++.h>
#include <Security/headermap.h>
#include <Security/inetreply.h>


namespace Security {
namespace Network {


//
// The Protocol object for the HTTP protocol
//
class HTTPProtocol : public Protocol {
public:
    class HTTPTransfer;
    static const IPPort defaultHttpPort = 80;

    HTTPProtocol(Manager &mgr, const char *scheme = "http");
    
public:
    HTTPTransfer *makeTransfer(const Target &target, Operation operation);
    
private:
    class HTTPHeaderMap : public HeaderMap {
    public:
        void merge(string key, string &old, string newValue);
    };

protected:
    //
    // Our persistent connection object
    //
    class HTTPConnection : public TCPConnection {
        static const int defaultSubVersion = 1;	// default to HTTP/1.1
    public:
        HTTPConnection(Protocol &proto, const HostTarget &tgt);
    
        // state machine master state
        enum State {
            errorState,				// invalid state marker
            connecting,				// awaiting transport level connection
            primaryResponse,		// read primary response line
            readHeaders,			// read initial headers
            readWholeBody,			// read basic body (Transfer-Encoding: identity)
            
            idle,					// between requests on persistent connection
            dead,					// RIP
            
            // state submachine for Transfer-Encoding: chunked
            chunkHeader,			// chunk header line (hex-length CRNL)
            chunkDownload,			// data of chunk (read in autoReadInput mode)
            chunkGap,				// empty line after chunk (now why did they do *that*?)
            chunkTrailer,			// reading trailer header fields (why not :-)
            
            START = primaryResponse
        };
        
        HTTPTransfer &transfer() { return transferAs<HTTPTransfer>(); }
        HeaderMap &headers();
        
        void request(const char *operation);
        void abort();
        
    protected:
        void transit(Event event, char *input, size_t inputLength);
        void transitError(const CssmCommonError &error);
        void finish();
        void fail(bool forceDrop = false);	// direct fail
        void fail(Transfer::ResultClass why, OSStatus how = Transfer::defaultOSStatusError)
        { transfer().fail(why, how); }	// use in transit(): setup, throws, gets caught, then fails
        bool validate();
        
        void sendRequest();
        void hostHeader();
        void authorizationHeader(const char *headerName,
            const HostTarget &host,
            ParameterSource::Key userKey, ParameterSource::Key passKey);
        void chooseRetain();

    protected:
        int subVersion;						// HTTP/1.x sub-protocol version
        State state;						// master state machine switch
        bool deferSendRequest;				// allows a subclass to interrupt state machine
        string mOperation;					// requested HTTP operation
        unsigned int httpVersionMajor;		// major version of peer
        unsigned int httpVersionMinor;		// minor version of peer
    };

public:
    //
    // A generic Transfer object. All HTTP transfers are transactional (headers in, optional data in,
    // headers out, optional data out), so there's no reason to distinguish subclasses.
    //
    class HTTPTransfer : public Transfer {
    public:
        HTTPTransfer(Protocol &proto, const Target &tgt, Operation operation, IPPort defaultPort);
        
        // access to HTTP-specific protocol details
        string &httpResponse()		{ return mPrimaryResponseString; }
        unsigned int &httpResponseCode() { return mPrimaryResponseCode; }
        unsigned int httpResponseCode() const { return mPrimaryResponseCode; }
        HeaderMap &httpHeaders()	{ return mHeaders; }
        
        void fail(ResultClass how, OSStatus err = defaultOSStatusError);
        
        // diagnostics
        ResultClass resultClass() const;
        
        void startRequest();				// start request on our Connection
        virtual bool useProxyHeaders() const; // should we use proxy form of request headers?
 
    protected:
        void start();						// start HTTP
        void abort();						// abort the Transfer

    private:
        string mPrimaryResponseString;		// HTTP protocol first response line
        unsigned int mPrimaryResponseCode;	// numeric response code
        ResultClass mResultClass;			// explicit classification (unclassified if not set)
        HTTPHeaderMap mHeaders;				// map of response headers
    };
};


//
// Deferred inlines
//
inline HeaderMap &HTTPProtocol::HTTPConnection::headers()
{ return transfer().httpHeaders(); }


}	// end namespace Network
}	// end namespace Security


#endif //_H_HTTP_PROTOCOL
