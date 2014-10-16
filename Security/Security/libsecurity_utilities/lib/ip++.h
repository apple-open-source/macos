/*
 * Copyright (c) 2000-2001,2003-2004,2011,2014 Apple Inc. All Rights Reserved.
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
// ip++ - C++ layer for IP socket and address management
//
// Key to comments:
//	HBO = host byte order, NBO = network byte order
//
// Rules for byte ordering: C++ objects store addresses and ports in NBO.
// Struct in_addr arguments are in NBO. Integer type arguments are in HBO.
// Stick with the conversion methods and you win. Cast around and you lose.
//
// @@@ Which namespace should we be in?
//
#ifndef _H_IPPLUSPLUS
#define _H_IPPLUSPLUS

#include "unix++.h"
#include "timeflow.h"
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/un.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <cstdio>
#include <cstdarg>
#include <map>

using namespace UnixPlusPlus;


namespace Security {
namespace IPPlusPlus {

class Host;


//
// For now, ports are simply a short unsigned integer type, in HBO.
//
typedef UInt16 IPPort;


//
// An IP host address.
//
class IPAddress : public in_addr {
public:
    IPAddress()						{ s_addr = htonl(INADDR_ANY); }
    IPAddress(const struct in_addr &addr) { s_addr = addr.s_addr; }
    explicit IPAddress(UInt32 addr)	{ s_addr = htonl(addr); }
    IPAddress(const char *s);		// ONLY dotted-quad form - use hosts.h for name resolution
    
    operator UInt32 () const		{ return ntohl(s_addr); }
    operator string () const;		// "n.n.n.n" (no name resolution)
    
public:
    bool operator == (const IPAddress &other) const	{ return s_addr == other.s_addr; }
    bool operator != (const IPAddress &other) const	{ return s_addr != other.s_addr; }
    bool operator < (const IPAddress &other) const	{ return s_addr < other.s_addr; }
    
    operator bool () const			{ return s_addr != htonl(INADDR_ANY); }
    bool operator ! () const		{ return s_addr == htonl(INADDR_ANY); }
    
public:
    static const IPAddress &any;
};


//
// An IP "socket address", i.e. a combined host address and port.
//
class IPSockAddress : public sockaddr_in {
public:
    IPSockAddress();
    IPSockAddress(const struct sockaddr_in &sockaddr)	{ *(sockaddr_in *)this = sockaddr; }
    IPSockAddress(const IPAddress &addr, IPPort port);
    
    IPAddress address() const		{ return sin_addr; }
    void address(IPAddress addr)	{ sin_addr = addr; }
    IPPort port() const				{ return ntohs(sin_port); }
    void port(IPPort p)				{ sin_port = htons(p); }
    
    operator string () const;		// "n.n.n.n:p" (no name resolution)

    // automatically convert to struct sockaddr * for use in system calls
    operator struct sockaddr * ()
    { return reinterpret_cast<struct sockaddr *>(this); }
    operator const struct sockaddr * () const
    { return reinterpret_cast<const struct sockaddr *>(this); }
    
    // conveniences
    IPSockAddress defaults(const IPSockAddress &defaultAddr) const;
    IPSockAddress defaults(const IPAddress &defaultAddr, IPPort defaultPort = 0) const;
    IPSockAddress defaults(IPPort defaultPort) const;
};


//
// UNIX Domain Socket addresses, for those who care.
// An "UNAddress", such as it were, is simply a string.
//
class UNSockAddress : public sockaddr_un {
public:
	UNSockAddress();
	UNSockAddress(const char *path);
	UNSockAddress(const std::string &path);
	
	string path() const;
	operator string () const		{ return path(); }

    // automatically convert to struct sockaddr * for use in system calls
    operator struct sockaddr * ()
    { return reinterpret_cast<struct sockaddr *>(this); }
    operator const struct sockaddr * () const
    { return reinterpret_cast<const struct sockaddr *>(this); }
};


//
// An IP socket.
// This inherits all functionality of a FileDesc, so I/O is fun and easy.
// Socket is "passive"; it doesn't own any resources and does nothing on destruction.
// On the upside, you can assign Sockets freely.
// If you want self-managing sockets that clean up after themselves,
// use the subclasses below.
//
class Socket : public FileDesc {
public:
    Socket() { }
	explicit Socket(int domain, int type, int protocol = 0);
    explicit Socket(int type);
    
    Socket &operator = (int fd)				{ setFd(fd); return *this; }
    
    // basic open (socket system call)
	void open(int domain, int type, int protocol = 0);
	void open(int type)						{ open(AF_INET, type, 0); }
    
    // standard socket operations
    void bind(const IPSockAddress &addr);	// to this socket address
    void bind(const IPAddress &addr = IPAddress::any, IPPort port = 0);
	void bind(const UNSockAddress &addr);	// to this UNIX domain socket
    void listen(int backlog = 1);
    void accept(Socket &s);
    void accept(Socket &s, IPSockAddress &peer);
    void accept(Socket &s, UNSockAddress &peer);
	bool connect(const struct sockaddr *peer);
    bool connect(const IPSockAddress &peer);
    bool connect(const IPAddress &addr, IPPort port);
	bool connect(const UNSockAddress &peer);
    void connect(const Host &host, IPPort port);	// any address of this host
    void shutdown(int type);
    enum { shutdownRead = 0, shutdownWrite = 1, shutdownBoth = 2 };
    
    // get endpoint addresses
    IPSockAddress localAddress() const;
    IPSockAddress peerAddress() const;
    
    // socket options
    void setOption(const void *value, int length, int name, int level = SOL_SOCKET) const;
    void getOption(void *value, socklen_t &length, int name, int level = SOL_SOCKET) const;
    
    template <class T> void setOption(const T &value, int name, int level = SOL_SOCKET) const
    { setOption(&value, sizeof(value), name, level); }
    
    template <class T> T getOption(int name, int level = SOL_SOCKET) const
    {
        T value; socklen_t length = sizeof(value);
        getOption(&value, length, name, level);
        assert(length == sizeof(value));
        return value;
    }
    
    // some specific useful options
    int type() const		{ return getOption<int>(SO_TYPE); }
    int error() const		{ return getOption<int>(SO_ERROR); }
    
public:
#if defined(SOMAXCONN)
    static const int listenMaxQueue = SOMAXCONN;
#else
    static const int listenMaxQueue = 5;	// the traditional BSD UNIX value
#endif

protected:
    void prepare(int fdFlags, int domain, int type, int protocol = 0);
};


//
// A TCPClientSocket is a self-connecting TCP socket that connects (actively) to a server.
// Since TCP, once established, is symmetric, it can also be used for the server side
// of a TCP pipe. You can think of it as the least complex embodiment of a TCP connection.
//
class TCPClientSocket : public Socket {
    NOCOPY(TCPClientSocket)
public:
    TCPClientSocket() { }
    ~TCPClientSocket();	// closes connection
    
#if BUG_GCC
    void open(int type, int protocol = 0)	{ Socket::open(type, protocol); }
#else
    using Socket::open;
#endif
    
    void open(const IPSockAddress &peer, int fdFlags = 0);
    void open(const IPAddress &addr, IPPort port, int fdFlags = 0);
    void open(const Host &host, IPPort port, int fdFlags = 0);

    TCPClientSocket(const IPSockAddress &peer, int fdFlags = 0)
    { open(peer, fdFlags); }
    TCPClientSocket(const IPAddress &addr, IPPort port, int fdFlags = 0)
    { open(addr, port, fdFlags); }
    TCPClientSocket(const Host &host, IPPort port, int fdFlags = 0)
    { open(host, port, fdFlags); }
    
protected:	// for serverSocket/clientSocket footsy play
    void setFd(int fd)			{ Socket::setFd(fd); }
    
private:
    TCPClientSocket(int sockfd);
};


//
// A TCPServerSocket is a self-initializing listener socket for incoming TCP requests
// (usually to a server). Its function operator yields the next incoming connection request
// as a TCPClientSocket (see above). For one-shot receivers, the receive() method will
// create the client and close the listener atomically (which is sometimes faster).
//
class TCPServerSocket : public Socket {
    NOCOPY(TCPServerSocket)
public:
    TCPServerSocket() { }
    ~TCPServerSocket();	// closes listener; existing connections unaffected
    
    void open(const IPSockAddress &local, int depth = 1);
    void open(IPPort port = 0, int depth = 1)
    { open(IPSockAddress(IPAddress::any, port), depth); }

    TCPServerSocket(const IPSockAddress &local, int depth = 1)	{ open(local, depth); }
    TCPServerSocket(IPPort port, int depth = 1)					{ open(port, depth); }
    
    void operator () (TCPClientSocket &newClient);	// retrieve next connection
    void receive(TCPClientSocket &client);			// accept once, then close listener
};


}	// end namespace IPPlusPlus
}	// end namespace Security


#endif //_H_IPPLUSPLUS
