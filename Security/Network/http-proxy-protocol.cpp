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
#include "http-proxy-protocol.h"


namespace Security {
namespace Network {


//
// Construct the protocol object
//
HTTPProxyProtocol::HTTPProxyProtocol(Manager &mgr, const HostTarget &proxy)
    : HTTPProtocol(mgr), host(proxy.defaultPort(defaultHttpPort))
{
    debug("uaproxy", "%p http proxy for %s", this, host.urlForm().c_str());
}


//
// Create a Transfer object for our protocol
//
HTTPProxyProtocol::HTTPTransfer *HTTPProxyProtocol::makeTransfer(const Target &target,
    Operation operation)
{
    return new HTTPTransfer(*this, target, operation, defaultHttpPort);
}


bool HTTPProxyProtocol::isProxy() const
{ return true; }

const HostTarget &HTTPProxyProtocol::proxyHost() const
{ return host; }


}	// end namespace Network
}	// end namespace Security
