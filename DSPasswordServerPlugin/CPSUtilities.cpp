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

#include <sys/types.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <netdb.h>
#include <sys/stat.h>
#include <syslog.h>

#include <ctype.h>
#include <sys/utsname.h>
#include <sys/ioctl.h>
#include <net/if.h>		// interface struture ifreq, ifconf
#include <net/if_dl.h>	// datalink structs
#include <net/if_types.h>

#include "CPSUtilities.h"
#include "SASLCode.h"
#include "CAuthFileBase.h"

#define kMinTrialTime		140000
#define kMaxTrialTime		1250000
#define kMaxIPAddrs			32

#if 1
static bool gDSDebuggingON = false;
#define DEBUGLOG(A,args...)		if (gDSDebuggingON) syslog( LOG_INFO, (A), ##args )
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
    
    if ( buf != NULL )
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
	
	if ( buf == NULL )
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
	unsigned long byteCount;
	
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
		consumeBuf = (char *) malloc( consumeLen );
		if ( consumeBuf == NULL ) {
			result.err = -1;
			return result;
		}
		byteCount = ::recvfrom( fd, consumeBuf, consumeLen, MSG_DONTWAIT, NULL, NULL );
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
	int compareLen;

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
    
    result = sasl_encode64( (char *)inData, inLen, tempBuf, bufLen, &outLen );
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
    
    result = sasl_decode64( readPtr, strlen(readPtr), (char *)outData, maxLen, &sasl_outlen );
    
    *outLen = (attached_outlen > 0) ? attached_outlen : (unsigned long)sasl_outlen;
    
    return result;
}


// ---------------------------------------------------------------------------
//	* ConnectToServer
// ---------------------------------------------------------------------------

long ConnectToServer( sPSContextData *inContext )
{
    long siResult = 0;
    char buf[1024];
    
	DEBUGLOG( "ConnectToServer trying %s:%s", inContext->psName, inContext->psPort);
	
    // connect to remote server
    siResult = getconn(inContext->psName, inContext->psPort, &inContext->fd);
    if ( siResult != 0 )
        return( siResult );
    
	gOpenCount++;
	
    inContext->serverIn = fdopen(inContext->fd, "r");
    inContext->serverOut = fdopen(inContext->fd, "w");
    
    // discard the greeting message
    readFromServer(inContext->fd, buf, sizeof(buf));
    
    return siResult;
}


// ----------------------------------------------------------------------------
//	* Connected
//
//	Is the socket connection still open?
// ----------------------------------------------------------------------------

Boolean Connected( sPSContextData *inContext )
{
	int		bytesReadable = 0;
	char	temp[1];

	if ( inContext->fd == 0 )
		return false;
	
	bytesReadable = ::recvfrom( inContext->fd, temp, sizeof (temp), (MSG_DONTWAIT | MSG_PEEK), NULL, NULL );
	
	DEBUGLOG( "Connected bytesReadable = %d, errno = %d", bytesReadable, errno );
	
	if ( bytesReadable == -1 )
	{
		// The problem here is that the plug-in is multi-threaded and it is
		// too likely that errno could be changed before it can be read.
		// However, since a disconnected socket typically returns bytesReadable==0,
		// it is more or less safe to assume that -1 means the socket is still open.
		
		switch ( errno )
		{
			case EAGAIN:
				// no data in the socket but socket is still open and connected
				return true;
				break;
			
			case EBADF:
			case ENOTCONN:
			case ENOTSOCK:
			case EINTR:
			case EFAULT:
				// valid failing error
				return false;
				break;
			
			default:
				// invalid error
				return true;
				break;
		}
	}
	
	// recvfrom() only returns 0 when the peer has closed the connection (read an EOF)
	if ( bytesReadable == 0 )
	{
		return( false );
	}

	return( true );

} // Connected


// ----------------------------------------------------------------------------
//	* IdentifyReachableReplica
//
//	RETURNS: CPSUtilities error enum
//	
//	If our current server is not responsive, this routine is used to
//	start asynchronous connections to all replicas. The first one to answer
//	becomes our new favored server.
//	If the connection is establish using TCP, outSock is returned. Otherwise,
//	it is set to -1.
// ----------------------------------------------------------------------------

long IdentifyReachableReplica( CFMutableArrayRef inServerArray, const char *inHexHash, sPSServerEntry *outReplica, int *outSock )
{
	long siResult = 0;
	char *portNumStr;
	sPSServerEntry *entrylist = NULL;
	CFIndex servIndex, servCount;
	int connectedSocket = -1;
	bool connectedSocketIsTCP = false;
	int tempsock;
	struct timeval timeout;
	float connectTime;
	int levelIndex;
	int *socketList = NULL;
	bool checkUDPDescriptors = false;
	bool fallbackToTCP = false;
	bool usingDefaultPort106 = true;
	
	DEBUGLOG( "IdentifyReachableReplica inHexHash=%s", inHexHash ? inHexHash : "NULL" );
	
	if ( inServerArray == NULL || outReplica == NULL || outSock == NULL )
		return kCPSUtilParameterError;
	
	bzero( outReplica, sizeof(sPSServerEntry) );
	*outSock = -1;
	
	try
	{
		siResult = ConvertCFArrayToServerArray( inServerArray, &entrylist, &servCount );
		if ( siResult != kCPSUtilOK )
			return siResult;
		
		// nothing to do
		if ( servCount == 0 || entrylist == NULL )
			return kCPSUtilOK;
				
		for ( levelIndex = kReplicaIPSet_LocallyHosted;
				levelIndex <= kReplicaIPSet_Wide && connectedSocket == -1;
				levelIndex++ )
		{
DEBUGLOG( "for levelIndex" );

			socketList = (int *) calloc( servCount, sizeof(int) );
			
			if ( levelIndex == kReplicaIPSet_LocallyHosted )
			{
				timeout.tv_sec = 10;
				timeout.tv_usec = 0;
			}
			else
			{
				GetTrialTime( servCount, &timeout );
			}
			
			for ( servIndex = 0; servIndex < servCount; servIndex++ )
			{
DEBUGLOG( "for servIndex" );
				
				if ( entrylist[servIndex].ip[0] == '\0' ) {
					DEBUGLOG( "entrylist[servIndex].ip[0] == 0" );
					continue;
				}
				
				if ( inHexHash != NULL && entrylist[servIndex].id[0] != '\0' && strcmp( inHexHash, entrylist[servIndex].id ) != 0 )
					continue;
				
				if ( servCount > 1 )
				{
					if ( ! ReplicaInIPSet( &(entrylist[servIndex]), (ReplicaIPLevel)levelIndex ) )
						continue;
				}
				
				portNumStr = strchr( entrylist[servIndex].ip, ':' );
				if ( portNumStr != NULL )
				{
					*portNumStr = '\0';
					strncpy(entrylist[servIndex].port, portNumStr+1, 10);
					entrylist[servIndex].port[9] = '\0';
					usingDefaultPort106 = false;
				}
				else
				{
					strcpy(entrylist[servIndex].port, "106");
				}
				
				siResult = 0;
				checkUDPDescriptors = false;
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
					siResult = getconn_async( entrylist[servIndex].ip, entrylist[servIndex].port, &timeout, &connectTime, &tempsock );
					
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
			}
			
			// check any queued UDP requests
			if ( checkUDPDescriptors && socketList != NULL )
			{
				int structlength;
				int byteCount;
				struct sockaddr_in cin;
				char packetData;
				fd_set fdset;
				struct timeval selectTimeout = { 0, 750000 };
				
				if ( levelIndex == kReplicaIPSet_LocallyHosted )
				{
					selectTimeout.tv_sec = 10;
					selectTimeout.tv_usec = 0;
				}    
			
				bzero( &cin, sizeof(cin) );
				cin.sin_family = AF_INET;
				cin.sin_addr.s_addr = htonl( INADDR_ANY );
				cin.sin_port = htons( 0 );
				
				// TODO: should use select()
				FD_ZERO( &fdset );
				for ( servIndex = 0; servIndex < servCount; servIndex++ )
					if ( socketList[servIndex] > 0 )
						FD_SET( socketList[servIndex], &fdset );
				
				select( FD_SETSIZE, &fdset, NULL, NULL, &selectTimeout );
				
				for ( servIndex = 0; servIndex < servCount && connectedSocket == -1; servIndex++ )
				{
DEBUGLOG( "for servIndex2" );
					if ( socketList[servIndex] > 0 )
					{
						structlength = sizeof( cin );
						byteCount = recvfrom( socketList[servIndex], &packetData, sizeof(packetData), MSG_DONTWAIT, (struct sockaddr *)&cin, &structlength );
						DEBUGLOG( "recvfrom() byteCount=%d.", byteCount );
					
						if ( byteCount > 0 && packetData != '0' )
						{
							connectedSocket = socketList[servIndex];
							memcpy( outReplica, &entrylist[servIndex], sizeof(sPSServerEntry) );
							
							// can use the new port #
							if ( usingDefaultPort106 )
								strcpy( outReplica->port, kPasswordServerPortStr );
							break;
						}
					}
				}
				
				// close all the sockets
				for ( servIndex = 0; servIndex < servCount; servIndex++ )
				{
DEBUGLOG( "for servIndex3" );
					if ( socketList[servIndex] > 0 )
					{
						close( socketList[servIndex] );
						socketList[servIndex] = -1;
					}
				}
			}
			if ( socketList != NULL ) {
				free( socketList );
				socketList = NULL;
			}
			
			if ( servCount == 1 )
				break;
		}
	}
	catch( long error )
	{
		siResult = error;
	}
	catch( ... )
	{
		siResult = kCPSUtilServiceUnavailable;
	}
	
	// make sure to return a replica or an error
	if ( siResult == 0 && connectedSocket <= 0 )
		siResult = kCPSUtilServiceUnavailable;
	
	// return the socket
	if ( connectedSocketIsTCP )
		*outSock = connectedSocket;
	
	// clean up
	if ( entrylist != NULL )
		free( entrylist );
	
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

long ConvertCFArrayToServerArray( CFArrayRef inCFArray, sPSServerEntry **outServerArray, CFIndex *outCount )
{
	CFDataRef serverRef;
	sPSServerEntry *entrylist = NULL;
	CFIndex servIndex;
	CFIndex servCount = 0;
	
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
			
			memcpy( &(entrylist[servIndex]), CFDataGetBytePtr(serverRef), sizeof(sPSServerEntry) );
	DEBUGLOG( "entrylist[%d].ip=%s, entrylist[].id=%s, ipFromNode=%d", servIndex, entrylist[servIndex].ip, entrylist[servIndex].id, entrylist[servIndex].ipFromNode );
		}
	}
	
	*outServerArray = entrylist;
	*outCount = servCount;
	
	return kCPSUtilOK;
}


// ---------------------------------------------------------------------------
//	* GetTrialTime
//
//	Returns: number of microseconds, less than a full second, to attempt
//			 binding.
// ---------------------------------------------------------------------------

void GetTrialTime( long inReplicaCount, struct timeval *outTrialTime )
{
	long timeval;

	if ( inReplicaCount <= 0 )
		timeval = kMaxTrialTime;
	else
	{
		timeval = ( kMaxTrialTime / inReplicaCount );
		if ( timeval < kMinTrialTime )
			timeval = kMinTrialTime;
	}
	
	outTrialTime->tv_sec = timeval / 1000000;
	outTrialTime->tv_usec = timeval % 1000000;
}


// ---------------------------------------------------------------------------
//	* GetBigNumber
//
//	RETURNS: CPSUtilities enum
// ---------------------------------------------------------------------------

long GetBigNumber( sPSContextData *inContext, char **outBigNumStr )
{
	long				siResult			= kCPSUtilOK;
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
	
	if ( inContext->castKeySet )
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
		if ( inContext->castKeySet )
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

long
GetPasswordServerList( CFMutableArrayRef *outServerList, int inConfigSearchOptions )
{
	long status = 0;
	long status1 = 0;
	CFMutableArrayRef serverArray;
	
DEBUGLOG( "GetPasswordServerList");

	if ( outServerList == NULL )
		return -1;
	
	*outServerList = NULL;
	
	if ( inConfigSearchOptions == 0 )
		return 0;
	
	serverArray = CFArrayCreateMutable( kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks );
	if ( serverArray == NULL )
		return -1;
	
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
//	* GetServerListFromLocalCache
// ---------------------------------------------------------------------------

long
GetServerListFromLocalCache( CFMutableArrayRef inOutServerList )
{
	CFStringRef myReplicaDataFilePathRef;
	CFURLRef myReplicaDataFileRef;
	CFReadStreamRef myReadStreamRef;
	CFPropertyListRef myPropertyListRef = NULL;
	CFStringRef errorString;
	CFPropertyListFormat myPLFormat;
	CFIndex index, arrayCount;
	CFDataRef dataRef;
	long status = 0;
    sPSServerEntry *anEntryPtr;
	bool bLoadPreConfiguredFile = false;
	struct stat sb;
	
DEBUGLOG( "GetServerListFromLocalCache");

	if ( stat( kPWReplicaPreConfiguredFile, &sb ) == 0 )
	{
		bLoadPreConfiguredFile = true;
	}
	else
	{
		if ( stat( kPWReplicaLocalFile, &sb ) != 0 )
			return -1;
		
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
			return -1;
		
		myReplicaDataFileRef = CFURLCreateWithFileSystemPath( kCFAllocatorDefault, myReplicaDataFilePathRef, kCFURLPOSIXPathStyle, false );
		
		CFRelease( myReplicaDataFilePathRef );
		
		if ( myReplicaDataFileRef == NULL )
			return -1;
		
		myReadStreamRef = CFReadStreamCreateWithFile( kCFAllocatorDefault, myReplicaDataFileRef );
		
		CFRelease( myReplicaDataFileRef );
		
		if ( myReadStreamRef == NULL )
			return -1;
		
		CFReadStreamOpen( myReadStreamRef );
		
		errorString = NULL;
		myPLFormat = kCFPropertyListXMLFormat_v1_0;
		myPropertyListRef = CFPropertyListCreateFromStream( kCFAllocatorDefault, myReadStreamRef, 0, kCFPropertyListMutableContainersAndLeaves, &myPLFormat, &errorString );
		
		CFReadStreamClose( myReadStreamRef );
		CFRelease( myReadStreamRef );
		
		if ( errorString != NULL )
		{
			char errMsg[256];
			
			if ( CFStringGetCString( errorString, errMsg, sizeof(errMsg), kCFStringEncodingUTF8 ) )
				DEBUGLOG( "could not load the local replica cache file, error = %s", errMsg );
			CFRelease( errorString );
		}
		
		if ( myPropertyListRef == NULL )
			return -1;
	
		if ( CFGetTypeID(myPropertyListRef) != CFArrayGetTypeID() )
		{
			CFRelease( myPropertyListRef );
			return -1;
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

long
GetServerListFromFile( CFMutableArrayRef inOutServerList )
{
	CReplicaFile replicaFile;
	long status = 0;
	
	status = GetServerListFromXML( &replicaFile, inOutServerList );
DEBUGLOG( "GetServerListFromFile = %d", (int)status);

	return status;
}


// ---------------------------------------------------------------------------
//	* GetServerListFromConfig
// ---------------------------------------------------------------------------

long GetServerListFromConfig( CFMutableArrayRef *outServerList, CReplicaFile *inReplicaData )
{
	long status = 0;
	CFMutableArrayRef serverArray;
	
DEBUGLOG( "GetServerListFromConfig");

	if ( outServerList == NULL || inReplicaData == NULL )
		return -1;
	
	*outServerList = NULL;
	
	serverArray = CFArrayCreateMutable( kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks );
	if ( serverArray == NULL )
		return -1;
	
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

long
GetServerListFromXML( CReplicaFile *inReplicaFile, CFMutableArrayRef inOutServerList )
{
	CFDictionaryRef serverDict;
	UInt32 repIndex;
	UInt32 repCount;
	long status = 0;
	sPSServerEntry serverEntry;
	char serverID[33];
	
DEBUGLOG( "in GetServerListFromXML");

	if ( inOutServerList == NULL )
		return -1;
	
	bzero( &serverEntry, sizeof(sPSServerEntry) );
	
	if ( inReplicaFile->GetUniqueID( serverID ) )
		strcpy( serverEntry.id, serverID );

DEBUGLOG( "serverEntry.id=%s", serverEntry.id);
	
	serverDict = inReplicaFile->GetParent();
	if ( serverDict != NULL )
	{
DEBUGLOG( "has parent");
		status = GetServerFromDict( serverDict, &serverEntry );
		if ( status == 0 )
			AppendToArrayIfUnique( inOutServerList, &serverEntry );
	}
else
{
DEBUGLOG( "no parent");
}
	
	repCount = inReplicaFile->ReplicaCount();
DEBUGLOG( "repCount=%d", (int)repCount);
	for ( repIndex = 0; repIndex < repCount; repIndex++ )
	{
		serverDict = inReplicaFile->GetReplica( repIndex );
		if ( serverDict != NULL )
		{
			status = GetServerFromDict( serverDict, &serverEntry );
			if ( status == 0 )
				AppendToArrayIfUnique( inOutServerList, &serverEntry );
		}
	}
	
DEBUGLOG( "GetServerListFromXML = %d", (int)status);
	return status;
}


// ---------------------------------------------------------------------------
//	* GetServerFromDict
// ---------------------------------------------------------------------------

long
GetServerFromDict( CFDictionaryRef serverDict, sPSServerEntry *outServerEntry )
{
	long status = 0;
	CFTypeRef anIPRef;
	CFStringRef aString;
	
DEBUGLOG( "GetServerListFromDict");

	if ( serverDict == NULL || outServerEntry == NULL )
		return -1;
	
	// IP
	if ( ! CFDictionaryGetValueIfPresent( serverDict, CFSTR(kPWReplicaIPKey), (const void **)&anIPRef ) )
		return -1;
	
	if ( CFGetTypeID(anIPRef) == CFStringGetTypeID() )
	{
		if ( ! CFStringGetCString( (CFStringRef)anIPRef, outServerEntry->ip, sizeof(outServerEntry->ip), kCFStringEncodingUTF8 ) )
			return -1;
	}
	else
	if ( CFGetTypeID(anIPRef) == CFArrayGetTypeID() )
	{
		aString = (CFStringRef) CFArrayGetValueAtIndex( (CFArrayRef)anIPRef, 0 );
		if ( aString == NULL || CFGetTypeID(aString) != CFStringGetTypeID() )
			return -1;
		
		if ( ! CFStringGetCString( aString, outServerEntry->ip, sizeof(outServerEntry->ip), kCFStringEncodingUTF8 ) )
			return -1;
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
	
	replicaCount = CFArrayGetCount( inReplicaArray );
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
	
	// This is getting corrupted somehow
	//serverRef = CFDataCreate( kCFAllocatorDefault, (const unsigned char *)anEntryPtr, sizeof(sPSServerEntry) );
	
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


bool ReplicaInIPSet( sPSServerEntry *inReplica, ReplicaIPLevel inLevel )
{
	unsigned char s1, s2;
	char *ipStr = inReplica->ip;
	long err = 0;
	struct in_addr ipAddr, ourIPAddr;
	unsigned long *iplist = NULL;
	int index;
	
	// any valid IP is in wide
	if ( inLevel == kReplicaIPSet_Wide )
		return true;
// DEBUG
//else return false;

	// check special values
	// priority is:
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

long pwsf_LocalIPList( unsigned long **outIPList )
{
	// interface structures
	struct ifconf ifc;
	struct ifreq ifrbuf[30];
	struct ifreq *ifrptr;
	struct sockaddr_in *sain;
	unsigned long *iplist;
	register int sock = 0;
	register int i = 0;
	register int ipcount = 0;
	int rc = 0;
	long result = kCPSUtilOK;
	
	if ( outIPList == NULL )
		return kCPSUtilParameterError;
	*outIPList = NULL;
	
	iplist = (unsigned long *) calloc( sizeof(unsigned long), kMaxIPAddrs + 1 );
	if ( iplist == NULL )
		return kCPSUtilMemoryError;
	
	try
	{
		sock = socket(AF_INET, SOCK_DGRAM, 0);
		if ( sock == -1 )
			throw(1);
		
		ifc.ifc_buf = (caddr_t)ifrbuf;
		ifc.ifc_len = sizeof(ifrbuf);
		rc = ::ioctl(sock, SIOCGIFCONF, &ifc);
		close( sock );
		if ( rc == -1 )
			throw(1);
		
		// walk the interface and  address list, only interested in ethernet and AF_INET
		ipcount = 0;
		for ( ifrptr = (struct ifreq *)ifc.ifc_buf, i=0;
				(char *) ifrptr < &ifc.ifc_buf[ifc.ifc_len] && i < kMaxIPAddrs;
				ifrptr = IFR_NEXT(ifrptr), i++ )
		{
			if ( (strncmp( ifrptr->ifr_name, "en", 2) == 0) &&
				 (ifrptr->ifr_addr.sa_family == AF_INET) )
			{
				// ethernet interface
				sain = (struct sockaddr_in *)&(ifrptr->ifr_addr);
				iplist[ipcount] = ntohl(sain->sin_addr.s_addr);
				ipcount++;
			}
		}
	} // try
	catch ( ... )
	{
		if ( iplist != NULL ) {
			free( iplist );
			iplist = NULL;
		}
		
		result = kCPSUtilFail;
	}
	
	*outIPList = iplist;
	
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

long getconn_async( const char *host, const char *port, struct timeval *inOpenTimeout, float *outConnectTime, int *inOutSocket )
{
    char servername[1024];
    struct sockaddr_in sin;
	struct addrinfo *res, *res0;
    int sock = -1;
    long siResult = 0;
    int rc, err;
	struct in_addr inetAddr;
	char *endPtr = NULL;
	struct timeval startTime, endTime;
	struct timeval recvTimeoutVal = { 30, 0 };
	struct timezone tz = { 0, 0 };
	fd_set fdset;
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
			strncpy(servername, host, sizeof(servername) - 1);
			servername[sizeof(servername) - 1] = '\0';
			
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
					throw((long)kCPSUtilServiceUnavailable);
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
				throw((long)kCPSUtilParameterError);
			}
			
			sin.sin_family = AF_INET;
			
			/* connect */
			for ( int dontgetzero = 0; dontgetzero < 5; dontgetzero++ )
			{
				sock = socket(AF_INET, SOCK_STREAM, 0);
				if (sock < 0) {
					DEBUGLOG("socket");
					throw((long)kCPSUtilServiceUnavailable);
				}
				
				if ( sock != 0 )
					break;
			}
			if ( sock == 0 )
			{
				DEBUGLOG("socket() keeps giving me zero. hate that!");
				throw((long)kCPSUtilServiceUnavailable);
			}
			
			gOpenCount++;
			bOurSocketToCloseOnErr = true;
			
			if ( setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &recvTimeoutVal, sizeof(recvTimeoutVal) ) == -1 )
			{
				DEBUGLOG("setsockopt");
				throw((long)kCPSUtilServiceUnavailable);
			}
			
			fcntlFlags = fcntl(sock, F_GETFL, 0);
			if ( fcntlFlags == -1 )
			{
				DEBUGLOG("fcntl");
				throw((long)kCPSUtilServiceUnavailable);
			}
			if ( fcntl(sock, F_SETFL, fcntlFlags | O_NONBLOCK) == -1 )
			{
				DEBUGLOG("fcntl");
				throw((long)kCPSUtilServiceUnavailable);
			}
		}
		else
		{
			sock = *inOutSocket;
		}
		
		gettimeofday( &startTime, &tz );
        siResult = connect(sock, (struct sockaddr *) &sin, sizeof (sin));
		
		// reset to normal blocking I/O
		if ( fcntl(sock, F_SETFL, fcntlFlags) == -1 )
		{
			DEBUGLOG("fcntl");
			throw((long)kCPSUtilServiceUnavailable);
		}
		
		// If the connect succeeds immediately, the result is 0.
		// it should return -1 with errno set to EINPROGRESS.
		if ( siResult == -1 )
		{
			// threaded, can't depend on errno, so just call select and see what happens
			// to make things worse, if an IP is valid but there are no listeners on the port,
			// select returns that the descriptors are ready (it would be more convenient for
			// this routine if it returned -1).
			FD_ZERO(&fdset);
			FD_SET(sock, &fdset);
			
			// select() for writing is the recommended way to test for connect() completion
			siResult = select(FD_SETSIZE, NULL, &fdset, NULL, inOpenTimeout);
			if ( siResult > 0 )
			{
				char test;
				
				// we got this far, recvfrom/errno is the only choice for confirmation
				// if the IP is valid but there are no listeners on the port, errno is ECONNREFUSED.
				// if recvfrom() == 0 then the connection was reset by peer.
				errno = 0;
				siResult = recvfrom( sock, &test, 1, (MSG_PEEK | MSG_DONTWAIT), NULL, NULL );
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
					throw((long)kCPSUtilServiceUnavailable);
				}
			}
			else
			{
				close( sock );
				sock = -1;
				gOpenCount--;
				throw((long)kCPSUtilServiceUnavailable);
			}
		}
		gettimeofday( &endTime, &tz );
		    
		if ( outConnectTime != NULL )
			*outConnectTime = (endTime.tv_sec - startTime.tv_sec) + (float)(endTime.tv_usec - startTime.tv_usec)/(float)1000000;
	}
	
    catch( long error )
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

long testconn_udp( const char *host, const char *port, int *outSocket )
{
    char servername[1024];
    struct sockaddr_in sin, cin;
    int sock = 0;
    long siResult = 0;
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
		strncpy(servername, host, sizeof(servername) - 1);
		servername[sizeof(servername) - 1] = '\0';
		
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
				throw((long)kCPSUtilServiceUnavailable);
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
			throw((long)kCPSUtilParameterError);
		}
		
		sin.sin_family = AF_INET;
		
		/* connect */
		for ( int dontgetzero = 0; dontgetzero < 5; dontgetzero++ )
		{
			sock = socket(AF_INET, SOCK_DGRAM, 0);
			if (sock < 0) {
				DEBUGLOG("socket");
				throw((long)kCPSUtilServiceUnavailable);
			}
			
			if ( sock != 0 )
				break;
		}
		if ( sock == 0 )
		{
			DEBUGLOG("socket() keeps giving me zero. hate that!");
			throw((long)kCPSUtilServiceUnavailable);
		}
				
		bzero( &cin, sizeof(cin) );
		cin.sin_family = AF_INET;
		cin.sin_addr.s_addr = htonl( INADDR_ANY );
		cin.sin_port = htons( 0 );
		
        siResult = bind( sock, (struct sockaddr *) &cin, sizeof(cin) );
		if ( siResult < 0 )
		{
			DEBUGLOG( "bind() failed." );
			throw( (long)kCPSUtilServiceUnavailable );
		}
		
		byteCount = sendto( sock, packetData, strlen(packetData), 0, (struct sockaddr *)&sin, sizeof(sin) );
		if ( byteCount < 0 )
		{
			DEBUGLOG( "sendto() failed." );
			throw( (long)kCPSUtilServiceUnavailable );
		}
	}
	
    catch( long error )
    {
        siResult = error;
    }
    
    *outSocket = sock;
    
    return siResult;
}






