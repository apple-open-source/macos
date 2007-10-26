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
 * @header CClientEndPoint
 * Mach endpoint for DirectoryService Framework.
 */
#include <stdio.h>
#include <stdlib.h>				// for malloc()
#include <string.h>
#include <unistd.h>			   	// for getpid()
#include <time.h>				// for time()
#include <mach/mach.h>
#include <mach/mach_error.h>
#include <servers/bootstrap.h>
#include <syslog.h>

#include "CClientEndPoint.h"
#include "DirServicesTypes.h"
#include "DirServicesConst.h"
#include "DirServicesPriv.h"
#include "PrivateTypes.h"
#include "DSCThread.h"
#include "DirectoryServiceMIG.h"

#define kMsgSize	sizeof( sComData )
#define kIPCMsgSize	sizeof( sIPCMsg )

UInt32 CClientEndPoint::fMessageID	 = 0; //start with zero since GetMessageID pre-increments

//------------------------------------------------------------------------------
//	* CClientEndPoint:
//
//------------------------------------------------------------------------------

CClientEndPoint::CClientEndPoint ( const char *inSrvrName )
{
	fSrvrName = nil;

	if ( inSrvrName != nil )
	{
		fSrvrName = strdup(inSrvrName);
	}
	
	fReplyMsg	= NULL;
	fServerPort	= MACH_PORT_NULL;
	fSessionPort = MACH_PORT_NULL;
	
} // CClientEndPoint


//------------------------------------------------------------------------------
//	* ~CClientEndPoint:
//
//------------------------------------------------------------------------------

CClientEndPoint::~CClientEndPoint ( void )
{
	if ( fSrvrName != nil )
	{
		free( fSrvrName );
		fSrvrName = nil;
	}
	
	if ( fReplyMsg != NULL )
	{
		free( fReplyMsg );
		fReplyMsg = NULL;
	}
	
	if ( fServerPort != MACH_PORT_NULL )
	{
		mach_port_mod_refs( mach_task_self(), fServerPort, MACH_PORT_RIGHT_SEND, -1 );
		fServerPort = MACH_PORT_NULL;
	}
	
	if ( fSessionPort != MACH_PORT_NULL )
	{
		mach_port_mod_refs( mach_task_self(), fSessionPort, MACH_PORT_RIGHT_SEND, -1 );
		fSessionPort = MACH_PORT_NULL;
	}
} // ~CClientEndPoint


//------------------------------------------------------------------------------
//	* ~CClientEndPoint:
//
//------------------------------------------------------------------------------

inline UInt32 CClientEndPoint::GetMessageID ( void )
{
	return( ++fMessageID );
} // GetMessageID


//------------------------------------------------------------------------------
//	* Initialize
//
//------------------------------------------------------------------------------

SInt32 CClientEndPoint::Initialize ( void )
{
	kern_return_t kr = KERN_FAILURE;

	// let's get the server boot port right off the bat...
	if( fServerPort != MACH_PORT_NULL )
	{
		mach_port_mod_refs( mach_task_self(), fServerPort, MACH_PORT_RIGHT_SEND, -1 );
		fServerPort = MACH_PORT_NULL;
	}
	
	if( fSessionPort != MACH_PORT_NULL )
	{
		mach_port_mod_refs( mach_task_self(), fSessionPort, MACH_PORT_RIGHT_SEND, -1 );
		fSessionPort = MACH_PORT_NULL;
	}
	
	if (fSrvrName != nil) //always used
	{
		kr = bootstrap_look_up( bootstrap_port, fSrvrName, &fServerPort );
	}

	//pure retry for normal daemon connection if we weren't trying to open the normal port or we had no name
	if (kr != KERN_SUCCESS && (fSrvrName == nil || strcmp(fSrvrName, kDSStdMachPortName) != 0) ) 
	{
		kr = bootstrap_look_up( bootstrap_port, kDSStdMachPortName, &fServerPort );
		if (kr != KERN_SUCCESS)
		{
			LOG3( kStdErr, "*** failed bootstrap_look_up: %s at: %d: Msg = %s\n", __FILE__, __LINE__, mach_error_string( kr ) );
		}
	}
	
	return ( (kr == KERN_SUCCESS && dsmig_create_api_session(fServerPort, &fSessionPort) == KERN_SUCCESS) ? eDSNoErr : eServerSendError );
} // Initialize


//------------------------------------------------------------------------------
//	* SendServerMessage
//
//------------------------------------------------------------------------------

SInt32 CClientEndPoint::SendServerMessage ( sComData *inMsg )
{
	SInt32					result		= eServerSendError;
	bool					bTryAgain	= false;
	
	do
	{
		kern_return_t			kr			= MACH_SEND_INVALID_DEST;
		mach_msg_type_name_t	serverPoly	= MACH_MSG_TYPE_COPY_SEND; 
		
		// let's do the call
		if( fSessionPort != MACH_PORT_NULL )
		{
			char					replyFixedBuffer[ kMaxFixedMsg ];
			
			sComDataPtr				replyFixedData	= (sComDataPtr) replyFixedBuffer;
			mach_msg_type_number_t	replyFixedLen	= 0;
			vm_address_t			sendData		= 0;
			mach_msg_type_number_t	sendLen			= sizeof(sComData) + inMsg->fDataLength - 1;
			vm_address_t			replyData		= 0;
			mach_msg_type_number_t	replyLen		= 0;
			
			if( inMsg->fDataLength <= kMaxFixedMsgData )
			{
				kr = dsmig_api_call( fSessionPort, serverPoly, inMsg, sendLen, 0, 0, replyFixedData, &replyFixedLen, &replyData, &replyLen );
			}
			else
			{
				vm_read( mach_task_self(), (vm_address_t)inMsg, sendLen, &sendData, &sendLen );
				
				kr = dsmig_api_call( fSessionPort, serverPoly, NULL, 0, sendData, sendLen, replyFixedData, &replyFixedLen, &replyData, &replyLen );
				
				// let's deallocate the memory we allocated... if we failed..
				if( kr == MACH_SEND_INVALID_DEST )
				{
					vm_deallocate( mach_task_self(), sendData, sendLen );
				}
			}
			
			if( kr == KERN_SUCCESS )
			{
				sComDataPtr		pComData = NULL;
				UInt32			uiLength = 0;
				
				// if we have OOL data, we need to copy it and deallocate it..
				if( replyFixedLen )
				{
					pComData = (sComDataPtr) replyFixedData;
					uiLength = replyFixedLen;
				}
				else if( replyLen ) 
				{
					pComData = (sComDataPtr) replyData;
					uiLength = replyLen;
				}
				
				// if this is a valid reply..
				if( pComData != NULL && pComData->fDataLength == (uiLength - (sizeof(sComData) - 1)) )
				{
					fReplyMsg = (sComData *) calloc( sizeof(char), sizeof(sComData) + pComData->fDataSize );
					
					bcopy( pComData, fReplyMsg, uiLength );
					
					result = eDSNoErr;
				}
				
				// if we had reply data OOL, let's free it appropriately...
				if( replyLen )
				{
					vm_deallocate( mach_task_self(), replyData, replyLen );			
				}
			}
		}
		
		if( bTryAgain == false && kr == MACH_SEND_INVALID_DEST )
		{
			mach_port_mod_refs( mach_task_self(), fSessionPort, MACH_PORT_RIGHT_SEND, -1 );
			fSessionPort = MACH_PORT_NULL;
			
			if( dsmig_create_api_session( fServerPort, &fSessionPort ) == KERN_SUCCESS )
			{
				bTryAgain = true;
			}
			else
			{
				// maybe bootstrap port is bad?  Let's try recovering that instead.
				bTryAgain = (Initialize() == 0);
			}
		}
		else
		{
			bTryAgain = false;
		}
		
	} while( bTryAgain );
		
	return result;
} // SendServerMessage


//------------------------------------------------------------------------------
//	* GetServerReply
//
//------------------------------------------------------------------------------

SInt32 CClientEndPoint::GetServerReply ( sComData **outMsg )
{
	SInt32	siResult = eServerReplyError;
	
	if( fReplyMsg != NULL )
	{
		if( *outMsg )
			free( *outMsg );

		*outMsg = fReplyMsg;
		fReplyMsg = NULL;
		
		siResult = eDSNoErr;
	}

	return( siResult );

} // GetServerReply

