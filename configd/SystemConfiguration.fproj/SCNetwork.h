/*
 * Copyright (c) 2000 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 *
 * The contents of this file constitute Original Code as defined in and
 * are subject to the Apple Public Source License Version 1.1 (the
 * "License").  You may not use this file except in compliance with the
 * License.  Please obtain a copy of the License at
 * http://www.apple.com/publicsource and read it before using this file.
 *
 * This Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License.
 *
 * @APPLE_LICENSE_HEADER_END@
 */

#ifndef _SCPNETWORK_H
#define _SCPNETWORK_H

#include <sys/cdefs.h>
#include <sys/socket.h>

/*!
	@header SCPNetwork.h
	The SystemConfiguration framework provides access to the data used
		to configure a running system.

	Specifically, the SCPNetworkXXX() API's allow an application
		to determine the status of the systems current network
		configuration.

	The APIs provided by this framework communicate with the "configd"
		daemon to obtain information regarding the systems current
		configuration.
 */

/*!
	@enum SCNStatus
	@discussion Returned status codes from the SCNIsReachableByAddress()
		and SCNIsReachableByName() functions.

		The term "reachable" in these status codes reflects whether
		a data packet, sent by an application into the network stack,
		will be able to reach the destination host.

		Please not that being "able" to reach the destination host
		does not guarantee that the data packet "will" reach the
		host.

	@constant SCN_REACHABLE_UNKNOWN
		A determination could not be made regarding the reachability
		of the specified nodename/address.

	@constant SCN_REACHABLE_NO
		The specified nodename/address can not be reached using the
		current network configuration.

	@constant SCN_REACHABLE_CONNECTION_REQUIRED
		The specified nodename/address can be reached using the
		current network configuration but a connection must first
		be established.

		This status would be returned for a dialup connection which
		was not currently active but could handle network traffic for
		the target system.

	@constant SCN_REACHABLE_YES
		The specified nodename/address can be reached using the
		current network configuration.
 */
typedef enum {
	SCN_REACHABLE_UNKNOWN			= -1,
	SCN_REACHABLE_NO			=  0,
	SCN_REACHABLE_CONNECTION_REQUIRED	=  1,
	SCN_REACHABLE_YES			=  2,
} SCNStatus;


/*!
	@enum SCNConnectionFlags
	@discussion Additional flags which reflect whether a network connection
		to the specified nodename/address is reachable, requires a
		connection, requires some user intervention in establishing
		the connection, and whether the calling application must initiate
		the connection using the SCNEstablishConnection() API.

	@constant kSCNFlagsTransientConnection
		This flag indicates that the specified nodename/address can
		be reached via a transient (e.g. PPP) connection.

	@constant kSCNFlagsConnectionAutomatic
		The specified nodename/address can be reached using the
		current network configuration but a connection must first
		be established.  Any traffic directed to the specified
		name/address will initiate the connection.

	@constant kSCNFlagsInterventionRequired
		The specified nodename/address can be reached using the
		current network configuration but a connection must first
		be established.  In addition, some form of user intervention
		will be required to establish this connection (e.g. providing
		a password, authentication token, etc.).
 */
typedef enum {
	kSCNFlagsTransientConnection	=  1<<0,
	kSCNFlagsConnectionAutomatic	=  1<<1,
	kSCNFlagsInterventionRequired	=  1<<2,
} SCNConnectionFlags;


__BEGIN_DECLS

/*!
	@function SCNIsReachableByAddress
	@discussion Determines if the given network address is
		reachable using the current network configuration.

		Note: This API is not thread safe.
	@param address Pass the network address of the desired host.
	@param addrlen Pass the length, in bytes, of the address.
	@param flags A pointer to memory which will be filled with a
		set of SCNConnectionFlags related to the reachability
		of the specified address.  If NULL, no flags will be
		returned.
	@param status A pointer to memory which will be filled with the
		error status associated with any error communicating with
		the system configuration daemon.
	@result A constant of type SCNStatus indicating the reachability
		of the specified node address.
 */
SCNStatus	SCNIsReachableByAddress	(const struct sockaddr	*address,
					 const int		addrlen,
					 int			*flags,
					 const char		**errorMessage);

/*!
	@function SCNIsReachableByName
	@discussion Determines if the given network host/node name is
		reachable using the current network configuration.
	@param nodename Pass a node name of the desired host. This name would
		be the same as that passed to gethostbyname() or getaddrinfo().
	@param flags A pointer to memory which will be filled with a
		set of SCNConnectionFlags related to the reachability
		of the specified address.  If NULL, no flags will be
		returned.
	@param status A pointer to memory which will be filled with the
		error status associated with any error communicating with
		the system configuration daemon.
	@result A constant of type SCNStatus indicating the reachability
		of the specified node address.
 */
SCNStatus	SCNIsReachableByName	(const char		*nodename,
					 int			*flags,
					 const char		**errorMessage);

__END_DECLS

#endif /* _SCPNETWORK_H */
