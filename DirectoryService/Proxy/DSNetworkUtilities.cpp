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

#include <string.h>
#include <ctype.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/utsname.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/errno.h>

#include <errno.h>
#include <netinet/in.h>
#include <net/if.h>		// interface struture ifreq, ifconf
#include <net/if_dl.h>	// datalink structs
#include <net/if_types.h>

#include <arpa/inet.h>	// for inet_*
#include <arpa/nameser.h>
#include <resolv.h>
#include <string.h>
#include <stdlib.h>

#include "DSNetworkUtilities.h"
#ifdef DSSERVERTCP
#include "CLog.h"
#endif

#define IFR_NEXT(ifr)   \
    ((struct ifreq *) ((char *) (ifr) + sizeof(*(ifr)) + \
      MAX(0, (int) (ifr)->ifr_addr.sa_len - (int) sizeof((ifr)->ifr_addr))))

// ------------------------------------------------------------------------
// Static class variables
// ------------------------------------------------------------------------
Boolean				DSNetworkUtilities::sNetworkInitialized	= false;
Boolean				DSNetworkUtilities::sAppleTalkAvailable	= false;
Boolean				DSNetworkUtilities::sTCPAvailable		= false;

short				DSNetworkUtilities::sIPAddrCount			= 0;	// count of IP addresses for this server
short				DSNetworkUtilities::sAliasCount			= 0;

InetDomainName		DSNetworkUtilities::sLocalNodeName		= "\0";

InetDomainName		DSNetworkUtilities::sLocalHostName		= "\0";
InetHost			DSNetworkUtilities::sLocalHostIPAddr		= 0;

MultiHomeIPInfo		DSNetworkUtilities::sIPInfo[ kMaxIPAddrs ];
IPAddressInfo		DSNetworkUtilities::sAddrList[ kMaxIPAddrs ];
InetDomainName		DSNetworkUtilities::sAliasList[ kMaxIPAddrs ];

DSMutexSemaphore	   *DSNetworkUtilities::sNetSemaphore		= NULL;

// ------------------------------------------------------------------------
//	* Initialize ()
// ------------------------------------------------------------------------

OSStatus DSNetworkUtilities::Initialize ( void )
{
	register int rc = 0;
	struct utsname	myname;

	if (sNetworkInitialized == true)
	{
		return eDSNoErr;
	}

	sNetSemaphore = new DSMutexSemaphore();
	::memset( &sLocalHostName, 0, sizeof( sLocalHostName ) );
	::memset( &sIPInfo, 0, sizeof(sIPInfo) );
	::memset( &sAddrList, 0, sizeof(sAddrList) );
	::memset( &sAliasList, 0, sizeof(sAliasList) );

	// fill in our local node name
    if ( ::uname(&myname) == 0 )
	{
		::strncpy( sLocalNodeName, myname.nodename, sizeof( sLocalNodeName ) );
	}
	else
	{
        ::strcpy( sLocalNodeName, "localhost" );
	}

	try
	{
#ifdef DSSERVERTCP
		SRVRLOG( kLogApplication, "Initializing TCP ..." );
#endif
		rc = InitializeTCP();
		if ( rc != 0 )
		{
#ifdef DSSERVERTCP
			ERRORLOG1( kLogApplication, "*** Warning*** TCP is not available.  Error: %d", rc );
			DBGLOG( kLogTCPEndpoint, "DSNetworkUtilities::Initialize(): TCP not available." );
#else
			LOG1( kStdErr, "*** Warning*** DSNetworkUtilities::Initialize(): TCP is not available.  Error: %d", rc );
#endif
			throw( (sInt32)rc );
		}

	}

	catch( sInt32 err )
	{
		sNetworkInitialized = false;
#ifdef DSSERVERTCP
		DBGLOG1( kLogTCPEndpoint, "DSNetworkUtilities::Initialize failed.  Error: %d", err );
#else
		LOG1( kStdErr, "DSNetworkUtilities::Initialize failed.  Error: %d", err );
#endif
		return err;
	}

	sNetworkInitialized = true;
#ifdef DSSERVERTCP
	DBGLOG( kLogTCPEndpoint, "DSNetworkUtilities::Initialized." );
#endif

	return( eDSNoErr );

} // Initialize


// ------------------------------------------------------------------------
//	* ResolveToIPAddress ()
// ------------------------------------------------------------------------

OSStatus DSNetworkUtilities::ResolveToIPAddress ( const InetDomainName inDomainName, InetHost* outInetHost )
{
	register struct hostent *hp = NULL;

	DSNetworkUtilities::Wait();

	try
	{
		if ( inDomainName[0] != '\0' && outInetHost != NULL)
		{
			hp = ::gethostbyname( inDomainName );
			if ( hp == NULL )
			{
				throw( (sInt32)h_errno );
			}
			*outInetHost = ntohl( ((struct in_addr *)(hp->h_addr_list[0]))->s_addr );

			DSNetworkUtilities::Signal();
			return( eDSNoErr );
		}
	} // try

	catch( sInt32 err )
	{
#ifdef DSSERVERTCP
		ERRORLOG1( kLogTCPEndpoint, "Unable to resolve the IP address for %s.", inDomainName );
#else
		LOG1( kStdErr, "Unable to resolve the IP address for %s.", inDomainName );
#endif
		DSNetworkUtilities::Signal();
		return( err );
	}

    return( eDSNoErr );

} // ResolveToIPAddress


// ------------------------------------------------------------------------
//
// ------------------------------------------------------------------------

InetHost DSNetworkUtilities::GetOurIPAddress ( short inIndex )
{
	if ( sNetworkInitialized == false )
	{
		if ( Initialize() != eDSNoErr )
		{
			return 0;
		}
	}

	if (inIndex  < sIPAddrCount && inIndex < kMaxIPAddrs)
	{
		return( sIPInfo[ inIndex ].IPAddress );
	}

	return 0;

} // GetOurIPAddress


// ------------------------------------------------------------------------
//
// ------------------------------------------------------------------------
const char *
DSNetworkUtilities::GetOurIPAddressString (short inIndex)
{
	if (inIndex < sIPAddrCount && inIndex < kMaxIPAddrs)
		return( sIPInfo[ inIndex ].IPAddressString );

	return NULL;
}


// ------------------------------------------------------------------------
//	* GetOurIPAddressString2 ()
// ------------------------------------------------------------------------

void DSNetworkUtilities::GetOurIPAddressString2 ( short inIndex, char *ioBuffer, int inBufferSize )
{
	if ( ioBuffer != NULL && inIndex < sIPAddrCount && inIndex < kMaxIPAddrs )
	{
		::strncpy(ioBuffer, sIPInfo[inIndex].IPAddressString, inBufferSize);
	}

} // GetOurIPAddressString2


// ------------------------------------------------------------------------
//	* DoesIPAddrMatch ()
//
//		- Does this IP address match one of my addresses
// ------------------------------------------------------------------------

Boolean DSNetworkUtilities::DoesIPAddrMatch ( InetHost inIPAddr )
{
	if ((inIPAddr != 0x00000000) && (inIPAddr != 0xFFFFFFFF) )
	{
		for ( int i=0; i < sIPAddrCount && i < kMaxIPAddrs; i++ )
		{
			if (inIPAddr == sIPInfo[i].IPAddress)
				return true;
		}
	}
	return false;
}


//--------------------------------------------------------------------------------------------------
//	IsValidAddressString
//
//		- Check if the string passed in is a valid IP address string. Returns true and converts
//		 it to InetHost(IP address) or retunrs false if not a valid address string.
//
//--------------------------------------------------------------------------------------------
Boolean
DSNetworkUtilities::IsValidAddressString (const char *inString, InetHost *outInetHost)
{
	register const char	*cp;
	struct in_addr	hostaddr;

	if (inString == NULL)
		return false;

	/* Make sure it has only digits and dots. */
	for (cp = inString; *cp; ++cp) {
		if ((isdigit(*cp) !=0) && *cp != '.') {
			return false;
		}
	}

	/* If it has a trailing dot, don't treat it as an address. */
	if (*--cp != '.') {
		if (::inet_aton(inString, &hostaddr) == 1) {
			*outInetHost = ntohl(hostaddr.s_addr);
			return true;
		}
	}

	return false;
}


//--------------------------------------------------------------------------------------------------
//	GetIPAddressByName
//
//		- lookup the IP address of a given hostname
//
//--------------------------------------------------------------------------------------------
InetHost
DSNetworkUtilities::GetIPAddressByName(const char *inName)
{
	register struct hostent	*hp = NULL;
	register InetHost	IPAddr = 0;

	DSNetworkUtilities::Wait();

	try {

		hp = ::gethostbyname(inName);
		if (hp == NULL) {
			throw((sInt32)h_errno);
		}

		IPAddr = ntohl(((struct in_addr *)hp->h_addr)->s_addr);
		DSNetworkUtilities::Signal();
		return IPAddr;
	}

	catch( sInt32 err )
	{
#ifdef DSSERVERTCP
		ERRORLOG2( kLogTCPEndpoint, "GetIPAddressByName for %s failed: %d.", inName, err );
#else
		LOG2( kStdErr, "GetIPAddressByName for %s failed: %d.", inName, err );
#endif
		DSNetworkUtilities::Signal();
		return IPAddr;
	}

}

//--------------------------------------------------------------------------
//	IPAddrToString
//
//	Takes a IP address convert it to the string format with dots
//	address passed in should be in host byte order
//---------------------------------------------------------------------------
int
DSNetworkUtilities::IPAddrToString(const InetHost inAddr, char *ioNameBuffer, const int inBufferSize)
{
	char	*result;
	struct in_addr	InetAddr;

	DSNetworkUtilities::Wait();

	InetAddr.s_addr = htonl(inAddr);
	result = ::inet_ntoa(InetAddr);
	if ( result != NULL )
		::strncpy(ioNameBuffer, result, inBufferSize);
	else {
#ifdef DSSERVERTCP
		ERRORLOG1( kLogTCPEndpoint, "IPAddrToString for %u failed.", inAddr );
#else
		LOG1( kStdErr, "IPAddrToString for %u failed.", inAddr );
#endif
		ioNameBuffer[0] = '\0';
		DSNetworkUtilities::Signal();
		return -1;
	}

	DSNetworkUtilities::Signal();
	return 0;
}


//-------------------------------------------------------------------------------
//	StringToIPAddr
//
//	Takes a IP address string (with dots) and convert it to a real IP address
//	Address returned is in host byte order
//--------------------------------------------------------------------------------
int
DSNetworkUtilities::StringToIPAddr(const char *inAddrStr, InetHost *ioIPAddr)
{
	struct in_addr	InetAddr;
	int		rc;

	if ( inAddrStr  && ioIPAddr ) {
		DSNetworkUtilities::Wait();

		rc = ::inet_aton(inAddrStr, &InetAddr);
		if ( rc == 1 ) {
			*ioIPAddr = ntohl(InetAddr.s_addr);
			DSNetworkUtilities::Signal();
			return 0;
		}

#ifdef DSSERVERTCP
		ERRORLOG1( kLogTCPEndpoint, "StringToIPAddr() failed for %s", inAddrStr );
#else
		LOG1( kStdErr, "StringToIPAddr() failed for %s", inAddrStr );
#endif
		DSNetworkUtilities::Signal();
	}
	return -1;
}


// ------------------------------------------------------------------------
// proctected member functions
// ------------------------------------------------------------------------

// ------------------------------------------------------------------------
//	* InitializeTCP ()
//
// ------------------------------------------------------------------------

int DSNetworkUtilities::InitializeTCP ( void )
{
	// interface structures
	struct ifconf	ifc;
	struct ifreq	ifrbuf[30];
	register struct ifreq	*ifrptr;
	register struct sockaddr_in *sain;
	register int	sock = 0;
	register int	i = 0;
	register int	ipcount = 0;
	int rc = 0;
	int err = 0;


	sTCPAvailable = false;

	DSNetworkUtilities::Wait();

	try {
		sock = socket(AF_INET, SOCK_DGRAM, 0);
		if ( sock == -1 )
		{
			err = errno;
#ifdef DSSERVERTCP
			ERRORLOG1( kLogTCPEndpoint, "SOCKET: %d.", err );
#else
			LOG1( kStdErr, "SOCKET: %d.", err );
#endif
			throw((sInt32)err);
		}

		ifc.ifc_buf = (caddr_t)ifrbuf;
		ifc.ifc_len = sizeof(ifrbuf);
		rc = ::ioctl(sock, SIOCGIFCONF, &ifc);
		if ( rc == -1 )
		{
			err = errno;
#ifdef DSSERVERTCP
			ERRORLOG1( kLogTCPEndpoint, "ioctl:SIOCGIFCONF: %d.", err );
#else
			LOG1( kStdErr, "ioctl:SIOCGIFCONF: %d.", err );
#endif
			throw((sInt32)err);
		}

		// walk the interface and  address list, only interested in ethernet and AF_INET
		ipcount = 0;
		for ( ifrptr = (struct ifreq *)ifc.ifc_buf, i=0;
				(char *) ifrptr < &ifc.ifc_buf[ifc.ifc_len] && i < kMaxIPAddrs;
				ifrptr = IFR_NEXT(ifrptr), i++ )
		{
			if ( (*ifrptr->ifr_name != '\0') && (ifrptr->ifr_addr.sa_family == AF_INET) )
			{
				if ( *ifrptr->ifr_name == 'e' )
				{
					// ethernet interface
					sain = (struct sockaddr_in *)&(ifrptr->ifr_addr);
					sIPInfo[ipcount].IPAddress = ntohl(sain->sin_addr.s_addr);
					IPAddrToString(sIPInfo[ipcount].IPAddress, sIPInfo[ipcount].IPAddressString, MAXIPADDRSTRLEN);
					ipcount ++;
				}
				else if (*ifrptr->ifr_name == 'l')
				{
					// localhost "lo0"
					sain = (struct sockaddr_in *)&(ifrptr->ifr_addr);
					sLocalHostIPAddr = ntohl(sain->sin_addr.s_addr);
					::strcpy(sLocalHostName, "localhost");
				}
			}
		}

		sIPAddrCount = ipcount;
		sTCPAvailable = true;
		DSNetworkUtilities::Signal();

	} // try

	catch( sInt32 someError )
	{
		DSNetworkUtilities::Signal();
#ifdef DSSERVERTCP
		ERRORLOG( kLogTCPEndpoint, "DSNetworkUtilities::InitializeTCP failed." );
#else
		LOG( kStdErr, "DSNetworkUtilities::InitializeTCP failed." );
#endif
		sTCPAvailable = false;
		return someError;
	}

	return eDSNoErr;

} // InitializeTCP


void DSNetworkUtilities::Signal ( void )
{
	if ( sNetSemaphore != nil )
	{
		sNetSemaphore->Signal();
	}
	else
	{
#ifdef DSSERVERTCP
		DBGLOG( kLogApplication,"DSNetworkUtilities::Signal -- sNetSemaphore is NULL" );
#else
		LOG( kStdErr,"DSNetworkUtilities::Signal -- sNetSemaphore is NULL" );
#endif
	}
}

long DSNetworkUtilities::Wait ( sInt32 milliSecs )
{
	if ( sNetSemaphore != nil )
	{
		return sNetSemaphore->Wait(milliSecs);
	}
	else
	{
#ifdef DSSERVERTCP
		DBGLOG( kLogApplication,"DSNetworkUtilities::Wait -- sNetSemaphore is NULL" );
#else
		LOG( kStdErr,"DSNetworkUtilities::Wait -- sNetSemaphore is NULL" );
#endif
		return (long)DSSemaphore::semOtherErr;
	}
}

