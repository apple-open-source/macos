/*
 * Copyright (c) 2000-2004 Apple Computer, Inc. All Rights Reserved.
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
#include "socks++5.h"
#include "hosts.h"


namespace Security {
namespace IPPlusPlus {
namespace Socks5 {


//
// Socks5 Protocol implementation
//
void Server::open(Socket &s, Support &my)
{
    s.open(SOCK_STREAM);
    s.connect(my.mServer->address());
    secdebug("socks", "%d connected to server %s", s.fd(), string(my.mServer->address()).c_str());
    Byte request[] = { 5, 1, socksAuthPublic };
    s.write(request, sizeof(request));
    Byte reply[2];
    s.read(reply, sizeof(reply));
    if (reply[0] != 5 || reply[1] != socksAuthPublic) {
        secdebug("socks", "%d server failed (v%d auth=%d)", s.fd(), reply[0], reply[1]);
        s.close();
        UnixError::throwMe(EPROTONOSUPPORT);
    }
}

void Server::connect(SocksClientSocket &me, const IPSockAddress &peer)
{
    open(me, me);
    Message request(socksConnect, peer.address(), peer.port());
    request.send(me);
    Message reply(me);
    me.mLocalAddress = reply.address();
    me.mPeerAddress = peer;
    secdebug("socks", "%d socks connected to %s", me.fd(), string(peer).c_str());
}

void Server::connect(SocksClientSocket &me, const Host &host, IPPort port)
{
#if 1
    //@@@ should be using Hostname (server resolution) mode, but this won't get us
    //@@@ any useful peer address to use for bind relaying. Need to rethink this scenario.
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
#else
    open(me, me);
    Message request(socksConnect, host.name().c_str(), port);
    request.send(me);
    Message reply(me);
    me.mLocalAddress = reply.address();
    //me.mPeerAddress = not provided by Socks5 protocol;
    secdebug("socks", "%d socks connected to %s", me.fd(), host.name().c_str());
#endif
}


void Server::bind(SocksServerSocket &me, const IPAddress &peer, IPPort port)
{
    open(me, me);
    Message request(socksBind, peer, port);
    request.send(me);
    Message reply(me);
    me.mLocalAddress = reply.address();
    //me.mPeerAddress not available yet;
    secdebug("socks", "%d socks bound to %s", me.fd(), string(me.mLocalAddress).c_str());
}

void Server::receive(SocksServerSocket &me, SocksClientSocket &receiver)
{
    Message reply(me);
    receiver.setFd(me.fd(), me.mLocalAddress, reply.address());
    me.clear();					// clear our own (don't close on destruction)
    secdebug("socks", "%d socks received from %s", receiver.fd(), string(reply.address()).c_str());
}


//
// Construct a request from an IPv4 address and port
//
Message::Message(Command cmd, IPAddress addr, IPPort port)
{
    version = 5;
    message = cmd;
    reserved = 0;
    addressType = socksIPv4;
    this->addr = addr;
    this->port = htons(port);
    length = 4 + sizeof(this->addr) + sizeof(this->port);
}


//
// Construct a request from a hostname and port (server resolves name)
//
Message::Message(Command cmd, const char *hostname, IPPort port)
{
    version = 5;
    message = cmd;
    reserved = 0;
    addressType = socksName;
    
    size_t nameLength = strlen(hostname);
    if (nameLength > 255)
        UnixError::throwMe(ENAMETOOLONG);
    char *addrp = reinterpret_cast<char *>(&addr);
    addrp[0] = nameLength;
    memcpy(addrp + 1, hostname, nameLength);
    IPPort nboPort = htons(port);
    memcpy(addrp + 1 + nameLength, &nboPort, sizeof(nboPort));
    length = 4 + 1 + nameLength + sizeof(nboPort);
}


//
// Send a completed request message
//
void Message::send(Socket &s)
{
    if (s.write(this, length) != length) {
        s.close();
        UnixError::throwMe(EIO);
    }
}


//
// Construct a reply object from a socket source.
// Throws exceptions if the reply is not successful and supported.
//
Message::Message(Socket &socket)
{
    length = 4 + sizeof(addr) + sizeof(port);	//@@@ calculate if addrType != 1 supported
    
    if (socket.read(this, length) != length) {
        socket.close();
        UnixError::throwMe(EIO);
    }

    // check error code
    switch (message) {
    case socksSuccess:
        break;
    case socksDenied:
        UnixError::throwMe(EPERM);
    case socksNetUnreach:
        UnixError::throwMe(ENETUNREACH);
    case socksHostUnreach:
        UnixError::throwMe(EHOSTUNREACH);
    case socksConRefused:
        UnixError::throwMe(ECONNREFUSED);
    case socksTTLExpired:
        UnixError::throwMe(ETIMEDOUT);	// not really, but what's better?
    case socksUnsupported:
        UnixError::throwMe(EOPNOTSUPP);
    case socksAddressNotSupported:
        UnixError::throwMe(EADDRNOTAVAIL);
    default:
        UnixError::throwMe(EIO);	// what else? :-)
    }
    
    // can't deal with non-IPv4 address replies
    if (addressType != socksIPv4 || reserved != 0)
        UnixError::throwMe(ENOTSUP);
}


}	// end namespace Socks
}	// end namespace IPPlusPlus
}	// end namespace Security
