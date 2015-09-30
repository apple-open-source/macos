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
// socks - socks version of IP sockets
//
// This Socks implementation replaces the TCP-functional layer of the socket interface
// (TCPClientSocket and TCPServerSocket), not the raw Socket layer. Remember what
// Socks was invented for -- it's NOT a generic socket abstraction layer, valiant efforts
// of the various -lsocks libraries nonwithstanding.
// Do note that these are not virtual overrides, but textual replacements.
//
// This implementation supports Socks versions 4 and 5, as well as direct (un-socksed) sockets.
// The choice is per socket object.
//
// API Synopsis:
//	SocksServer *server = SocksServer::make(version, IP-address);
//	SocksServer::defaultServer(server);	// for new sockets
//	SocksClientSocket clientSocket(...);
//		clientSocket.server(server);		// for this socket
//	SocksServerSocket serverSocket(...);	// only supports .receive()
// Otherwise, Socks{Client,Server}Socket is functionally equivalent to {Client,Server}Socket.
// Sockets without a Server (explicit or by default) are direct.
//
// Minimum replacement strategy:
//	#define TCPClientSocket SocksClientSocket
//	#define TCPServerSocket SocksServerSocket
//	SocksServer::defaultServer(SocksServer::make(...));
//
// Limitations:
// There is no UDP Socks support.
// @@@ Nonblocking sockets may not work quite right.
//
#ifndef _H_SOCKSPLUSPLUS
#define _H_SOCKSPLUSPLUS

#include "ip++.h"
#include <security_utilities/threading.h>
#include <security_utilities/globalizer.h>


using namespace UnixPlusPlus;


namespace Security {
namespace IPPlusPlus {


class SocksServerSocket;
class SocksClientSocket;


//
// A particular Socks server and version. Get one by calling SocksServer::make().
// You can express "no socks server" (direct connect) with a NULL pointer (or version==0).
//
class SocksServer {
public:
    class Support; friend class Support;

private:
    struct Global {
        mutable Mutex lock;			// lock for mGlobalServerAddress
        SocksServer *mServer;		// global default server
        ThreadNexus<IPAddress> lastConnected; // last address connected to (for aux. bind)
        
        Global() : mServer(NULL) { }
        
        void server(SocksServer *srv)	{ StLock<Mutex> _(lock); mServer = srv; }
        SocksServer *server() const		{ StLock<Mutex> _(lock); return mServer; }
    };
    static ModuleNexus<Global> global; // global state

public:
    typedef unsigned int Version;
    
    static SocksServer *make(Version version, const IPSockAddress &addr);

    const IPSockAddress &address() const	{ return mServerAddress; }
    Version version() const					{ return mVersion; }
    
public:
    static SocksServer *defaultServer()		{ return global().server(); }
    static void defaultServer(SocksServer *server) { global().server(server); }

protected:
	virtual ~SocksServer();
	
    virtual void connect(SocksClientSocket &me, const IPSockAddress &peer) = 0;
    virtual void connect(SocksClientSocket &me, const Host &host, IPPort port) = 0;
    virtual void bind(SocksServerSocket &me, const IPAddress &peer, IPPort port) = 0;
    virtual void receive(SocksServerSocket &me, SocksClientSocket &receiver) = 0;

    SocksServer(Version v, const IPSockAddress &addr) : mVersion(v), mServerAddress(addr) { }

protected:
    Version mVersion;
    IPSockAddress mServerAddress;

public:
    class Support {
    public:
        SocksServer *server() const		{ return mServer; }
        void server(SocksServer *srv)	{ mServer = srv; }
        
        IPSockAddress localAddress(const Socket &me) const;
        IPSockAddress peerAddress(const Socket &me) const;
        
    protected:
        Support() : mServer(defaultServer()) { }

        void connect(SocksClientSocket &me, const IPSockAddress &peer)	
        { mServer->connect(me, peer); }
        void connect(SocksClientSocket &me, const Host &host, IPPort port) 
        { mServer->connect(me, host, port); }
        void bind(SocksServerSocket &me, const IPAddress &peer, IPPort port)
        { mServer->bind(me, peer, port); }
        void receive(SocksServerSocket &me, SocksClientSocket &receiver)
        { mServer->receive(me, receiver); }
        
        void lastConnected(IPAddress addr)	{ global().lastConnected() = addr; }
        IPAddress lastConnected() const		{ return global().lastConnected(); }
    
    public:
        SocksServer *mServer;				// server for this socket
        IPSockAddress mLocalAddress;		// my own address, as reported by server
        IPSockAddress mPeerAddress;			// peer address
    };
};


//
// The Socks version of a TCPClientSocket
//
class SocksClientSocket : public TCPClientSocket, public SocksServer::Support {
public:
    SocksClientSocket() { }
    SocksClientSocket(const IPSockAddress &peer)				{ open(peer); }
    SocksClientSocket(const IPAddress &addr, IPPort port)		{ open(addr, port); }
    SocksClientSocket(const Host &host, IPPort port)			{ open(host, port); }
    
    void open(const IPSockAddress &peer);
    void open(const IPAddress &addr, IPPort port);
    void open(const Host &host, IPPort port);

    IPSockAddress localAddress() const		{ return Support::localAddress(*this); }
    IPSockAddress peerAddress() const		{ return Support::peerAddress(*this); }
    
public:
    void setFd(int fd, const IPSockAddress &local, const IPSockAddress &peer);
};


//
// The Socks version of a TCPServerSocket.
// Note that this version only supports the receive() access method.
// By the nature of things, the queue-length argument is ignored (it's always 1).
//
// A note about setMainConnection: There is a structural problem
// with the Socks protocol. When a SocksServerSocket goes active,
// the protocol requires the IP address of the host the connection will be
// coming from. Typical Socks library layers simply assume that this will
// be the address of the last server connected to by another (TCP) socket.
// We do this heuristic too, but it's unreliable: it's a per-thread global, and will
// fail if you interleave multiple socks "sessions" in the same thread. For this
// case (or if you just want to be safe and explicit), you can call setMainConnection to
// explicitly link this socket to a TCPClientSocket whose peer we should use.
// Do note that this call does not exist in the plain (non-socks) socket layer.
//
class SocksServerSocket : public TCPServerSocket, public SocksServer::Support {
public:
    SocksServerSocket() { }
    SocksServerSocket(const IPSockAddress &local, int = 1) { open(local); }
    SocksServerSocket(IPPort port, int = 1)					{ open(port); }
    
    void open(const IPSockAddress &local, int = 1);
    void open(IPPort port = 0, int = 1)
    { open(IPSockAddress(IPAddress::any, port)); }
    
    void receive(SocksClientSocket &client);		// accept incoming and close listener

    IPSockAddress localAddress() const		{ return Support::localAddress(*this); }
    IPSockAddress peerAddress() const		{ return Support::peerAddress(*this); }
    
    // this special call is not an overlay of TCPServerSocket - it exists only for Socks
    void setMainConnection(TCPClientSocket &main)
    { mConnectionPeer = main.peerAddress().address(); }
    
private:
    IPAddress mConnectionPeer;						// address to say we're peered with
    
private:
    void operator () (TCPClientSocket &newClient);	// not supported by Socks
};


}	// end namespace IPPlusPlus
}	// end namespace Security


#endif //_H_IPPLUSPLUS
