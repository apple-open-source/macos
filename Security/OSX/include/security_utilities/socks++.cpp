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
// socks - socks version of IP sockets
//
// [Also see comments in header file.]
//
// This file contains the "generic" Socks functionality.
// Socks4 and Socks5 implementations are in their separate files.
//
#include "socks++.h"
#include "socks++4.h"
#include "socks++5.h"
#include "hosts.h"


namespace Security {
namespace IPPlusPlus {


//
// Static objects
//
ModuleNexus<SocksServer::Global> SocksServer::global;


//
// SocksServer destruction
//
SocksServer::~SocksServer()
{ /* virtual */ }


//
// Create a SocksServer object
//
SocksServer *SocksServer::make(Version version, const IPSockAddress &addr)
{
    switch (version) {
    case 0:
        return NULL;		// no socks
    case 4:
        return new Socks4::Server(addr);
    case 5:
        return new Socks5::Server(addr);
    default:
        UnixError::throwMe(EINVAL);
    }
}


//
// TCPClientSockets (CONNECT access)
//    
void SocksClientSocket::open(const IPSockAddress &peer)
{
    if (mServer) {
        Support::connect(*this, peer);
        lastConnected(mPeerAddress.address());
    } else {
        TCPClientSocket::open(peer);
    }
}

void SocksClientSocket::open(const IPAddress &addr, IPPort port)
{
    open(IPSockAddress(addr, port));
}

void SocksClientSocket::open(const Host &host, IPPort port)
{
    if (mServer) {
        Support::connect(*this, host, port);
        lastConnected(mPeerAddress.address());
    } else {
        TCPClientSocket::open(host, port);
    }
}

void SocksClientSocket::setFd(int fd, const IPSockAddress &local, const IPSockAddress &peer)
{
    Socket::setFd(fd);
    mLocalAddress = local;
    mPeerAddress = peer;
}


//
// TCPServerSockets (BIND access)
//
void SocksServerSocket::open(const IPSockAddress &local, int)
{
    if (mServer) {
#if BUG_GCC
        if (mConnectionPeer)
            Support::bind(*this, mConnectionPeer, local.port());
        else
            Support::bind(*this, lastConnected(), local.port());
#else
        Support::bind(*this,
            mConnectionPeer ? mConnectionPeer : lastConnected(),
            local.port());
#endif
    } else {
        TCPServerSocket::open(local, 1);
    }
}

void SocksServerSocket::receive(SocksClientSocket &client)
{
    if (mServer) {
        Support::receive(*this, client);
    } else {
        TCPServerSocket::receive(client);
    }
}


//
// Address functions
//
IPSockAddress SocksServer::Support::localAddress(const Socket &me) const
{
    if (mServer)
        return mLocalAddress;
    else
        return me.localAddress();
}

IPSockAddress SocksServer::Support::peerAddress(const Socket &me) const
{
    if (mServer)
        return mPeerAddress;
    else
        return me.peerAddress();
}


}	// end namespace IPPlusPlus
}	// end namespace Security
