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
// ip++ - C++ layer for IP socket and address management
//
// [Also see comments in header file.]
//
#include "ip++.h"
#include "hosts.h"
#include <Security/debugging.h>
#include <arpa/inet.h>
#include <netdb.h>


namespace Security {
namespace IPPlusPlus {


typedef unsigned char Byte;		// occasionally useful


//
// IPAddress
//
static const struct in_addr in_addr_any = { INADDR_ANY };
#if BUG_GCC
const IPAddress &IPAddress::any = *static_cast<const IPAddress *>(&in_addr_any);
#else
const IPAddress &IPAddress::any = static_cast<const IPAddress &>(in_addr_any);
#endif

IPAddress::IPAddress(const char *s)
{
    if (!inet_aton(s, this))
        UnixError::throwMe(EINVAL);
}

IPAddress::operator string() const
{
    // This code is esentially equivalent to inet_ntoa, which we can't use for thread safety.
    // Note: contents in NBO = always high-endian, thus this cast works everywhere.
    const Byte *p = reinterpret_cast<const Byte *>(this);
    char buffer[(3+1)*4];	// nnn.nnn.nnn.nnn\0
    snprintf(buffer, sizeof(buffer), "%d.%d.%d.%d", p[0], p[1], p[2], p[3]);
    return buffer;
}


//
// IPSockAddress
//
IPSockAddress::IPSockAddress()
{
    sin_family = AF_INET;
}

IPSockAddress::IPSockAddress(const IPAddress &addr, IPPort port)
{
    sin_family = AF_INET;
    sin_addr = addr;
    sin_port = htons(port);
}

IPSockAddress::operator string () const
{
    char buffer[4*(3+1)+5+1];	// nnn.nnn.nnn.nnn:ppppp
    snprintf(buffer, sizeof(buffer), "%s:%d", string(address()).c_str(), port());
    return buffer;
}


IPSockAddress IPSockAddress::defaults(const IPSockAddress &defaultAddr) const
{
    return defaults(defaultAddr.address(), defaultAddr.port());
}

IPSockAddress IPSockAddress::defaults(const IPAddress &defaultAddr, IPPort defaultPort) const
{
    return IPSockAddress(
        address() ? address() : defaultAddr,
        port() ? port() : defaultPort
    );
}

IPSockAddress IPSockAddress::defaults(IPPort defaultPort) const
{
    return IPSockAddress(address(), port() ? port() : defaultPort);
}


//
// Sockets
//
Socket::Socket(int type, int protocol)
{
    open(type, protocol);
}

void Socket::open(int type, int protocol)
{
    checkSetFd(::socket(AF_INET, type, protocol));
    mAtEnd = false;
    debug("sockio", "socket(%d,%d) -> %d", type, protocol, fd());
}

void Socket::prepare(int fdFlags, int type, int protocol)
{
    // if file descriptor is closed, open it - otherwise take what's there
    if (!isOpen())
        open(type, protocol);
        
    // if flags were passed in, set them on the file descriptor now
    if (fdFlags)
        setFlag(fdFlags);
}


void Socket::bind(const IPAddress &addr, IPPort port)
{
    bind(IPSockAddress(addr, port));
}

void Socket::bind(const IPSockAddress &local)
{
    checkError(::bind(fd(), local, sizeof(local)));
    IFDEBUG(debug("sockio", "%d bind to %s", fd(), string(local).c_str()));
}


void Socket::listen(int backlog)
{
    checkError(::listen(fd(), backlog));
}


void Socket::accept(Socket &s)
{
    IPSockAddress dummy;	// ignored
    return accept(s, dummy);
}

void Socket::accept(Socket &s, IPSockAddress &peer)
{
    int length = sizeof(IPSockAddress);
    s.checkSetFd(::accept(fd(), peer, &length));
    assert(length == sizeof(IPSockAddress));
}


bool Socket::connect(const IPSockAddress &peer)
{
    if (::connect(fd(), peer, sizeof(peer))) {
        switch (errno) {
        case EINPROGRESS:
            IFDEBUG(debug("sockio", "%d connecting to %s", fd(), string(peer).c_str()));
            return false;
        case EALREADY:
            if (int err = error())		// connect failed
                UnixError::throwMe(err);
            // just keep trying
            IFDEBUG(debug("sockio", "%d still trying to connect", fd()));
            return false;
        case EISCONN:
            if (flags() & O_NONBLOCK) {
                debug("sockio", "%d now connected", fd());
                return true;
            } else {
                UnixError::throwMe();
            }
        default:
            UnixError::throwMe();
        }
    } else {
        IFDEBUG(debug("sockio", "%d connect to %s", fd(), string(peer).c_str()));
        return true;
    }
}

bool Socket::connect(const IPAddress &addr, IPPort port)
{
    return connect(IPSockAddress(addr, port));
}

// void Socket::connect(const Host &host, ...): see below.


void Socket::shutdown(int how)
{
    assert(how >= 0 && how <= 2);
    checkError(::shutdown(fd(), how));
}


IPSockAddress Socket::localAddress() const
{
    IPSockAddress addr;
    int length = sizeof(addr);
    checkError(::getsockname(fd(), addr, &length));
    assert(length == sizeof(addr));
    return addr;
}

IPSockAddress Socket::peerAddress() const
{
    IPSockAddress addr;
    int length = sizeof(addr);
    checkError(::getpeername(fd(), addr, &length));
    assert(length == sizeof(addr));
    return addr;
}

void Socket::getOption(void *value, int &length, int name, int level = SOL_SOCKET) const
{
    UnixError::check(::getsockopt(fd(), level, name, value, &length));
}

void Socket::setOption(const void *value, int length, int name, int level = SOL_SOCKET) const
{
    UnixError::check(::setsockopt(fd(), level, name, value, length));
}

    
//
// Connect to a Host object.
// This version of connect performs nontrivial work and makes interesting decisions.
//
void Socket::connect(const Host &host, IPPort port)
{
    //@@@ use two-step stutter algorithm?
    //@@@ randomize order?
    //@@@ keep worked-recently information?
    //@@@ what about nonblocking operation?
    set<IPAddress> addrs = host.addresses();
    for (set<IPAddress>::const_iterator it = addrs.begin(); it != addrs.end(); it++) {
        const IPSockAddress address(*it, port);
        if (::connect(fd(), address, sizeof(IPSockAddress)) == 0) {
            IFDEBUG(debug("sockio", "%d connect to %s", fd(), string(address).c_str()));
            return;
        }
    }
    // no joy on any of the candidate addresses. Throw last error
    //@@@ clean up errno?
    UnixError::throwMe();
}


//
// TCP*Sockets.
// Note that these will TCP*Socket::open() will *use* its existing file descriptor,
// on the theory that the caller may have prepared it specially (e.g. to make it nonblocking).
//
void TCPClientSocket::open(const IPSockAddress &peer, int fdFlags)
{
    prepare(fdFlags, SOCK_STREAM);
    connect(peer);
}

void TCPClientSocket::open(const IPAddress &addr, IPPort port, int fdFlags)
{
    prepare(fdFlags, SOCK_STREAM);
    connect(addr, port);
}

void TCPClientSocket::open(const Host &host, IPPort port, int fdFlags)
{
    prepare(fdFlags, SOCK_STREAM);
    connect(host, port);
}

TCPClientSocket::~TCPClientSocket()
{
    close();
}


void TCPServerSocket::open(const IPSockAddress &addr, int depth)
{
    prepare(0, SOCK_STREAM);
    bind(addr);
    listen(depth);
}

void TCPServerSocket::operator () (TCPClientSocket &newClient)
{
    accept(newClient);
}

void TCPServerSocket::receive(TCPClientSocket &newClient)
{
    accept(newClient);
    close();
}

TCPServerSocket::~TCPServerSocket()
{
    close();
}


}	// end namespace IPPlusPlus
}	// end namespace Security
