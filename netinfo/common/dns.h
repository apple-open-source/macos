/*
 * Copyright (c) 1999 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * "Portions Copyright (c) 1999 Apple Computer, Inc.  All Rights
 * Reserved.  This file contains Original Code and/or Modifications of
 * Original Code as defined in and that are subject to the Apple Public
 * Source License Version 1.0 (the 'License').  You may not use this file
 * except in compliance with the License.  Please obtain a copy of the
 * License at http://www.apple.com/publicsource and read it before using
 * this file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License."
 * 
 * @APPLE_LICENSE_HEADER_END@
 */

/*
 * Thread-safe DNS client library
 *
 * Copyright (c) 1998 Apple Computer Inc.  All Rights Reserved.
 * Written by Marc Majka
 */

#ifndef __DNS_CLIENT_H__
#define __DNS_CLIENT_H__

#include <NetInfo/config.h>
#include <sys/types.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#ifdef _UNIX_BSD_43_
#define ssize_t int
#define __P(X) X
#else
#include <sys/cdefs.h>
#endif

#define IN_ADDR_DOMAIN_STRING "in-addr.arpa"
#define LOCAL_DOMAIN_STRING "local"
#define LOCAL_DOMAIN_MULTICAST_ADDR "224.0.0.251"
#define DNS_SERVICE_PORT 53
#define DNS_LOCAL_DOMAIN_SERVICE_PORT 5353

/*
 * Status returned in a dns_reply_t
 */
#define DNS_STATUS_OK					0
#define DNS_STATUS_BAD_HANDLE			1
#define DNS_STATUS_MALFORMED_QUERY		2
#define DNS_STATUS_TIMEOUT				3
#define DNS_STATUS_SEND_FAILED			4
#define DNS_STATUS_RECEIVE_FAILED		5
#define DNS_STATUS_CONNECTION_FAILED		6
#define DNS_STATUS_WRONG_SERVER			7
#define DNS_STATUS_WRONG_XID				8
#define DNS_STATUS_WRONG_QUESTION		9

#define DNS_MAX_DNAME_LENGTH 63

#define DNS_FLAGS_QR_MASK  0x8000
#define DNS_FLAGS_QR_QUERY 0x0000
#define DNS_FLAGS_QR_REPLY 0x8000

#define DNS_FLAGS_OPCODE_MASK    0x7800
#define DNS_FLAGS_QUERY_STANDARD 0x0000
#define DNS_FLAGS_QUERY_INVERSE  0x0800
#define DNS_FLAGS_QUERY_STATUS   0x1000

#define DNS_FLAGS_AA 0x0400
#define DNS_FLAGS_TC 0x0200
#define DNS_FLAGS_RD 0x0100
#define DNS_FLAGS_RA 0x0080

#define DNS_FLAGS_RCODE_MASK            0x000f
#define DNS_FLAGS_RCODE_NO_ERROR        0x0000
#define DNS_FLAGS_RCODE_FORMAT_ERROR    0x0001
#define DNS_FLAGS_RCODE_SERVER_FAILURE  0x0002
#define DNS_FLAGS_RCODE_NAME_ERROR      0x0003
#define DNS_FLAGS_RCODE_NOT_IMPLEMENTED 0x0004
#define DNS_FLAGS_RCODE_REFUSED         0x0005

#define DNS_TYPE_A		1
#define DNS_TYPE_NS		2
#define DNS_TYPE_CNAME	5
#define DNS_TYPE_SOA		6
#define DNS_TYPE_MB		7
#define DNS_TYPE_MG		8
#define DNS_TYPE_MR		9
#define DNS_TYPE_NULL	10
#define DNS_TYPE_WKS		11
#define DNS_TYPE_PTR		12
#define DNS_TYPE_HINFO	13
#define DNS_TYPE_MINFO	14
#define DNS_TYPE_MX		15
#define DNS_TYPE_TXT		16
#define DNS_TYPE_RP		17
#define DNS_TYPE_AFSDB	18
#define DNS_TYPE_X25		19
#define DNS_TYPE_ISDN	20
#define DNS_TYPE_RT		21
#define DNS_TYPE_AAAA	28
#define DNS_TYPE_LOC		29
#define DNS_TYPE_SRV		33
#define DNS_TYPE_AXFR	252
#define DNS_TYPE_MAILB	253
#define DNS_TYPE_MAILA	254
#define DNS_TYPE_ANY		255

#define DNS_CLASS_IN 1
#define DNS_CLASS_CS 2
#define DNS_CLASS_CH 3
#define DNS_CLASS_HS 4

#define DNS_HEADER_SIZE 12

#define DNS_LOG_NONE     0x0000
#define DNS_LOG_STDERR   0x0001
#define DNS_LOG_SYSLOG   0x0002
#define DNS_LOG_FILE     0x0004
#define DNS_LOG_CALLBACK 0x0008

#define DNS_SOCK_UDP 0
#define DNS_SOCK_TCP_UNCONNECTED 1
#define DNS_SOCK_TCP_CONNECTED 2

/*
 * Client handle returned by dns_open and dns_connect
 */
typedef struct
{
	u_int16_t xid;
	char *domain;
	u_int32_t latency_adjust;
	struct sockaddr_in *server;
	u_int32_t *server_latency;
	u_int32_t server_count;
	u_int32_t selected_server;
	char **search;
	u_int32_t search_count;
#ifdef DNS_EXCLUSION
	char **exclude;
	u_int32_t exclude_count;
	u_int32_t exclusive;
#endif
	struct timeval timeout;
	struct timeval server_timeout;	
	u_int32_t server_retries;
	int sock;
	int sockstate;
	int protocol;
	struct sockaddr_in src;
	u_int32_t ias_dots;
	u_int32_t log_dest;
	FILE *log_file;
	int (*log_callback)(int, char *);
	char *log_title;
	u_int32_t *sort_addr;
	u_int32_t *sort_mask;
	u_int32_t sort_count;
} dns_handle_t;

/*
 * DNS query / reply header
 */
typedef struct {
	u_int16_t xid;
	u_int16_t flags;
	u_int16_t qdcount;
	u_int16_t ancount;
	u_int16_t nscount;
	u_int16_t arcount;
} dns_header_t;

typedef struct
{
	char *name;
	u_int16_t type;
	u_int16_t class;
} dns_question_t;


typedef struct
{
	u_int16_t length;
	char *data;
} dns_raw_resource_record_t;

typedef struct
{
	struct in_addr addr;
} dns_address_record_t;

typedef struct
{
	struct in6_addr addr;
} dns_in6_address_record_t;

typedef struct
{
	char *name;
} dns_domain_name_record_t;

typedef struct
{
	char *mname;
	char *rname;
	u_int32_t serial;
	u_int32_t refresh;
	u_int32_t retry;
	u_int32_t expire;
	u_int32_t minimum;
} dns_SOA_record_t;

typedef struct
{
	char *cpu;
	char *os;
} dns_HINFO_record_t;

typedef struct
{
	char *rmailbx;
	char *emailbx;
} dns_MINFO_record_t;

typedef struct
{
	u_int16_t preference;
	char *name;
} dns_MX_record_t;

typedef struct
{
	u_int32_t string_count;
	char **strings;
} dns_TXT_record_t;

typedef struct
{
	struct in_addr addr;
	u_int8_t protocol;
	u_int32_t maplength;
	u_int8_t *map;
} dns_WKS_record_t;

typedef struct
{
	char *mailbox;
	char *txtdname;
} dns_RP_record_t;

typedef struct
{
	u_int32_t subtype;
	char *hostname;
} dns_AFSDB_record_t;

typedef struct
{
	char *psdn_address;
} dns_X25_record_t;

typedef struct
{
	char *isdn_address;
	char *subaddress;
} dns_ISDN_record_t;

typedef struct
{
	u_int16_t preference;
	char * intermediate;
} dns_RT_record_t;

typedef struct
{
	u_int8_t version;
	u_int8_t size;
	u_int8_t horizontal_precision;
	u_int8_t vertical_precision;
	u_int32_t latitude;
	u_int32_t longitude;
	u_int32_t altitude;
} dns_LOC_record_t;

typedef struct
{
	u_int16_t priority;
	u_int16_t weight;
	u_int16_t port;
	char *target;
} dns_SRV_record_t;

typedef struct
{
	char *name;
	u_int16_t type;
	u_int16_t class;
	u_int32_t ttl;
	union
	{
		dns_address_record_t *A;
		dns_in6_address_record_t *AAAA;
		dns_domain_name_record_t *CNAME;
		dns_domain_name_record_t *MB;
		dns_domain_name_record_t *MG;
		dns_domain_name_record_t *MR;
		dns_domain_name_record_t *PTR;
		dns_domain_name_record_t *NS;
		dns_SOA_record_t *SOA;
		dns_WKS_record_t *WKS;
		dns_raw_resource_record_t *DNSNULL;
		dns_HINFO_record_t *HINFO;
		dns_MINFO_record_t *MINFO;
		dns_MX_record_t *MX;
		dns_TXT_record_t *TXT;
		dns_RP_record_t *RP;
		dns_AFSDB_record_t *AFSDB;
		dns_X25_record_t *X25;
		dns_ISDN_record_t *ISDN;
		dns_RT_record_t *RT;
		dns_LOC_record_t *LOC;
		dns_SRV_record_t *SRV;
	} data;
} dns_resource_record_t;

typedef struct
{
	u_int32_t status;
	struct in_addr server;
	dns_header_t *header;
	dns_question_t **question;
	dns_resource_record_t **answer;
	dns_resource_record_t **authority;
	dns_resource_record_t **additional;
} dns_reply_t;

typedef struct
{
	u_int32_t count;
	dns_reply_t **reply;
} dns_reply_list_t;

/*
 * Open a connection to a domain by name
 */
dns_handle_t *dns_open __P((char *));

/*
 * Connect to a server for the given domain name at the specified address
 */
dns_handle_t *dns_connect __P((char *, struct sockaddr_in *));

/*
 * Release global static memory
 */
void dns_shutdown __P((void));

/*
 * Add a new server address to a handle
 */
void dns_add_server __P((dns_handle_t *, struct sockaddr_in *));

/*
 * Remove the server address at the given index from a handle
 */
void dns_remove_server __P((dns_handle_t *, u_int32_t));

/*
 * Close a connection and free its resources
 */
void dns_free __P((dns_handle_t *));

/*
 * Free a resource record structure
 */
void dns_free_resource_record __P((dns_resource_record_t *));

/*
 * Free a reply structure
 */
void dns_free_reply __P((dns_reply_t *));

/*
 * Free a reply list
 */
void dns_free_reply_list __P((dns_reply_list_t *));

/*
 * Set the xid for the next query
 */
void dns_set_xid __P((dns_handle_t *, u_int32_t));

/*
 * Set the connection protocol to IPPROTO_UDP or IPPROTO_TCP
 */
void dns_set_protocol __P((dns_handle_t *, u_int32_t));

/*
 * Select a specific server which will be tried first 
 */
void dns_select_server __P((dns_handle_t *, u_int32_t));

/*
 * Set the total timeout 
 */
void dns_set_timeout __P((dns_handle_t *, struct timeval *));

/*
 * Set the per-server timeout 
 */
void dns_set_server_timeout __P((dns_handle_t *, struct timeval *));

/*
 * Set the per-server number of retries 
 */
void dns_set_server_retries __P((dns_handle_t *, u_int32_t));

/*
 * Parses a reply packet into a reply structure 
 */
dns_reply_t *dns_parse_packet __P((char *));

/*
 * Builds a reply packet from a reply structure
 */
char *dns_build_reply __P((dns_reply_t *, u_int16_t *));

/*
 * Parses a query packet into a question structure 
 */
dns_question_t *dns_parse_question __P((char *, char **));

/*
 * Parses a resource record into a structure 
 */
dns_resource_record_t *dns_parse_resource_record __P((char *, char **));

/*
 * Builds a query packet from a question structure 
 */
char *dns_build_query __P((dns_handle_t *, dns_question_t *, u_int32_t *));

/*
 * Resolve a query
 *
 * This is the preferred API to use for most queries.  It appends
 * the local domain name to the query if necessary.  It tries all the
 * servers in turn.  It will switch from UDP to TCP if the reply is
 * truncated.
 */
dns_reply_t *dns_query __P((dns_handle_t *, dns_question_t *));

/*
 * Send a question to the server with the given index.
 *
 * dns_query() calls this routine with a -1 index, meaning that all servers
 * should be tried in turn.
 */
dns_reply_t *dns_query_server __P((dns_handle_t *, u_int32_t, dns_question_t *));

/*
 * Resolve a fully-qualified query
 *
 * Use this routine only if the query is already fully qualified.
 * It tries all the servers in turn, and will switch from UDP to TCP
 * if the reply is truncated.
 */
dns_reply_t *dns_fqdn_query __P((dns_handle_t *, dns_question_t *));

/*
 * Resolve a fully-qualified query on a specific server.
 *
 * dns_fqdn_query() calls this routine with a -1 index, meaning that
 * all servers should be tried in turn.
 */
dns_reply_t *dns_fqdn_query_server __P((dns_handle_t *, u_int32_t, dns_question_t *));

/*
 * Zone transfer - fetch all records of the specified class
 */
dns_reply_list_t * dns_zone_transfer(dns_handle_t *, u_int16_t);

/*
 * Log utilities
 */
void dns_open_log __P((dns_handle_t *, char *, int, FILE *, int, int, int (*)(int, char *)));
void dns_close_log __P((dns_handle_t *));
void dns_log __P((dns_handle_t *, int, char *));

#endif __DNS_CLIENT_H__
