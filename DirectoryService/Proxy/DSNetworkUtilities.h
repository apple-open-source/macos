/*
 * Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
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

/*!
 * @header DSNetworkUtilities
 */

#ifndef __DSNetworkUtilities_h__
#define __DSNetworkUtilities_h__ 1

#include <sys/param.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/nameser.h>	// DNS 
#include <resolv.h>			// resolver global state

#include "DSMutexSemaphore.h"

#define	 MAXIPADDRSTRLEN	 32

// --------------------------------------------------------------------------------------------
//	* Typedefs and enums
// --------------------------------------------------------------------------------------------

// IP address string  --  Need to change this for IPV6 -- MED --
typedef char		IPAddrStr[ MAXIPADDRSTRLEN ]; // define a type for "ddd.ddd.ddd.ddd\0"

enum { kMaxHostNameLength = 255 };

typedef	 u_short	InetPort;
typedef	 u_long		InetHost;
typedef	 char		InetDomainName[ kMaxHostNameLength + 1 ];

typedef	 struct MSInetHostInfo
{
	InetDomainName	name;
	InetHost		addrs[ 32 ];
} MSInetHostInfo;

typedef struct MultiHomeIPInfo {
	InetHost		IPAddress;				// our IP address
	IPAddrStr		IPAddressString;		// ASCII IP address string ddd.ddd.ddd.ddd\0
	InetDomainName	DNSName;				// our host name	
} MultiHomeIPInfo;

	
typedef struct IPAddressInfo {
	InetHost	addr;
	char		addrStr[MAXIPADDRSTRLEN];
} IPAddressInfo;


// --------------------------------------------------------------------------------------------
//	* Constants
// --------------------------------------------------------------------------------------------

const	short	kPrimaryIPAddr	= 0;
const	short	kMaxIPAddrs		= 32;

const	OSStatus kMAPRBLKnownSpammerResult = -10101;
const	OSStatus kNoValidIPAddress4ThisName = -10102;

class DSNetworkUtilities 
{
public:
	enum {	kDNSQuerySuccess = 0,
			kDNSQueryFailure,
			kDNSNotAvailable,
			kTCPNotAvailable
	};
	
	static OSStatus		Initialize				( void );
	static void			DeInitialize			( void );

	// local host information
	static Boolean		IsTCPAvailable			( void ) { return sTCPAvailable; }
	static Boolean		IsAppleTalkAvailable	( void ) { return sAppleTalkAvailable; }

	static const char *	GetOurNodeName			( void ) { return sLocalNodeName; }
	static const char *	GetLocalHostName		( void ) { return sLocalHostName; }
	static InetHost		GetLocalHostIPAddress	( void ) { return sLocalHostIPAddr; }
	static int			GetOurIPAddressCount	( void ) { return sIPAddrCount; }
	static InetHost		GetOurIPAddress 		( short inIndex = kPrimaryIPAddr );
	static const char *	GetOurIPAddressString	( short inIndex = kPrimaryIPAddr );
	static void			GetOurIPAddressString2	( short inIndex, char *ioBuffer, int inBufferSize );

	static Boolean		DoesIPAddrMatch			( InetHost inIPAddr );

	// generic network routines
	static Boolean		IsValidAddressString	( const char *inAddrStr, InetHost *outInetHost );
	static InetHost		GetIPAddressByName		( const char *inName );	
	static int			IPAddrToString			( const InetHost inAddr, char *ioNameBuffer, const int inBufferSize);
	static int			StringToIPAddr			( const char *inAddrStr, InetHost *ioIPAddr );

	static OSStatus		ResolveToIPAddress 		( const InetDomainName inDomainName, InetHost* outInetHost );

protected:
	static int				InitializeTCP		( void );
	static void		 		Signal				( void );
	static long				Wait				( sInt32 milliSecs = DSSemaphore::kForever );


	// Data members

	static Boolean			sNetworkInitialized;
	static Boolean			sAppleTalkAvailable;
	static Boolean			sTCPAvailable;

	static short			sIPAddrCount;		// count of IP addresses for this server
	static short			sAliasCount;

	static InetDomainName	sLocalNodeName;		// our system node name

	static InetDomainName	sLocalHostName;		// our "localhost" loopback name and address
	static InetHost			sLocalHostIPAddr;	// 127.0.0.1

	static MultiHomeIPInfo	sIPInfo[ kMaxIPAddrs ];
	static IPAddressInfo	sAddrList[ kMaxIPAddrs ];
	static InetDomainName	sAliasList[ kMaxIPAddrs ];

	static DSMutexSemaphore	*sNetSemaphore;
			
}; // class DSNetworkUtilities


#endif // __DSNetworkUtilities_h__
