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
// http-proxy protocol: HTTP variant for proxying
//
#ifndef _H_HTTP_PROXY_PROTOCOL
#define _H_HTTP_PROXY_PROTOCOL

#include "http-protocol.h"


namespace Security {
namespace Network {


//
// The protocol object for proxy HTTP.
// Since HTTPProtocol contains code to support the proxy variant,
// this Protocol object is quite trivial.
//
class HTTPProxyProtocol : public HTTPProtocol {
public:
    HTTPProxyProtocol(Manager &mgr, const HostTarget &proxy);
    
    HTTPTransfer *makeTransfer(const Target &target, Operation operation);
    
public:
    bool isProxy() const;
    const HostTarget &proxyHost() const;
    
private:
    const HostTarget host;
};


}	// end namespace Network
}	// end namespace Security


#endif //_H_HTTP_PROXY_PROTOCOL
