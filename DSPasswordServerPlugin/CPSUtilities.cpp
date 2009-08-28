/*
 * Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
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

/*!
 * @header CPSUtilities
 */

/*
 *  CPSUtilities.cpp
 *  PasswordServerPlugin
 *
 */

#include <sys/poll.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <ifaddrs.h>
#include <fcntl.h>
#include <netdb.h>
#include <sys/stat.h>
#include <sys/sysctl.h>				// for struct kinfo_proc and sysctl()
#include <syslog.h>
#include <dirent.h>

#include <ctype.h>
#include <sys/utsname.h>

#include <unistd.h>
#include <stdlib.h>
#include <sys/un.h>
#include <sys/event.h>

#include "CPSUtilities.h"
#include "SASLCode.h"
#include "CAuthFileBase.h"

#define kMinTrialTime		140000
#define kMaxTrialTime		1250000
#define kMaxIPAddrs			32

#if 1
static bool gDSDebuggingON = false;
#define DEBUGLOG(A,args...)		if (gDSDebuggingON) syslog( LOG_ALERT, (A), ##args )
#else
#define DEBUGLOG(A,args...)		
#endif

#define IFR_NEXT(ifr)   \
    ((struct ifreq *) ((char *) (ifr) + sizeof(*(ifr)) + \
      MAX(0, (int) (ifr)->ifr_addr.sa_len - (int) sizeof((ifr)->ifr_addr))))


long gOpenCount = 0;

void psfwSetUSR1Debug( bool on )
{
	gDSDebuggingON = on;
}


void writeToServer( FILE *out, char *buf )
{
    DEBUGLOG( "sending: %s", buf);
    
    if ( buf != NULL && out != NULL )
    {
        fwrite(buf, strlen(buf), 1, out);
        fflush(out);
    }
}


PWServerError readFromServer( int fd, char *buf, unsigned long bufLen )
{
    PWServerError result;
	unsigned long byteCount;
	
	result = readFromServerGetData( fd, buf, bufLen, &byteCount );
	if ( result.err == 0 )
		result = readFromServerGetLine( fd, buf, bufLen, true, &byteCount );
	if ( result.err == 0 )
		result = readFromServerGetErrorCode( buf );
	
    DEBUGLOG( "received: %s", buf );
    
    return result;
}


void writeToServerWithCASTKey( FILE *out, char *buf, CAST_KEY *inKey, unsigned char *inOutIV )
{
	unsigned char *ebuf;
	long bufLen;
	
	if ( out == NULL || buf == NULL || inKey == NULL || inOutIV == NULL )
		return;
	
	DEBUGLOG( "encrypting and sending: %s", buf );
    
	bufLen = strlen( buf );
	
	// pad enough zeros to cover the last block
	bzero( buf + bufLen, CAST_BLOCK );
	
	ebuf = (unsigned char *) malloc( bufLen + CAST_BLOCK + 1 );
	if ( ebuf == NULL )
		return;
	
	// round up to a complete block
	if (bufLen % CAST_BLOCK)
		bufLen += CAST_BLOCK - (bufLen % CAST_BLOCK);
	
	CAST_cbc_encrypt( (unsigned char *)buf, ebuf, bufLen, inKey, inOutIV, CAST_ENCRYPT );
	
	ebuf[bufLen] = '\0';
	DEBUGLOG( "len = %d, encrypted: %s", bufLen, ebuf );
    
	fwrite( ebuf, bufLen, 1, out );
	fflush( out );
	
	free( ebuf );
}


PWServerError readFromServerWithCASTKey( int fd, char *buf, unsigned long bufLen, CAST_KEY *inKey, unsigned char *inOutIV )
{
    PWServerError result;
	unsigned char *dbuf;
	unsigned long byteCount = 0;
	
	result = readFromServerGetData( fd, buf, bufLen, &byteCount );
DEBUGLOG( "byteCount1=%d (bufLen=%d)", byteCount, bufLen );
	if ( result.err != 0 || byteCount == 0 )
		return result;
	
	dbuf = (unsigned char *) malloc( byteCount + CAST_BLOCK + 1 );
	if ( dbuf == NULL )
	{
		result.err = -1;
		result.type = kConnectionError;
	}
	
	// if (byteCount % CAST_BLOCK) something_is_wrong();
	CAST_cbc_encrypt( (unsigned char *)buf, dbuf, byteCount, inKey, inOutIV, CAST_DECRYPT );
	memcpy( buf, dbuf, byteCount );
	result = readFromServerGetLine( fd, buf, bufLen, false, &byteCount );
DEBUGLOG( "byteCount2=%d", byteCount );
	if ( result.err == 0 )
	{
		result = readFromServerGetErrorCode( (char *)buf );		
		DEBUGLOG( "decrypted and received: %s", buf );
	}
	
	free( dbuf );
    
    return result;
}


// ---------------------------------------------------------------------------
//	readFromServerGetData
//
//	Returns: PWServerError
//
//	IMPORTANT: This function does not consume data from the TCP stack. It
//				uses MSG_PEEK to get the data and discover the available
//				length. The readFromServerGetLine() function consumes the
//				data.
// ---------------------------------------------------------------------------

PWServerError readFromServerGetData( int fd, char *buf, unsigned long bufLen, unsigned long *outByteCount )
{
    char readChar = '\0';
    PWServerError result = {0, kPolicyError};
	ssize_t byteCount = 0;
	
	if ( buf == NULL || bufLen < 3 ) {
        result.err = -1;
        return result;
    }
    
	if ( outByteCount != NULL )
		*outByteCount = 0;
	
	buf[0] = '\0';
	
	// wait for the first character to arrive
	byteCount = ::recvfrom( fd, &readChar, sizeof(readChar), (MSG_WAITALL | MSG_PEEK), NULL, NULL );
	if ( byteCount == 0 || byteCount == -1 )
	{
		result.err = -1;
		result.type = kConnectionError;
		return result;
	}
	
	// peek the buffer to get the length
	byteCount = ::recvfrom( fd, buf, bufLen - 1, (MSG_DONTWAIT | MSG_PEEK), NULL, NULL );
	DEBUGLOG( "byteCount (peek): %d", (int)byteCount);
	
	if ( outByteCount != NULL )
		*outByteCount = byteCount;
	
    return result;
}


PWServerError readFromServerGetLine( int fd, char *buf, unsigned long bufLen, bool inCanReadMore, unsigned long *inOutByteCount )
{
    char readChar = '\0';
    char *tstr = NULL;
	char *consumeBuf;
    PWServerError result = {0, kPolicyError};
	ssize_t byteCount = *inOutByteCount;
	ssize_t consumeLen;
	char stackConsumeBuf[2048];
	
	if ( buf == NULL || bufLen < 3 ) {
        result.err = -1;
        return result;
    }
    
	// pull to EOL or available data
	if ( byteCount >= 2 )
	{
		if ( inCanReadMore )
		{
			buf[byteCount] = '\0';
			tstr = strstr( buf, "\r\n" );
		}
		consumeLen = (tstr != NULL) ? (tstr - buf + 2) : byteCount;
		if ( consumeLen < (ssize_t)sizeof(stackConsumeBuf) )
			consumeBuf = stackConsumeBuf;
		else {
			consumeBuf = (char *) malloc( consumeLen );
			if ( consumeBuf == NULL ) {
				result.err = -1;
				return result;
			}
		}
		byteCount = ::recvfrom( fd, consumeBuf, consumeLen, MSG_DONTWAIT, NULL, NULL );
		if ( consumeBuf != stackConsumeBuf )
			free( consumeBuf );
		if ( inOutByteCount != NULL )
			*inOutByteCount = byteCount;
		DEBUGLOG( "byteCount: %d", (int)byteCount);
		buf[byteCount] = '\0';
	}
	
	// if not at EOL, pull by character until one arrives 
	if ( inCanReadMore && tstr == NULL && byteCount < (ssize_t)bufLen - 1 )
	{
		tstr = buf + byteCount;
		do
		{
			byteCount = ::recvfrom( fd, &readChar, sizeof(readChar), MSG_WAITALL, NULL, NULL );
			if ( byteCount == 0 || byteCount == -1 )
			{
				*tstr = '\0';
				result.err = -1;
				result.type = kConnectionError;
				return result;
			}
			
			if ( (unsigned long)(tstr - buf) < bufLen - 1 )
				*tstr++ = readChar;
			
			if ( inOutByteCount != NULL )
				(*inOutByteCount)++;
		}
		while ( readChar != '\n' );
		*tstr = '\0';
	}
	
    //DEBUGLOG( "returning line: %s", buf );
	
    return result;
}


PWServerError readFromServerGetErrorCode( char *buf )
{
    char *tstr = NULL;
    PWServerError result = {0, kPolicyError};
	size_t compareLen;

	tstr = buf;
    compareLen = strlen(kPasswordServerErrPrefixStr);
    if ( strncmp( tstr, kPasswordServerErrPrefixStr, compareLen ) == 0 )
    {
        tstr += compareLen;
        
        // check if err is a PasswordServer or SASL error
        compareLen = strlen(kPasswordServerSASLErrPrefixStr);
        if ( strncmp(tstr, kPasswordServerSASLErrPrefixStr, compareLen) == 0 ) {
            tstr += compareLen;
            result.type = kSASLError;
        }
        
        sscanf( tstr, "%d", &result.err );
		if ( result.err == 0 )
			result.err = -1;
    }
    else
	{
		compareLen = strlen(kPasswordServerAuthErrPrefixStr);
		if ( strncmp( tstr, kPasswordServerAuthErrPrefixStr, compareLen ) == 0 )
		{
			tstr += compareLen;
			sscanf( tstr, "%d", &result.err );
			if ( result.err == 0 )
				result.err = -1;
		}
	}
	
    return result;
}


// --------------------------------------------------------------------------------
//	ConvertHexToBinary
// --------------------------------------------------------------------------------

void ConvertHexToBinary( const char *inHexStr, unsigned char *outData, unsigned long *outLen )
{
    unsigned char *tptr = outData;
    unsigned char val;
    
    while ( *inHexStr && *(inHexStr+1) )
    {
        if ( *inHexStr >= 'A' )
            val = (*inHexStr - 'A' + 0x0A) << 4;
        else
            val = (*inHexStr - '0') << 4;
        
        inHexStr++;
        
        if ( *inHexStr >= 'A' )
            val += (*inHexStr - 'A' + 0x0A);
        else
            val += (*inHexStr - '0');
        
        inHexStr++;
        
        *tptr++ = val;
    }
    
    *outLen = (tptr - outData);
}

// --------------------------------------------------------------------------------
//	ConvertBinaryTo64
//
//	Since Base64 rounds up to the nearest multiple of 4/3, prepend
//	the original length of the data to the string.
// --------------------------------------------------------------------------------
int ConvertBinaryTo64( const char *inData, unsigned long inLen, char *outHexStr )
{
    int result;
    unsigned int outLen;
    char *tempBuf;
    unsigned long bufLen = (inLen+3) * 4 / 3 + 1;
    
    tempBuf = (char *) malloc( bufLen + 1 );
    if ( tempBuf == NULL )
        return -1;
    
    result = sasl_encode64( (char *)inData, (unsigned int)inLen, tempBuf, (unsigned int)bufLen, &outLen );
    tempBuf[outLen] = '\0';
    sprintf( outHexStr, "{%lu}%s", inLen, tempBuf );
    
    free( tempBuf );
    
    return result;
}


// --------------------------------------------------------------------------------
//	Convert64ToBinary
// --------------------------------------------------------------------------------
int Convert64ToBinary( const char *inHexStr, char *outData, unsigned long maxLen, unsigned long *outLen )
{
    int result;
    unsigned int sasl_outlen;
    unsigned long attached_outlen = 0;
    const char *readPtr = inHexStr;
	
    if ( readPtr == NULL )
        return -1;
    
    // get the original length
    if ( *readPtr == '{' )
    {
        sscanf( readPtr + 1, "%lu", &attached_outlen );
        
        readPtr = strchr( readPtr, '}' );
        if ( readPtr == NULL )
            return -1;
        
        readPtr++;
    }
    
    result = sasl_decode64( readPtr, (unsigned int)strlen(readPtr), (char *)outData, (unsigned int)maxLen, &sasl_outlen );
    
    *outLen = (attached_outlen > 0) ? attached_outlen : (unsigned long)sasl_outlen;
    
    return result;
}


int getconn_domain_socket(void)
{
    register int s;
    size_t len;
    struct sockaddr_un sun;
	
    if ((s = socket(AF_UNIX, SOCK_STREAM, 0)) < 0)
		return -1;
	
    sun.sun_family = AF_UNIX;
    strcpy(sun.sun_path, kPWUNIXDomainSocketAddress);
	len = sizeof(sun.sun_family) + strlen(sun.sun_path) + 1;
	
    if (connect(s, (sockaddr *)&sun, (socklen_t)len) < 0)
        return -1;
	
	return s;
}


// ---------------------------------------------------------------------------
//	* ConnectToServer
// ---------------------------------------------------------------------------

int ConnectToServer( sPSContextData *inContext )
{
    int siResult = 0;
	PWServerError pwsError;
    char buf[1024];
	char *cur, *tptr;
	int index = 0;
	
	DEBUGLOG( "ConnectToServer trying %s:%s", inContext->psName, inContext->psPort);
	
	// is it local?
	inContext->isUNIXDomainSocket = false;
	if ( strcmp(inContext->psName, "127.0.0.1") == 0 ||
		 strcmp(inContext->psName, "localhost") == 0 )
	{
		inContext->fd = getconn_domain_socket();
		if ( inContext->fd != -1 )
			inContext->isUNIXDomainSocket = true;
	}
	
	if ( !inContext->isUNIXDomainSocket )
	{
		// connect to remote server
		siResult = getconn( inContext->psName, inContext->psPort, &inContext->fd );
		if ( siResult != 0 )
			return( siResult );
	}
	
    // get password server greeting
    pwsError = readFromServer(inContext->fd, buf, sizeof(buf));
    if ( pwsError.err < 0 && pwsError.type == kConnectionError )
	{
		close( inContext->fd );
		inContext->fd = -1;
		siResult = pwsError.err;
	}
	else
	{
		gOpenCount++;
		inContext->serverOut = fdopen(inContext->fd, "w");
		
		// get password server version from the greeting
		if ( (tptr = strstr(buf, "ApplePasswordServer")) != NULL )
		{
			tptr += sizeof( "ApplePasswordServer" );
			while ( (cur = strsep(&tptr, ".")) != NULL ) {
				sscanf( cur, "%d", &inContext->serverVers[index++] );
				if ( index >= 4 )
					break;
			}
		}
    }
	
    return siResult;
}


// ----------------------------------------------------------------------------
//	* Connected
//
//	Is the socket connection still open?
// ----------------------------------------------------------------------------

Boolean Connected( sPSContextData *inContext )
{
	struct pollfd fdToPoll;
	int result;
	
	if ( inContext->fd == 0 || inContext->fd == -1 )
		return false;
	
	fdToPoll.fd = inContext->fd;
	fdToPoll.events = POLLSTANDARD;
	fdToPoll.revents = 0;
	result = poll( &fdToPoll, 1, 0 );
	DEBUGLOG( "XXXX poll = %d, events = %d", result, fdToPoll.revents );
	if ( result == -1 )
		return false;
	return ( (fdToPoll.revents & POLLHUP) == 0 );
}


// ----------------------------------------------------------------------------
//	* IdentifyReachableReplica
//
//	RETURNS: CPSUtilities error enum
//	
//	If our current server is not responsive, this routine is used to
//	start asynchronous connections to all replicas. The first one to answer
//	becomes our new favored server.
//	If the connection is established using TCP, outSock is returned. Otherwise,
//	it is set to -1.
// ----------------------------------------------------------------------------

int IdentifyReachableReplica( CFMutableArrayRef inServerArray, const char *inHexHash, sPSServerEntry *outReplica, int *outSock )
{
	sPSServerEntry *entrylist = NULL;
	CFIndex servIndex = 0;
	CFIndex servCount = 0;
	int result = kCPSUtilServiceUnavailable;
	
	if ( ConvertCFArrayToServerArray(inServerArray, &entrylist, &servCount) != kCPSUtilOK )
		return kCPSUtilServiceUnavailable;

	result = IdentifyReachableReplicaByIP( entrylist, servCount, inHexHash, outReplica, outSock );
	if ( result == kCPSUtilServiceUnavailable )
	{
		bool foundNewIPToTry = false;
		int err = 0;
		struct addrinfo *res = NULL;
		struct addrinfo *res0 = NULL;
		char testStr[256];
		
		for ( servIndex = 0; servIndex < servCount; servIndex++ )
		{
			if ( entrylist[servIndex].dns[0] != '\0' )
			{
				err = getaddrinfo( entrylist[servIndex].dns, NULL, NULL, &res0 );
				if ( err == 0 )
				{
					for ( res = res0; res != NULL; res = res->ai_next )
					{
						if ( res->ai_family != AF_INET || res->ai_addrlen != sizeof(sockaddr_in) )
							continue;
						
						if ( inet_ntop(AF_INET, &(((struct sockaddr_in *)(res->ai_addr))->sin_addr.s_addr), testStr, (socklen_t)sizeof(testStr)) != NULL &&
							 strcmp(testStr, entrylist[servIndex].ip) != 0 )
						{
							strlcpy( entrylist[servIndex].ip, testStr, sizeof(entrylist[servIndex].ip) );
							foundNewIPToTry = true;
						}
					}
					
					freeaddrinfo( res0 );
				}
			}
		}
		
		if ( foundNewIPToTry )
			result = IdentifyReachableReplicaByIP( entrylist, servCount, inHexHash, outReplica, outSock );
		
	}
	if ( entrylist != NULL )
		free( entrylist );
	
	return result;
}


// ----------------------------------------------------------------------------
//	* pwsf_SortServerEntries
// ----------------------------------------------------------------------------

int pwsf_SortServerEntryCompare(const void *a, const void *b)
{
	return ( ((sPSServerEntry *)a)->sortVal - ((sPSServerEntry *)b)->sortVal );
}

void pwsf_SortServerEntries(sPSServerEntry *inEntryList, CFIndex servCount)
{
	CFIndex servIndex = 0;
	in_addr_t *iplist = NULL;
	
	if ( servCount <= 1 )
		return;
	
	// get locally hosted IP list
	if ( pwsf_LocalIPList(&iplist) != kCPSUtilOK || iplist == NULL )
		return;
	
	// assign priorities
	for ( servIndex = 0; servIndex < servCount; servIndex++ )
		inEntryList[servIndex].sortVal = ReplicaPriority( &inEntryList[servIndex], iplist );
	free( iplist );
	
	// sort
	qsort( inEntryList, servCount, sizeof(sPSServerEntry), pwsf_SortServerEntryCompare );
	
#if DEBUG
	for ( servIndex = 0; servIndex < servCount; servIndex++ )
		DEBUGLOG("ip=%s, sortVal=%d", inEntryList[servIndex].ip, inEntryList[servIndex].sortVal);
#endif
}


// ----------------------------------------------------------------------------
//	* IdentifyReachableReplicaByIP
//
//	RETURNS: CPSUtilities error enum
//	
//	If our current server is not responsive, this routine is used to
//	start asynchronous connections to all replicas. The first one to answer
//	becomes our new favored server.
//	If the connection is established using TCP, outSock is returned. Otherwise,
//	it is set to -1.
// ----------------------------------------------------------------------------

int IdentifyReachableReplicaByIP(
	sPSServerEntry *entrylist,
	CFIndex servCount,
	const char *inHexHash,
	sPSServerEntry *outReplica,
	int *outSock )
{
	int siResult = 0;
	char *portNumStr = NULL;
	CFIndex servIndex = 0;
	CFIndex descIndex = 0;
	int connectedSocket = -1;
	bool connectedSocketIsTCP = false;
	int tempsock = -1;
	struct timeval tcpOpenTimeout = { 1, 0 };
	float connectTime = 0;
	int *socketList = NULL;
	bool checkUDPDescriptors = false;
	bool fallbackToTCP = false;
	bool usingDefaultPort106 = true;
	struct pollfd* pollList = NULL;
	
	DEBUGLOG( "IdentifyReachableReplica inHexHash=%s", inHexHash ? inHexHash : "NULL" );
	
	if ( entrylist == NULL || outReplica == NULL || outSock == NULL )
		return kCPSUtilParameterError;
	
	bzero( outReplica, sizeof(sPSServerEntry) );
	*outSock = -1;
	
	// nothing to do
	if ( servCount == 0 || entrylist == NULL )
		return kCPSUtilOK;
		
	pwsf_SortServerEntries( entrylist, servCount );

	try
	{
		socketList = (int *) malloc( servCount * sizeof(int) );
		if ( socketList == NULL )
			throw (int)kCPSUtilMemoryError;

		memset( socketList, -1, servCount * sizeof(int) );
		
		pollList = (struct pollfd *)malloc( servCount * sizeof(*pollList) );
		if (pollList == NULL )
			throw (int)kCPSUtilMemoryError;
	
		checkUDPDescriptors = false;
		
		for ( servIndex = 0; servIndex < servCount && connectedSocket == -1; servIndex++ )
		{
			if ( entrylist[servIndex].ip[0] == '\0' ) {
				DEBUGLOG( "entrylist[servIndex].ip[0] == 0" );
				continue;
			}
			else {
				DEBUGLOG( "trying %s", entrylist[servIndex].ip );
			}
			
			if ( inHexHash != NULL && entrylist[servIndex].id[0] != '\0' && strcmp( inHexHash, entrylist[servIndex].id ) != 0 ) {
				DEBUGLOG("rejecting %s based on id", entrylist[servIndex].ip);
				continue;
			}
			
			portNumStr = strchr( entrylist[servIndex].ip, ':' );
			if ( portNumStr != NULL )
			{
				*portNumStr = '\0';
				strlcpy(entrylist[servIndex].port, portNumStr+1, 10);
				usingDefaultPort106 = false;
			}
			else
			{
				strcpy(entrylist[servIndex].port, "106");
			}
			
			siResult = 0;
			fallbackToTCP = false;
			if ( ! entrylist[servIndex].ipFromNode )
			{
				// for wider subnets and WANs, the least loaded server in the first
				// batch to respond wins.
				
				DEBUGLOG("testing %s:%s", entrylist[servIndex].ip, kPasswordServerPortStr);
				
				// connect to remote server
				siResult = testconn_udp( entrylist[servIndex].ip, kPasswordServerPortStr, &socketList[servIndex] );
				DEBUGLOG( "testconn_udp result = %d, sock=%d", (int)siResult, socketList[servIndex] );
				checkUDPDescriptors |= ( siResult == 0 );
				fallbackToTCP = ( siResult != 0 );
				siResult = 0;
			}
			
			if ( entrylist[servIndex].ipFromNode || fallbackToTCP )
			{
				DEBUGLOG("testing %s:%s", entrylist[servIndex].ip, entrylist[servIndex].port);
				
				// wait for this CPU and for non-routed addresses
				// these will generally succeed.
				tempsock = 0;
				siResult = getconn_async( entrylist[servIndex].ip, entrylist[servIndex].port, &tcpOpenTimeout, &connectTime, &tempsock );
				DEBUGLOG( "getconn_async = %ld", siResult );
				if ( siResult == kCPSUtilOK )
				{
					char tstr[256];
					
					// connected immediately
					sprintf(tstr, "Connect time: %.3f", connectTime );
					DEBUGLOG( "%s", tstr );
					connectedSocket = tempsock;
					connectedSocketIsTCP = true;
					memcpy( outReplica, &entrylist[servIndex], sizeof(sPSServerEntry) );
				}
			}
			
			// check any queued UDP requests
			if ( checkUDPDescriptors && socketList != NULL )
			{
				socklen_t structlength;
				size_t byteCount = 0;
				int descCount = 0;
				struct sockaddr_in cin;
				char packetData[64];
				int pollTimeout = (servIndex==0) ? 250 : 130;  // milliseconds
				
				bzero( &cin, sizeof(cin) );
				cin.sin_family = AF_INET;
				cin.sin_addr.s_addr = htonl( INADDR_ANY );
				cin.sin_port = htons( 0 );
				
				for ( descIndex = 0; descIndex < servCount; descIndex++ )
				{
					if ( socketList[descIndex] != -1 )
					{
						pollList[descIndex].fd = socketList[descIndex];
						pollList[descIndex].events = POLLIN;
						pollList[descIndex].revents = 0;
					}
				}
				
				descCount = poll( pollList, (nfds_t)servCount, pollTimeout );
				DEBUGLOG( "poll = %d", descCount );
				if ( descCount > 0 )
				{
					for ( descIndex = 0; descIndex < servCount && connectedSocket == -1; descIndex++ )
					{
						if ( pollList[descIndex].revents & POLLIN )
						{
							structlength = (socklen_t)sizeof( cin );
							byteCount = recvfrom( pollList[descIndex].fd, packetData, sizeof(packetData) - 1,
													MSG_DONTWAIT, (struct sockaddr *)&cin, &structlength );
							DEBUGLOG( "recvfrom() byteCount=%d", byteCount );
							
							if ( byteCount > 0 && packetData[0] != '0' )
							{
								// if the server's key hash is available, opportunistically do more verification
								if ( inHexHash != NULL && byteCount > 33 )
								{
									// guarantee termination
									packetData[byteCount] = '\0';
									
									// find delimiter
									char *serverHash = strchr( packetData, ';' );
									if ( serverHash != NULL )
									{
										serverHash++;
										
										// find delimiter or end
										char *endHashPtr = strchr( serverHash, ';' );
										if ( endHashPtr != NULL )
											*endHashPtr = '\0';
										
										// continue if a mismatch is confirmed
										long hashLen = strlen( serverHash );
										if ( hashLen == 32 && strcmp( inHexHash, serverHash ) != 0 )
											continue;
									}
								}
								
								connectedSocket = pollList[descIndex].fd;
								memcpy( outReplica, &entrylist[descIndex], sizeof(sPSServerEntry) );
								
								// can use the new port #
								if ( usingDefaultPort106 )
									strcpy( outReplica->port, kPasswordServerPortStr );
								break;
							}
						}
					}
				}
			}
		}
	}
	catch( int error )
	{
		siResult = error;
	}
	catch( ... )
	{
		siResult = kCPSUtilServiceUnavailable;
	}
	
	if ( socketList != NULL )
	{
		// close all the sockets
		for ( descIndex = 0; descIndex < servCount; descIndex++ )
			if ( socketList[descIndex] > 0 )
				close( socketList[descIndex] );
		
		free( socketList );
		socketList = NULL;
	}
	
	if ( pollList != NULL )
	{
		free( pollList );
		pollList = NULL;
	}
    
	// make sure to return a replica or an error
	if ( siResult == 0 && connectedSocket <= 0 )
		siResult = kCPSUtilServiceUnavailable;
	
	// return the socket
	if ( connectedSocketIsTCP )
		*outSock = connectedSocket;
		
DEBUGLOG( "IdentifyRR returning %d", (int)siResult );

	return siResult;
}


// ---------------------------------------------------------------------------
//	* ConvertCFArrayToServerArray
//
//	Returns: CPSUtilities error enum
//
//  <outServerArray> contains malloced memory that must be freed by the caller
// ---------------------------------------------------------------------------

int ConvertCFArrayToServerArray( CFArrayRef inCFArray, sPSServerEntry **outServerArray, CFIndex *outCount )
{
	CFDataRef serverRef;
	sPSServerEntry *entrylist = NULL;
	CFIndex servIndex;
	CFIndex servCount = 0;
	const UInt8 *bytePtr = NULL;
	
	if ( inCFArray == NULL || outServerArray == NULL || outCount == NULL )
	{
		DEBUGLOG( "ConvertCFArrayToServerArray called with a NULL parameter." );
		return kCPSUtilParameterError;
	}
	
	*outServerArray = NULL;
	*outCount = 0;
	
	servCount = CFArrayGetCount( inCFArray );
	DEBUGLOG( "Server list contains %d servers.", (int)servCount );
	
	// nothing to do
	if ( servCount > 0 )
	{
		entrylist = (sPSServerEntry *) calloc( servCount, sizeof(sPSServerEntry) );
		if ( entrylist == NULL ) return kCPSUtilMemoryError;
		
		// extract the array
		for ( servIndex = 0; servIndex < servCount; servIndex++ )
		{
			serverRef = (CFDataRef) CFArrayGetValueAtIndex( inCFArray, servIndex );
			if ( serverRef == NULL ) {
				DEBUGLOG( "serverRef == NULL" );
				continue;
			}
			
			bytePtr = CFDataGetBytePtr( serverRef );
			if ( bytePtr != NULL ) {
				memcpy( &(entrylist[servIndex]), bytePtr, sizeof(sPSServerEntry) );
	DEBUGLOG( "entrylist[%d].ip=%s, entrylist[].id=%s, ipFromNode=%d", servIndex, entrylist[servIndex].ip, entrylist[servIndex].id, entrylist[servIndex].ipFromNode );
			}
		}
	}
	
	*outServerArray = entrylist;
	*outCount = servCount;
	
	return kCPSUtilOK;
}


// ---------------------------------------------------------------------------
//	* GetBigNumber
//
//	RETURNS: CPSUtilities enum
// ---------------------------------------------------------------------------

int GetBigNumber( sPSContextData *inContext, char **outBigNumStr )
{
	int                 siResult			= kCPSUtilOK;
    BIGNUM				*nonce				= NULL;
	char				*bnStr				= NULL;
	
	if ( inContext == NULL || outBigNumStr == NULL )
		return kCPSUtilParameterError;
	*outBigNumStr = NULL;
	
	// make nonce
	nonce = BN_new();
	if ( nonce == NULL )
		return kCPSUtilMemoryError;
	
	// Generate a random challenge (256-bits)
	BN_rand(nonce, 256, 0, 0);
	bnStr = BN_bn2dec(nonce);
	
	BN_clear_free(nonce);
	
	if ( bnStr == NULL )
		return kCPSUtilMemoryError;
	
	DEBUGLOG( "nonce = %s", bnStr);
	*outBigNumStr = bnStr;
    
    return siResult;
}


// ---------------------------------------------------------------------------
//	* SendFlush
//
// ---------------------------------------------------------------------------

PWServerError
SendFlush(
	sPSContextData *inContext,
	const char *inCommandStr,
	const char *inArg1Str,
	const char *inArg2Str )
{
	PWServerError result = { kAuthOK, kPolicyError };
	char *commandBuf;
	
	if ( inCommandStr == NULL ) {
		result.err = kAuthFail;
		return result;
	}
	
	commandBuf = SendFlushReadAssembleCommand( inCommandStr, inArg1Str, inArg2Str );
	if ( commandBuf == NULL ) {
		result.err = kAuthFail;
		return result;
	}
	
	if ( inContext->castKeySet && !inContext->isUNIXDomainSocket )
		writeToServerWithCASTKey( inContext->serverOut, commandBuf, &inContext->castKey, inContext->castIV );
	else
		writeToServer( inContext->serverOut, commandBuf );
	
	free( commandBuf );
	
	return result;
}


// ---------------------------------------------------------------------------
//	* SendFlushRead
//
// ---------------------------------------------------------------------------

PWServerError
SendFlushRead(
	sPSContextData *inContext,
	const char *inCommandStr,
	const char *inArg1Str,
	const char *inArg2Str,
	char *inOutBuf,
	unsigned long inBufLen
	)
{
	PWServerError result = { kAuthOK, kPolicyError };
	
	if ( inCommandStr == NULL || inOutBuf == NULL ) {
		result.err = kAuthFail;
		return result;
	}
	
	result = SendFlush( inContext, inCommandStr, inArg1Str, inArg2Str );	
	
   	if ( result.err == kAuthOK )
	{
		if ( inContext->castKeySet && !inContext->isUNIXDomainSocket )
			result = readFromServerWithCASTKey( inContext->fd, inOutBuf, inBufLen, &inContext->castKey, inContext->castReceiveIV );
		else
			result = readFromServer( inContext->fd, inOutBuf, inBufLen );
	}
	
	return result;
}


// ---------------------------------------------------------------------------
//	SendFlushReadAssembleCommand
//
//  Returns: a malloced c-str (must be freed by caller)
// ---------------------------------------------------------------------------

char *SendFlushReadAssembleCommand(
	const char *inCommandStr,
	const char *inArg1Str,
	const char *inArg2Str )
{
	long commandsize = 0;
	char *commandBuf = NULL;
	
	if ( inCommandStr == NULL)
		return NULL;
	
	commandsize = strlen( inCommandStr ) + 4 + CAST_BLOCK;
	if ( inArg1Str != NULL )
		commandsize += strlen( inArg1Str ) + 1;
	if ( inArg2Str != NULL )
		commandsize += strlen( inArg2Str ) + 1;
	
	commandBuf = (char *) malloc( commandsize );
	if ( commandBuf == NULL )
		return NULL;
	
	if ( inArg1Str == NULL )
		sprintf( commandBuf, "%s\r\n", inCommandStr );
	else
	if ( inArg2Str == NULL )
		sprintf( commandBuf, "%s %s\r\n", inCommandStr, inArg1Str );
	else
		sprintf( commandBuf, "%s %s %s\r\n", inCommandStr, inArg1Str, inArg2Str );
	
	return commandBuf;
}


// ---------------------------------------------------------------------------
//	StripRSAKey
// ---------------------------------------------------------------------------

void StripRSAKey( char *inOutUserID )
{
    if ( inOutUserID == NULL )
        return;
    
    char *delim = strchr( inOutUserID, ',' );
    if ( delim )
        *delim = '\0';
}

	
// ---------------------------------------------------------------------------
//	* GetPasswordServerList
//
//	If any search method succeeds, the result is 0 and the found
//	server list is returned. If no password servers are found, the first
//	error encountered is returned.
// ---------------------------------------------------------------------------

int
GetPasswordServerList( CFMutableArrayRef *outServerList, int inConfigSearchOptions )
{
	int status = 0;
	int status1 = 0;
	CFMutableArrayRef serverArray;
	
DEBUGLOG( "GetPasswordServerList");

	if ( outServerList == NULL )
		return kAuthFail;
	
	*outServerList = NULL;
	
	if ( inConfigSearchOptions == 0 )
		return 0;
	
	serverArray = CFArrayCreateMutable( kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks );
	if ( serverArray == NULL )
		return kAuthFail;
	
	if ( inConfigSearchOptions & kPWSearchLocalFile )
		status = GetServerListFromLocalCache( serverArray );
	
	if ( inConfigSearchOptions & kPWSearchReplicaFile )
	{
		status1 = GetServerListFromFile( serverArray );
		if ( status1 != noErr && status == noErr )
			status = status1;
	}
		
	if ( CFArrayGetCount(serverArray) > 0 )
	{
		*outServerList = serverArray;
		status = 0;
	}
	else
	{
		CFRelease( serverArray );
	}
	
	return status;
}


// ---------------------------------------------------------------------------
//	* GetPasswordServerListForKeyHash
// ---------------------------------------------------------------------------

int
GetPasswordServerListForKeyHash( CFMutableArrayRef *outServerList, int inConfigSearchOptions, const char *inKeyHash )
{
	int status = 0;
	int status1 = 0;
	CFMutableArrayRef serverArray;
	
DEBUGLOG( "GetPasswordServerListForKeyHash");

	if ( outServerList == NULL )
		return kAuthFail;
	
	*outServerList = NULL;
	
	if ( inConfigSearchOptions == 0 )
		return 0;
	
	serverArray = CFArrayCreateMutable( kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks );
	if ( serverArray == NULL )
		return kAuthFail;
	
	if ( inConfigSearchOptions & kPWSearchLocalFile )
		status = GetServerListFromLocalCache( serverArray );
	
	if ( inConfigSearchOptions & kPWSearchReplicaFile )
	{
		status1 = GetServerListFromFileForKeyHash( serverArray, inKeyHash );
		if ( status1 != noErr && status == noErr )
			status = status1;
	}
		
	if ( CFArrayGetCount(serverArray) > 0 )
	{
		*outServerList = serverArray;
		status = 0;
	}
	else
	{
		CFRelease( serverArray );
	}
	
	return status;	
}


// ---------------------------------------------------------------------------
//	* GetServerListFromLocalCache
// ---------------------------------------------------------------------------

int
GetServerListFromLocalCache( CFMutableArrayRef inOutServerList )
{
	CFStringRef myReplicaDataFilePathRef;
	CFURLRef myReplicaDataFileRef;
	CFReadStreamRef myReadStreamRef;
	CFPropertyListRef myPropertyListRef = NULL;
	CFStringRef errorString = NULL;
	CFPropertyListFormat myPLFormat;
	CFIndex index, arrayCount;
	CFDataRef dataRef;
	int status = 0;
    sPSServerEntry *anEntryPtr;
	bool bLoadPreConfiguredFile = false;
	struct stat sb;
	
DEBUGLOG( "GetServerListFromLocalCache");

	if ( lstat( kPWReplicaPreConfiguredFile, &sb ) == 0 )
	{
		bLoadPreConfiguredFile = true;
	}
	else
	{
		if ( lstat( kPWReplicaLocalFile, &sb ) != 0 )
			return kAuthFail;
		
		bLoadPreConfiguredFile = false;
	}
	
	if ( bLoadPreConfiguredFile )
	{
		CReplicaFile replicaFile( true, kPWReplicaPreConfiguredFile );
		
		status = GetServerListFromXML( &replicaFile, inOutServerList );
		DEBUGLOG( "loaded manual config file = %d", (int)status );
	}
	else
	{
		myReplicaDataFilePathRef = CFStringCreateWithCString( kCFAllocatorDefault, kPWReplicaLocalFile, kCFStringEncodingUTF8 );
		if ( myReplicaDataFilePathRef == NULL )
			return kAuthFail;
		
		myReplicaDataFileRef = CFURLCreateWithFileSystemPath( kCFAllocatorDefault, myReplicaDataFilePathRef, kCFURLPOSIXPathStyle, false );
		
		CFRelease( myReplicaDataFilePathRef );
		
		if ( myReplicaDataFileRef == NULL )
			return kAuthFail;
		
		myReadStreamRef = CFReadStreamCreateWithFile( kCFAllocatorDefault, myReplicaDataFileRef );
		
		CFRelease( myReplicaDataFileRef );
		
		if ( myReadStreamRef == NULL )
			return kAuthFail;
		
		if ( CFReadStreamOpen( myReadStreamRef ) )
		{
			myPLFormat = kCFPropertyListXMLFormat_v1_0;
			myPropertyListRef = CFPropertyListCreateFromStream( kCFAllocatorDefault, myReadStreamRef, 0, kCFPropertyListMutableContainersAndLeaves, &myPLFormat, &errorString );
			CFReadStreamClose( myReadStreamRef );
		}
		CFRelease( myReadStreamRef );
		
		if ( errorString != NULL )
		{
			char errMsg[256];
			
			if ( CFStringGetCString( errorString, errMsg, sizeof(errMsg), kCFStringEncodingUTF8 ) )
				DEBUGLOG( "could not load the local replica cache file, error = %s", errMsg );
			CFRelease( errorString );
		}
		
		if ( myPropertyListRef == NULL )
			return kAuthFail;
		
		if ( CFGetTypeID(myPropertyListRef) != CFArrayGetTypeID() )
		{
			CFRelease( myPropertyListRef );
			return kAuthFail;
		}
		
		// put the last contacted server on top
		arrayCount = CFArrayGetCount( (CFArrayRef) myPropertyListRef );
		for ( index = 0; index < arrayCount; index++ )
		{
			dataRef = (CFDataRef) CFArrayGetValueAtIndex( (CFArrayRef) myPropertyListRef, index );
			if ( dataRef == NULL )
				continue;
			
			anEntryPtr = (sPSServerEntry *) CFDataGetBytePtr( dataRef );
			if ( anEntryPtr->lastContact )
			{
				// if the index is 0, nothing to be done but we can stop
				if ( index > 0 )
					CFArrayExchangeValuesAtIndices( (CFMutableArrayRef) myPropertyListRef, index, 0 );
				break;
			}
		}
		
		// add to the master array
		// Note: documentation claims that the range should be [0-(N-1)], but the real answer is [0-N].
		if ( arrayCount > 0 )
			CFArrayAppendArray( inOutServerList, (CFArrayRef) myPropertyListRef, CFRangeMake(0, arrayCount) );
		
		//DEBUGLOG( "append Count=%d",(int)CFArrayGetCount(inOutServerList) );
		CFRelease( myPropertyListRef );
	}
	
	return status;
}


// ---------------------------------------------------------------------------
//	* GetServerListFromFile
// ---------------------------------------------------------------------------

int
GetServerListFromFile( CFMutableArrayRef inOutServerList )
{
	return GetServerListFromFileForKeyHash( inOutServerList, NULL );
}


// ---------------------------------------------------------------------------
//	* GetServerListFromFileForKeyHash
// ---------------------------------------------------------------------------

int
GetServerListFromFileForKeyHash( CFMutableArrayRef inOutServerList, const char *inKeyHash )
{
	int status = 0;
	CReplicaFile *replicaFile = NULL;
	sPSServerEntry serverEntry;
	bool gotID;
	
	replicaFile = new CReplicaFile();
	if ( replicaFile == NULL )
		return kAuthFail;
	
	bzero( &serverEntry, sizeof(sPSServerEntry) );
	gotID = replicaFile->GetUniqueID( serverEntry.id );
	
	// optimization: if we're alone in the world, return loopback
	// requirements are no replicas, and file is present (able to get server ID)
	if ( (gotID) && (replicaFile->ReplicaCount() == 0) )
	{
		if ( (inKeyHash == NULL) || (strcmp(inKeyHash, serverEntry.id) == 0) )
		{
			strcpy( serverEntry.ip, "127.0.0.1" );
			strcpy( serverEntry.port, kPasswordServerPortStr );
			AppendToArrayIfUnique( inOutServerList, &serverEntry );
			
			delete replicaFile;
			return 0;
		}
	}
	
	// if there is no local password server or if the RSA keys don't match,
	// see if there is a remote password server we've discovered in the past.
	if ( inKeyHash != NULL )
	{
		if ( (!gotID) || (strcmp( inKeyHash, serverEntry.id ) != 0) )
		{
			char filePath[sizeof(kPWReplicaRemoteFilePrefix) + strlen(inKeyHash)];
			
			// construct cache file path
			strcpy( filePath, kPWReplicaRemoteFilePrefix );
			strcat( filePath, inKeyHash );
			
			delete replicaFile;
			replicaFile = new CReplicaFile( true, filePath );
			
			// not really an error if there's no remote file
			if ( replicaFile == NULL )
				return 0;
		}
	}
	
	status = GetServerListFromXML( replicaFile, inOutServerList );
	DEBUGLOG( "GetServerListFromFile = %d", (int)status);
	
	// clean up
	delete replicaFile;
	
	return status;
}


// ---------------------------------------------------------------------------
//	* GetServerListFromConfig
// ---------------------------------------------------------------------------

int GetServerListFromConfig( CFMutableArrayRef *outServerList, CReplicaFile *inReplicaData )
{
	int status = 0;
	CFMutableArrayRef serverArray;
	
DEBUGLOG( "GetServerListFromConfig");

	if ( outServerList == NULL || inReplicaData == NULL )
		return kAuthFail;
	
	*outServerList = NULL;
	
	serverArray = CFArrayCreateMutable( kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks );
	if ( serverArray == NULL )
		return kAuthFail;
	
	status = GetServerListFromXML( inReplicaData, serverArray );
	
	if ( CFArrayGetCount(serverArray) > 0 )
	{
		*outServerList = serverArray;
		status = 0;
	}
	else
	{
		CFRelease( serverArray );
	}
	
	return status;
}


// ---------------------------------------------------------------------------
//	* GetServerListFromXML
// ---------------------------------------------------------------------------

int
GetServerListFromXML( CReplicaFile *inReplicaFile, CFMutableArrayRef inOutServerList )
{
	CFDictionaryRef serverDict = NULL;
	CFStringRef ldapServerString = NULL;
	UInt32 repIndex;
	UInt32 repCount;
	int ipIndex = 0;
	int status = 0;
	sPSServerEntry serverEntry;
	char serverID[33];
	char ldapServerStr[256] = {0};
	
DEBUGLOG( "in GetServerListFromXML");

	if ( inOutServerList == NULL )
		return kAuthFail;
	
	bzero( &serverEntry, sizeof(sPSServerEntry) );
	
	ldapServerString = inReplicaFile->CurrentServerForLDAP();
	if ( ldapServerString != NULL )
	{
		CFStringGetCString( ldapServerString, ldapServerStr, sizeof(ldapServerStr), kCFStringEncodingUTF8 );
		CFRelease( ldapServerString );
	}
	
	if ( inReplicaFile->GetUniqueID( serverID ) )
		strcpy( serverEntry.id, serverID );

DEBUGLOG( "serverEntry.id=%s", serverEntry.id);
	
	serverDict = inReplicaFile->GetParent();
	if ( serverDict != NULL )
	{
		for ( ipIndex = 0, status = 0; status == 0; ipIndex++ )
		{
			status = GetServerFromDict( serverDict, ipIndex, &serverEntry );
			if ( status == 0 )
			{
				if ( strcmp(serverEntry.ip, ldapServerStr) == 0 )
					serverEntry.currentServerForLDAP = true;
					
				AppendToArrayIfUnique( inOutServerList, &serverEntry );
			}
		}
	}
	
	repCount = inReplicaFile->ReplicaCount();
DEBUGLOG( "repCount=%d", (int)repCount);
	for ( repIndex = 0; repIndex < repCount; repIndex++ )
	{
		serverDict = inReplicaFile->GetReplica( repIndex );
		if ( serverDict != NULL )
		{
			for ( ipIndex = 0, status = 0; status == 0; ipIndex++ )
			{
				status = GetServerFromDict( serverDict, ipIndex, &serverEntry );
				if ( status == 0 )
				{
					if ( strcmp(serverEntry.ip, ldapServerStr) == 0 )
						serverEntry.currentServerForLDAP = true;
					
					AppendToArrayIfUnique( inOutServerList, &serverEntry );
				}
			}
		}
	}
	
	status = ( (CFArrayGetCount(inOutServerList) > 0) ? 0 : kAuthFail );
	
DEBUGLOG( "GetServerListFromXML = %d", (int)status);
	return status;
}


// ---------------------------------------------------------------------------
//	* GetServerFromDict
// ---------------------------------------------------------------------------

int
GetServerFromDict( CFDictionaryRef serverDict, int inIPIndex, sPSServerEntry *outServerEntry )
{
	int status = 0;
	CFTypeRef anIPRef;
	CFStringRef aString;
	
DEBUGLOG( "GetServerListFromDict");

	if ( serverDict == NULL || outServerEntry == NULL )
		return kAuthFail;
	
	// IP
	if ( ! CFDictionaryGetValueIfPresent( serverDict, CFSTR(kPWReplicaIPKey), (const void **)&anIPRef ) )
		return kAuthFail;
	
	if ( CFGetTypeID(anIPRef) == CFStringGetTypeID() )
	{
		// only one value
		if ( inIPIndex != 0 )
			return kAuthFail;
		
		if ( ! CFStringGetCString( (CFStringRef)anIPRef, outServerEntry->ip, sizeof(outServerEntry->ip), kCFStringEncodingUTF8 ) )
			return kAuthFail;
	}
	else
	if ( CFGetTypeID(anIPRef) == CFArrayGetTypeID() )
	{
		if ( inIPIndex >= CFArrayGetCount((CFArrayRef)anIPRef) )
			return kAuthFail;
		
		aString = (CFStringRef) CFArrayGetValueAtIndex( (CFArrayRef)anIPRef, inIPIndex );
		if ( aString == NULL || CFGetTypeID(aString) != CFStringGetTypeID() )
			return kAuthFail;
		
		if ( ! CFStringGetCString( aString, outServerEntry->ip, sizeof(outServerEntry->ip), kCFStringEncodingUTF8 ) )
			return kAuthFail;
	}
	
	// DNS
	if ( CFDictionaryGetValueIfPresent( serverDict, CFSTR("DNS"), (const void **)&aString ) &&
		 CFGetTypeID(aString) != CFStringGetTypeID() )
	{
		CFStringGetCString( aString, outServerEntry->dns, sizeof(outServerEntry->dns), kCFStringEncodingUTF8 );
	}
	
	
DEBUGLOG( "GetServerListFromDict = %d", (int)status);
	return status;
}


// ---------------------------------------------------------------------------
//	* SaveLocalReplicaCache
// ---------------------------------------------------------------------------

int SaveLocalReplicaCache( CFMutableArrayRef inReplicaArray, sPSServerEntry *inLastContactEntry )
{
	CFIndex index, replicaCount;
	CFMutableDataRef dataRef;
	sPSServerEntry *anEntryPtr;
	
	if ( inReplicaArray == NULL )
		return -1;
	
	replicaCount = CFArrayGetCount( inReplicaArray );
	if ( replicaCount == 0 )
		return -1;
	
	for ( index = 0; index < replicaCount; index++ )
	{
		dataRef = (CFMutableDataRef) CFArrayGetValueAtIndex( inReplicaArray, index );
		if ( dataRef == NULL )
			continue;
		
		anEntryPtr = (sPSServerEntry *)CFDataGetMutableBytePtr( dataRef );
		if ( anEntryPtr == NULL )
			continue;
		
		anEntryPtr->lastContact = ( strcmp( anEntryPtr->ip, inLastContactEntry->ip ) == 0 );
		if ( anEntryPtr->lastContact && anEntryPtr->id[0] == '\0' )
			memcpy( anEntryPtr->id, inLastContactEntry->id, sizeof(anEntryPtr->id) );
	}
	
	// Note: kPWReplicaLocalFile must be in a directory that exists or SaveXMLData() can hang.
	//			The method guarantees that /var/db/authserver exists; therefore, kPWReplicaLocalFile
	//			should always lead there.
	return CReplicaFile::SaveXMLData( (CFPropertyListRef) inReplicaArray, kPWReplicaLocalFile );
}


// ---------------------------------------------------------------------------
//	* AppendToArrayIfUnique
// ---------------------------------------------------------------------------

void AppendToArrayIfUnique( CFMutableArrayRef inArray, sPSServerEntry *inServerEntry )
{
	CFIndex serverIndex, serverCount;
	CFDataRef serverRef;
	sPSServerEntry anEntry;
	sPSServerEntry *anEntryPtr;
	
	serverCount = CFArrayGetCount( inArray );
	for ( serverIndex = 0; serverIndex < serverCount; serverIndex++ )
	{
		serverRef = (CFDataRef) CFArrayGetValueAtIndex( inArray, serverIndex );
		if ( serverRef == NULL )
			continue;
		
		memcpy( &anEntry, CFDataGetBytePtr(serverRef), sizeof(anEntry) );
		if ( strcmp( inServerEntry->ip, anEntry.ip ) == 0 )
			return;
	}
	
	anEntryPtr = (sPSServerEntry *) malloc( sizeof(sPSServerEntry) );
	if ( anEntryPtr == NULL )
		return;
	memcpy( anEntryPtr, inServerEntry, sizeof(sPSServerEntry) );
	
	serverRef = CFDataCreateWithBytesNoCopy( kCFAllocatorDefault, (const unsigned char *)anEntryPtr, sizeof(sPSServerEntry), kCFAllocatorMalloc );
	if ( serverRef != NULL ) {
		CFArrayAppendValue( inArray, serverRef );
		CFRelease( serverRef );
	}
DEBUGLOG( "AppendToArrayIfUnique adding: %s %s", inServerEntry->ip, inServerEntry->id );
}


// --------------------------------------------------------------------------------
//	* ReplicaPriority
//
//	RETURNS: the priority an IP address should be given for contact
// --------------------------------------------------------------------------------

ReplicaIPLevel ReplicaPriority( sPSServerEntry *inReplica, in_addr_t *iplist )
{
	unsigned char s1, s2;
	char *ipStr = inReplica->ip;
	struct in_addr ipAddr, ourIPAddr;
	int index;
	
	// 1. current LDAP server is highest
	if ( inReplica->currentServerForLDAP )
		return kReplicaIPSet_CurrentForLDAP;

	// 2. locally hosted
	if ( strcmp( ipStr, "127.0.0.1" ) == 0 )
		return kReplicaIPSet_LocallyHosted;
	
	// extract IP address
	if ( inet_pton( AF_INET, ipStr, &ipAddr ) != 1 )
		return kReplicaIPSet_Wide;
	
	// IPs hosted on this CPU
	for ( index = 0; index < kMaxIPAddrs && iplist[index] != 0; index++ )
	{
		if ( ipAddr.s_addr == iplist[index] )
			return kReplicaIPSet_LocallyHosted;
	}

	// 3. subnet
	// NOTE: in order to do this right, we need to get the actual
	// subnet for each interface. In the absence of that information, 
	// be conservative-but-reasonable and assume that servers inside
	// 255.255.255.0 are in the subnet.
	
	ipAddr.s_addr = (ipAddr.s_addr & 0xFFFFFF00);
	
	for ( index = 0; index < kMaxIPAddrs && iplist[index] != 0; index++ )
	{
		ourIPAddr.s_addr = (iplist[index] & 0xFFFFFF00);
		
		if ( ourIPAddr.s_addr == ipAddr.s_addr )
			return kReplicaIPSet_InSubnet;
	}
	
	// 4. private net
	// s1 and s2 are ip segments 1&2
	s1 = *((unsigned char *)&ipAddr.s_addr);
	s2 = *(((unsigned char *)&ipAddr.s_addr) + 1);
	
	// private class A
	if ( s1 == 10 )
		return kReplicaIPSet_PrivateNet;
	
	// private class B
	if ( s1 == 172 && s2 <= 31 && s2 >= 16 )
		return kReplicaIPSet_PrivateNet;
	
	// private class C
	if ( s1 == 192 && s2 == 168 )
		return kReplicaIPSet_PrivateNet;
	
	// 5. any valid IP is in wide
	return kReplicaIPSet_Wide;
}


// --------------------------------------------------------------------------------
//	* ReplicaInIPSet
//
//	RETURNS: TRUE if inReplica is of class inLevel
// --------------------------------------------------------------------------------

bool ReplicaInIPSet( sPSServerEntry *inReplica, ReplicaIPLevel inLevel )
{
	unsigned char s1, s2;
	char *ipStr = inReplica->ip;
	int err = 0;
	struct in_addr ipAddr, ourIPAddr;
	in_addr_t *iplist = NULL;
	int index;
	
	// any valid IP is in wide
	if ( inLevel == kReplicaIPSet_Wide )
		return true;

	// current LDAP server is easy to check
	if ( inLevel == kReplicaIPSet_CurrentForLDAP )
		return inReplica->currentServerForLDAP;
	
// DEBUG
//else return false;

	// check special values
	// priority is:
	//	CurrentForLDAP
	//	localhost
	//	IPs hosted by this CPU's hostname
	//	10.x.x.x (private class A)
	//	172.16.x.x - 172.31.x.x (private class B)
	//	192.168.x.x (private class C)

	// localhost
	if ( strcmp( ipStr, "127.0.0.1" ) == 0 )
		return (inLevel == kReplicaIPSet_LocallyHosted);
	
	// extract IP address
	if ( inet_pton( AF_INET, ipStr, &ipAddr ) != 1 )
		return false;
	
	// IPs hosted on this CPU
	err = pwsf_LocalIPList( &iplist );
	if ( err == kCPSUtilOK && iplist != NULL )
	{
		for ( index = 0; index < kMaxIPAddrs && iplist[index] != 0; index++ )
		{
			if ( ipAddr.s_addr == iplist[index] )
			{
				free( iplist );
				return (inLevel == kReplicaIPSet_LocallyHosted);
			}
		}
	}
	
	if ( inLevel == kReplicaIPSet_LocallyHosted ) {
		if ( iplist != NULL )
			free( iplist );
		return false;
	}
		
	// NOTE: in order to do this right, we need to get the actual
	// subnet for each interface. In the absence of that information, 
	// be conservative-but-reasonable and assume that servers inside
	// 255.255.255.0 are in the subnet.
	
	if ( err == kCPSUtilOK && iplist != NULL )
	{
		ipAddr.s_addr = (ipAddr.s_addr & 0xFFFFFF00);
		
		for ( index = 0; index < kMaxIPAddrs && iplist[index] != 0; index++ )
		{
			ourIPAddr.s_addr = (iplist[index] & 0xFFFFFF00);
			
			if ( ourIPAddr.s_addr == ipAddr.s_addr )
			{
				free( iplist );
				return (inLevel == kReplicaIPSet_InSubnet);
			}
		}
		
		free( iplist );
		iplist = NULL;
	}
	
	if ( inLevel == kReplicaIPSet_InSubnet )
		return false;
	
	// s1 and s2 are ip segments 1&2
	s1 = *((unsigned char *)&ipAddr.s_addr);
	s2 = *(((unsigned char *)&ipAddr.s_addr) + 1);
	
	// private class A
	if ( s1 == 10 )
		return (inLevel == kReplicaIPSet_PrivateNet);
	
	// private class B
	if ( s1 == 172 && s2 <= 31 && s2 >= 16 )
		return (inLevel == kReplicaIPSet_PrivateNet);
	
	// private class C
	if ( s1 == 192 && s2 == 168 )
		return (inLevel == kReplicaIPSet_PrivateNet);
	
	if ( inLevel == kReplicaIPSet_PrivateNet )
		return false;
	
	return false;
}


// --------------------------------------------------------------------------------
//	* pwsf_LocalIPList
//
//	RETURNS: CPSUtilies error enum
//
//	Retrieves the interface list and extracts all of the IP addresses for this
//	system. A 0L value terminates the list. The calling function is responsible
//	for calling free() if the returned array is non-NULL.
// --------------------------------------------------------------------------------

int pwsf_LocalIPList( in_addr_t **outIPList )
{
	if ( outIPList == NULL )
		return (int)kCPSUtilParameterError;

	int result = (int)kCPSUtilOK;
	*outIPList = NULL;
    
	in_addr_t* ipList = (in_addr_t *) calloc( sizeof(in_addr_t), kMaxIPAddrs + 1 );
	if ( ipList == NULL )
    {
		result = (int)kCPSUtilMemoryError;
	}
    else
    {
        struct ifaddrs *ifaList = NULL;
        if (getifaddrs(&ifaList) != 0)
        {
            result = (int)kCPSUtilFail;
            free(ipList);
            ipList = NULL;
        }
        else
        {
            int ipCount = 0;
            struct ifaddrs* ifa;
            for (ifa = ifaList;  ifa != NULL && ipCount < kMaxIPAddrs;  ifa = ifa->ifa_next)
            {        
                if (ifa->ifa_addr != NULL && ifa->ifa_addr->sa_family == AF_INET)
                {
                    struct sockaddr_in *sain = (struct sockaddr_in *)(ifa->ifa_addr);
                    ipList[ipCount++] = ntohl(sain->sin_addr.s_addr);
                }
            }
            
            freeifaddrs(ifaList);
            ifaList = NULL;
        }
    }

	*outIPList = ipList;
	return result;
}


// --------------------------------------------------------------------------------
//	* getconn_async
//
//	RETURNS: CPSUtilies error enum
//
//	Connects to a socket with a semi-sync behavior. The function will block
//	for up to <inOpenTimeout> and then return. If a connection is established,
//	the socket is returned in <inOutSocket>. Otherwise, the socket is closed.
// --------------------------------------------------------------------------------

int getconn_async( const char *host, const char *port, struct timeval *inOpenTimeout, float *outConnectTime, int *inOutSocket )
{
    char servername[1024];
    struct sockaddr_in sin;
	struct addrinfo *res, *res0;
    int sock = -1;
    int siResult = 0;
    int rc, err;
	struct in_addr inetAddr;
	char *endPtr = NULL;
	struct timeval startTime, endTime;
	struct timeval recvTimeoutVal = { 10, 0 };
	struct timeval sendTimeoutVal = { 10, 0 };
	struct timezone tz = { 0, 0 };
	int fcntlFlags = 0;
	bool bOurSocketToCloseOnErr = false;
	
    if ( host==NULL || port==NULL || inOutSocket==NULL )
        return kCPSUtilParameterError;
    
	if ( outConnectTime != NULL )
		*outConnectTime = 0;
	
    try
    {
		if ( *inOutSocket == 0 )
		{
			strlcpy(servername, host, sizeof(servername));
			
			/* map hostname -> IP */
			rc = inet_aton(servername, &inetAddr);
			if ( rc == 1 )
			{
				sin.sin_addr.s_addr = inetAddr.s_addr;
			}
			else
			{
				err = getaddrinfo( servername, NULL, NULL, &res0 );
				if (err != 0) {
					DEBUGLOG("getaddrinfo");
					throw((int)kCPSUtilServiceUnavailable);
				}
				
				for ( res = res0; res != NULL; res = res->ai_next )
				{
					if ( res->ai_family != AF_INET || res->ai_addrlen != sizeof(sockaddr_in) )
						continue;
					
					memcpy( &sin.sin_addr, &(((struct sockaddr_in *)(res->ai_addr))->sin_addr.s_addr), 4 );
				}
				
				freeaddrinfo( res0 );
			}
			
			/* map port -> num */
			sin.sin_port = htons(strtol(port, &endPtr, 10));
			if ((sin.sin_port == 0) || (endPtr == port)) {
				DEBUGLOG( "port '%s' unknown", port);
				throw((int)kCPSUtilParameterError);
			}
			
			sin.sin_family = AF_INET;
			
			/* connect */
			for ( int dontgetzero = 0; dontgetzero < 5; dontgetzero++ )
			{
				sock = socket(AF_INET, SOCK_STREAM, 0);
				if (sock < 0) {
					DEBUGLOG("socket");
					throw((int)kCPSUtilServiceUnavailable);
				}
				
				if ( sock != 0 )
					break;
			}
			if ( sock == 0 )
			{
				DEBUGLOG("socket() keeps giving me zero. hate that!");
				throw((int)kCPSUtilServiceUnavailable);
			}
			
			gOpenCount++;
			bOurSocketToCloseOnErr = true;
			
			if ( setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &recvTimeoutVal, (socklen_t)sizeof(recvTimeoutVal) ) == -1 )
			{
				DEBUGLOG("setsockopt SO_RCVTIMEO");
				throw((int)kCPSUtilServiceUnavailable);
			}
			if ( setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &sendTimeoutVal, (socklen_t)sizeof(sendTimeoutVal) ) == -1 )
			{
				DEBUGLOG("setsockopt SO_SNDTIMEO");
				//throw((int)kCPSUtilServiceUnavailable); // not fatal
			}
			
			fcntlFlags = fcntl(sock, F_GETFL, 0);
			if ( fcntlFlags == -1 )
			{
				DEBUGLOG("fcntl");
				throw((int)kCPSUtilServiceUnavailable);
			}
			if ( fcntl(sock, F_SETFL, fcntlFlags | O_NONBLOCK) == -1 )
			{
				DEBUGLOG("fcntl");
				throw((int)kCPSUtilServiceUnavailable);
			}
		}
		else
		{
			sock = *inOutSocket;
		}
		
		gettimeofday( &startTime, &tz );
        siResult = connect(sock, (struct sockaddr *) &sin, (socklen_t)sizeof (sin));
        
		// reset to normal blocking I/O
		if ( fcntl(sock, F_SETFL, fcntlFlags) == -1 )
		{
			DEBUGLOG("fcntl");
			throw((int)kCPSUtilServiceUnavailable);
		}

		// If the connect succeeds immediately, the result is 0 and there's nothing more to do.
		// Normally it'll return -1 with errno set to EINPROGRESS and we need to see if the
		// connect completes.  If errno is anything else then the connect failed.
		if ( siResult == -1 && errno == EINPROGRESS )
		{
			// Now see if the socket is available for writing.	That should indicate
			// the connection is ready.
			struct pollfd pfd = { sock, POLLOUT, 0 };
			siResult = poll( &pfd, 1, (int)(inOpenTimeout->tv_sec * 1000 + inOpenTimeout->tv_usec / 1000) ); 
			if ( siResult == 1 && (pfd.revents & POLLOUT) )
			{
				char test;
				
				// we got this far, recvfrom/errno is the only choice for confirmation
				// if the IP is valid but there are no listeners on the port, errno is ECONNREFUSED.
				// if recvfrom() == 0 then the connection was reset by peer.
				// if errno is EAGAIN then the connect process is still in progress.
				siResult = (int)recvfrom( sock, &test, 1, (MSG_PEEK | MSG_DONTWAIT), NULL, NULL );
				DEBUGLOG( "getconn_async recvfrom = %d, errno = %d", (int)siResult, errno );
				if ( siResult > 0 || (siResult == -1 && errno == EAGAIN) )
				{
					siResult = 0;
				}
				else
				{
					close( sock );
					sock = -1;
					gOpenCount--;
					throw((int)kCPSUtilServiceUnavailable);
				}
			}
			else
			{
				close( sock );
				sock = -1;
				gOpenCount--;
				throw((int)kCPSUtilServiceUnavailable);
			}
		}
		gettimeofday( &endTime, &tz );
		    
		if ( outConnectTime != NULL )
			*outConnectTime = (float)(endTime.tv_sec - startTime.tv_sec) + (float)(endTime.tv_usec - startTime.tv_usec)/(float)1000000;
	}
	
    catch( int error )
    {
        siResult = error;
    }
    
	if ( siResult != 0 && bOurSocketToCloseOnErr && sock != -1 )
	{
		close( sock );
		sock = -1;
		gOpenCount--;
	}
	else
		*inOutSocket = sock;
    
    return siResult;
}


// --------------------------------------------------------------------------------
//	* testconn_udp
//
//	RETURNS: CPSUtilies error enum
//
//	Sends a udp request to the password server. If the request was sent, the
//	socket is returned in <outSocket>.
// --------------------------------------------------------------------------------

int testconn_udp( const char *host, const char *port, int *outSocket )
{
    char servername[1024];
    struct sockaddr_in sin, cin;
    int sock = -1;
    int siResult = 0;
    int rc;
	struct in_addr inetAddr;
	char *endPtr = NULL;
	const char *packetData = "What do you know?";
	ssize_t byteCount;
	struct addrinfo *res, *res0;
    
    if ( host==NULL || port==NULL || outSocket==NULL )
        return kCPSUtilParameterError;
    
    try
    {
		strlcpy(servername, host, sizeof(servername));
		
		/* map hostname -> IP */
		rc = inet_aton(servername, &inetAddr);
		if ( rc == 1 )
		{
			sin.sin_addr.s_addr = inetAddr.s_addr;
		}
		else
		{
			rc = getaddrinfo( servername, NULL, NULL, &res0 );
			if (rc != 0) {
				DEBUGLOG("getaddrinfo");
				throw(kCPSUtilServiceUnavailable);
			}
			
			for ( res = res0; res != NULL; res = res->ai_next )
			{
				if ( res->ai_family != AF_INET || res->ai_addrlen != sizeof(sockaddr_in) )
					continue;
				
				memcpy( &sin.sin_addr, &(((struct sockaddr_in *)(res->ai_addr))->sin_addr.s_addr), 4 );
			}
			
			freeaddrinfo( res0 );
		}
		
		/* map port -> num */
		sin.sin_port = htons(strtol(port, &endPtr, 10));
		if ((sin.sin_port == 0) || (endPtr == port)) {
			DEBUGLOG( "port '%s' unknown", port);
			throw(kCPSUtilParameterError);
		}
		
		sin.sin_family = AF_INET;
		
		/* connect */
		for ( int dontgetzero = 0; dontgetzero < 5; dontgetzero++ )
		{
			sock = socket(AF_INET, SOCK_DGRAM, 0);
			if (sock < 0) {
				DEBUGLOG("socket");
				throw(kCPSUtilServiceUnavailable);
			}
			
			if ( sock != 0 )
				break;
		}
		if ( sock == 0 )
		{
			DEBUGLOG("socket() keeps giving me zero. hate that!");
			throw(kCPSUtilServiceUnavailable);
		}
		
		bzero( &cin, sizeof(cin) );
		cin.sin_family = AF_INET;
		cin.sin_addr.s_addr = htonl( INADDR_ANY );
		cin.sin_port = htons( 0 );
		
        siResult = bind( sock, (struct sockaddr *) &cin, (socklen_t)sizeof(cin) );
		if ( siResult < 0 )
		{
			DEBUGLOG( "bind() failed." );
			throw( kCPSUtilServiceUnavailable );
		}
		
		byteCount = sendto( sock, packetData, strlen(packetData), 0, (struct sockaddr *)&sin, (socklen_t)sizeof(sin) );
		if ( byteCount < 0 )
		{
			DEBUGLOG( "sendto() failed." );
			throw( kCPSUtilServiceUnavailable );
		}
	}
	
    catch( int error )
    {
		if ( error != 0 && sock > 0 )
		{
			close( sock );
			sock = -1;
		}
        siResult = error;
    }
    
    *outSocket = sock;
    
    return siResult;
}

// ---------------------------------------------------------------------------
//	* pwsf_pingpws
//
//  Returns: 1 if responding, 0 if not, -1 if error
// ---------------------------------------------------------------------------

int pwsf_pingpws( const char *host, const struct timespec *timeout )
{
	int sd = -1;
	int kq = -1;
	int ret = 0;
	int r;
	struct kevent inev[1];
	struct kevent outev[1];
	unsigned char buf[128];

	do {
		if( testconn_udp( host, kPasswordServerPortStr, &sd) != 0 ) {
			ret = -1;
			break;
		}

		EV_SET(&inev[0], sd, EVFILT_READ, EV_ADD, 0, 0, 0);
	
		kq = kqueue();
		if( kq < 0 ) {
			ret = -1;
			break;
		}

		r = kevent(kq, inev, 1, outev, 1, timeout);
		if( r == -1 ) {
			ret = -1;
			break;
		} else if( r == 0 ) {
			ret = 0;
			break;
		}

		r = recvfrom(sd, buf, sizeof(buf)-1, MSG_DONTWAIT, NULL, 0);
		if( r < 0 ) {
			ret = -1;
			break;
		} else if( (r > 0) && (buf[0] != '0') ) {
			ret = 1;
			break;
		}
		ret = 0;
	}while(0);

	if( sd != -1 )
		close(sd);
	if( kq != -1 )
		close(kq);

	return ret;
}

// ---------------------------------------------------------------------------
//	* pwsf_ProcessIsRunning
//
//  Returns: -1 if not running, or pid
// ---------------------------------------------------------------------------

pid_t pwsf_ProcessIsRunning( const char *inProcName )
{
	register size_t		i ;
	register pid_t 		pidLast		= -1 ;
	int					mib[]		= { CTL_KERN, KERN_PROC, KERN_PROC_ALL, 0 };
	size_t				ulSize		= 0;

	// Allocate space for complete process list
	if ( 0 > sysctl( mib, 4, NULL, &ulSize, NULL, 0) )
	{
		return( pidLast );
	}

	i = ulSize / sizeof( struct kinfo_proc );
	struct kinfo_proc	*kpspArray = new kinfo_proc[ i ];
	if ( !kpspArray )
	{
		return( pidLast );
	}

	// Get the proc list
	ulSize = i * sizeof( struct kinfo_proc );
	if ( 0 > sysctl( mib, 4, kpspArray, &ulSize, NULL, 0 ) )
	{
		delete [] kpspArray;
		return( pidLast );
	}

	register struct kinfo_proc	*kpsp = kpspArray;
	//register pid_t 				pidParent = -1, pidProcGroup = -1;

	for ( ; i-- ; kpsp++ )
	{
		// Skip names that don't match
		if ( strcmp( kpsp->kp_proc.p_comm, inProcName ) != 0 )
		{
			continue;
		}

		// Skip our id
		if ( kpsp->kp_proc.p_pid == ::getpid() )
		{
			continue;
		}

		// If it's not us, is it a zombie
		if ( kpsp->kp_proc.p_stat == SZOMB )
		{
			continue;
		}

		// If the name matches, break
		if ( strcmp( kpsp->kp_proc.p_comm, inProcName ) == 0 )
		{
			pidLast = kpsp->kp_proc.p_pid;
			break;
		}
	}

	delete [] kpspArray;

	return( pidLast );
} // pwsf_ProcessIsRunning


// ----------------------------------------------------------------------------------------
//  pwsf_GetSASLMechInfo
//
//	Returns: TRUE if <inMechName> is in the table.
// ----------------------------------------------------------------------------------------

bool pwsf_GetSASLMechInfo( const char *inMechName, char **outPluginPath, bool *outRequiresPlainTextOnDisk )
{
	int index;
	bool found = false;
	SASLMechInfo knownMechList[] =
	{
		{"APOP",				"apop.la",				true},				
		{"CRAM-MD5",			"libcrammd5.la",		false},
		{"CRYPT",				"crypt.la",				false},
		{"DHX",					"dhx.la",				false},
		{"DIGEST-MD5",			"libdigestmd5.la",		false},
		{"GSSAPI",				"libgssapiv2.la",		false},
		{"KERBEROS_V4",			"libkerberos4.la",		false},
		{"MS-CHAPv2",			"mschapv2.la",			false},
		{"NTLM",				"libntlm.la",			false},
		{"OTP",					"libotp.la",			false},
		{"PPS",					"libpps.la",			false},
		{"SMB-LAN-MANAGER",		"smb_lm.la",			false},
		{"SMB-NT",				"smb_nt.la",			false},
		{"SMB-NTLMv2",			"smb_ntlmv2.la",		false},
		{"TWOWAYRANDOM",		"twowayrandom.la",		true},
		{"WEBDAV-DIGEST",		"digestmd5WebDAV.la",	true},
		{"",					"",						false}
	};
	
	for ( index = 0; knownMechList[index].name[0] != '\0'; index++ )
	{
		if ( strcasecmp(inMechName, knownMechList[index].name) == 0 )
		{
			if ( outPluginPath != NULL ) {
				*outPluginPath = (char *) malloc( strlen(knownMechList[index].filename) + 1 );
				if ( *outPluginPath != NULL ) {
					strcpy( *outPluginPath, knownMechList[index].filename );
				}
			}
			
			if ( outRequiresPlainTextOnDisk != NULL )
				*outRequiresPlainTextOnDisk = knownMechList[index].requiresPlain;
			
			found = true;
			break;
		}
	}
	
	return found;
}


// ----------------------------------------------------------------------------------------
//  pwsf_mkdir_p
//
//	Returns: 0=ok, -1=fail
// ----------------------------------------------------------------------------------------

int pwsf_mkdir_p( const char *path, mode_t mode )
{
	int err = 0;
	char buffer[PATH_MAX];
	char *segPtr;
	char *inPtr;
		
	// make the directory
	int len = snprintf( buffer, sizeof(buffer), "%s", path );
	if ( len >= (int)sizeof(buffer) - 1 )
		return -1;
	
	inPtr = buffer;
	if ( *inPtr == '/' )
		inPtr++;
	while ( inPtr != NULL )
	{
		segPtr = strsep( &inPtr, "/" );
		if ( segPtr != NULL )
		{
			err = mkdir( buffer, mode );
			if ( err != 0 && errno != EEXIST )
				break;
			err = 0;
			
			if ( inPtr != NULL )
				*(inPtr - 1) = '/';
		}
	}
	
	return err;
}


//------------------------------------------------------------------------------------------------
//	pwsf_EnumerateDirectory
//
//	Returns: 0 = noErr, -1 or errno
//------------------------------------------------------------------------------------------------

// smooth transition for the symbol
int EnumerateDirectory( const char *inDirPath, const char *inStartsWith, CFMutableArrayRef *outFileArray )
{
	return pwsf_EnumerateDirectory( inDirPath, inStartsWith, outFileArray );
}

int pwsf_EnumerateDirectory( const char *inDirPath, const char *inStartsWith, CFMutableArrayRef *outFileArray )
{
    /* 1 for '/' 1 for trailing '\0' */
	char prefix[PATH_MAX+2] = {0,};
	char str[PATH_MAX] = {0,};
	char c;
    int pos;
    int position;
    DIR *dp;
    struct dirent *dir;
	size_t minFileLength = 0;
	CFMutableArrayRef lArray = NULL;
	CFMutableStringRef filePathRef;
	
	if ( inDirPath == NULL || outFileArray == NULL )
		return -1;
	
	if ( inStartsWith != NULL )
		minFileLength = strlen( inStartsWith );
	
	lArray = CFArrayCreateMutable( kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks );
	if ( lArray == NULL )
		return -1;
	
    position = 0;
    do
	{
		pos = 0;
		do {
			c = inDirPath[position];
			position++;
			str[pos] = c;
			pos++;
		}
		while ((c != ':') && (c != '=') && (c != 0));
		str[pos-1] = '\0';
		
		strcpy( prefix, str );
		strcat( prefix, "/" );
		
		dp = opendir( str );
		if ( dp != NULL ) /* ignore errors */    
		{
			while ( (dir = readdir(dp)) != NULL )
			{
				size_t length;
				
				/* check file type */
				if ( dir->d_type != DT_REG )
					continue;
				
				length = strlen( dir->d_name );
				
				/* can not possibly be what we're looking for */
				if ( length < minFileLength ) 
					continue; 
				
				/* too big */
				if ( length + pos >= PATH_MAX )
					continue; 
				
				/* starts-with */
				if ( inStartsWith != NULL && strncmp(dir->d_name, inStartsWith, minFileLength) != 0 )
					continue;
				
				filePathRef = CFStringCreateMutable( kCFAllocatorDefault, 0 );
				if ( filePathRef == NULL )
					return -1;
				
				CFStringAppendCString( filePathRef, prefix, kCFStringEncodingUTF8 );
				CFStringAppendCString( filePathRef, dir->d_name, kCFStringEncodingUTF8 );
				CFArrayAppendValue( lArray, filePathRef ); 
				CFRelease( filePathRef );
				filePathRef = NULL;
			}
			
			closedir( dp );
		}
    }
	while ( (c != '=') && (c != 0) );
	
	*outFileArray = lArray;
    return 0;
}


//--------------------------------------------------------------------------------------------------
//	pwsf_LaunchTask
//
//	Returns: exit status of the task
//--------------------------------------------------------------------------------------------------

int pwsf_LaunchTask(const char *path, char *const argv[])
{
	int outputPipe[2] = {0};
	pid_t pid = -1;
	int status = -1;
	int waitResult = 0;
	
	pipe(outputPipe);
	
	pid = vfork();
	if (pid == -1)
		return -1;
	
	/* Handle the child */
	if (pid == 0)
	{
		dup2(outputPipe[1], fileno(stdout));
		execv(path, argv);

		/* This should never be reached */
		_exit(1);
	}
	
	/* Now the parent */
	waitResult = waitpid( pid, &status, 0 );
	if ( waitResult <= 0 )
		status = 0;
	else
	if ( waitResult > 0 && WIFEXITED(status) )
		status = WEXITSTATUS( status );
	
	close(outputPipe[1]);
	close(outputPipe[0]);
	
	return status;
}


//--------------------------------------------------------------------------------------------------
//	pwsf_LaunchTaskWithIO
//
//	Returns: exit status of the task
//--------------------------------------------------------------------------------------------------

int pwsf_LaunchTaskWithIO(
	const char *path,
	char *const argv[],
	const char* inputBuf,
	char* outputBuf,
	size_t outputBufSize,
	bool *outExitedBeforeInput)
{
	int inputPipe[2];
	int outputPipe[2];
	pid_t pid;
	int status;
	int waitResult = 0;
	bool exitedBeforeInput = false;
	
	if (inputBuf != NULL)
		pipe(inputPipe);
	
	if (outputBuf != NULL)
		pipe(outputPipe);
	
	pid = vfork();
	if (pid == -1)
		return -1;

	/* Handle the child */
	if (pid == 0)
	{
		if (inputBuf != NULL)
			dup2(inputPipe[0], fileno(stdin));
		if (outputBuf != NULL)
			dup2(outputPipe[1], fileno(stdout));
		
		execv(path, argv);

		/* This should never be reached */
		_exit(1);
	}
	
	/* Now the parent */
	if (inputBuf != NULL)
	{
		close(inputPipe[0]);
		
		/* We're about to write. Check for miscarriages. */
		waitResult = waitpid( pid, &status, WNOHANG );
		if ( waitResult == -1 )
		{
			switch(errno)
			{
				case ECHILD:
				case EFAULT:
				case EINVAL:
					exitedBeforeInput = true;
					break;
				
				case EINTR:
				default:
					break;
			}
		}
		else
		{
			exitedBeforeInput = (waitResult == pid);
		}
		if ( !exitedBeforeInput )
			write(inputPipe[1], inputBuf, strlen(inputBuf));
		close(inputPipe[1]);
	}
	
	if ( ! exitedBeforeInput )
	{
		waitResult = waitpid( pid, &status, WNOHANG );
		for ( int waitCount = 0; waitResult == 0 || waitResult == -1; waitCount++ )
		{
			if ( waitResult == -1 )
			{
				if ( errno == ECHILD ) {
					/* we're done */
					break;
				}
				else if ( errno == EFAULT || errno == EINVAL ) {
					/* huh? */
					status = -4;
					break;
				}
				/* keep going if EINTR */
			}
			
			// give up after 10 minutes
			if ( waitCount > (100*60*10) )
			{
				// time for euthanasia
				kill( pid, SIGKILL );
				
				// cannot use WNOHANG to reap a SIGKILL
				waitResult = waitpid( pid, &status, 0 );
				status = -4;
				break;
			}
			
			// measured 10ms to poll 4X min, 10X avg on a dual G4/1.4GHz, 10.3.2 (7D28)
			usleep(10000);
			waitResult = waitpid( pid, &status, WNOHANG );
			if ( status == -4 )
				break;
		}
	}
	
	if (outputBuf != NULL)
	{
		size_t sizeRead;
		close(outputPipe[1]);
		sizeRead = read(outputPipe[0], outputBuf, outputBufSize-1);
		outputBuf[sizeRead] = 0;
		close(outputPipe[0]);
	}
	
	if ( status != -4 )
		status = WEXITSTATUS(status);
	
	if (outExitedBeforeInput != NULL)
		*outExitedBeforeInput = exitedBeforeInput;
	
	return status;
}


//--------------------------------------------------------------------------------------------------
//	LaunchTaskWithIO2
//
//	Returns: exit status of the task, or -4 if Kerberos is stuck
//
//	Variant of LaunchTaskWithIO that also pipes stderr.
//--------------------------------------------------------------------------------------------------

int pwsf_LaunchTaskWithIO2(
	const char *path,
	char *const argv[],
	const char* inputBuf,
	char* outputBuf,
	size_t outputBufSize,
	char* errBuf,
	size_t errBufSize)
{
	int inputPipe[2];
	int outputPipe[2];
	int errPipe[2];
	pid_t pid;
	int status;
	int waitResult = 0;
	bool exitedBeforeInput = false;
	int waitCount = 0;
	
	if (inputBuf != NULL)
		pipe(inputPipe);
	
	if (outputBuf != NULL)
		pipe(outputPipe);
		
	if (errBuf != NULL)
		pipe(errPipe);
	
	pid = vfork();
	if (pid == -1)
		return -1;

	/* Handle the child */
	if (pid == 0)
	{
		if (inputBuf != NULL)
			dup2(inputPipe[0], fileno(stdin));
		if (outputBuf != NULL)
			dup2(outputPipe[1], fileno(stdout));
		if (errBuf != NULL)
			dup2(errPipe[1], fileno(stderr));
		
		execv(path, argv);

		/* This should never be reached */
		_exit(1);
	}
	
	/* Now the parent */
	if (inputBuf != NULL)
	{
		close(inputPipe[0]);
		
		/* We're about to write. Check for miscarriages. */
		waitResult = waitpid( pid, &status, WNOHANG );
		if ( waitResult == -1 )
		{
			switch(errno)
			{
				case ECHILD:
				case EFAULT:
				case EINVAL:
					exitedBeforeInput = true;
					break;
				
				case EINTR:
				default:
					break;
			}
		}
		else
		{
			exitedBeforeInput = (waitResult == pid);
		}
		if ( !exitedBeforeInput )
			write(inputPipe[1], inputBuf, strlen(inputBuf));
		close(inputPipe[1]);
	}
	
	if ( ! exitedBeforeInput )
	{
		waitResult = waitpid( pid, &status, WNOHANG );
		for ( waitCount = 0; waitResult == 0 || waitResult == -1; waitCount++ )
		{
			if ( waitResult == -1 )
			{
				if ( errno == ECHILD ) {
					/* we're done */
					break;
				}
				else if ( errno == EFAULT || errno == EINVAL ) {
					/* huh? */
					status = -4;
					break;
				}
				/* keep going if EINTR */
			}
			
			// give up after 10 minutes
			if ( waitCount > (100*60*10) )
			{
				// time for euthanasia
				kill( pid, SIGKILL );
				
				// cannot use WNOHANG to reap a SIGKILL
				waitResult = waitpid( pid, &status, 0 );
				status = -4;
				break;
			}
			
			// measured 10ms to poll 4X min, 10X avg on a dual G4/1.4GHz, 10.3.2 (7D28)
			usleep(10000);
			waitResult = waitpid( pid, &status, WNOHANG );
			if ( status == -4 )
				break;
		}
	}
	
	if (outputBuf != NULL)
	{
		size_t sizeRead;
		close(outputPipe[1]);
		sizeRead = read(outputPipe[0], outputBuf, outputBufSize-1);
		outputBuf[sizeRead] = 0;
		close(outputPipe[0]);
	}
	if (errBuf != NULL)
	{
		size_t sizeRead;
		close(errPipe[1]);
		sizeRead = read(errPipe[0], errBuf, errBufSize-1);
		errBuf[sizeRead] = 0;
		close(errPipe[0]);
	}
	
	if ( status != -4 )
		status = WEXITSTATUS(status);
	
	return status;
}

