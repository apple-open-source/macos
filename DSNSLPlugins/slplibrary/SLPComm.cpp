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
 *  @header SLPComm
 *  IPC calling conventions for communication with slpd
 *   0                   1                   2                   3
 *   0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
 *  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *  |            Code               |            Length             |
 *  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *  |           Status              |                Data           /
 *  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
*/

#define _MATH_H_		// to squash OVERFLOWFLAG redef

#include <stdio.h>
#include <string.h>
#include <sys/un.h>
#include <sys/socket.h>
#include <sys/ipc.h>
#include <unistd.h>
#include <sys/wait.h> // for wait
#include <sys/time.h>	// for struct timeval (via resource.h)
#include <sys/resource.h>	// for getrlimit()

#include <DirectoryService/DirServicesTypes.h>

#include "mslp_sd.h"
#include "slp.h"
#include "mslp.h"
#include "mslpd_store.h"
#include "mslp_dat.h"
#include "mslplib.h"

#include "SLPDefines.h"
#include "SLPComm.h"
#include "CNSLTimingUtils.h"

#pragma mark еее Communication Routines еее
OSStatus SendDataToSLPd(	char* data,
                            UInt32 dataLen,
                            char** returnData,
                            UInt32* returnDataLen )
{
    SLP_LOG( SLP_LOG_DEBUG, "******** RunSLPLoad called ********" );
    return SendDataViaIPC( kSLPdPath, data, dataLen, returnData, returnDataLen );
}

OSStatus SendDataToSLPRAdmin(	char* data,
                                UInt32 dataLen,
                                char** returnData,
                                UInt32* returnDataLen )
{
    return SendDataViaIPC( kRAdminIPCPath, data, dataLen, returnData, returnDataLen );
}

OSStatus SendDataViaIPC(	char* ipc_path,
                            char* data,
                            UInt32 dataLen,
                            char** returnData,
                            UInt32* returnDataLen )
{
    OSStatus 				status = noErr;
    struct sockaddr_un		ourAddrBlock;
    int 					servlen, socketDescriptor = socket( AF_UNIX, SOCK_STREAM, 0 );

    *returnData = NULL;
    *returnDataLen = 0;

    if ( socketDescriptor < 0 )
    {
        status = socketDescriptor;
    }
    else
    {
        char		internalBuffer[kTCPBufferSize];

        bzero( (char*)&ourAddrBlock, sizeof(ourAddrBlock) );

        ourAddrBlock.sun_family				= AF_UNIX;

        strcpy( ourAddrBlock.sun_path, ipc_path );
        servlen = strlen(ourAddrBlock.sun_path) + sizeof(ourAddrBlock.sun_family) + 1;

        status = connect( socketDescriptor, (struct sockaddr*)&ourAddrBlock, servlen );

        if ( status < 0 && strcmp(ipc_path, kRAdminIPCPath) != 0 )
        {
            if ( errno == EINTR )
                status = noErr;		// try again
            else            // slpd may not be running, let's fire it up with our slpLoad tool
                status = RunSLPLoad();

            // try again
            if ( !status )
                status = connect( socketDescriptor, (struct sockaddr*)&ourAddrBlock, servlen );
       
            if ( status )
                status = errno;
        }

        if ( status < 0 )
        {
            if ( strcmp(ipc_path, kRAdminIPCPath) != 0 )
                SLP_LOG( SLP_LOG_ERR, "SendDataToSLPd: can't connect to server! error:%s", strerror(errno) );
            
            if ( status )
                status = errno;
        }
        else
            status = 0;

        if ( !status )
        {
            status = write( socketDescriptor, data, dataLen );
            
            if ( status < 0 && errno == EINTR )
            {
                SLP_LOG( SLP_LOG_ERR, "SendDataToSLPd: write received EINTR, try again" );
                
                status = write( socketDescriptor, data, dataLen );
            }
            
            if ( (UInt32)status != dataLen )
            {
                SLP_LOG( SLP_LOG_ERR, "SendDataToSLPd: write error on socket! error:%s", strerror(errno) );
       
                if ( status )
                    status = errno;
            }
            else
            {
                status = noErr;			// reset this as it contains the number of bytes written...
                
                int readnResult = readn( socketDescriptor, internalBuffer, sizeof(SLPdMessageHeader) );
 
                if ( readnResult < 0 )
                {
                    if (errno == EINTR) 
                    {
                        SLP_LOG( SLP_LOG_DROP, "SendDataToSLPd readn received EINTR, try again" );
                        readnResult = readn( socketDescriptor, internalBuffer, sizeof(SLPdMessageHeader) );
                    }
                    
                    if ( readnResult < 0 )
                        status = errno;
                    else
                        *returnDataLen = readnResult;
                }
                else
                    *returnDataLen = readnResult;
                
                if ( *returnDataLen < sizeof(SLPdMessageHeader) && !status )
                {
                    status = kSLPInvalidMessageType;
                    SLP_LOG( SLP_LOG_ERR, "SendDataToSLPd, received < sizeof(SLPdMessageHeader) from sender (%ld bytes)", *returnDataLen );
                }
                else if ( !status )
                {
                    SLPdMessageHeader*	headPtr = (SLPdMessageHeader*)internalBuffer;
                    readnResult = readn( socketDescriptor, &internalBuffer[*returnDataLen], (headPtr->messageLength)-sizeof(SLPdMessageHeader) );

                    if ( readnResult < 0 )
                        status = errno;
                    else
                        *returnDataLen += readnResult;

                    SLP_LOG( SLP_LOG_DEBUG, "SendDataToSLPd, received %ld bytes from sender", *returnDataLen );
                }
            }
        }

        if ( !status && *returnDataLen )
        {
            *returnData = (char*)::malloc( *returnDataLen );
            ::memcpy( *returnData, internalBuffer, *returnDataLen );
        }
    
        close( socketDescriptor );
    }

    return status;
}

static void detach (void)
{
	register int	i = OPEN_MAX ;
	struct rlimit	lim ;

	// Daemonize, i.e. change our parent process to init (pid==1)
	if (getppid() != 1)
		if (fork() != 0)
			exit (0);

	// Find the true upper limit on file descriptors.
	if (!getrlimit (RLIMIT_NOFILE, &lim))
		i = lim.rlim_cur ;
	// Close all file descriptors except std*.
	while (i--)
		close (i) ;

	// Additional daemon initialization.
	errno = 0;
	setsid ();
	chdir ("/");
	umask (S_IWGRP|S_IWOTH);
}

OSStatus RunSLPLoad( void )
{
    OSStatus	status = noErr;

    SLP_LOG( SLP_LOG_DEBUG, "RunSLPLoad called" );
        
    register pid_t  pidChild = -1 ;

    if ( (pidChild = ::fork()) != 0 )
    {
    // No processing in the parent; pause for the child.
        if( pidChild == -1 )
            status = pidChild;
		else
		{
			int		nStatus, nErr;
			do
			{
				nErr = ::waitpid( pidChild, &nStatus, 0 );
			}
			while(( nErr == -1 ) && ( errno == EINTR ));
			status = nStatus;
		}

        SmartSleep(1*USEC_PER_SEC);

        return status;
    }

    //Launch the loader.

    detach();
    
    status = execl ("/usr/sbin/slpd", "slpd", "-f", "/etc/slpsa.conf", 0);

    return status;

}

#pragma mark -
#pragma mark еее Creation Routines еее
char* MakeSLPRegistrationDataBuffer( char* scopeList, UInt32 scopeListLen, char* url, UInt32 urlLen, char* attributeList, UInt32 attributeListLen, UInt32* dataBufferLen )
{
    return MakeSLPRegDeregDataBuffer( kSLPRegisterURL, scopeList, scopeListLen, url, urlLen, attributeList, attributeListLen, dataBufferLen );
}

char* MakeSLPDeregistrationDataBuffer( char* scopeList, UInt32 scopeListLen, char* url, UInt32 urlLen, UInt32* dataBufferLen )
{
    return MakeSLPRegDeregDataBuffer( kSLPDeregisterURL, scopeList, scopeListLen, url, urlLen, NULL, 0, dataBufferLen );
}

char* MakeSLPRegDeregDataBuffer( SLPdMessageType messageType, char* scopeList, UInt32 scopeListLen, char* url, UInt32 urlLen, char* attributeList, UInt32 attributeListLen, UInt32* dataBufferLen )
{
    char*		curPtr = NULL;
    char*		dataBuffer = NULL;

    *dataBufferLen = sizeof(SLPdMessageHeader) + sizeof(scopeListLen) + scopeListLen + sizeof(urlLen) + urlLen + sizeof(attributeListLen) + attributeListLen;
    dataBuffer = (char*)::malloc( *dataBufferLen );

    if ( dataBuffer )
    {
        SLPdMessageHeader*	header = (SLPdMessageHeader*)dataBuffer;
        header->messageType = messageType;
        header->messageLength = *dataBufferLen;
        header->messageStatus = 0;

        curPtr = dataBuffer + sizeof(SLPdMessageHeader);

        ::memcpy( curPtr, &scopeListLen, sizeof(UInt32) );
        curPtr += sizeof(UInt32);

        ::memcpy( curPtr, scopeList, scopeListLen );
        curPtr += scopeListLen;

        ::memcpy( curPtr, &urlLen, sizeof(UInt32) );
        curPtr += sizeof(UInt32);

        ::memcpy( curPtr, url, urlLen );
        curPtr += urlLen;

        ::memcpy( curPtr, &attributeListLen, sizeof(UInt32) );
        curPtr += sizeof(UInt32);

        ::memcpy( curPtr, attributeList, attributeListLen );
    }
    else
    {
        dataBuffer = NULL;
        *dataBufferLen = 0;
    }

    return dataBuffer;
}

char* MakeSLPScopeListDataBuffer( char* scopeListPtr, UInt32 scopeListLen, UInt32* dataBufferLen )
{
    return _MakeSLPScopeListDataBuffer( kSLPScopeList, scopeListPtr, scopeListLen, dataBufferLen );
}

char* MakeSLPSetScopeListDataBuffer( char* scopeListPtr, UInt32 scopeListLen, UInt32* dataBufferLen )
{
    return _MakeSLPScopeListDataBuffer( kSLPSetScopeList, scopeListPtr, scopeListLen, dataBufferLen );
}

char* _MakeSLPScopeListDataBuffer( SLPdMessageType type, char* scopeListPtr, UInt32 scopeListLen, UInt32* dataBufferLen )
{
    char*		curPtr = NULL;
    char*		dataBuffer = NULL;

    if ( scopeListPtr )
    {
        *dataBufferLen = sizeof(SLPdMessageHeader) + scopeListLen;
        dataBuffer = (char*)::malloc( *dataBufferLen );
    }

    if ( dataBuffer )
    {
        SLPdMessageHeader*	header = (SLPdMessageHeader*)dataBuffer;
        header->messageType = type;
        header->messageLength = *dataBufferLen;
        header->messageStatus = 0;

        curPtr = dataBuffer + sizeof(SLPdMessageHeader);

        ::memcpy( curPtr, scopeListPtr, scopeListLen );
    }
    else
    {
        dataBuffer = NULL;
        *dataBufferLen = 0;
    }

    return dataBuffer;
}

OSStatus MakeSLPSimpleRequestDataBuffer( SLPdMessageType messageType, UInt32* dataBufferLen, char** dataBuffer )
{
    OSStatus		status = noErr;

    *dataBufferLen = sizeof(SLPdMessageHeader);
    *dataBuffer = (char*)::malloc( *dataBufferLen );

    if ( *dataBuffer )
    {
        SLPdMessageHeader*	header = *((SLPdMessageHeader**)dataBuffer);
        header->messageType = (UInt16)messageType;
        header->messageLength = *dataBufferLen;
        header->messageStatus = 0;
    }
    else
        status = eMemoryAllocError;

    return status;
}

OSStatus MakeSLPGetDAStatusDataBuffer( UInt32* dataBufferLen, char** dataBuffer )
{
    return MakeSLPSimpleRequestDataBuffer( kSLPDAStatusQuery, dataBufferLen, dataBuffer );
}

OSStatus MakeSLPDAStatus( SLPDAStatus daStatus, UInt32* dataBufferLen, char** dataBuffer )
{
    OSStatus		status = noErr;

    *dataBufferLen = sizeof(SLPdMessageHeader);
    *dataBuffer = (char*)::malloc( *dataBufferLen );

    if ( *dataBuffer )
    {
        SLPdMessageHeader*	header = *((SLPdMessageHeader**)dataBuffer);
        header->messageType = (UInt16)kSLPDAStatusReply;
        header->messageLength = *dataBufferLen;
        header->messageStatus = daStatus;
    }
    else
        status = eMemoryAllocError;

    return status;
}


OSStatus MakeSLPTurnOnDADataBuffer( UInt32* dataBufferLen, char** dataBuffer )
{
    return MakeSLPSimpleRequestDataBuffer( kSLPTurnOnDA, dataBufferLen, dataBuffer );
}

OSStatus MakeSLPTurnOffDADataBuffer( UInt32* dataBufferLen, char** dataBuffer )
{
    return MakeSLPSimpleRequestDataBuffer( kSLPTurnOffDA, dataBufferLen, dataBuffer );
}

OSStatus MakeSLPGetDAScopeListDataBuffer( UInt32* dataBufferLen, char** dataBuffer )
{
    return MakeSLPSimpleRequestDataBuffer( kSLPGetScopeList, dataBufferLen, dataBuffer );
}

OSStatus MakeSLPGetLoggingOptionsDataBuffer( UInt32* dataBufferLen, char** dataBuffer )
{
    return MakeSLPSimpleRequestDataBuffer( kSLPGetLoggingLevel, dataBufferLen, dataBuffer );
}

OSStatus MakeSLPSetLoggingOptionsDataBuffer( UInt32 logLevel, UInt32* dataBufferLen, char** dataBuffer )
{
    char*		curPtr = NULL;
    OSStatus	status = noErr;
    
    if ( dataBuffer )
    {
        *dataBufferLen = sizeof(SLPdMessageHeader) + sizeof(UInt32);
        *dataBuffer = (char*)::malloc( *dataBufferLen );

        SLPdMessageHeader*	header = (SLPdMessageHeader*)dataBuffer;
        header->messageType = kSLPSetLoggingLevel;
        header->messageLength = *dataBufferLen;
        header->messageStatus = 0;

        curPtr = (char*)header + sizeof(SLPdMessageHeader);

        ::memcpy( curPtr, &logLevel, sizeof(UInt32) );
    }
    else
    {
        status = kSLPNullBufferErr;
        *dataBufferLen = 0;
    }

    return status;
}

OSStatus MakeSLPGetRegisteredSvcsDataBuffer( UInt32* dataBufferLen, char** dataBuffer )
{
    return MakeSLPSimpleRequestDataBuffer( kSLPGetRegisteredSvcs, dataBufferLen, dataBuffer );
}

char* MakeSLPScopeDeletedDataBuffer(  char* scopePtr,
                                     UInt32 scopeLen,
                                     UInt32* dataBufferLen )
 {
    char*		curPtr = NULL;
    char*		dataBuffer = NULL;

    if ( scopePtr )
    {
        *dataBufferLen = sizeof(SLPdMessageHeader) + scopeLen;
        dataBuffer = (char*)::malloc( *dataBufferLen );
    }

    if ( dataBuffer )
    {
        SLPdMessageHeader*	header = (SLPdMessageHeader*)dataBuffer;
        header->messageType = kSLPScopeDeleted;
        header->messageLength = *dataBufferLen;
        header->messageStatus = 0;

        curPtr = dataBuffer + sizeof(SLPdMessageHeader);

        ::memcpy( curPtr, scopePtr, scopeLen );
    }
    else
    {
        dataBuffer = NULL;
        *dataBufferLen = 0;
    }

    return dataBuffer;
}

char* MakeSLPIOCallBuffer( 	Boolean	turnOnSLPd,
							UInt32* dataBufferLen )
 {
    char*		dataBuffer = NULL;
	
   *dataBufferLen = sizeof(SLPdMessageHeader);
	dataBuffer = (char*)::malloc( *dataBufferLen );

    if ( dataBuffer )
    {
        SLPdMessageHeader*	header = (SLPdMessageHeader*)dataBuffer;
        header->messageType = (turnOnSLPd)?kSLPStartUp:kSLPShutDown;
        header->messageLength = *dataBufferLen;
        header->messageStatus = 0;
    }
    else
    {
        dataBuffer = NULL;
        *dataBufferLen = 0;
    }

    return dataBuffer;
}

#pragma mark -
#pragma mark еее Parsing Routines еее
SLPdMessageType GetSLPMessageType( void* dataBuffer )
{
    SLPdMessageType		messageType = (SLPdMessageType)((SLPdMessageHeader*)dataBuffer)->messageType;

    if ( messageType < kSLPFirstMessageType || messageType > kSLPLastMessageType )
        messageType = kSLPInvalidMessageType;

    return messageType;
}

#pragma mark -
OSStatus GetSLPRegistrationDataFromBuffer( char* dataBuffer, char** scopeListPtr, UInt32* scopeListLen, char** urlPtr, UInt32* urlLen, char** attributeListPtr, UInt32* attributeListLen )
{
    return GetSLPRegDeregDataFromBuffer( kSLPRegisterURL, dataBuffer, scopeListPtr, scopeListLen, urlPtr, urlLen, attributeListPtr, attributeListLen );
}

OSStatus GetSLPDeregistrationDataFromBuffer( char* dataBuffer, char** scopeListPtr, UInt32* scopeListLen, char** urlPtr, UInt32* urlLen )
{
    char*		attributeListPtr = NULL;
	UInt32		attributeListLen = 0;
	
	return GetSLPRegDeregDataFromBuffer( kSLPDeregisterURL, dataBuffer, scopeListPtr, scopeListLen, urlPtr, urlLen, &attributeListPtr, &attributeListLen );
}

OSStatus GetSLPRegDeregDataFromBuffer( SLPdMessageType messageType, char* dataBuffer, char** scopeListPtr, UInt32* scopeListLen, char** urlPtr, UInt32* urlLen, char** attributeListPtr, UInt32* attributeListLen )
{
    char*		curPtr = (char*)dataBuffer;
    OSStatus	status = noErr;

    if ( dataBuffer && urlPtr && scopeListPtr )
    {
        *scopeListPtr = NULL;
        *urlPtr = NULL;
        *attributeListPtr = NULL;
        
        if ( ((SLPdMessageHeader*)curPtr)->messageType == (UInt16)messageType )
        {
            status = ((SLPdMessageHeader*)curPtr)->messageStatus;

            if ( !status )
            {
                curPtr = (*(char**)&dataBuffer) += sizeof(SLPdMessageHeader);

                *scopeListLen = *((UInt32*)curPtr);
                curPtr += sizeof(UInt32);

                if ( *scopeListLen )
                {
                    *scopeListPtr = curPtr;
                    curPtr += *scopeListLen;
                }

                *urlLen = *((UInt32*)curPtr);
                curPtr += sizeof(UInt32);

                if ( *urlLen )
                {
                    *urlPtr = curPtr;
                }
            }
            
            if ( messageType == kSLPRegisterURL )
            {
                curPtr += *urlLen;
                *attributeListLen = *((UInt32*)curPtr);
                curPtr += sizeof(UInt32);
                
                if( *attributeListLen )
                    *attributeListPtr = curPtr;
            }
        }
        else
            status = kSLPWrongMessageTypeErr;
    }
    else
    {
        status = kSLPNullBufferErr;
    }

    return status;
}

OSStatus GetSLPScopeListFromBuffer( 	char* dataBuffer,
                                        char** scopeListPtr,
                                        UInt32* scopeListLen )
{
    char*		curPtr = (char*)dataBuffer;
    OSStatus	status = noErr;

    if ( dataBuffer && scopeListPtr )
    {
        *scopeListPtr = NULL;

        if ( ((SLPdMessageHeader*)curPtr)->messageType == kSLPScopeList )
        {
            status = ((SLPdMessageHeader*)curPtr)->messageStatus;

            if ( !status )
            {
                curPtr = (*(char**)&dataBuffer) += sizeof(SLPdMessageHeader);

                *scopeListLen = *((UInt32*)curPtr);
                curPtr += sizeof(UInt32);

                if ( *scopeListLen )
                {
                    *scopeListPtr = curPtr;
                }
            }
        }
        else
            status = kSLPWrongMessageTypeErr;
    }
    else
    {
        status = kSLPNullBufferErr;
    }

    return status;
}

OSStatus GetSLPDALoggingLevelFromBuffer( 	char* dataBuffer,
                                            UInt32* loggingLevel )
{
    char*		curPtr = (char*)dataBuffer;
    OSStatus	status = noErr;

    if ( dataBuffer && loggingLevel )
    {
        if ( ((SLPdMessageHeader*)curPtr)->messageType == kSLPSetLoggingLevel || ((SLPdMessageHeader*)curPtr)->messageType == kSLPSetLoggingLevel )
        {
            status = ((SLPdMessageHeader*)curPtr)->messageStatus;

            if ( !status )
            {
                curPtr = (*(char**)&dataBuffer) += sizeof(SLPdMessageHeader);

                *loggingLevel = *((UInt32*)curPtr);
            }
        }
        else
            status = kSLPWrongMessageTypeErr;
    }
    else
    {
        status = kSLPNullBufferErr;
    }

    return status;
}

#pragma mark -
OSStatus GetSLPDAStatusFromBuffer( char* dataBuffer, SLPDAStatus* daStatus )
{
    OSStatus		status = noErr;

    if ( dataBuffer )
    {
        SLPdMessageHeader*	header = (SLPdMessageHeader*) dataBuffer;

        if ( header->messageType == kSLPDAStatusReply )
            *daStatus = header->messageStatus;
        else
            status = kSLPInvalidMessageType;
    }
    else
        status = kSLPNullBufferErr;

    return status;
}

const char* PrintableHeaderType( void* message )
{
	SLPdMessageHeader*	header = (SLPdMessageHeader*)(message);
    const char*			returnString;
    
    switch ( header->messageType )
    {
        case kSLPRegisterURL:					returnString = "kSLPRegisterURL"; break;
        case kSLPDeregisterURL:					returnString = "kSLPDeregisterURL"; break;
        case kSLPDAStatusQuery:					returnString = "kSLPDAStatusQuery"; break;
        case kSLPDAStatusReply:					returnString = "kSLPDAStatusReply"; break;
        case kSLPTurnOnDA:						returnString = "kSLPTurnOnDA"; break;
        case kSLPTurnOffDA:						returnString = "kSLPTurnOffDA"; break;
        case kSLPGetScopeList:					returnString = "kSLPGetScopeList"; break;
        case kSLPSetScopeList:					returnString = "kSLPSetScopeList"; break;
        case kSLPAddScope:						returnString = "kSLPAddScope"; break;
        case kSLPDeleteScope:					returnString = "kSLPDeleteScope"; break;
        case kSLPScopeList:						returnString = "kSLPScopeList"; break;
        case kSLPGetLoggingLevel:				returnString = "kSLPGetLoggingLevel"; break;
        case kSLPSetLoggingLevel:				returnString = "kSLPSetLoggingLevel"; break;
        case kSLPGetRegisteredSvcs:				returnString = "kSLPGetRegisteredSvcs"; break;
        case kSLPTurnOnRAdminNotifications:		returnString = "kSLPTurnOnRAdminNotifications"; break;
        case kSLPTurnOffRAdminNotifications:	returnString = "kSLPTurnOffRAdminNotifications"; break;
        case kSLPScopeDeleted:					returnString = "kSLPScopeDeleted"; break;
        
        default:								returnString = "kInvalidMessageType"; break;
    }
    
    return returnString;
}
