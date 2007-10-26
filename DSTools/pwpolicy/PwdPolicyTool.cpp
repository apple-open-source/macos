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

extern "C" {
extern tDirStatus dsFillAuthBuffer(
	tDataBufferPtr inOutAuthBuffer,
	unsigned long inCount,
	unsigned long inLen,
	const void *inData, ... );
};

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

tDirStatus PwdPolicyTool::Initialize ( void )
{
	tDirStatus		siStatus		= eDSNoErr;
	char			**pNodeName		= NULL;
	
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
		if ( pNodeName[0] != NULL ) {
			siStatus = OpenDirNode( pNodeName[0], &fSearchNodeRef );
			free( pNodeName[0] );
		}
		
		free( pNodeName );
		pNodeName = NULL;
		
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

tDirStatus PwdPolicyTool::Deinitialize ( void )
{
	tDirStatus			siStatus		= eDSNoErr;

	siStatus = DeallocateTDataBuff();
	if ( siStatus != eDSNoErr )
	{
		PrintError( siStatus, "DeallocateTDataBuff" );
	}

	// local node
	siStatus = CloseDirectoryNode( fLocalNodeRef );
	if ( siStatus != eDSNoErr )
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

tDirStatus PwdPolicyTool::OpenDirectoryServices ( void )
{
	tDirStatus		error	= eDSNoErr;

	if ( gVerbose == true )
	{
		fprintf( stderr, "\n----- Opening Directory Services -----\n" );
	}

	error = dsOpenDirService( &fDSRef );
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

tDirStatus PwdPolicyTool::CloseDirectoryServices ( void )
{
	tDirStatus		error	= eDSNoErr;

	if ( gVerbose == true )
	{
		fprintf( stderr, "\n----- Closing Directory Services -----\n" );
	}

	error = dsCloseDirService( fDSRef );
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

tDirStatus PwdPolicyTool::AllocateTDataBuff ( void )
{
	tDirStatus		error	= eDSNoErr;

	if ( gVerbose == true )
	{
		fprintf( stderr, "\n----- Allocating a %ldK buffer -----\n", kBuffSize / 1024 );
	}

	fTDataBuff = dsDataBufferAllocate( fDSRef, kBuffSize );
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

tDirStatus PwdPolicyTool::DeallocateTDataBuff ( void )
{
	tDirStatus		error	= eDSNoErr;

	if ( gVerbose == true )
	{
		fprintf( stderr, "\n----- Deallocating default buffer -----\n" );
	}

	error = dsDataBufferDeAllocate( fDSRef, fTDataBuff );
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

tDirStatus PwdPolicyTool::DoGetRecordList (	tDirNodeReference   inNodeRef,
										const char			*inRecName,
										const char			*inRecType,
										char				*inAttrType,
										tDirPatternMatch	 inMatchType,	// eDSExact, eDSContains ...
										char				**outAuthAuthority,
										char				**outNodeName )
{
	tDirStatus				error			= eDSNoErr;
	tDirStatus				error2			= eDSNoErr;
	UInt32					recCount		= 1;
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

	pRecName = dsBuildListFromStrings( fDSRef, inRecName, nil );
	if ( pRecName != nil )
	{
		pRecType = dsBuildListFromStrings( fDSRef, inRecType, nil );
		if ( pRecType != nil )
		{
			pAttrType = dsBuildListFromStrings( fDSRef, inAttrType, kDSNAttrMetaNodeLocation, nil );
			if ( pAttrType != nil )
			{
				*outAuthAuthority = NULL;
				*outNodeName = NULL;
				
				do
				{
					error = dsGetRecordList( inNodeRef, fTDataBuff, pRecName, inMatchType, pRecType,
												pAttrType, false, &recCount, &context );
					if ( error == eDSNoErr )
					{
						error = GetDataFromDataBuff( inNodeRef, fTDataBuff, recCount, outAuthAuthority, outNodeName );
					} 
					else if ( error == eDSBufferTooSmall )
					{
						UInt32 buffSize = fTDataBuff->fBufferSize;
						dsDataBufferDeAllocate( fDSRef, fTDataBuff );
						fTDataBuff = nil;
						fTDataBuff = dsDataBufferAllocate( fDSRef, buffSize * 2 );
					}
				} while ( ((error == eDSNoErr) && (context != nil)) || (error == eDSBufferTooSmall) );

				error2 = dsDataListDeallocate( fDSRef, pAttrType );
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

			error2 = dsDataListDeallocate( fDSRef, pRecType );
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

		error2 = dsDataListDeallocate( fDSRef, pRecName );
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

tDirStatus PwdPolicyTool::GetDataFromDataBuff(
	tDirNodeReference inNodeRef,
	tDataBuffer *inTDataBuff,
	UInt32 inRecCount,
	char **outAuthAuthority,
	char **outNodeName )
{
	tDirStatus				error			= eDSNoErr;
	UInt32					i				= 0;
	UInt32					j				= 0;
	UInt32					k				= 0;
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
			error = dsGetRecordEntry( inNodeRef, inTDataBuff, i, &attrListRef, &pRecEntry );
			if ( error == eDSNoErr && pRecEntry != NULL )
			{
				error = dsGetRecordNameFromEntry( pRecEntry, &pRecNameStr );
				if ( error == eDSNoErr )
				{
					error = dsGetRecordTypeFromEntry( pRecEntry, &pRecTypeStr );
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
							error = dsGetAttributeEntry( inNodeRef, inTDataBuff, attrListRef, j, &valueRef, &pAttrEntry );
							if ( error == eDSNoErr && pAttrEntry != NULL )
							{
								for ( k = 1; (k <= pAttrEntry->fAttributeValueCount) && (error == eDSNoErr); k++ )
								{
									error = dsGetAttributeValue( inNodeRef, inTDataBuff, k, valueRef, &pValueEntry );
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
											dsDeallocAttributeValueEntry( fDSRef, pValueEntry );
											pValueEntry = NULL;
											found = true;
										}
										else
										if ( strcasestr( pAttrEntry->fAttributeSignature.fBufferData, kDSNAttrMetaNodeLocation ) != NULL )
										{
											*outNodeName = (char *) malloc( pValueEntry->fAttributeValueData.fBufferLength + 1 );
											strcpy( *outNodeName, pValueEntry->fAttributeValueData.fBufferData );
											dsDeallocAttributeValueEntry( fDSRef, pValueEntry );
											pValueEntry = NULL;
										}
										
										dsDeallocAttributeValueEntry( fDSRef, pValueEntry );
										pValueEntry = NULL;
									}
									else
									{
										PrintError( error, "dsGetAttributeValue" );
									}
								}
								dsDeallocAttributeEntry( fDSRef, pAttrEntry );
								pAttrEntry = NULL;
								dsCloseAttributeValueList(valueRef);
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
				dsDeallocRecordEntry( fDSRef, pRecEntry );
				pRecEntry = NULL;
				dsCloseAttributeList(	attrListRef	);
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

tDirStatus PwdPolicyTool::FindDirectoryNodes( char			*inNodeName,
									  	tDirPatternMatch	inMatch,
									  	char			  **outNodeNameList[],
										bool				inPrintNames )
{
	tDirStatus		error			= eDSNoErr;
	tDirStatus		error2			= eDSNoErr;
	UInt32			uiCount			= 0;
	UInt32			uiIndex			= 0;
	UInt32			outIndex		= 0;
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
			throw( (tDirStatus)eDSNullParameter );
		
		if ( inNodeName != nil )
		{
			pNodeNameList = dsBuildFromPath( fDSRef, inNodeName, "/" );
			if ( pNodeNameList == nil )
			{
				PrintError( eMemoryAllocError, "dsBuildFromPath" );
				throw( (tDirStatus)eMemoryAllocError );
			}
		}

		do {
			error = dsFindDirNodes( fDSRef, fTDataBuff, pNodeNameList, inMatch, &uiCount, nil );
			if ( error == eDSBufferTooSmall )
			{
				UInt32 buffSize = fTDataBuff->fBufferSize;
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
				*outNodeNameList = (char **) calloc(uiCount + 1, sizeof(char *));
				if ( *outNodeNameList == NULL )
					throw((tDirStatus)eMemoryError);
				
				pDataList = dsDataListAllocate( fDSRef );
				if ( pDataList != nil )
				{
					for ( uiIndex = 1; (uiIndex <= uiCount) && (error == eDSNoErr); uiIndex++ )
					{
						error = dsGetDirNodeName( fDSRef, fTDataBuff, uiIndex, &pDataList );
						if ( error == eDSNoErr )
						{
							pNodeName = dsGetPathFromList( fDSRef, pDataList, "/" );
							if ( pNodeName != nil )
							{
								if ( inPrintNames || gVerbose )
								{
									fprintf( stderr, "  %2ld - Node Name = %s\n", uiIndex, pNodeName );
								}

								(*outNodeNameList)[outIndex++] = pNodeName;

								error2 = dsDataListDeallocate( fDSRef, pDataList );
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
			error2 = dsDataListDeallocate( fDSRef, pNodeNameList );
			if ( error2 != eDSNoErr )
			{
				PrintError( error2, "dsDataListDeallocate" );
			}
		}
	}

	catch ( tDirStatus err )
	{
		PrintError( err, "FindDirectoryNodes" );
		error = err;
	}

	return( error );

} // FindDirectoryNodes


//--------------------------------------------------------------------------------------------------
// * OpenLocalNode ()
//
//--------------------------------------------------------------------------------------------------

tDirStatus PwdPolicyTool::OpenLocalNode( tDirNodeReference *outNodeRef )
{
	char **localNodeNameList = NULL;
	
	tDirStatus result = this->FindDirectoryNodes( NULL, eDSLocalNodeNames, &localNodeNameList, false );
	if ( result == eDSNoErr )
	{
		if ( localNodeNameList != NULL && localNodeNameList[0] != NULL )
		{
			result = this->OpenDirNode( localNodeNameList[0], outNodeRef );
			free( localNodeNameList[0] );
			free( localNodeNameList );
		}
		else
		{
			result = eDSOpenNodeFailed;
		}
	}
	
	return result;
}


//--------------------------------------------------------------------------------------------------
// * OpenDirNode ()
//
//--------------------------------------------------------------------------------------------------

tDirStatus PwdPolicyTool::OpenDirNode ( char *inNodeName, tDirNodeReference *outNodeRef )
{
	tDirStatus		error		= eDSNoErr;
	tDirStatus		error2		= eDSNoErr;
	tDataList	   *pDataList	= nil;

	if ( gVerbose == true )
	{
		fprintf( stderr, "\n----- Opening Directory Node -----\n" );
		fprintf( stderr, "    Node Name:      %s\n", inNodeName );
	}


	pDataList = dsBuildFromPath( fDSRef, inNodeName, "/" );
	if ( pDataList != nil )
	{
		error = dsOpenDirNode( fDSRef, pDataList, outNodeRef );
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

		error2 = dsDataListDeallocate( fDSRef, pDataList );
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

tDirStatus PwdPolicyTool::CloseDirectoryNode ( tDirNodeReference inNodeRef )
{
	tDirStatus			error		= eDSNoErr;

	if ( gVerbose == true )
	{
		fprintf( stderr, "\n----- Closing Directory Node -----\n" );
		fprintf( stderr, "    Node Reference: %lu\n", inNodeRef );
	}
	
	if ( inNodeRef == 0 )
		return eDSNoErr;
	
	error = dsCloseDirNode( inNodeRef );
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

tDirStatus
PwdPolicyTool::DoNodePWAuth(
	tDirNodeReference inNode,
	const char *inName,
	const char *inPasswd,
	const char *inMethod,
	char *inUserName,
	const char *inOther,
	const char *inRecordType,
	char *outResult )
{
	tDirStatus		error			= eDSNoErr;
	tDirStatus		error2			= eDSNoErr;
	tDataBuffer	   *pAuthBuff		= nil;
	tDataBuffer	   *pStepBuff		= nil;
	tDataNode	   *pAuthType		= nil;
	tDataNodePtr	recordTypeNode	= NULL;
	
	// kDSStdAuthNewUser
	// "dsAuthMethodStandard:dsAuthNewUser"
    
	if ( gVerbose == true )
	{
		fprintf( stderr, "\n----- Node Password Server Auth -----\n" );
		fprintf( stderr, "  User Name   = %s\n", inName );
	}
	
	recordTypeNode = dsDataNodeAllocateString( fDSRef, inRecordType ? inRecordType : kDSStdRecordTypeUsers );
	
	error = SetUpAuthBuffs( &pAuthBuff, 2048, &pStepBuff, 2048, &pAuthType, inMethod );
	if ( error == eDSNoErr )
	{
		if ( inName == NULL )
			inName = "";
		if ( inPasswd == NULL )
			inPasswd = "";
		
		if ( strcmp(kDSStdAuthGetEffectivePolicy, inMethod) == 0 )
		{
			error = dsFillAuthBuffer( pAuthBuff, 1,
									 ::strlen( inUserName ), inUserName);
		}
		else
		if ( inOther != NULL )
		{
			error = dsFillAuthBuffer( pAuthBuff, 4,
									::strlen( inName ), inName,
									::strlen( inPasswd ), inPasswd,
									::strlen( inUserName ), inUserName,
									::strlen( inOther ), inOther );
		}
		else
		if ( inUserName != NULL )
		{
			error = dsFillAuthBuffer( pAuthBuff, 3,
									::strlen( inName ), inName,
									::strlen( inPasswd ), inPasswd,
									::strlen( inUserName ), inUserName );
		}
		else
		{
			error = dsFillAuthBuffer( pAuthBuff, 2,
									::strlen( inName ), inName,
									::strlen( inPasswd ), inPasswd );
		}
		
		if ( error == eDSNoErr )
		{
			if ( inRecordType == NULL || strcmp(inRecordType, kDSStdRecordTypeUsers) == 0 )
				error = dsDoDirNodeAuth( inNode, pAuthType, true, pAuthBuff, pStepBuff, nil );
			else
				error = dsDoDirNodeAuthOnRecordType( inNode, pAuthType, true, pAuthBuff, pStepBuff, nil, recordTypeNode );
			if ( error == eDSNoErr )
			{
				UInt32 len;
				
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
		
		error2 = dsDataBufferDeAllocate( fDSRef, pAuthBuff );
		if ( error2 != eDSNoErr )
		{
			PrintError( error2, "dsDataBufferDeAllocate" );
		}

		error2 = dsDataBufferDeAllocate( fDSRef, pStepBuff );
		if ( error2 != eDSNoErr )
		{
			PrintError( error2, "dsDataBufferDeAllocate" );
		}

		error2 = dsDataBufferDeAllocate( fDSRef, pAuthType );
		if ( error2 != eDSNoErr )
		{
			PrintError( error2, "dsDataBufferDeAllocate" );
		}
	}
	
	if ( recordTypeNode )
		dsDataNodeDeAllocate( fDSRef, recordTypeNode );
	
	return( error );

} // DoNodePWAuth

//--------------------------------------------------------------------------------------------------
// * DoNodeNativeAuth ()
//
//--------------------------------------------------------------------------------------------------

tDirStatus PwdPolicyTool::DoNodeNativeAuth ( tDirNodeReference inNode, const char *inName, char *inPasswd )
{
	tDirStatus		error			= eDSNoErr;
	tDirStatus		error2			= eDSNoErr;
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

		error = dsFillAuthBuffer( pAuthBuff, 2, strlen( inName ), inName, strlen( inPasswd ), inPasswd );
		if ( error == eDSNoErr )
		{
			error = dsDoDirNodeAuth( inNode, pAuthType, false, pAuthBuff, pStepBuff, nil );
			if ( error != eDSNoErr )
			{
				PrintError( error, "dsDoDirNodeAuth" );
			}
		}
		
		error2 = dsDataBufferDeAllocate( fDSRef, pAuthBuff );
		if ( error2 != eDSNoErr )
		{
			PrintError( error2, "dsDataBufferDeAllocate" );
		}

		error2 = dsDataBufferDeAllocate( fDSRef, pStepBuff );
		if ( error2 != eDSNoErr )
		{
			PrintError( error2, "dsDataBufferDeAllocate" );
		}

		error2 = dsDataBufferDeAllocate( fDSRef, pAuthType );
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
		fprintf( stderr, "\n***Error: %s : (%ld)\n", statusString, inErrCode );
	}
	else
	{
		fprintf( stderr, "\n***Error: %s : (%ld) for %s\n", statusString, inErrCode, messageTag );
	}

	free(statusString);
	statusString = nil;

	fflush( stderr );

} // PrintError


//--------------------------------------------------------------------------------------------------
// * SetUpAuthBuffs ()
//
//--------------------------------------------------------------------------------------------------

tDirStatus PwdPolicyTool::SetUpAuthBuffs ( tDataBuffer **outAuthBuff,
									UInt32 inAuthBuffSize,
									tDataBuffer **outStepBuff,
									UInt32 inStepBuffSize,
									tDataBuffer **outTypeBuff,
									const char *inAuthMethod )
{
	tDirStatus		error	= eDSNoErr;
	tDirStatus		error2	= eDSNoErr;

	if ( (outAuthBuff == nil) || (outStepBuff == nil) ||
		 (outTypeBuff == nil) || (inAuthMethod == nil) )
	{
		return( eDSNullParameter );
	}

	*outAuthBuff = dsDataBufferAllocate( fDSRef, inAuthBuffSize );
	if ( *outAuthBuff != nil )
	{
		*outStepBuff = dsDataBufferAllocate( fDSRef, inStepBuffSize );
		if ( *outStepBuff != nil )
		{
			*outTypeBuff = dsDataNodeAllocateString( fDSRef, inAuthMethod );
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
			error2 = dsDataBufferDeAllocate( fDSRef, *outAuthBuff );
			if ( error2 != eDSNoErr )
			{
				PrintError( error2, "dsDataBufferDeAllocate" );
			}
		}

		if ( *outStepBuff != nil )
		{
			error2 = dsDataBufferDeAllocate( fDSRef, *outStepBuff );
			if ( error2 != eDSNoErr )
			{
				PrintError( error2, "dsDataBufferDeAllocate" );
			}
		}

		if ( *outTypeBuff != nil )
		{
			error2 = dsDataBufferDeAllocate( fDSRef, *outTypeBuff );
			if ( error2 != eDSNoErr )
			{
				PrintError( error2, "dsDataBufferDeAllocate" );
			}
		}
	}

	return( error );

} // SetUpAuthBuffs


//--------------------------------------------------------------------------------------------------
// * GetUserByName ()
//
//--------------------------------------------------------------------------------------------------

tDirStatus
PwdPolicyTool::GetUserByName(
	tDirNodeReference inNode,
	const char *inUserName,
	const char *inRecordType,
	char **outAuthAuthority,
	char **outNodeName )
{
	tDirStatus status = eDSNoErr;

	if (gVerbose)
		fprintf( stderr, "\n----- Getting user by name: %s -----\n", inUserName );

	status = DoGetRecordList( inNode, inUserName, inRecordType, kDSNAttrAuthenticationAuthority, eDSExact, outAuthAuthority, outNodeName );
	if ( outAuthAuthority && *outAuthAuthority == NULL )
	{
		char *unamePlusDollar = (char *) malloc( strlen(inUserName) + 2 );
		strcpy( unamePlusDollar, inUserName );
		strcat( unamePlusDollar, "$" );
		status = DoGetRecordList( inNode, unamePlusDollar, inRecordType, kDSNAttrAuthenticationAuthority, eDSExact, outAuthAuthority, outNodeName );
	}
	
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
	UInt32					attrValIndex		= 0;
    UInt32					attrValCount		= 0;
    tDataNode				   *attrTypeNode		= nil;
    tAttributeEntryPtr			pAttrEntry			= nil;
    char						*aaVersion			= nil;
    char						*aaTag				= nil;
    char						*aaData				= nil;
    UInt32					attrValueIDToReplace = 0;
	
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
	UInt32					attrValIndex		= 0;
    UInt32					attrValCount		= 0;
    tDataNode				   *attrTypeNode		= NULL;
    tAttributeEntryPtr			pAttrEntry			= NULL;
    char						*aaVersion			= NULL;
    char						*aaTag				= NULL;
    char						*aaData				= NULL;
	char						*aaNewData			= NULL;
    UInt32					attrValueIDToReplace = 0;
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
	bool serverOS = false;
	struct stat statResult;
	CFIndex typeCount;
	CFRange arrayRange;
	CFStringRef stringRef;
	char mech[256];
	
	// get the current list
	long status = GetHashTypeArray( &hashTypeArray );
	if ( status != eDSNoErr )
		return status;
	
	serverOS = (stat( "/System/Library/CoreServices/ServerVersion.plist", &statResult ) == 0);
	
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
			// RECOVERABLE is always prohibited on Desktop OS so do not
			// add it to the list
			if ( serverOS || strcmp(argv[argIndex], "RECOVERABLE") != 0 ) {
				if ( ! CFArrayContainsValue( hashTypeArray, arrayRange, (const void *)stringRef ) )
					CFArrayAppendValue( hashTypeArray, (const void *)stringRef );
			}
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
		status = this->OpenLocalNode( &localNodeRef );
		if ( status != eDSNoErr ) throw ( status );
		
		if ( inName != NULL && inPasswd != NULL )
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
	bool serverOS = false;
	struct stat statResult;
	tDirNodeReference localNodeRef = 0;
	
	if ( outHashTypeArray == NULL )
		return -1;
	*outHashTypeArray = CFArrayCreateMutable( kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks );
	
	serverOS = (stat( "/System/Library/CoreServices/ServerVersion.plist", &statResult ) == 0);
	
	try
	{
		status = this->OpenLocalNode( &localNodeRef );
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
			
			// RECOVERABLE is always prohibited on Desktop OS so filter
			// it out of the list
			if ( serverOS || strcmp(pAttrValueEntry->fAttributeValueData.fBufferData, "RECOVERABLE") != 0 )
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
		
		// return the default set
		// If Server OS, add more mechs.
		if ( serverOS )
		{
			AppendHashTypeToArray( "CRAM-MD5", *outHashTypeArray );
			AppendHashTypeToArray( "RECOVERABLE", *outHashTypeArray );
			AppendHashTypeToArray( "SMB-LAN-MANAGER", *outHashTypeArray );
			AppendHashTypeToArray( "SMB-NT", *outHashTypeArray );
		}
		
		AppendHashTypeToArray( "SALTED-SHA1", *outHashTypeArray );
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



