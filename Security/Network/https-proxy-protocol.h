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
// https-proxy - CONNECT style transparent proxy connection to SSL host
//
#ifndef _H_HTTPS_PROXY_PROTOCOL
#define _H_HTTPS_PROXY_PROTOCOL

#include "https-protocol.h"


namespace Security {
namespace Network {


//
// The CONNECT protocol is a subclass of the secure (SSL) HTTP protocol.
//
class ConnectHTTPProtocol : public SecureHTTPProtocol {
    class ConnectHTTPTransfer;
public:
    ConnectHTTPProtocol(Manager &mgr, const HostTarget &proxy);
    
public:
    ConnectHTTPTransfer *makeTransfer(const Target &target, Operation operation);
    
private:
    //
    // Our persistent connection object
    //
    class ConnectHTTPConnection : public SecureHTTPConnection {
    public:
        ConnectHTTPConnection(Protocol &proto, const HostTarget &tgt);
        ~ConnectHTTPConnection();
        
        enum {
            connectConnecting,			// TCP layer connecting pending
            connectStartup,				// starting conversation
            connectPrimaryResponse,		// sent CONNECT, waiting for primary response
            connectReadHeaders,			// reading proxy headers
            connectReady				// in transparent mode
        } connectState;
        
        void connectRequest();
        
    protected:
        void transit(Event event, char *input, size_t inputLength);
    };


    //
    // A generic Transfer object. All HTTP transfers are transactional (headers in, optional data in,
    // headers out, optional data out), so there's no reason to distinguish subclasses.
    //
    class ConnectHTTPTransfer : public SecureHTTPTransfer {
    public:
        ConnectHTTPTransfer(Protocol &proto, 
            const Target &tgt, Operation operation, IPPort defaultPort);
            
    protected:
        void start();
        
        bool useProxyHeaders() const;
    };
    
public:
    bool isProxy() const;
    const HostTarget &proxyHost() const;
    
private:
    const HostTarget host;
};


}	// end namespace Network
}	// end namespace Security


#endif //_H_HTTPS_PROXY_PROTOCOL
