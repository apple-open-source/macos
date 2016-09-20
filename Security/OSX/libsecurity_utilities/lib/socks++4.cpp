/*
 * Copyright (c) 2000-2004,2011,2014 Apple Inc. All Rights Reserved.
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
// socks++int - internal Socks implementation
//
#include "socks++4.h"
#include "hosts.h"
#include <set>


namespace Security {
namespace IPPlusPlus {
namespace Socks4 {



//
// Socks4 Protocol implementation
//
void Server::connect(SocksClientSocket &me, const IPSockAddress &peer)
{
    me.Socket::open(SOCK_STREAM);
    me.Socket::connect(mServerAddress);
    Message request(socksConnect, peer);
    request.send(me, "nobody");
    (Message(me));				// read and check reply message
    me.mPeerAddress = peer;		// best guess, Mr. Sulu
    secinfo("socks", "%d socks4 connected to %s", me.fd(), string(peer).c_str());
}

void Server::connect(SocksClientSocket &me, const Host &host, IPPort port)
{
    // Socks4 has no name resolution support. Do it here
    //@@@ error reporting sucks here
    set<IPAddress> addrs = host.addresses();
    for (set<IPAddress>::const_iterator it = addrs.begin(); it != addrs.end(); it++) {
        try {
            IPSockAddress addr(*it, port);
            connect(me, addr);
            return;
        } catch (const UnixError &err) {
            errno = err.error;
        }
    }
    // exhausted
    UnixError::throwMe();
}


void Server::bind(SocksServerSocket &me, const IPAddress &peer, IPPort port)
{
    me.Socket::open(SOCK_STREAM);
    me.Socket::connect(mServerAddress);
    Message request(socksBind, IPSockAddress(peer, port));
    request.send(me, "nobody");
    Message reply(me);
    me.mLocalAddress = reply.address().defaults(mServerAddress.address());
    secinfo("socks", "%d socks4 bound to %s", me.fd(), string(me.mLocalAddress).c_str());
}

void Server::receive(SocksServerSocket &me, SocksClientSocket &receiver)
{
    Message reply(me);
    receiver.setFd(me.fd(), me.mLocalAddress, reply.address());
    me.clear();					// clear our own (don't close on destruction)
    secinfo("socks", "%d socks4 inbound connect", receiver.fd());
}


//
// Message properties
//
Message::Message(Command cmd, const IPSockAddress &address)
    : version(4), message(cmd), port(htons(address.port())), addr(address.address())
{
}


void Message::send(Socket &s, const char *userid)
{
    if (s.write(this, sizeof(*this)) != sizeof(*this))
        UnixError::throwMe();
    // now append zero-terminated userid (what a crock)
    size_t length = strlen(userid) + 1;
    if (s.write(userid, length) != length) {
        s.close();
        UnixError::throwMe();
    }
}

Message::Message(Socket &s)
{
    if (s.read(this, sizeof(*this)) != sizeof(*this)) {
        s.close();
        UnixError::throwMe();
    }
    if (version != 0) {
        s.close();
        UnixError::throwMe(EPROTONOSUPPORT);
    }
    switch (message) {
    case requestAccepted:
        return;
    default:
        UnixError::throwMe(ECONNREFUSED);	//@@@ hardly any diagnostics here
    }
}


}	// end namespace Socks
}	// end namespace IPPlusPlus
}	// end namespace Security
