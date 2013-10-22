//
//  tlssocket.h
//  tlsnke
//
//  Created by Fabrice Gautier on 1/6/12.
//  Copyright (c) 2012 Apple, Inc. All rights reserved.
//

#ifndef __TLSSOCKET_H__
#define __TLSSOCKET_H__

#include <Security/SecureTransportPriv.h>

/* 
   Attach the TLS socket filter.
 
   This makes a socket a TLS socket by attaching the TLS socket filter to that socket.
   Return a positive TLS handle or a negative error.
   The return TLS handle can be used to route VPN data directly through this TLS 
   socket
 */
int TLSSocket_Attach(int socket);

/* 
 Detach the TLS socket filter.
 
 Return 0 or negative error. 
 If the TLS Socket is used with SecureTransport, one should make sure 
 to tear down the SecureTransport session before calling this.
 It is not required to use this, as closing the socket would have the same effect.
*/
int TLSSocket_Detach(int socket);

/*
    Secure Transport Record Layer functions for TLS Sockets.
 
    To use SecureTransport with a TLS kernel socket, pass this to SSLSetRecordFuncs and
    the socket descriptor to SSLSetRecordContext
 */ 
const struct SSLRecordFuncs TLSSocket_Funcs;


#endif
