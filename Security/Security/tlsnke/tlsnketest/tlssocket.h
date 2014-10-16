/*
 * Copyright (c) 2012,2014 Apple Inc. All Rights Reserved.
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
