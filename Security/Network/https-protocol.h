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
// https-protocol - SSL based HTTP
//
#ifndef _H_HTTPS_PROTOCOL
#define _H_HTTPS_PROTOCOL

#include "http-protocol.h"
#include <Security/securetransport++.h>


namespace Security {
namespace Network {


//
// The protocol object for https.
// This is heavily based on the HTTP protocol, which provides hooks to filter
// the I/O channels to implement the crypto. Refer to HTTP for all the protocol
// stuff.
//
class SecureHTTPProtocol : public HTTPProtocol {
public:
    class SecureHTTPTransfer;
    static const IPPort defaultHttpsPort = 443;

    SecureHTTPProtocol(Manager &mgr);
    
public:
    const char *name() const;
    SecureHTTPTransfer *makeTransfer(const Target &target, Operation operation);
    
private:
    //
    // Our persistent connection object
    //
    typedef SecureTransport<Socket> SSL;

protected:
    class SecureHTTPConnection : public HTTPConnection, protected SSL {
    public:
        SecureHTTPConnection(Protocol &proto, const HostTarget &tgt);
        ~SecureHTTPConnection();
        
        void sslRequest();
        
    protected:
        enum {
            sslConnecting,			// awaiting transport level connection
            sslStartup,				// just connected
            sslHandshaking,			// SSL handshake proceeding
            sslConnected			// SSL established, I/O possible
        } sslState;
        
        void transit(Event event, char *input, size_t inputLength);
        void startSSL();
        
        bool validate();
        
    protected:
        bool deferStartSSL;			// protocol break for sub-protocols
        
    private:
        bool sslActive;				// using SSL for I/O
        
        // override I/O methods for TransferEngine::Client
        size_t read(void *data, size_t length);
        size_t write(const void *data, size_t length);
        bool atEnd() const;
    };

public:
    //
    // A generic Transfer object. All HTTP transfers are transactional (headers in, optional data in,
    // headers out, optional data out), so there's no reason to distinguish subclasses.
    //
    class SecureHTTPTransfer : public HTTPTransfer {
    public:
        SecureHTTPTransfer(Protocol &proto, 
            const Target &tgt, Operation operation, IPPort defaultPort);
            
    protected:
        void start();
    };
};


}	// end namespace Network
}	// end namespace Security


#endif //_H_HTTPS_PROTOCOL
