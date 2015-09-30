/*
 * Copyright (c) 2000-2001,2004,2011,2014 Apple Inc. All Rights Reserved.
 * 
 * @APPLE_LICENSE_HEADER_START@
 * 
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */


//
// socks++int - internal header for Socks implementation
//
#ifndef _H_SOCKSPLUSPLUSINT
#define _H_SOCKSPLUSPLUSINT

#include "socks++.h"


namespace Security {
namespace IPPlusPlus {
namespace Socks4 {


typedef unsigned char Byte;


enum Command {
    socksConnect = 1,
    socksBind = 2
};

enum Reply {
    requestAccepted = 90,
    requestFailed = 91,
    requestIdentFailed = 92,
    requestIdentRejected = 93
};


class Server : public SocksServer {
public:
    Server(const IPSockAddress &s) : SocksServer(4, s) { }
    
    virtual void connect(SocksClientSocket &me, const IPSockAddress &peer);
    virtual void connect(SocksClientSocket &me, const Host &host, IPPort port);
    virtual void bind(SocksServerSocket &me, const IPAddress &peer, IPPort port);
    virtual void receive(SocksServerSocket &me, SocksClientSocket &receiver);
};


struct Message {
    Byte version;
    Byte message;
    IPPort port;
    IPAddress addr;
    
    Message(Command cmd, const IPSockAddress &addr);
    void send(Socket &s, const char *userid);

    Message(Socket &s);
    
    IPSockAddress address() const	{ return IPSockAddress(addr, port); }
};



}	// end namespace Socks
}	// end namespace IPPlusPlus
}	// end namespace Security

#endif //_H_SOCKSPLUSPLUSINT
