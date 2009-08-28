/*
 * Copyright (c) 2000-2001,2004 Apple Computer, Inc. All Rights Reserved.
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
// socks++5 - version 5 Socks protocol
//
#ifndef _H_SOCKSPLUSPLUS5
#define _H_SOCKSPLUSPLUS5

#include "socks++.h"


namespace Security {
namespace IPPlusPlus {
namespace Socks5 {


typedef unsigned char Byte;


class Server : public SocksServer {
public:
    Server(const IPSockAddress &s) : SocksServer(5, s) { }
    
    virtual void connect(SocksClientSocket &me, const IPSockAddress &peer);
    virtual void connect(SocksClientSocket &me, const Host &host, IPPort port);
    virtual void bind(SocksServerSocket &me, const IPAddress &peer, IPPort port);
    virtual void receive(SocksServerSocket &me, SocksClientSocket &receiver);
    
private:
    void open(Socket &s, Support &me);
};


// request code (message field outbound)
enum Command {
    socksConnect = 1,					// connect (outbound)
    socksBind = 2,						// bind (single inbound)
    socksUDP = 3						// UDP associate (not implemented)
};

// reply code (message field inbound)
enum SocksReply {
    socksSuccess = 0,
    socksFailed = 1,
    socksDenied = 2,
    socksNetUnreach = 3,
    socksHostUnreach = 4,
    socksConRefused = 5,
    socksTTLExpired = 6,
    socksUnsupported = 7,
    socksAddressNotSupported = 8
};

// authentication type (in setup request)
enum AuthenticationType {
    socksAuthPublic = 0,				// anonymous access
    socksAuthGSSAPI = 1,				// GSSAPI (yuk)
    socksAuthUsername = 2,				// username/password
    socksAuthNoneAcceptable = 0xff		// can't help you there...
};

// address types (inbound/outbound)
enum AddressType {
    socksIPv4 = 1,
    socksName = 3,
    socksIPv6 = 4
};


//
// A Message object contains a single request or reply of the Socks5 protocol.
// Since some of the data is dynamically sized, we have to fudge a bit. The static
// layout corresponds to IPv4 addresses, the common case. The object itself is big
// enough for all cases.
//
struct Message {
    Byte version;			// Socks version
    Byte message;			// message/reply
    Byte reserved;			// not used (zero)
    Byte addressType;		// address type
    IPAddress addr;			// address starts here (IPv4 case)
    // following fields dynamically located if (addressType != socksIPv4)
    IPPort port;			// port field IF addr is IPv4
    Byte pad[256-sizeof(IPAddress)-sizeof(IPPort)]; // enough room for type 3 addresses (256 bytes)

    // the following fields are not part of the message data
    size_t length;			// calculated length of message (bytes, starting at version)
    
    Message(Command cmd, IPAddress addr, IPPort port);	// type 1 request
    Message(Command cmd, const char *hostname, IPPort port); // type 3 request
    void send(Socket &s);	// send request

    Message(Socket &socket); // receive (type 1 only)
    
    IPSockAddress address() const	{ return IPSockAddress(addr, ntohs(port)); }
};


}	// end namespace Socks
}	// end namespace IPPlusPlus
}	// end namespace Security

#endif //_H_SOCKSPLUSPLUS5
