/*
 * (c) Copyright 1992 by Panagiotis Tsirigotis
 * (c) Sections Copyright 1998-2001 by Rob Braun
 * All rights reserved.  The file named COPYRIGHT specifies the terms 
 * and conditions for redistribution.
 */

#ifndef ATTR_H
#define ATTR_H

/*
 * $Id: attr.h,v 1.4 2003/08/06 01:15:35 rbraun Exp $
 */

/*
 * Attribute IDs
 */
#define A_NONE              0
#define A_WAIT              1
#define A_SOCKET_TYPE       2
#define A_PROTOCOL          3
#define A_USER              4
#define A_GROUP             5
#define A_SERVER            6
#define A_SERVER_ARGS       7
#define A_INSTANCES         8
#define A_ID                9
#define A_ONLY_FROM        10
#define A_ACCESS_TIMES     11
#define A_RPC_VERSION      12
#define A_LOG_TYPE         13
#define A_NO_ACCESS        14
#define A_TYPE             15
#define A_LOG_ON_FAILURE   16
#define A_LOG_ON_SUCCESS   17
#define A_ENV              18
#define A_PORT             19
#define A_PASSENV          20
#define A_FLAGS            21
#define A_RPC_NUMBER       22
#define A_NICE             23
#define A_REDIR            24
#define A_BIND             25
#define A_BANNER           26
#define A_PER_SOURCE       27
#define A_GROUPS           28
#define A_BANNER_SUCCESS   29
#define A_BANNER_FAIL      30
#define A_MAX_LOAD         31
#define A_CPS              32
#define A_SVCDISABLE       33
#define A_RLIMIT_AS        34
#define A_RLIMIT_CPU       35
#define A_RLIMIT_DATA      36
#define A_RLIMIT_RSS       37
#define A_RLIMIT_STACK     38
#define A_V6ONLY           39
#define A_DENY_TIME        40
#define A_UMASK            41
#define A_ENABLED          42
#define A_DISABLED         43
#define A_MDNS             44
#define A_SESSIONCREATE    45

/*
 * SERVICE_ATTRIBUTES is the number of service attributes and also
 * the number from which defaults-only attributes start.
 */
#define SERVICE_ATTRIBUTES      ( A_SESSIONCREATE + 1 )

/*
 * Mask of attributes that must be specified.
 */
#define NECESSARY_ATTRS         ( XMASK( A_SOCKET_TYPE ) + XMASK( A_WAIT ) )
#define NECESSARY_ATTRS_EXTERNAL     ( XMASK( A_SERVER ) + XMASK( A_USER ) )
#define NECESSARY_ATTRS_UNLISTED     ( XMASK( A_PROTOCOL ) + XMASK( A_PORT ) )
#define NECESSARY_ATTRS_UNLISTED_MUX ( XMASK( A_PROTOCOL ) )
#define NECESSARY_ATTRS_RPC          ( XMASK( A_PROTOCOL ) + \
                                       XMASK( A_RPC_VERSION ) )
#define NECESSARY_ATTRS_RPC_UNLISTED XMASK( A_RPC_NUMBER )

#endif   /* ATTR_H */
