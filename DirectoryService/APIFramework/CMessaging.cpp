/*
 * Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
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

/*!
 * @header CMessaging
 * Communications class for the DirectoryService Framework providing
 * connection with a DirectoryService daemon via either a mach or TCP endpoint
 * and handling all the message data packing and unpacking.
 */

#include "CMessaging.h"
#include "PrivateTypes.h"
#include "DSMutexSemaphore.h"
#include "DSCThread.h"
#include "DSEncryptedEndpoint.h"

#include "DirServicesTypes.h"
#include "DirServicesPriv.h"
#include "DirServicesUtils.h"

#ifdef SERVERINTERNAL
#include "CHandlers.h"
#endif

// Sys
#include <servers/bootstrap_defs.h>		// for BOOTSTRAP_UNKNOWN_SERVICE
#include <mach/mach.h>
#include <string.h>
#include <stdlib.h>
#include <stddef.h>		// for offsetof()
#include <unistd.h>		// for sleep()
#include <syslog.h>		// for syslog()
#include <ctype.h>		// for isalpha()

extern DSMutexSemaphore	   *gLock;
extern dsBool				gResetSession;
extern sInt32				gProcessPID;

//------------------------------------------------------------------------------------
//	* CMessaging
//------------------------------------------------------------------------------------

CMessaging::CMessaging ( void )
{
	bMachEndpoint = true;

	fLock = new DSMutexSemaphore();

	fMsgData = (sComData *)::calloc( 1, sizeof( sComData ) + kMsgBlockSize );
	if ( fMsgData != nil )
	{
		fMsgData->fDataSize		= kMsgBlockSize;
		fMsgData->fDataLength	= 0;
	}

#ifndef SERVERINTERNAL
	fCommPort		= nil;
	fTCPEndpoint	= nil;
#endif
} // CMessaging


//------------------------------------------------------------------------------------
//	* CMessaging
//------------------------------------------------------------------------------------

CMessaging::CMessaging ( Boolean inMachEndpoint )
{
	bMachEndpoint = inMachEndpoint;

	fLock = new DSMutexSemaphore();

	fMsgData = (sComData *)::calloc( 1, sizeof( sComData ) + kMsgBlockSize );
	if ( fMsgData != nil )
	{
		fMsgData->fDataSize		= kMsgBlockSize;
		fMsgData->fDataLength	= 0;
	}

#ifndef SERVERINTERNAL
	fCommPort		= nil;
	fTCPEndpoint	= nil;
#endif	
} // CMessaging


//------------------------------------------------------------------------------------
//	* ~CMessaging
//------------------------------------------------------------------------------------

CMessaging::~CMessaging ( void )
{
	if ( fLock != nil )
	{
		delete( fLock );
		fLock = nil;
	}

	if (fMsgData != nil)
	{
		free(fMsgData);
		fMsgData = nil;
	}
} // ~CMessaging


//--------------------------------------------------------------------------------------------------
//	* ConfigTCP()
//--------------------------------------------------------------------------------------------------

sInt32 CMessaging::ConfigTCP (	const char *inRemoteIPAddress,
								uInt32 inRemotePort )
{
	sInt32		result		= eDSNoErr;
#ifndef SERVERINTERNAL
	sInt32		siResult	= eDSNoErr;

	// check on the input vars and allow domain to be entered ie. convert it
	if (inRemoteIPAddress != nil)
	{
		if (!DSNetworkUtilities::IsValidAddressString(inRemoteIPAddress,&fRemoteIPAddress))
		{
			siResult = DSNetworkUtilities::ResolveToIPAddress(inRemoteIPAddress, &fRemoteIPAddress);
			if (siResult != eDSNoErr)
			{
				result = eDSUnknownHost;
			}
		}
		fRemotePort = inRemotePort;
	}
#endif
	return( result );

} // ConfigTCP

//------------------------------------------------------------------------------------
//	* OpenCommPort
//------------------------------------------------------------------------------------

sInt32 CMessaging::OpenCommPort ( void )
{
	sInt32			siStatus	= eDSNoErr;
#ifndef SERVERINTERNAL
	siStatus	= eMemoryAllocError;
	DSSemaphore		timedWait;
	time_t			waitTime	= ::time( nil ) + 30;

	fCommPort = new CClientEndPoint( "DirectoryService" );
	if ( fCommPort != nil )
	{
		siStatus = fCommPort->Initialize();
		if (siStatus == eDSNoErr)
		{
			siStatus = fCommPort->CheckForServer();
			if (siStatus == BOOTSTRAP_UNKNOWN_SERVICE)
			{
				//looks like DirectoryService is not yet running
				//use -appleframework argument for global mach port scope kick off of DirectoryService by configd
				//KW do we need to ever retry this system call????
				system( "/usr/sbin/DirectoryService -appleframework&" );
				do
				{
					// Check every .5 seconds
					timedWait.Wait( (uInt32)(.5 * kMilliSecsPerSec) );

					// Wait for 30 seconds MAX
					if ( ::time( nil ) > waitTime )
					{
						siStatus = eServerNotRunning;
						break;
					}
					siStatus = fCommPort->CheckForServer();
				}
				while (siStatus == BOOTSTRAP_UNKNOWN_SERVICE);
				if (siStatus != eDSNoErr)
				{
					//again check for case of different error on either
					//the bootstrap port retrieval or the bootstrap lookup
					siStatus = eMemoryAllocError;
				}
			}
			else if ( siStatus != eDSNoErr )
			{
				//either bootstrap port retrieval failed or another unknown error on the lookup
				siStatus = eMemoryAllocError;
			}
			
		} // Initialize succeeded
	}
#endif
	return( siStatus ); 
} // OpenCommPort


//------------------------------------------------------------------------------------
//	* CloseCommPort
//------------------------------------------------------------------------------------

sInt32 CMessaging::CloseCommPort ( void )
{
	sInt32 result	= eDSNoErr;
#ifndef SERVERINTERNAL
	if ( fCommPort != nil )
	{
		delete(fCommPort);
		fCommPort = nil;
	}
#endif
	return( result );

} // CloseCommPort


//--------------------------------------------------------------------------------------------------
//	* OpenTCPEndpoint()
//--------------------------------------------------------------------------------------------------

sInt32 CMessaging::OpenTCPEndpoint ( void )
{
	sInt32 result = eDSNoErr;
#ifndef SERVERINTERNAL
	if ( fTCPEndpoint == nil )
	{
		fTCPEndpoint = new DSEncryptedEndpoint((uInt32)::time(NULL), kTCPOpenTimeout, kTCPRWTimeout);
		if ( fTCPEndpoint != nil )
		{
				//attempt to connect to a port on a remote machine
				result = fTCPEndpoint->ConnectTo( fRemoteIPAddress, fRemotePort );
				if (result == eDSNoErr)
				{
					result = ((DSEncryptedEndpoint*)fTCPEndpoint)->ClientNegotiateKey();
					if (result != eDSNoErr)
					{
						fTCPEndpoint->CloseConnection();
					}
				}
		}
		else
		{
			result = eMemoryError;
		}
	}
#endif
	return( result );

} // OpenTCPEndpoint


//------------------------------------------------------------------------------------
//	* CloseTCPEndpoint
//------------------------------------------------------------------------------------

sInt32 CMessaging::CloseTCPEndpoint ( void )
{
	sInt32 result	= eDSNoErr;
#ifndef SERVERINTERNAL
	if ( fTCPEndpoint != nil )
	{
		delete(fTCPEndpoint);
		fTCPEndpoint = nil;
	}
#endif
	return( result );

} // CloseTCPEndpoint

#ifndef SERVERINTERNAL
//------------------------------------------------------------------------------------
//	* SendRemoteMessage
//------------------------------------------------------------------------------------

sInt32 CMessaging::SendRemoteMessage ( void )
{
	sInt32		result	= eDSDirSrvcNotOpened;

	if ( fTCPEndpoint == nil )
	{
		result = OpenTCPEndpoint();
	}
	
	if ( (fTCPEndpoint != nil) && (fMsgData != nil) )
	{
		LOG3( kStdErr, "CMessaging::SendRemoteMessage: before ep send - Correlate the message type: %d with the actual CMessaging class ptr %d and the endpoint class %d.", fMsgData->type.msgt_name, (uInt32)this, (uInt32)fTCPEndpoint );
		result = fTCPEndpoint->SendServerMessage( fMsgData );
	}

	return( result );

} // SendRemoteMessage


//------------------------------------------------------------------------------------
//	* SendServerMessage
//------------------------------------------------------------------------------------

sInt32 CMessaging::SendServerMessage ( void )
{
	sInt32		result	= eDSDirSrvcNotOpened;

	if ( fCommPort == nil )
	{
		result = OpenCommPort();
	}
	
	if ( (fCommPort != nil) && (fMsgData != nil) )
	{
		result = fCommPort->SendServerMessage( fMsgData );
	}

	return( result );

} // SendServerMessage
#endif

//------------------------------------------------------------------------------------
//	* GetReplyMessage
//------------------------------------------------------------------------------------

sInt32 CMessaging::GetReplyMessage ( void )
{
#ifdef SERVERINTERNAL
    return 0; // reply already got generated during call to SendInlineMessage
#else
	sInt32		result	= eDSDirSrvcNotOpened;

	if (bMachEndpoint)
	{
		if ( fCommPort == nil )
		{
			result = OpenCommPort();
		}
		
		if ( (fCommPort != nil) && (fMsgData != nil) )
		{
			result = fCommPort->GetServerReply( &fMsgData );
		}
		
		if (result == eServerSendError)
		{
			syslog(LOG_INFO,"DirectoryService Framework::CMessaging::Mach msg receiver error - out of order mach msgs.");
		}
	
		//if the mach reply failed due to an interrupt then we need to reset the entire session
		if (result == eDSCannotAccessSession)
		{
			syslog(LOG_INFO,"DirectoryService Framework::CMessaging::Mach msg receiver interrupt error = %d", result);
	
			gLock->Wait();
	
			gResetSession = true;
	
			gLock->Signal();
		}
	}
	else
	{
	//TCP listen code
		if ( (fTCPEndpoint != nil) && (fMsgData != nil) )
		{
			result = fTCPEndpoint->GetServerReply( &fMsgData );
			LOG3( kStdErr, "CMessaging::GetReplyMessage: after ep reply - Correlate the message type: %d with the actual CMessaging class ptr %d and the endpoint class %d.", fMsgData->type.msgt_name, (uInt32)this, (uInt32)fTCPEndpoint );
		}
	}

	return( result );
#endif
} // GetReplyMessage


//------------------------------------------------------------------------------------
//	* SendInlineMessage
//------------------------------------------------------------------------------------

sInt32 CMessaging::SendInlineMessage ( uInt32 inMsgType )
{
#ifdef SERVERINTERNAL
    CRequestHandler handler;
		fMsgData->type.msgt_name		= inMsgType;
		fMsgData->type.msgt_size		= 32;
		fMsgData->type.msgt_number		= 0;
		fMsgData->type.msgt_inline		= true;
		fMsgData->type.msgt_longform	= false;
		fMsgData->type.msgt_deallocate	= false;
		fMsgData->type.msgt_unused		= 0;

    handler.HandleRequest(&fMsgData);
    return 0;
#else
	sInt32			result	= eDSNoErr;

	if (bMachEndpoint)
	{
		fMsgData->type.msgt_name		= inMsgType;
		fMsgData->type.msgt_size		= 32;
		fMsgData->type.msgt_number		= 0;
		fMsgData->type.msgt_inline		= true;
		fMsgData->type.msgt_longform	= false;
		fMsgData->type.msgt_deallocate	= false;
		fMsgData->type.msgt_unused		= 0;
	
		result = this->SendServerMessage();
	
		//if the mach send was interrupted then we need to reset the entire session
		if (result == eDSCannotAccessSession)
		{
			syslog(LOG_INFO,"DirectoryService Framework::CMessaging::Mach msg send interrupt error = %d.",result);
	
			gLock->Wait();
	
			gResetSession = true;
	
			gLock->Signal();
		}
	}
	else
	{
		fMsgData->type.msgt_name= inMsgType;
		fMsgData->fPID			= gProcessPID;
		fMsgData->fMsgID		= ::time( nil );

		result = this->SendRemoteMessage();
	}
	
	return( result );
#endif
} // SendInlineMessage


//------------------------------------------------------------------------------------
//	* Add_tDataBuff_ToMsg
//------------------------------------------------------------------------------------

sInt32 CMessaging::Add_tDataBuff_ToMsg ( tDataBuffer *inBuff, eValueType inType )
{
	sInt32				result			= eDSNoErr;
	sObject			   *pObj			= nil;
	uInt32				offset			= 0;
	uInt32				length			= 0;

	result = GetEmptyObj( fMsgData, inType, &pObj );
	if ( result == eDSNoErr )
	{
		pObj->type		= inType; // == ktDataBuff
		pObj->count		= 1;
		if ( inBuff != nil )
		{
			pObj->used	 = inBuff->fBufferLength;
			pObj->length = inBuff->fBufferSize;
			offset		 = pObj->offset;
			length		 = pObj->length;

			Grow( offset, length );

			::memcpy( (char *)fMsgData + offset, inBuff->fBufferData, inBuff->fBufferSize );
			fMsgData->fDataLength += inBuff->fBufferSize;
		}
		else
		{
			pObj->length = 0;
		}
	}

	return( result );

} // Add_tDataBuff_ToMsg


//------------------------------------------------------------------------------------
//	* Add_tDataList_ToMsg
//------------------------------------------------------------------------------------

sInt32 CMessaging::Add_tDataList_ToMsg ( tDataList *inList, eValueType inType )
{
	sInt32				result		= eDSNoErr;
	bool				done		= false;
	uInt32				len			= 0;
	uInt32				offset		= 0;
	uInt32				length		= 0;
	tDataNodePtr		pCurrNode	= nil;
	tDataBufferPriv	   *pPrivData	= nil;
	sObject			   *pObj		= nil;

	if ( inList != nil )
	{
		result = GetEmptyObj( fMsgData, inType, &pObj );
		if ( result == eDSNoErr )
		{
			pObj->type		= inType;	// == ktDataList
			pObj->count		= inList->fDataNodeCount;
			pObj->length	= ::dsGetDataLength( inList ) + (inList->fDataNodeCount * sizeof( uInt32 ));
			offset			= pObj->offset;
			length			= pObj->length;

			Grow( offset, length );

			pCurrNode = inList->fDataListHead;

			// Get the list total length
			while ( !done )
			{
				pPrivData = (tDataBufferPriv *)pCurrNode;
				len = pPrivData->fBufferLength;		// <- should use fBufferSize
				::memcpy( (char *)fMsgData + offset, &len, 4 );
				fMsgData->fDataLength += 4;
				offset += 4;

				::memcpy( (char *)fMsgData + offset, pPrivData->fBufferData, len );
				fMsgData->fDataLength += len;
				offset += len;

				if ( pPrivData->fNextPtr == nil )
				{
					done = true;
				}
				else
				{
					pCurrNode = pPrivData->fNextPtr;
				}
			}
		}
	}

	return( result );

} // Add_tDataList_ToMsg




//------------------------------------------------------------------------------------
//	* Add_Value_ToMsg
//------------------------------------------------------------------------------------

sInt32 CMessaging::Add_Value_ToMsg ( uInt32 inValue, eValueType inType )
{
	sInt32			result		= eDSNoErr;
	sObject		   *pObj		= nil;

	result = GetEmptyObj( fMsgData, inType, &pObj );
	if ( result == eDSNoErr )
	{
		pObj->type		= inType;
		pObj->count		= inValue;
		pObj->length	= 0;
	}

	return( result );

} // Add_Value_ToMsg

//------------------------------------------------------------------------------------
//	* Add_tAttrEntry_ToMsg
//------------------------------------------------------------------------------------

sInt32 CMessaging::Add_tAttrEntry_ToMsg ( tAttributeEntry *inData )
{
	sInt32			result	= eDSNoErr;
	sObject		   *pObj	= nil;
	uInt32			offset	= 0;
	uInt32			length	= 0;

	result = GetEmptyObj( fMsgData, ktAttrValueEntry, &pObj );
	if ( result == eDSNoErr )
	{
		pObj->type		= ktAttrEntry;
		pObj->count		= 1;

		if ( inData != nil )
		{
			pObj->length = sizeof( tAttributeEntry ) + inData->fAttributeSignature.fBufferSize;

			offset = pObj->offset;
			length = pObj->length;

			Grow( offset, length );

			::memcpy( (char *)fMsgData + offset, inData, length );
			fMsgData->fDataLength += length;
		}
		else
		{
			pObj->length = 0;
		}
	}

	return( result );

} // Add_tAttrEntry_ToMsg


//------------------------------------------------------------------------------------
//	* Add_tAttrValueEntry_ToMsg
//------------------------------------------------------------------------------------

sInt32 CMessaging::Add_tAttrValueEntry_ToMsg ( tAttributeValueEntry *inData )
{
	sInt32			result	= eDSNoErr;
	sObject		   *pObj	= nil;
	uInt32			offset	= 0;
	uInt32			length	= 0;

	result = GetEmptyObj( fMsgData, ktAttrValueEntry, &pObj );
	if ( result == eDSNoErr )
	{
		pObj->type	= ktAttrValueEntry;
		pObj->count	= 1;

		if ( inData != nil )
		{
			pObj->length = sizeof( tAttributeValueEntry ) + inData->fAttributeValueData.fBufferSize;

			offset = pObj->offset;
			length = pObj->length;

			Grow( offset, length );

			::memcpy( (char *)fMsgData + offset, inData, length );
			fMsgData->fDataLength += length;
		}
		else
		{
			pObj->count		= 0;
			pObj->length	= 0;
		}
	}

	return( result );

} // Add_tAttrValueEntry_ToMsg


//------------------------------------------------------------------------------------
//	* Add_tRecordEntry_ToMsg
//------------------------------------------------------------------------------------

sInt32 CMessaging::Add_tRecordEntry_ToMsg ( tRecordEntry *inData )
{
	sInt32			result	= eDSNoErr;
	sObject		   *pObj	= nil;
	uInt32			offset	= 0;
	uInt32			length	= 0;

	result = GetEmptyObj( fMsgData, ktRecordEntry, &pObj );
	if ( result == eDSNoErr )
	{
		pObj->type	= ktRecordEntry;
		pObj->count	= 1;
		if ( inData != nil )
		{
			pObj->length = sizeof( tRecordEntry ) + inData->fRecordNameAndType.fBufferSize;

			offset = pObj->offset;
			length = pObj->length;

			Grow( offset, length );

			::memcpy( (char *)fMsgData + offset, inData, length );
			fMsgData->fDataLength += length;
		}
		else
		{
			pObj->length = 0;
		}
	}

	return( result );

} // Add_tRecordEntry_ToMsg


//------------------------------------------------------------------------------------
//	* Get_tDataBuff_FromMsg		ktDataBuff
//------------------------------------------------------------------------------------

sInt32 CMessaging::Get_tDataBuff_FromMsg ( tDataBuffer **outBuff, eValueType inType )
{
	sInt32		result		= eDSNoErr;
	uInt32		offset		= 0;
	uInt32		length		= 0;
	sObject	   *pObj		= nil;

	result = GetThisObj( fMsgData, inType, &pObj );
	if ( result == eDSNoErr )
	{
		offset = pObj->offset;
		length = pObj->length;

		if ( length >= 0 )
		{
			if ( *outBuff == nil )
			{
				*outBuff = ::dsDataBufferAllocate( 0x00F0F0F0, length );
			}

			if ( *outBuff != nil )
			{
				// Null out the buffer
				::memset( (*outBuff)->fBufferData, 0, (*outBuff)->fBufferSize );

				if ( (*outBuff)->fBufferSize >= pObj->length )
				{
					::memcpy( (*outBuff)->fBufferData, (char *)fMsgData + offset, length );
					(*outBuff)->fBufferLength = pObj->used;
				}
				else
				{//KW what is this for?
					::memcpy( (*outBuff)->fBufferData, (char *)fMsgData + offset, (*outBuff)->fBufferSize );
					(*outBuff)->fBufferLength = pObj->used;
					result = eDSBufferTooSmall;
				}
			}
			else
			{
				result = eDSNullDataBuff;
			}
		}
	}

	return( result );

} // Get_tDataBuff_FromMsg


//------------------------------------------------------------------------------------
//	* Get_tDataList_FromMsg		ktDataList
//------------------------------------------------------------------------------------

sInt32 CMessaging::Get_tDataList_FromMsg ( tDataList **outList, eValueType inType )
{
	sInt32		siResult	= eDSNoErr;
	uInt32		offset		= 0;
	uInt32		length		= 0;
	uInt32		count		= 0;
	uInt32		cntr		= 0;
	sObject	   *pObj		= nil;
	tDataList  *pOutList	= nil;
	char	   *tmpStr		= nil;

	try
	{
		siResult = GetThisObj( fMsgData, inType, &pObj );
		if ( siResult == eDSNoErr )
		{
			if ( outList != nil )
			{
				pOutList = ::dsDataListAllocate( 0x00F0F0F0 );
				if ( pOutList != nil )
				{
					offset	= pObj->offset;
					count	= pObj->count;

					while ( cntr < count )
					{
						::memcpy( &length, (char *)fMsgData + offset, 4 );
						offset += 4;

						tmpStr = (char *)calloc(1, length+1);
						if (tmpStr == nil) throw((sInt32)eMemoryAllocError);
						strncpy(tmpStr, (char *)fMsgData + offset, length);
						::dsAppendStringToListAlloc( 0x00F0F0F0, pOutList, tmpStr );
						free(tmpStr);

						offset += length;
						cntr++;
					}
					*outList = pOutList;
				}
				else
				{
					siResult = eMemoryError;
				}
			}
			else
			{
				siResult = eDSNullDataList;
			}
		}
	}

	catch( sInt32 err )
	{
		if ( pOutList != nil )
		{
			::dsDataListDeAllocate( 0x00F0F0F0, pOutList, true );
			free(pOutList);
			*outList = nil;
		}
		siResult = err;
	}

	return( siResult );

} // Get_tDataList_FromMsg


//------------------------------------------------------------------------------------
//	* Get_Value_FromMsg
//------------------------------------------------------------------------------------

sInt32 CMessaging::Get_Value_FromMsg ( uInt32 *outValue, eValueType inType )
{
	uInt32		result	= eDSNoErr;
	sObject	   *pObj	= nil;

	result = GetThisObj( fMsgData, inType, &pObj );
	if ( result == eDSNoErr )
	{
		*outValue = pObj->count;
	}

	return( result );

} // Get_Value_FromMsg


//------------------------------------------------------------------------------------
//	* Get_tAttrEntry_FromMsg
//------------------------------------------------------------------------------------

sInt32 CMessaging::Get_tAttrEntry_FromMsg ( tAttributeEntry **outAttrEntry, eValueType inType )
{
	sInt32				result		= eDSNoErr;
	sObject			   *pObj		= nil;
	tAttributeEntry	   *pAttrEntry	= nil;

	result = GetThisObj( fMsgData, inType, &pObj );
	if ( result == eDSNoErr )
	{
		pAttrEntry = (tAttributeEntry *)::calloc( 1, pObj->length );
		if ( pAttrEntry != nil )
		{
			::memcpy( pAttrEntry, (char *)fMsgData + pObj->offset, pObj->length );

			*outAttrEntry = pAttrEntry;
		}
		else
		{
			result = eDSNullAttribute;
		}
	}

	return( result );

} // Get_tAttrEntry_FromMsg



//------------------------------------------------------------------------------------
//	* Get_tAttrValueEntry_FromMsg
//------------------------------------------------------------------------------------

sInt32 CMessaging::Get_tAttrValueEntry_FromMsg ( tAttributeValueEntry **outAttrValue,
												 eValueType				inType )
{
	sInt32						result			= eDSNoErr;
	sObject					   *pObj			= nil;
	tAttributeValueEntry	   *pAttrValueEntry	= nil;

	result = GetThisObj( fMsgData, inType, &pObj );
	if ( result == eDSNoErr )
	{
		pAttrValueEntry = (tAttributeValueEntry *)::calloc( 1, pObj->length );
		if ( pAttrValueEntry != nil )
		{
			// Fill the struct
			::memcpy( pAttrValueEntry, (char *)fMsgData + pObj->offset, pObj->length );

			*outAttrValue = pAttrValueEntry;

		}
		else
		{
			result = eDSNullAttributeValue;
		}
	}

	return( result );

} // Get_tAttrValueEntry_FromMsg


//------------------------------------------------------------------------------------
//	* Get_tRecordEntry_FromMsg
//------------------------------------------------------------------------------------

sInt32 CMessaging::Get_tRecordEntry_FromMsg ( tRecordEntry **outRecEntry, eValueType inType )
{
	sInt32		   		result			= eDSNoErr;
	sObject		   	   *pObj			= nil;
	tRecordEntry	   *pRecordEntry	= nil;

	result = GetThisObj( fMsgData, inType, &pObj );
	if ( result == eDSNoErr )
	{
		pRecordEntry = (tRecordEntry *)::calloc( 1, pObj->length );
		if ( pRecordEntry != nil )
		{
			// Fill the struct
			::memcpy( pRecordEntry, (char *)fMsgData + pObj->offset, pObj->length );

			*outRecEntry = pRecordEntry;

		}
		else
		{
			result = eDSNullRecEntryPtr;
		}
	}

	return( result );

} // Get_tRecordEntry_FromMsg


//------------------------------------------------------------------------------------
//	* GetEmptyObj
//------------------------------------------------------------------------------------

sInt32 CMessaging::GetEmptyObj ( sComData *inMsg, eValueType inType, sObject **outObj )
{
	sInt32		siResult	= eDSIndexNotFound;
	uInt32		i;

	for ( i = 0; i < 10; i++ )
	{
		if ( inMsg->obj[ i ].type == 0 )
		{
			*outObj = &inMsg->obj[ i ];
			if ( i == 0 )
			{
                //offset was incorrect ie. missing 2 more uInt32 fixed below ie. now 3*
                // but should really use offsetof here since it is a fixed size object
                (*outObj)->offset = offsetof(struct sComData, data);
                //(*outObj)->offset = sizeof( mach_msg_header_t ) +
                //					sizeof( mach_msg_type_t ) +
                //					3*sizeof( uInt32 ) +
                //				   (sizeof( sObject ) * 10);
			}
			else
			{
				(*outObj)->offset = inMsg->obj[ i - 1 ].offset + inMsg->obj[ i - 1 ].length;
			}
			siResult = eDSNoErr;
			break;
		}
		else if ( inMsg->obj[ i ].type == (uInt32)inType )
		{
			//siResult = kDupInList;
			break;
		}
	}

	return( siResult );

} // GetEmptyObj


//------------------------------------------------------------------------------------
//	* GetThisObj
//------------------------------------------------------------------------------------

sInt32 CMessaging::GetThisObj ( sComData *inMsg, eValueType inType, sObject **outObj )
{
	sInt32		siResult	= eDSIndexNotFound;
	uInt32		i;

	for ( i = 0; i < 10; i++ )
	{
		if ( inMsg->obj[ i ].type == (uInt32)inType )
		{
			*outObj = &inMsg->obj[ i ];
			siResult = eDSNoErr;
			break;
		}
	}

	return( siResult );

} // GetThisObj


//------------------------------------------------------------------------------------
//	* Grow
//------------------------------------------------------------------------------------

void CMessaging::Grow ( uInt32 inOffset, uInt32 inSize )
{
	uInt32		newSize		= 0;
	char	   *pNewPtr		= nil;
	uInt32		length		= 0;

	// Is there anything to do
	if ( inSize == 0 )
	{
		return;
	}

	// Do we need a bigger object
	if ( (fMsgData->fDataLength + inSize) > fMsgData->fDataSize )
	{
		newSize = fMsgData->fDataSize;
		//idea is to make the newSize a multiple of kMsgBlockSize
		while( newSize < (fMsgData->fDataLength + inSize) )
		{
			newSize += kMsgBlockSize;
		}
		length = fMsgData->fDataLength;
		
		// Create the new pointer
		pNewPtr = (char *)::calloc( 1, sizeof( sComData ) + newSize );
		if ( pNewPtr == nil )
		{
			throw( (sInt32)eMemoryAllocError );
		}

		// Copy the old data to the new destination
		::memcpy( pNewPtr, fMsgData, sizeof(sComData) + fMsgData->fDataLength );

		// Dump the old data block
		::free( fMsgData );
		fMsgData = nil;

		// Assign the new data block
		fMsgData = (sComData *)pNewPtr;
		pNewPtr = nil;

		// Set the new data size
		fMsgData->fDataSize		= newSize;
		fMsgData->fDataLength	= length;
	}
} // Grow


//------------------------------------------------------------------------------------
//	* Lock
//------------------------------------------------------------------------------------

void CMessaging::Lock ( void )
{
	if ( fLock != nil )
	{
		fLock->Wait();
	}
} // Lock


//------------------------------------------------------------------------------------
//	* Unlock
//------------------------------------------------------------------------------------

void CMessaging::Unlock ( void )
{
	if ( fLock != nil )
	{
		fLock->Signal();
	}
} // Unlock


//------------------------------------------------------------------------------------
//	* ClearMessageBlock
//------------------------------------------------------------------------------------

void CMessaging::ClearMessageBlock ( void )
{
	uInt32	size	= 0;
	if ( fMsgData != nil )
	{
		size = fMsgData->fDataSize;
		::memset( fMsgData, 0, sizeof( sComData ) );
		fMsgData->fDataSize		= size;
		fMsgData->fDataLength	= 0;
	}
} // ClearMessageBlock
