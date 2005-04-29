/*
 * Copyright (c) 2000 - 2003 Apple Computer, Inc. All rights reserved.
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
 * @header PwdPolicyTool
 */


#include <sys/stat.h>
#include <sys/time.h>
#include <PasswordServer/AuthFile.h>
#include "PwdPolicyTool.h"

#define debugerr(ERR, A, args...)	if (gVerbose && (ERR)) {fprintf(stderr, (A), ##args);}

// Static Locals
const 	long		kBuffSize			= 8192;

// ---------------------------------------------------------------------------
//	PwdPolicyTool ()
// ---------------------------------------------------------------------------

PwdPolicyTool::PwdPolicyTool ( void )
{
	fDSRef			= 0;
	fTDataBuff		= 0;
	fLocalNodeRef	= 0;
	fSearchNodeRef	= 0;
} // PwdPolicyTool


// ---------------------------------------------------------------------------
//	~PwdPolicyTool ()
// ---------------------------------------------------------------------------

PwdPolicyTool::~PwdPolicyTool ( void )
{
} // ~PwdPolicyTool

tDirNodeReference PwdPolicyTool::GetLocalNodeRef ( void )
{
	return( fLocalNodeRef );
}

// ---------------------------------------------------------------------------
//	GetSearchNodeRef ()
// ---------------------------------------------------------------------------

tDirNodeReference PwdPolicyTool::GetSearchNodeRef ( void )
{
	return( fSearchNodeRef );
} // GetSearchNodeRef


// ---------------------------------------------------------------------------
//	Initialize ()
// ---------------------------------------------------------------------------

long PwdPolicyTool::Initialize ( void )
{
	long					siStatus		= eDSNoErr;
	char				   *pNodeName		= nil;

	siStatus = OpenDirectoryServices();
	if ( siStatus != eDSNoErr )
	{
		return( siStatus );
	}

	siStatus = AllocateTDataBuff();
	if ( siStatus != eDSNoErr )
	{
		return( siStatus );
	}

	// Find and open search node
	siStatus = FindDirectoryNodes( nil, eDSSearchNodeName, &pNodeName );
	if ( siStatus == eDSNoErr )
	{
		siStatus = OpenDirNode( pNodeName, &fSearchNodeRef );

		free( pNodeName );
		pNodeName = nil;

		if ( siStatus != eDSNoErr )
		{
			return( siStatus );
		}
	}
	else
	{
		return( siStatus );
	}

	return( siStatus );

} // Initialize


// ---------------------------------------------------------------------------
//	Deinitialize ()
// ---------------------------------------------------------------------------

long PwdPolicyTool::Deinitialize ( void )
{
	long			siStatus		= eDSNoErr;

	siStatus = DeallocateTDataBuff();
	if ( siStatus != eDSNoErr )
	{
		PrintError( siStatus, "DeallocateTDataBuff" );
	}

	// local node
	siStatus = CloseDirectoryNode( fLocalNodeRef );
	if ( siStatus != noErr )
	{
		::fprintf( stderr, "error in  CloseDirectoryNode %ld\n", siStatus );
	}

	// search node
	siStatus = CloseDirectoryNode( fSearchNodeRef );
	if ( siStatus != eDSNoErr )
	{
		PrintError( siStatus, "CloseDirectoryNode" );
	}

	siStatus = CloseDirectoryServices();
	if ( siStatus != eDSNoErr )
	{
		PrintError( siStatus, "CloseDirectoryServices" );
	}

	return( siStatus );

} // Deinitialize


//--------------------------------------------------------------------------------------------------
// * OpenDirectoryServices ()
//
//--------------------------------------------------------------------------------------------------

long PwdPolicyTool::OpenDirectoryServices ( void )
{
	long		error	= eDSNoErr;

	if ( gVerbose == true )
	{
		fprintf( stderr, "\n----- Opening Directory Services -----\n" );
	}

		error = ::dsOpenDirService( &fDSRef );
		if ( error != eDSNoErr )
		{
		PrintError( error, "dsOpenDirService" );
		}
		else if ( gVerbose == true )
		{
			fprintf( stderr, "  Directory Reference = %ld.\n", fDSRef );
		}

	return( error );

} // OpenDirectoryServices


//--------------------------------------------------------------------------------------------------
// * CloseDirectoryServices ()
//
//--------------------------------------------------------------------------------------------------

long PwdPolicyTool::CloseDirectoryServices ( void )
{
	long		error	= eDSNoErr;

	if ( gVerbose == true )
	{
		fprintf( stderr, "\n----- Closing Directory Services -----\n" );
	}

		error = ::dsCloseDirService( fDSRef );
		if ( error != eDSNoErr )
		{
		PrintError( error, "dsCloseDirService" );
	}

	return( error );

} // CloseDirectoryServices


//--------------------------------------------------------------------------------------------------
// * AllocateTDataBuff ()
//
//--------------------------------------------------------------------------------------------------

long PwdPolicyTool::AllocateTDataBuff ( void )
{
	long		error	= eDSNoErr;

	if ( gVerbose == true )
	{
		fprintf( stderr, "\n----- Allocating a %ldK buffer -----\n", kBuffSize / 1024 );
	}

		fTDataBuff = ::dsDataBufferAllocate( fDSRef, kBuffSize );
		if ( fTDataBuff == nil )
		{
		PrintError( eMemoryAllocError, "dsDataBufferAllocate" );
		error = eMemoryAllocError;
		}

		if ( gVerbose == true )
		{
			fprintf( stderr, "  allocated buffer of %ld size.\n", fTDataBuff->fBufferSize );
		}

	return( error );

} // AllocateTDataBuff


//--------------------------------------------------------------------------------------------------
// * DeallocateTDataBuff ()
//
//--------------------------------------------------------------------------------------------------

long PwdPolicyTool::DeallocateTDataBuff ( void )
{
	long		error	= eDSNoErr;

	if ( gVerbose == true )
	{
		fprintf( stderr, "\n----- Deallocating default buffer -----\n" );
	}

		error = ::dsDataBufferDeAllocate( fDSRef, fTDataBuff );
		if ( error != eDSNoErr )
		{
		PrintError( error, "dsDataBufferDeAllocate" );
	}

	return( error );

} // DeallocateTDataBuff


//--------------------------------------------------------------------------------------------------
// * DoGetRecordList ()
//
//--------------------------------------------------------------------------------------------------

long PwdPolicyTool::DoGetRecordList (	tDirNodeReference   inNodeRef,
										const char			*inRecName,
										char				*inRecType,
										char				*inAttrType,
										tDirPatternMatch	 inMatchType,	// eDSExact, eDSContains ...
										char				**outAuthAuthority,
										char				**outNodeName )
{
	long					error			= eDSNoErr;
	long					error2			= eDSNoErr;
	unsigned long			recCount		= 0;
	tContextData			context			= nil;
	tDataList			   *pRecName		= nil;
	tDataList			   *pRecType		= nil;
	tDataList			   *pAttrType		= nil;
	
	if ( gVerbose == true )
	{
		fprintf( stderr, "\n----- Getting Record List -----\n" );
		fprintf( stderr, "  Record Name    = %s\n", inRecName );
		fprintf( stderr, "  Record Type    = %s\n", inRecType );
		fprintf( stderr, "  Attribute Type = %s\n", inAttrType );
	}

		pRecName = ::dsBuildListFromStrings( fDSRef, inRecName, nil );
		if ( pRecName != nil )
		{
			pRecType = ::dsBuildListFromStrings( fDSRef, inRecType, nil );
			if ( pRecType != nil )
			{
				pAttrType = ::dsBuildListFromStrings( fDSRef, inAttrType, kDSNAttrMetaNodeLocation, nil );
				if ( pAttrType != nil )
				{
					*outAuthAuthority = NULL;
					*outNodeName = NULL;
					
					do
					{
						error = ::dsGetRecordList( inNodeRef, fTDataBuff, pRecName, inMatchType, pRecType,
													pAttrType, false, &recCount, &context );
						if ( error == eDSNoErr )
						{
							error = GetDataFromDataBuff( inNodeRef, fTDataBuff, recCount, outAuthAuthority, outNodeName );
						} 
						else if ( error == eDSBufferTooSmall )
						{
							unsigned long buffSize = fTDataBuff->fBufferSize;
							dsDataBufferDeAllocate( fDSRef, fTDataBuff );
							fTDataBuff = nil;
							fTDataBuff = dsDataBufferAllocate( fDSRef, buffSize * 2 );
						}
					} while ( ((error == eDSNoErr) && (context != nil)) || (error == eDSBufferTooSmall) );

					error2 = ::dsDataListDeallocate( fDSRef, pAttrType );
					if ( error2 != eDSNoErr )
					{
					PrintError( error2, "dsDataListDeallocate" );
					}
				}
				else
				{
				PrintError( eMemoryAllocError, "dsBuildListFromStrings" );
				error = eMemoryAllocError;
				}

				error2 = ::dsDataListDeallocate( fDSRef, pRecType );
				if ( error2 != eDSNoErr )
				{
				PrintError( error2, "dsDataListDeallocate" );
				}
			}
			else
			{
			PrintError( eMemoryAllocError, "dsBuildListFromStrings" );
			error = eMemoryAllocError;
			}

			error2 = ::dsDataListDeallocate( fDSRef, pRecName );
			if ( error2 != eDSNoErr )
			{
			PrintError( error2, "dsDataListDeallocate" );
			}
		}
		else
		{
		PrintError( eMemoryAllocError, "dsBuildListFromStrings" );
		error = eMemoryAllocError;
	}

	return( error );

} // DoGetRecordList



//--------------------------------------------------------------------------------------------------
// * GetDataFromDataBuff ()
//
//--------------------------------------------------------------------------------------------------

long PwdPolicyTool::GetDataFromDataBuff(
	tDirNodeReference inNodeRef,
	tDataBuffer		   *inTDataBuff,
	unsigned long		inRecCount,
	char			  **outAuthAuthority,
	char			  **outNodeName )
{
	long					error			= eDSNoErr;
	unsigned long			i				= 0;
	unsigned long			j				= 0;
	unsigned long			k				= 0;
	char				   *pRecNameStr		= nil;
	char				   *pRecTypeStr		= nil;
	tRecordEntry		   *pRecEntry		= nil;
	tAttributeListRef		attrListRef		= 0;
	tAttributeValueListRef	valueRef		= 0;
	tAttributeEntry		   *pAttrEntry		= nil;
	tAttributeValueEntry   *pValueEntry		= nil;
	bool					found			= false;
	
	if ( gVerbose == true )
	{
		fprintf( stderr, "  Record count = %ld\n", inRecCount );
	}

	// Do not initialize to NULL; this method may be called multiple times
	// and we do not want to stomp on a successful result
	//*outAuthAuthority = NULL;
	//*outNodeName = NULL;
	
	if ( (inRecCount != 0) && (inNodeRef != 0) && (inTDataBuff != nil) )
	{
		for ( i = 1; (i <= inRecCount) && (error == eDSNoErr) && (!found); i++ )
		{
			error = ::dsGetRecordEntry( inNodeRef, inTDataBuff, i, &attrListRef, &pRecEntry );
			if ( error == eDSNoErr && pRecEntry != NULL )
			{
				error = ::dsGetRecordNameFromEntry( pRecEntry, &pRecNameStr );
				if ( error == eDSNoErr )
				{
					error = ::dsGetRecordTypeFromEntry( pRecEntry, &pRecTypeStr );
					if ( error == eDSNoErr )
					{
						if ( gVerbose == true )
						{
							fprintf( stderr, "\n" );
							fprintf( stderr, "    Record Number   = %ld\n", i );
							fprintf( stderr, "    Record Name     = %s\n", pRecNameStr );
							fprintf( stderr, "    Record Type     = %s\n", pRecTypeStr );
							fprintf( stderr, "    Attribute count = %ld\n", pRecEntry->fRecordAttributeCount );
						}

						for ( j = 1; (j <= pRecEntry->fRecordAttributeCount) && (error == eDSNoErr); j++ )
						{
							error = ::dsGetAttributeEntry( inNodeRef, inTDataBuff, attrListRef, j, &valueRef, &pAttrEntry );
							if ( error == eDSNoErr && pAttrEntry != NULL )
							{
								for ( k = 1; (k <= pAttrEntry->fAttributeValueCount) && (error == eDSNoErr); k++ )
								{
									error = ::dsGetAttributeValue( inNodeRef, inTDataBuff, k, valueRef, &pValueEntry );
									if ( error == eDSNoErr && pValueEntry != NULL )
									{
										if ( gVerbose == true )
										{
											fprintf( stderr, "      %ld - %ld: (%s) %s\n", j, k,
																	pAttrEntry->fAttributeSignature.fBufferData,
																	pValueEntry->fAttributeValueData.fBufferData );
										}
										
										if ( !found &&
											(strcasestr( pValueEntry->fAttributeValueData.fBufferData, kDSTagAuthAuthorityPasswordServer ) != NULL ||
											 strcasestr( pValueEntry->fAttributeValueData.fBufferData, kDSTagAuthAuthorityShadowHash ) != NULL) )
										{
											*outAuthAuthority = (char *) malloc( pValueEntry->fAttributeValueData.fBufferLength + 1 );
											strcpy( *outAuthAuthority, pValueEntry->fAttributeValueData.fBufferData );
											::dsDeallocAttributeValueEntry( fDSRef, pValueEntry );
											pValueEntry = NULL;
											found = true;
										}
										else
										if ( strcasestr( pAttrEntry->fAttributeSignature.fBufferData, kDSNAttrMetaNodeLocation ) != NULL )
										{
											*outNodeName = (char *) malloc( pValueEntry->fAttributeValueData.fBufferLength + 1 );
											strcpy( *outNodeName, pValueEntry->fAttributeValueData.fBufferData );
											::dsDeallocAttributeValueEntry( fDSRef, pValueEntry );
											pValueEntry = NULL;
										}
										
										::dsDeallocAttributeValueEntry( fDSRef, pValueEntry );
										pValueEntry = NULL;
									}
									else
									{
										PrintError( error, "dsGetAttributeValue" );
									}
								}
								::dsDeallocAttributeEntry( fDSRef, pAttrEntry );
								pAttrEntry = NULL;
								::dsCloseAttributeValueList(valueRef);
								valueRef = 0;
							}
							else
							{
								PrintError( error, "dsGetAttributeEntry" );
							}
						}

						delete( pRecTypeStr );
						pRecTypeStr = nil;
					}
					else
					{
						PrintError( error, "dsGetRecordTypeFromEntry" );
					}

					delete( pRecNameStr );
					pRecNameStr = nil;
				}
				else
				{
					PrintError( error, "dsGetRecordNameFromEntry" );
				}
				::dsDeallocRecordEntry( fDSRef, pRecEntry );
				pRecEntry = NULL;
				::dsCloseAttributeList(	attrListRef	);
				attrListRef = 0;
			}
			else
			{
				PrintError( error, "dsGetRecordEntry" );
			}
		}
	}

	return( error );

} // GetDataFromDataBuff


//--------------------------------------------------------------------------------------------------
// * FindDirectoryNodes ()
//
//--------------------------------------------------------------------------------------------------

long PwdPolicyTool::FindDirectoryNodes( char			   *inNodeName,
									  	tDirPatternMatch	inMatch,
									  	char			  **outNodeName,
										bool				inPrintNames )
{
	long			error			= eDSNoErr;
	long			error2			= eDSNoErr;
	bool			done			= false;
	unsigned long   uiCount			= 0;
	unsigned long   uiIndex			= 0;
	tDataList	   *pNodeNameList	= nil;
	tDataList	   *pDataList		= nil;
	char		   *pNodeName		= nil;

	if ( gVerbose == true )
	{
		fprintf( stderr, "\n----- Finding node(s) -----\n" );
		fprintf( stderr, "    Node Name:      %s\n", inNodeName );
		fprintf( stderr, "    Pattern Match:  %d\n", inMatch );
	}

	try
	{
		if ( fTDataBuff == NULL )
			throw( (long)eDSNullParameter );
		
		if ( inNodeName != nil )
		{
			pNodeNameList = ::dsBuildFromPath( fDSRef, inNodeName, "/" );
			if ( pNodeNameList == nil )
			{
				PrintError( eMemoryAllocError, "dsBuildFromPath" );
				throw( (long)eMemoryAllocError );
			}
		}

		do {
			error = ::dsFindDirNodes( fDSRef, fTDataBuff, pNodeNameList, inMatch, &uiCount, nil );
			if ( error == eDSBufferTooSmall )
			{
				unsigned long buffSize = fTDataBuff->fBufferSize;
				dsDataBufferDeAllocate( fDSRef, fTDataBuff );
				fTDataBuff = nil;
				fTDataBuff = dsDataBufferAllocate( fDSRef, buffSize * 2 );
			}
		} while ( error == eDSBufferTooSmall );
		if ( error == eDSNoErr )
		{
			if ( inPrintNames || gVerbose )
			{
				fprintf( stderr, " Node count = %ld.\n", uiCount );
			}

			if ( uiCount != 0 )
			{
				pDataList = ::dsDataListAllocate( fDSRef );
				if ( pDataList != nil )
				{
					for ( uiIndex = 1; (uiIndex <= uiCount) && (error == eDSNoErr); uiIndex++ )
					{
						error = ::dsGetDirNodeName( fDSRef, fTDataBuff, uiIndex, &pDataList );
						if ( error == eDSNoErr )
						{
							pNodeName = ::dsGetPathFromList( fDSRef, pDataList, "/" );
							if ( pNodeName != nil )
							{
								if ( inPrintNames || gVerbose )
								{
									fprintf( stderr, "  %2ld - Node Name = %s\n", uiIndex, pNodeName );
								}

								if ( (outNodeName != nil) && !done )
								{
									*outNodeName = pNodeName;
									done = true;
								}
								else
								{
									free( pNodeName );
									pNodeName = nil;
								}

								error2 = ::dsDataListDeallocate( fDSRef, pDataList );
								if ( error2 != eDSNoErr )
								{
									PrintError( error2, "dsDataListDeallocate" );
								}
							}
							else
							{
								PrintError( eMemoryAllocError, "dsGetPathFromList" );
								error = eMemoryAllocError;
							}
						}
						else
						{
							PrintError( error, "dsGetDirNodeName" );
						}
					}
				}
				else
				{
					PrintError( eMemoryAllocError, "dsDataListAllocate" );
					error = eMemoryAllocError;
				}
			}
		}
		else
		{
			PrintError( error, "dsFindDirNodes" );
		}

		if ( pNodeNameList != nil )
		{
			error2 = ::dsDataListDeallocate( fDSRef, pNodeNameList );
			if ( error2 != eDSNoErr )
			{
				PrintError( error2, "dsDataListDeallocate" );
			}
		}
	}

	catch ( long err )
	{
		PrintError( err, "FindDirectoryNodes" );
		error = err;
	}

	return( error );

} // FindDirectoryNodes


//--------------------------------------------------------------------------------------------------
// * OpenDirNode ()
//
//--------------------------------------------------------------------------------------------------

long PwdPolicyTool::OpenDirNode ( char *inNodeName, tDirNodeReference *outNodeRef )
{
	long			error		= eDSNoErr;
	long			error2		= eDSNoErr;
	tDataList	   *pDataList	= nil;

	if ( gVerbose == true )
	{
		fprintf( stderr, "\n----- Opening Directory Node -----\n" );
		fprintf( stderr, "    Node Name:      %s\n", inNodeName );
	}


		pDataList = ::dsBuildFromPath( fDSRef, inNodeName, "/" );
		if ( pDataList != nil )
		{
			error = ::dsOpenDirNode( fDSRef, pDataList, outNodeRef );
			if (error == eDSNoErr) 
			{
				if (gVerbose == true)
				{
					fprintf( stderr, "  Open Node Reference = %ld.\n", *outNodeRef );
				}
			}
		else
			{
			PrintError( error, "dsOpenDirNode" );
			}

			error2 = ::dsDataListDeallocate( fDSRef, pDataList );
			if ( error2 != eDSNoErr )
			{
			PrintError( error2, "dsDataListDeallocate" );
			}
		}
		else
		{
		PrintError( eMemoryAllocError, "dsBuildFromPath" );
		error = eMemoryAllocError;
	}

	return( error );

} // OpenDirNode


//--------------------------------------------------------------------------------------------------
// * CloseDirectoryNode ()
//
//--------------------------------------------------------------------------------------------------

long PwdPolicyTool::CloseDirectoryNode ( tDirNodeReference inNodeRef )
{
	long			error		= eDSNoErr;

	if ( gVerbose == true )
	{
		fprintf( stderr, "\n----- Closing Directory Node -----\n" );
		fprintf( stderr, "    Node Reference: %lu\n", inNodeRef );
	}
	
	if ( inNodeRef == 0 )
		return eDSNoErr;
	
	error = ::dsCloseDirNode( inNodeRef );
	if ( error != eDSNoErr )
	{
		PrintError( error, "dsCloseDirNode" );
	}

	return( error );

} // CloseDirectoryNode


//--------------------------------------------------------------------------------------------------
// * DoNodePWAuth ()
//
//--------------------------------------------------------------------------------------------------

long PwdPolicyTool::DoNodePWAuth ( tDirNodeReference inNode, const char *inName, char *inPasswd, const char *inMethod, char *inUserName, const char *inOther, char *outResult )
{
	long			error			= eDSNoErr;
	long			error2			= eDSNoErr;
	tDataBuffer	   *pAuthBuff		= nil;
	tDataBuffer	   *pStepBuff		= nil;
	tDataNode	   *pAuthType		= nil;
	
	// kDSStdAuthNewUser
	// "dsAuthMethodStandard:dsAuthNewUser"
    
	if ( gVerbose == true )
	{
		fprintf( stderr, "\n----- Node Password Server Auth -----\n" );
		fprintf( stderr, "  User Name   = %s\n", inName );
	}
    
	error = SetUpAuthBuffs( &pAuthBuff, 2048, &pStepBuff, 2048, &pAuthType, inMethod );
	if ( error == eDSNoErr )
	{
		if ( inName == NULL )
			inName = "";
		if ( inPasswd == NULL )
			inPasswd = "";
		if ( inUserName == NULL )
			inUserName = "";
			
		if ( inOther != NULL )
		{
			error = FillAuthBuff ( pAuthBuff, 4,
									::strlen( inName ), inName,
									::strlen( inPasswd ), inPasswd,
									::strlen( inUserName ), inUserName,
									::strlen( inOther ), inOther );
		}
		else
		{
			error = FillAuthBuff ( pAuthBuff, 3,
									::strlen( inName ), inName,
									::strlen( inPasswd ), inPasswd,
									::strlen( inUserName ), inUserName );
		}
		
		if ( error == eDSNoErr )
		{
			error = ::dsDoDirNodeAuth( inNode, pAuthType, true, pAuthBuff, pStepBuff, nil );
			if ( error == eDSNoErr )
			{
				unsigned long len;
				
				memcpy(&len, pStepBuff->fBufferData, 4);
				if ( len < pStepBuff->fBufferSize - 4 )
				{
					pStepBuff->fBufferData[len+4] = '\0';
					if ( outResult != NULL )
						strcpy( outResult, pStepBuff->fBufferData+4 );
					else
						fprintf( stdout, "%s\n", pStepBuff->fBufferData+4 );
				}
				else
				{
					if ( outResult != NULL )
						sprintf( outResult, "The buffer data length is invalid (len=%lu).", len );
					else
						fprintf( stdout, "The buffer data length is invalid (len=%lu).\n", len );
				}
			}
			else
			{
				PrintError( error, "dsDoDirNodeAuth" );
				fprintf( stderr, "  Method = %s\n", inMethod );
			}
		}
		
		error2 = ::dsDataBufferDeAllocate( fDSRef, pAuthBuff );
		if ( error2 != eDSNoErr )
		{
			PrintError( error2, "dsDataBufferDeAllocate" );
		}

		error2 = ::dsDataBufferDeAllocate( fDSRef, pStepBuff );
		if ( error2 != eDSNoErr )
		{
			PrintError( error2, "dsDataBufferDeAllocate" );
		}

		error2 = ::dsDataBufferDeAllocate( fDSRef, pAuthType );
		if ( error2 != eDSNoErr )
		{
			PrintError( error2, "dsDataBufferDeAllocate" );
		}
	}

	return( error );

} // DoNodePWAuth

//--------------------------------------------------------------------------------------------------
// * DoNodeNativeAuth ()
//
//--------------------------------------------------------------------------------------------------

long PwdPolicyTool::DoNodeNativeAuth ( tDirNodeReference inNode, const char *inName, char *inPasswd )
{
	long			error			= eDSNoErr;
	long			error2			= eDSNoErr;
	tDataBuffer	   *pAuthBuff		= nil;
	tDataBuffer	   *pStepBuff		= nil;
	tDataNode	   *pAuthType		= nil;
	
	if ( gVerbose == true )
	{
		fprintf( stderr, "\n----- Node Auth -----\n" );
		fprintf( stderr, "  User Name   = %s\n", inName );
	}
    
	error = SetUpAuthBuffs( &pAuthBuff, 2048, &pStepBuff, 2048, &pAuthType, kDSStdAuthNodeNativeClearTextOK );
	if ( error == eDSNoErr )
	{
		if ( inName == NULL )
			inName = "";
		if ( inPasswd == NULL )
			inPasswd = "";

		error = FillAuthBuff ( pAuthBuff, 2, strlen( inName ), inName, strlen( inPasswd ), inPasswd );
		if ( error == eDSNoErr )
		{
			error = ::dsDoDirNodeAuth( inNode, pAuthType, false, pAuthBuff, pStepBuff, nil );
			if ( error != eDSNoErr )
			{
				PrintError( error, "dsDoDirNodeAuth" );
			}
		}
		
		error2 = ::dsDataBufferDeAllocate( fDSRef, pAuthBuff );
		if ( error2 != eDSNoErr )
		{
			PrintError( error2, "dsDataBufferDeAllocate" );
		}

		error2 = ::dsDataBufferDeAllocate( fDSRef, pStepBuff );
		if ( error2 != eDSNoErr )
		{
			PrintError( error2, "dsDataBufferDeAllocate" );
		}

		error2 = ::dsDataBufferDeAllocate( fDSRef, pAuthType );
		if ( error2 != eDSNoErr )
		{
			PrintError( error2, "dsDataBufferDeAllocate" );
		}
	}

	return( error );

} // DoNodeNativeAuth


// ---------------------------------------------------------------------------
//  PrintError ()
//
// ---------------------------------------------------------------------------

void PwdPolicyTool::PrintError ( long inErrCode, const char *messageTag )
{
	char *statusString = nil;
	
	if (inErrCode == eDSNoErr)
	{
		return;
	}

	statusString = dsCopyDirStatusName(inErrCode);

	if ( messageTag == nil )
				{
		fprintf( stderr, "\n***Error: %s : (%d)\n", statusString, inErrCode );
				}
				else
				{
		fprintf( stderr, "\n***Error: %s : (%d) for %s\n", statusString, inErrCode, messageTag );
				}

	free(statusString);
	statusString = nil;

	fflush( stderr );

} // PrintError


//--------------------------------------------------------------------------------------------------
// * SetUpAuthBuffs ()
//
//--------------------------------------------------------------------------------------------------

long PwdPolicyTool::SetUpAuthBuffs ( tDataBuffer	  **outAuthBuff,
									unsigned long		inAuthBuffSize,
									tDataBuffer		  **outStepBuff,
									unsigned long		inStepBuffSize,
									tDataBuffer		  **outTypeBuff,
									const char	 *inAuthMethod )
{
	long		error	= eDSNoErr;
	long		error2	= eDSNoErr;

	if ( (outAuthBuff == nil) || (outStepBuff == nil) ||
		 (outTypeBuff == nil) || (inAuthMethod == nil) )
	{
		return( eDSNullParameter );
	}

	*outAuthBuff = ::dsDataBufferAllocate( fDSRef, inAuthBuffSize );
	if ( *outAuthBuff != nil )
	{
		*outStepBuff = ::dsDataBufferAllocate( fDSRef, inStepBuffSize );
		if ( *outStepBuff != nil )
		{
			*outTypeBuff = ::dsDataNodeAllocateString( fDSRef, inAuthMethod );
			if ( *outTypeBuff == nil )
			{
				PrintError( eMemoryAllocError, "dsDataNodeAllocateString" );
				error = eMemoryAllocError;
			}
		}
		else
		{
			PrintError( eMemoryAllocError, "dsDataBufferAllocate" );
			error = eMemoryAllocError;
		}
	}
	else
	{
		PrintError( eMemoryAllocError, "dsDataBufferAllocate" );
		error = eMemoryAllocError;
	}

	if ( error != eDSNoErr )
	{
		if ( *outAuthBuff != nil )
		{
			error2 = ::dsDataBufferDeAllocate( fDSRef, *outAuthBuff );
			if ( error2 != eDSNoErr )
			{
				PrintError( error2, "dsDataBufferDeAllocate" );
			}
		}

		if ( *outStepBuff != nil )
		{
			error2 = ::dsDataBufferDeAllocate( fDSRef, *outStepBuff );
			if ( error2 != eDSNoErr )
			{
				PrintError( error2, "dsDataBufferDeAllocate" );
			}
		}

		if ( *outTypeBuff != nil )
		{
			error2 = ::dsDataBufferDeAllocate( fDSRef, *outTypeBuff );
			if ( error2 != eDSNoErr )
			{
				PrintError( error2, "dsDataBufferDeAllocate" );
			}
		}
	}

	return( error );

} // SetUpAuthBuffs


//--------------------------------------------------------------------------------------------------
// * FillAuthBuff ()
//
//		inCount		== Number of unsigned long, void* pairs
//		va args		== unsigned long, void* pairs
//--------------------------------------------------------------------------------------------------

long PwdPolicyTool::FillAuthBuff ( tDataBuffer *inAuthBuff, unsigned long inCount, unsigned long inLen, const void *inData ... )
{
	long			error		= eDSNoErr;
	unsigned long   curr		= 0;
	unsigned long   buffSize	= 0;
	unsigned long   count		= inCount;
	unsigned long   len			= inLen;
	const void	   *data		= inData;
	bool			firstPass   = true;
	char	   *p			= nil;
	va_list		args;

	// If the buffer is nil, we have nowhere to put the data
	if ( inAuthBuff == nil )
	{
		return( eDSNullParameter );
	}

	// If the buffer is nil, we have nowhere to put the data
	if ( inAuthBuff->fBufferData == nil )
	{
		return( eDSNullParameter );
	}

	// Make sure we have data to copy
	if ( (inLen != 0) && (inData == nil) )
	{
		return( eDSNullParameter );
	}

	// Get buffer info
	p		 = inAuthBuff->fBufferData;
	buffSize = inAuthBuff->fBufferSize;

	// Set up the arg list
	va_start( args, inData );

	while ( count-- > 0 )
	{
		if ( !firstPass )
		{
			len = va_arg( args, unsigned long );
			data = va_arg( args, void * );
		}

		if ( (curr + len) > buffSize )
		{
			return( (long)eDSBufferTooSmall );
		}

		::memcpy( &(p[ curr ]), &len, sizeof( long ) );
		curr += sizeof( long );

		if ( len > 0 )
		{
			memcpy( &(p[ curr ]), data, len );
			curr += len;
		}
		firstPass = false;
	}

	inAuthBuff->fBufferLength = curr;

	return( error );

} // FillAuthBuff


//--------------------------------------------------------------------------------------------------
// * GetUserByName ()
//
//--------------------------------------------------------------------------------------------------

long PwdPolicyTool::GetUserByName( tDirNodeReference inNode, const char *inUserName, char **outAuthAuthority, char **outNodeName )
{
	long status = eDSNoErr;

	if (gVerbose)
		fprintf( stderr, "\n----- Getting user by name: %s -----\n", inUserName );

	status = DoGetRecordList( inNode, inUserName, kDSStdRecordTypeUsers, kDSNAttrAuthenticationAuthority, eDSExact, outAuthAuthority, outNodeName );
	if ( status != eDSNoErr )
	{
		fprintf( stderr, "  *** GetRecordList failed with error = %ld.\n", status );
	}

	return( status );

} // GetUserByName


//-----------------------------------------------------------------------------
//	 OpenRecord
//-----------------------------------------------------------------------------

tDirStatus
PwdPolicyTool::OpenRecord(
	tDirNodeReference inNodeRef,
	const char *inRecordType,
	const char *inRecordName,
	tRecordReference *outRecordRef,
	bool inCreate )
{
    tDirStatus				status				= eDSNoErr;
    tDataNodePtr			recordTypeNode		= NULL;
    tDataNodePtr			recordNameNode		= NULL;    

	// make sure the state is correct
	if ( fDSRef == 0 || inNodeRef == 0 || inRecordType == NULL || inRecordName == NULL || outRecordRef == NULL )
		return eParameterError;
	
	recordTypeNode = dsDataNodeAllocateString( fDSRef, inRecordType );
	recordNameNode = dsDataNodeAllocateString( fDSRef, inRecordName );
	
	status = dsOpenRecord( inNodeRef, recordTypeNode, recordNameNode, outRecordRef );
	if ( inCreate && status == eDSRecordNotFound )
		status = dsCreateRecordAndOpen( inNodeRef, recordTypeNode, recordNameNode, outRecordRef );
	
	if ( recordTypeNode ) {
		dsDataNodeDeAllocate( fDSRef, recordTypeNode );
		recordTypeNode = NULL;
	}
	if ( recordNameNode ) {
		dsDataNodeDeAllocate( fDSRef, recordNameNode );
		recordNameNode = NULL;
	}
	
	return status;
}


//-----------------------------------------------------------------------------
//	 ChangeAuthAuthorityToShadowHash
//-----------------------------------------------------------------------------

void
PwdPolicyTool::ChangeAuthAuthorityToShadowHash( tRecordReference inRecordRef )
{
    long						status				= eDSNoErr;
	tAttributeValueEntry	   *pExistingAttrValue	= NULL;
	unsigned long				attrValIndex		= 0;
    unsigned long				attrValCount		= 0;
    tDataNode				   *attrTypeNode		= nil;
    tAttributeEntryPtr			pAttrEntry			= nil;
    char						*aaVersion			= nil;
    char						*aaTag				= nil;
    char						*aaData				= nil;
    unsigned long				attrValueIDToReplace = 0;
	
    try
    {
		pExistingAttrValue = nil;
		attrValueIDToReplace = 0;
        
		// get info about this attribute
		attrTypeNode = dsDataNodeAllocateString( 0, kDSNAttrAuthenticationAuthority );
		status = dsGetRecordAttributeInfo( inRecordRef, attrTypeNode, &pAttrEntry );
		debugerr(status, "dsGetRecordAttributeInfo = %ld\n", status);
		if ( status == eDSNoErr )
		{
			// run through the values and replace the target authority if it exists
			attrValCount = pAttrEntry->fAttributeValueCount;
			for ( attrValIndex = 1; attrValIndex <= attrValCount; attrValIndex++ )
			{
				status = dsGetRecordAttributeValueByIndex( inRecordRef, attrTypeNode, attrValIndex, &pExistingAttrValue );
				debugerr(status, "dsGetRecordAttributeValueByIndex = %ld\n", status);
				if (status != eDSNoErr) continue;
				
				status = dsParseAuthAuthority( pExistingAttrValue->fAttributeValueData.fBufferData, &aaVersion, &aaTag, &aaData );
				if (status != eDSNoErr) continue;
				
				if ( strcasecmp( aaTag, kDSTagAuthAuthorityDisabledUser ) == 0 )
				{
					attrValueIDToReplace = pExistingAttrValue->fAttributeValueID;
					break;
				}
			}
		}
		
		if ( status == eDSNoErr || status == eDSAttributeNotFound || status == eDSAttributeDoesNotExist )
		{
			tDataNodePtr aaNode;
			char *aaNewData = NULL;
			tAttributeValueEntry *pNewPWAttrValue = NULL;
			bool attributeExists = ( status == eDSNoErr );
			
			// set the auth authority
			if ( strncasecmp( aaData, kDSValueAuthAuthorityShadowHash, sizeof(kDSValueAuthAuthorityShadowHash)-1 ) == 0 )
			{
				aaNewData = (char *) malloc( strlen(aaData) + 1 );
				strcpy( aaNewData, aaData );
			}
			else
			{
				aaNewData = (char *) malloc( sizeof(kDSValueAuthAuthorityShadowHash) );
				strcpy( aaNewData, kDSValueAuthAuthorityShadowHash );
			}
			
			if ( pExistingAttrValue != nil )
			{
				pNewPWAttrValue = dsAllocAttributeValueEntry(fDSRef, attrValueIDToReplace, aaNewData, strlen(aaNewData));
				if ( pNewPWAttrValue != nil )
				{
					status = dsSetAttributeValue( inRecordRef, attrTypeNode, pNewPWAttrValue );
					dsDeallocAttributeValueEntry( fDSRef, pNewPWAttrValue );
					pNewPWAttrValue = nil;
				}
			}
			else
			if ( attributeExists )
			{
				aaNode = dsDataNodeAllocateString( fDSRef, aaNewData );
				if ( aaNode )
				{
					status = dsAddAttributeValue( inRecordRef, attrTypeNode, aaNode );
					dsDataNodeDeAllocate( fDSRef, aaNode );
				}
			}
			else
			{
				// no authority
				aaNode = dsDataNodeAllocateString( fDSRef, aaNewData );
				if ( aaNode )
				{
					status = dsAddAttribute( inRecordRef, attrTypeNode, NULL, aaNode );
					dsDataNodeDeAllocate( fDSRef, aaNode );
				}
			}
			
			if ( aaNewData != NULL )
			{
				free( aaNewData );
				aaNewData = NULL;
			}
			
			debugerr( status, "status(1) = %ld.\n", status );
		}
		else
		{
			debugerr(status, "ds error = %ld\n", status);
		}
    }
    catch(...)
	{
	}
}


//-----------------------------------------------------------------------------
//	 SetUserHashList
//
//	Returns: 0==noErr, -1==caller should print usage.
//-----------------------------------------------------------------------------

int
PwdPolicyTool::SetUserHashList( tRecordReference inRecordRef, int firstArg, int argc, char * const *argv )
{
    long						status				= eDSNoErr;
	int							returnValue			= 0;
	tAttributeValueEntry	   *pExistingAttrValue	= NULL;
	unsigned long				attrValIndex		= 0;
    unsigned long				attrValCount		= 0;
    tDataNode				   *attrTypeNode		= NULL;
    tAttributeEntryPtr			pAttrEntry			= NULL;
    char						*aaVersion			= NULL;
    char						*aaTag				= NULL;
    char						*aaData				= NULL;
	char						*aaNewData			= NULL;
    unsigned long				attrValueIDToReplace = 0;
	CFMutableArrayRef			hashTypeArray		= NULL;
	CFStringRef					stringRef			= NULL;
	char						*newDataStr			= NULL;
	long						len					= 0;
	
    try
    {
		pExistingAttrValue = nil;
		attrValueIDToReplace = 0;
        
		// get info about this attribute
		attrTypeNode = dsDataNodeAllocateString( 0, kDSNAttrAuthenticationAuthority );
		status = dsGetRecordAttributeInfo( inRecordRef, attrTypeNode, &pAttrEntry );
		debugerr(status, "dsGetRecordAttributeInfo = %ld\n", status);
		if ( status == eDSNoErr )
		{
			// run through the values and replace the target authority if it exists
			attrValCount = pAttrEntry->fAttributeValueCount;
			for ( attrValIndex = 1; attrValIndex <= attrValCount; attrValIndex++ )
			{
				status = dsGetRecordAttributeValueByIndex( inRecordRef, attrTypeNode, attrValIndex, &pExistingAttrValue );
				debugerr(status, "dsGetRecordAttributeValueByIndex = %ld\n", status);
				if (status != eDSNoErr) continue;
				
				status = dsParseAuthAuthority( pExistingAttrValue->fAttributeValueData.fBufferData, &aaVersion, &aaTag, &aaData );
				if ( status == eDSNoErr )
				{
					if ( strcasecmp( aaTag, kDSTagAuthAuthorityShadowHash ) == 0 )
					{
						attrValueIDToReplace = pExistingAttrValue->fAttributeValueID;
						
						if ( pwsf_ShadowHashDataToArray( aaData, &hashTypeArray ) == 0 )
						{
							// no data, get the default list
							
							// global set
							if ( this->GetHashTypeArray( &hashTypeArray ) != 0 )
							{
								// default set
								hashTypeArray = CFArrayCreateMutable( kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks );
								if ( hashTypeArray == NULL ) {
									fprintf(stderr, "memory error\n");
									return 0;
								}
								
								// secure-by-default
								AppendHashTypeToArray( "SALTED-SHA1", hashTypeArray );
							}
						}
						break;
					}
					
					if ( aaVersion != NULL ) { free( aaVersion ); aaVersion = NULL; }
					if ( aaTag != NULL ) { free( aaTag ); aaTag = NULL; }
					if ( aaData != NULL ) { free( aaData ); aaData = NULL; }
				}
			}
		}
		
		// User must be a ShadowHash user
		if ( status == eDSNoErr && attrValueIDToReplace != 0 )
		{
			tAttributeValueEntry *pNewPWAttrValue = NULL;
			CFIndex typeCount;
			CFIndex hashTypeIndex;
			CFRange arrayRange;
			
			// pre-check
			if ( firstArg >= argc - 1 )
			{
				returnValue = -1;
				throw( returnValue );
			}
			
			// edit the list
			for ( int argIndex = firstArg; argIndex < argc - 1; argIndex += 2 )
			{
				stringRef = CFStringCreateWithCString( kCFAllocatorDefault, argv[argIndex], kCFStringEncodingUTF8 );
				if ( stringRef == NULL )
					continue;
				
				// the list size potentially changes each time in the loop
				typeCount = CFArrayGetCount( hashTypeArray );
				arrayRange = CFRangeMake( 0, typeCount );
				
				if ( strcasecmp( argv[argIndex + 1], "on" ) == 0 )
				{
					if ( ! CFArrayContainsValue( hashTypeArray, arrayRange, (const void *)stringRef ) )
						CFArrayAppendValue( hashTypeArray, (const void *)stringRef );
				}
				else
				if ( strcasecmp( argv[argIndex + 1], "off" ) == 0 )
				{
					do
					{
						hashTypeIndex = CFArrayGetFirstIndexOfValue( hashTypeArray, arrayRange, (const void *)stringRef );
						if ( hashTypeIndex != kCFNotFound )
						{
							CFArrayRemoveValueAtIndex( hashTypeArray, hashTypeIndex );
							typeCount--;
							arrayRange.length--;
						}
					}
					while ( hashTypeIndex != kCFNotFound );
				}
				else
				{
					returnValue = -1;
					throw( returnValue );
				}
				
				CFRelease( stringRef );
			}
			
			// build the new string
			newDataStr = pwsf_ShadowHashArrayToData( hashTypeArray, &len );
			
			// build the auth-authority
			aaNewData = (char *) malloc( sizeof(kDSValueAuthAuthorityShadowHash) + len + 1 );
			if ( aaNewData != NULL )
			{
				len = sprintf( aaNewData, "%s%s", kDSValueAuthAuthorityShadowHash, newDataStr );
				pNewPWAttrValue = dsAllocAttributeValueEntry( fDSRef, attrValueIDToReplace, aaNewData, len );
				if ( pNewPWAttrValue != nil )
				{
					status = dsSetAttributeValue( inRecordRef, attrTypeNode, pNewPWAttrValue );
					dsDeallocAttributeValueEntry( fDSRef, pNewPWAttrValue );
					pNewPWAttrValue = NULL;
				}
			}
			
			debugerr( status, "status(1) = %ld.\n", status );
		}
		else
		{
			debugerr(status, "ds error = %ld\n", status);
		}
    }
    catch(...)
	{
	}
	
	CFRelease( hashTypeArray );
	
	return returnValue;
}


//-----------------------------------------------------------------------------
//	 GetHashTypes
//-----------------------------------------------------------------------------

long
PwdPolicyTool::GetHashTypes( char **outHashTypesStr, bool inExcludeLMHash )
{
	CFMutableArrayRef hashTypeArray;
	CFIndex index, typeCount;
	CFStringRef stringRef;
	char mech[256];
	char scratchStr[256] = {0};
	
	long status = GetHashTypeArray( &hashTypeArray );
	if ( status != eDSNoErr )
		return status;
	
	typeCount = CFArrayGetCount( hashTypeArray );
	for ( index = 0; index < typeCount; index++ )
	{
		stringRef = (CFStringRef)CFArrayGetValueAtIndex( hashTypeArray, index );
		if ( stringRef == NULL )
			continue;
		if ( CFStringGetCString( stringRef, mech, sizeof(mech), kCFStringEncodingUTF8 ) )
		{
			if ( !inExcludeLMHash || strcasecmp(mech, "SMB-LAN-MANAGER") != 0 )
			{
				if ( scratchStr[0] != '\0' )
					strlcat( scratchStr, "\n", sizeof(scratchStr) );
				strlcat( scratchStr, mech, sizeof(scratchStr) );
			}
		}
	}
	
	*outHashTypesStr = (char *) malloc( strlen(scratchStr) + 1 );
	if ( (*outHashTypesStr) != NULL )
		strcpy( *outHashTypesStr, scratchStr );
	
	CFRelease( hashTypeArray );
	
	return status;
}


//-----------------------------------------------------------------------------
//	 SetHashTypes
//-----------------------------------------------------------------------------

long
PwdPolicyTool::SetHashTypes( const char *inName, char *inPasswd, int arg1, int argc, char * const *argv )
{
    int argIndex;
	CFIndex hashTypeIndex;
	CFMutableArrayRef hashTypeArray = NULL;
	tRecordReference recordRef = 0;
	tDirNodeReference localNodeRef = 0;
	tDataNode *attrTypeNode = NULL;
	tDataNode *attrValueNode = NULL;
	bool bNeedToAddAttribute;
	CFIndex typeCount;
	CFRange arrayRange;
	CFStringRef stringRef;
	char mech[256];
	
	// get the current list
	long status = GetHashTypeArray( &hashTypeArray );
	if ( status != eDSNoErr )
		return status;
	
	// edit the list
    for ( argIndex = arg1; argIndex < argc - 1; argIndex += 2 )
    {
		stringRef = CFStringCreateWithCString( kCFAllocatorDefault, argv[argIndex], kCFStringEncodingUTF8 );
		if ( stringRef == NULL )
			continue;
		
		// the list size potentially changes each time in the loop
		typeCount = CFArrayGetCount( hashTypeArray );
		arrayRange = CFRangeMake( 0, typeCount );
		
		if ( strcasecmp( argv[argIndex + 1], "on" ) == 0 )
		{
			if ( ! CFArrayContainsValue( hashTypeArray, arrayRange, (const void *)stringRef ) )
				CFArrayAppendValue( hashTypeArray, (const void *)stringRef );
		}
		else
		if ( strcasecmp( argv[argIndex + 1], "off" ) == 0 )
		{
			do
			{
				hashTypeIndex = CFArrayGetFirstIndexOfValue( hashTypeArray, arrayRange, (const void *)stringRef );
				if ( hashTypeIndex != kCFNotFound )
				{
					CFArrayRemoveValueAtIndex( hashTypeArray, hashTypeIndex );
					typeCount--;
					arrayRange.length--;
				}
			}
			while ( hashTypeIndex != kCFNotFound );
		}
		
		CFRelease( stringRef );
    }
	
	// replace the list
	try
	{
		status = this->OpenDirNode( "/NetInfo/DefaultLocalNode", &localNodeRef );
		if ( status != eDSNoErr ) throw ( status );
		
		if ( inName != NULL )
		{
			status = this->DoNodeNativeAuth( localNodeRef, inName, inPasswd );
			if ( status != eDSNoErr ) throw ( status );
		}
		
		status = this->OpenRecord( localNodeRef, kDSStdRecordTypeConfig, "shadowhash", &recordRef, true );
		if ( status != eDSNoErr ) throw ( status );
		
		// NetInfo node does not support dsSetAttributeValues() so we need to remove/replace
		attrTypeNode = dsDataNodeAllocateString( 0, kDSNativeAttrTypePrefix"optional_hash_list" );
		dsRemoveAttribute( recordRef, attrTypeNode );
		bNeedToAddAttribute = true;
		
		typeCount = CFArrayGetCount( hashTypeArray );
		for ( hashTypeIndex = 0; hashTypeIndex < typeCount; hashTypeIndex++ )
		{
			stringRef = (CFStringRef)CFArrayGetValueAtIndex( hashTypeArray, hashTypeIndex );
			if ( stringRef == NULL )
				continue;
			if ( CFStringGetCString( stringRef, mech, sizeof(mech), kCFStringEncodingUTF8 ) )
			{
				attrValueNode = dsDataNodeAllocateString( 0, mech );
				if ( attrValueNode != NULL )
				{
					if ( bNeedToAddAttribute )
					{
						status = dsAddAttribute( recordRef, attrTypeNode, NULL, attrValueNode );
						bNeedToAddAttribute = false;
					}
					else
					{
						status = dsAddAttributeValue( recordRef, attrTypeNode, attrValueNode );
					}
					
					dsDataNodeDeAllocate( 0, attrValueNode );
					attrValueNode = NULL;
				}
			}
		}
	}
	catch( long catchStatus )
	{
		status = catchStatus;
	}
	
	// clean-up
	if ( attrValueNode != NULL )
		dsDataNodeDeAllocate( 0, attrValueNode );
	if ( attrTypeNode != NULL )
		dsDataNodeDeAllocate( 0, attrTypeNode );
	if (recordRef != 0)
		dsCloseRecord( recordRef );
	this->CloseDirectoryNode( localNodeRef );
	if ( hashTypeArray != NULL )
		CFRelease( hashTypeArray );
	
	return status;
}


long
PwdPolicyTool::GetHashTypeArray( CFMutableArrayRef *outHashTypeArray )
{
	long status;
	tRecordReference recordRef = 0;
	tAttributeEntryPtr attributeInfo;
	tAttributeValueEntry *pAttrValueEntry = NULL;
	tDataNode *attrTypeNode = NULL;
	bool attributeExists = false;
	bool serverOS = true;
	struct stat statResult;
	tDirNodeReference localNodeRef = 0;
	
	if ( outHashTypeArray == NULL )
		return -1;
	*outHashTypeArray = CFArrayCreateMutable( kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks );
	
	try
	{
		status = this->OpenDirNode( "/NetInfo/DefaultLocalNode", &localNodeRef );
		if ( status != eDSNoErr )
			throw( status );
	
		status = this->OpenRecord( localNodeRef, kDSStdRecordTypeConfig, "shadowhash", &recordRef );
		if ( status != eDSNoErr )
			throw( status );
		
		attrTypeNode = dsDataNodeAllocateString( 0, kDSNativeAttrTypePrefix"optional_hash_list" );
		status = dsGetRecordAttributeInfo( recordRef, attrTypeNode, &attributeInfo );
		if ( status != eDSNoErr )
			throw( status );
		
		attributeExists = true;
		
		for ( unsigned int valueIndex = 1; valueIndex <= attributeInfo->fAttributeValueCount; valueIndex++ )
		{
			status = dsGetRecordAttributeValueByIndex( recordRef, attrTypeNode, valueIndex, &pAttrValueEntry );
			if ( status != eDSNoErr )
				continue;
			
			AppendHashTypeToArray( pAttrValueEntry->fAttributeValueData.fBufferData, *outHashTypeArray );
			dsDeallocAttributeValueEntry( fDSRef, pAttrValueEntry );
		}
	}
	catch( long catchStatus )
	{
		status = catchStatus;
	}
	
	if ( ! attributeExists )
	{
		status = eDSNoErr;
		
		serverOS = (stat( "/System/Library/CoreServices/ServerVersion.plist", &statResult ) == 0);
		
		// return the default set
		// If Server OS, add more mechs.
		if ( serverOS )
		{
			AppendHashTypeToArray( "CRAM-MD5", *outHashTypeArray );
			AppendHashTypeToArray( "RECOVERABLE", *outHashTypeArray );
		}
		
		AppendHashTypeToArray( "SALTED-SHA1", *outHashTypeArray );
		AppendHashTypeToArray( "SMB-LAN-MANAGER", *outHashTypeArray );
		AppendHashTypeToArray( "SMB-NT", *outHashTypeArray );
	}
	
	// clean up
	if ( attrTypeNode != NULL )
		dsDataNodeDeAllocate( 0, attrTypeNode );
	if (recordRef != 0)
		dsCloseRecord( recordRef );
	this->CloseDirectoryNode( localNodeRef );
	
	return status;
}


void
PwdPolicyTool::AppendHashTypeToArray( const char *inHashType, CFMutableArrayRef inHashTypeArray )
{
	CFStringRef stringRef = CFStringCreateWithCString( kCFAllocatorDefault, inHashType, kCFStringEncodingUTF8 );
	CFArrayAppendValue( inHashTypeArray, stringRef );
	CFRelease( stringRef );
}



