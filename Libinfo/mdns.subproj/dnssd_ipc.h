/*
 * Copyright (c) 2003 Apple Computer, Inc. All rights reserved.
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

    Change History (most recent first):

$Log: dnssd_ipc.h,v $
Revision 1.9  2004/09/22 20:05:38  majka
Integrated
3725573 - Need Error Codes for handling Lighthouse setup failure on NAT
3805822 - Socket-based APIs aren't endian-safe
3806739 - DNSServiceSetDefaultDomainForUser header comments incorrect

Revision 1.8.2.1  2004/09/20 21:54:33  ksekar
<rdar://problem/3805822> Socket-based APIs aren't endian-safe

Revision 1.8  2004/09/17 20:19:01  majka
Integrated 3804522

Revision 1.7.2.1  2004/09/17 20:15:30  ksekar
*** empty log message ***

Revision 1.7  2004/09/16 23:45:24  majka
Integrated 3775315 and 3765280.

Revision 1.6.4.1  2004/09/02 19:43:41  ksekar
<rdar://problem/3775315>: Sync dns-sd client files between Libinfo and
mDNSResponder projects

Revision 1.11  2004/08/10 06:24:56  cheshire
Use types with precisely defined sizes for 'op' and 'reg_index', for better
compatibility if the daemon and the client stub are built using different compilers

Revision 1.10  2004/07/07 17:39:25  shersche
Change MDNS_SERVERPORT from 5533 to 5354.

Revision 1.9  2004/06/25 00:26:27  rpantos
Changes to fix the Posix build on Solaris.

Revision 1.8  2004/06/18 04:56:51  rpantos
Add layer for platform code

Revision 1.7  2004/06/12 01:08:14  cheshire
Changes for Windows compatibility

Revision 1.6  2003/08/12 19:56:25  cheshire
Update to APSL 2.0

 */

#ifndef DNSSD_IPC_H
#define DNSSD_IPC_H

#include "dns_sd.h"


//
// Common cross platform services
//
#if defined(WIN32)
#	include <winsock2.h>
#	define dnssd_InvalidSocket	INVALID_SOCKET
#	define dnssd_EWOULDBLOCK	WSAEWOULDBLOCK
#	define dnssd_EINTR			WSAEINTR
#	define MSG_WAITALL 			0
#	define dnssd_sock_t			SOCKET
#	define dnssd_sockbuf_t		
#	define dnssd_close(sock)	closesocket(sock)
#	define dnssd_errno()		WSAGetLastError()
#	define ssize_t				int
#	define getpid				_getpid
#else
#	include <sys/types.h>
#	include <unistd.h>
#	include <sys/un.h>
#	include <string.h>
#	include <stdio.h>
#	include <stdlib.h>
#	include <sys/stat.h>
#	include <sys/socket.h>
#	include <netinet/in.h>
#	define dnssd_InvalidSocket	-1
#	define dnssd_EWOULDBLOCK	EWOULDBLOCK
#	define dnssd_EINTR			EINTR
#	define dnssd_EPIPE			EPIPE
#	define dnssd_sock_t			int
#	define dnssd_close(sock)	close(sock)
#	define dnssd_errno()		errno
#endif

#if defined(USE_TCP_LOOPBACK)
#	define AF_DNSSD				AF_INET
#	define MDNS_TCP_SERVERADDR	"127.0.0.1"
#	define MDNS_TCP_SERVERPORT	5354
#	define LISTENQ				5
#	define dnssd_sockaddr_t		struct sockaddr_in
#else
#	define AF_DNSSD				AF_LOCAL
#	define MDNS_UDS_SERVERPATH	"/var/run/mDNSResponder"
#	define LISTENQ				100
    // longest legal control path length
#	define MAX_CTLPATH			256	
#	define dnssd_sockaddr_t		struct sockaddr_un
#endif


//#define UDSDEBUG  // verbose debug output

// Compatibility workaround
#ifndef AF_LOCAL
#define	AF_LOCAL	AF_UNIX
#endif

// General UDS constants
#define TXT_RECORD_INDEX ((uint32_t)(-1))	// record index for default text record

// IPC data encoding constants and types
#define VERSION 1
#define IPC_FLAGS_NOREPLY 1	// set flag if no asynchronous replies are to be sent to client
#define IPC_FLAGS_REUSE_SOCKET 2 // set flag if synchronous errors are to be sent via the primary socket
                                // (if not set, first string in message buffer must be path to error socket

typedef enum
    {
    connection = 1,           // connected socket via DNSServiceConnect()
    reg_record_request,	  // reg/remove record only valid for connected sockets
    remove_record_request,
    enumeration_request,
    reg_service_request,
    browse_request,
    resolve_request,
    query_request,
    reconfirm_record_request,
    add_record_request,
    update_record_request,
    setdomain_request
    } request_op_t;

typedef enum
    {
    enumeration_reply = 64,
    reg_service_reply,
    browse_reply,
    resolve_reply,
    query_reply,
    reg_record_reply
    } reply_op_t;

typedef struct ipc_msg_hdr_struct ipc_msg_hdr;

// client stub callback to process message from server and deliver results to
// client application

typedef void (*process_reply_callback)
    (
    DNSServiceRef sdr,
    ipc_msg_hdr *hdr,
    char *msg
    );

// allow 64-bit client to interoperate w/ 32-bit daemon
typedef union
    {
    void *context;
    uint32_t ptr64[2];
    } client_context_t;

typedef struct __attribute__((__packed__)) ipc_msg_hdr_struct
    {
    uint32_t version;
    uint32_t datalen;
    uint32_t flags;
    uint32_t op;		// request_op_t or reply_op_t
    client_context_t client_context; // context passed from client, returned by server in corresponding reply
    uint32_t reg_index;            // identifier for a record registered via DNSServiceRegisterRecord() on a
    // socket connected by DNSServiceConnect().  Must be unique in the scope of the connection, such that and
    // index/socket pair uniquely identifies a record.  (Used to select records for removal by DNSServiceRemoveRecord())
    } ipc_msg_hdr_struct;

// it is advanced to point to the next field, or the end of the message
// routines to write to and extract data from message buffers.
// caller responsible for bounds checking.
// ptr is the address of the pointer to the start of the field.
// it is advanced to point to the next field, or the end of the message

void put_long(const uint32_t l, char **ptr);
uint32_t get_long(char **ptr);

void put_short(uint16_t s, char **ptr);
uint16_t get_short(char **ptr);

#define put_flags put_long
#define get_flags get_long

#define put_error_code put_long
#define get_error_code get_long

int put_string(const char *str, char **ptr);
int get_string(char **ptr, char *buffer, int buflen);

void put_rdata(const int rdlen, const char *rdata, char **ptr);
char *get_rdata(char **ptr, int rdlen);  // return value is rdata pointed to by *ptr -
                                         // rdata is not copied from buffer.

void ConvertHeaderBytes(ipc_msg_hdr *hdr);

#endif // DNSSD_IPC_H
