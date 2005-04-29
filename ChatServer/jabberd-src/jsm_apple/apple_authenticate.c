/*
 *
 * Copyright (c) 2005, Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_BSD_LICENSE_HEADER_START@
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer. 
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution. 
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission. 
 * 
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 * 
 * @APPLE_BSD_LICENSE_HEADER_END@
 * 
*/

#include "apple_authenticate.h"
#include <sys/types.h>
#include <pwd.h>
#include <Security/checkpw.h>

#include <syslog.h>
#include <CoreServices/CoreServices.h>

#include <CoreFoundation/CFData.h>
#include <CoreFoundation/CFString.h>
#include <CoreFoundation/CFNumber.h>
#include <CoreFoundation/CFPropertyList.h>

#include <DirectoryService/DirServices.h>
#include <DirectoryService/DirServicesUtils.h>
#include <DirectoryService/DirServicesConst.h>

enum { kLogErrors_disabled = 0, kLogErrors_enabled = 1 };
enum { kValidate_AuthMethod = 0, kValidate_User = 1 };

int authenticate_cram_md5 ( const char *inUserID, const char *inChallenge, const char *inResponse  );
int apple_checkpw( const char* userName, const char* password );
/*
    int checkpw( const char* userName, const char* password );
    
    username (input) username as a UTF8 string	
	password (input) password as a UTF8 string
    int result: CHECKPW_SUCCESS | CHECKPW_UNKNOWNUSER | CHECKPW_BADPASSWORD |CHECKPW_FAILURE
    
    CHECKPW_SUCCESS username/password correct	
	CHECKPW_UNKNOWNUSER no such user
	CHECKPW_BADPASSWORD wrong password
	CHECKPW_FAILURE failed to communicate with DirectoryServices

*/


int apple_checkpw( const char* userName, const char* password )
{
    if (CHECKPW_SUCCESS == checkpw(userName, password))
        return kAuthenticated;
     
     return kFailed;
}



/* -----------------------------------------------------------------
	sOpen_user_node ()
   ----------------------------------------------------------------- */

tDirStatus sOpen_user_node (  tDirReference inDirRef, const char *inUserLoc, tDirNodeReference *outUserNodeRef )
{
	tDirStatus		dsStatus	= eMemoryAllocError;
	tDataList	   *pUserNode	= NULL;

	pUserNode = dsBuildFromPath( inDirRef, inUserLoc, "/" );
	if ( pUserNode != NULL )
	{
		dsStatus = dsOpenDirNode( inDirRef, pUserNode, outUserNodeRef );

		(void)dsDataListDeAllocate( inDirRef, pUserNode, TRUE );
		free( pUserNode );
		pUserNode = NULL;
	}

	return( dsStatus );

} /* sOpen_user_node */



/* -----------------------------------------------------------------
	sGet_search_node ()
   ----------------------------------------------------------------- */

tDirStatus sGet_search_node ( tDirReference inDirRef,
							 tDirNodeReference *outSearchNodeRef )
{
	tDirStatus		dsStatus	= eMemoryAllocError;
	unsigned long	uiCount		= 0;
	tDataBuffer	   *pTDataBuff	= NULL;
	tDataList	   *pDataList	= NULL;

	pTDataBuff = dsDataBufferAllocate( inDirRef, 8192 );
	if ( pTDataBuff != NULL )
	{
		dsStatus = dsFindDirNodes( inDirRef, pTDataBuff, NULL, eDSSearchNodeName, &uiCount, NULL );
		if ( dsStatus == eDSNoErr )
		{
			dsStatus = eDSNodeNotFound;
			if ( uiCount == 1 )
			{
				dsStatus = dsGetDirNodeName( inDirRef, pTDataBuff, 1, &pDataList );
				if ( dsStatus == eDSNoErr )
				{
					dsStatus = dsOpenDirNode( inDirRef, pDataList, outSearchNodeRef );
				}

				if ( pDataList != NULL )
				{
					(void)dsDataListDeAllocate( inDirRef, pDataList, true );

					free( pDataList );
					pDataList = NULL;
				}
			}
		}
		(void)dsDataBufferDeAllocate( inDirRef, pTDataBuff );
		pTDataBuff = NULL;
	}

	return( dsStatus );

} /* sGet_search_node */



/* -----------------------------------------------------------------
	sLook_up_user ()
   ----------------------------------------------------------------- */

tDirStatus sLook_up_user ( tDirReference inDirRef,
						  tDirNodeReference inSearchNodeRef,
						  const char *inUserID,
						  char **outUserLocation )
{
	tDirStatus				dsStatus		= eMemoryAllocError;
	int						done			= FALSE;
	char				   *pAcctName		= NULL;
	unsigned long			uiRecCount		= 0;
	tDataBuffer			   *pTDataBuff		= NULL;
	tDataList			   *pUserRecType	= NULL;
	tDataList			   *pUserAttrType	= NULL;
	tRecordEntry		   *pRecEntry		= NULL;
	tAttributeEntry		   *pAttrEntry		= NULL;
	tAttributeValueEntry   *pValueEntry		= NULL;
	tAttributeValueListRef	valueRef		= 0;
	tAttributeListRef		attrListRef		= 0;
	tContextData			pContext		= NULL;
	tDataList				tdlRecName;

	memset( &tdlRecName,  0, sizeof( tDataList ) );

	pTDataBuff = dsDataBufferAllocate( inDirRef, 8192 );
	if ( pTDataBuff != NULL )
	{
		dsStatus = dsBuildListFromStringsAlloc( inDirRef, &tdlRecName, inUserID, NULL );
		if ( dsStatus == eDSNoErr )
		{
			dsStatus = eMemoryAllocError;

			pUserRecType = dsBuildListFromStrings( inDirRef, kDSStdRecordTypeUsers, NULL );
			if ( pUserRecType != NULL )
			{
				pUserAttrType = dsBuildListFromStrings( inDirRef, kDSNAttrMetaNodeLocation, NULL );
				if ( pUserAttrType != NULL );
				{
					do {
						/* Get the user record(s) that matches the name */
						dsStatus = dsGetRecordList( inSearchNodeRef, pTDataBuff, &tdlRecName, eDSiExact, pUserRecType,
													pUserAttrType, FALSE, &uiRecCount, &pContext );

						if ( dsStatus == eDSNoErr )
						{
							dsStatus = eDSInvalidName;
							if ( uiRecCount == 1 ) 
							{
								dsStatus = dsGetRecordEntry( inSearchNodeRef, pTDataBuff, 1, &attrListRef, &pRecEntry );
								if ( dsStatus == eDSNoErr )
								{
									/* Get the record name */
									(void)dsGetRecordNameFromEntry( pRecEntry, &pAcctName );
			
									dsStatus = dsGetAttributeEntry( inSearchNodeRef, pTDataBuff, attrListRef, 1, &valueRef, &pAttrEntry );
									if ( (dsStatus == eDSNoErr) && (pAttrEntry != NULL) )
									{
										dsStatus = dsGetAttributeValue( inSearchNodeRef, pTDataBuff, 1, valueRef, &pValueEntry );
										if ( (dsStatus == eDSNoErr) && (pValueEntry != NULL) )
										{
											if ( strcmp( pAttrEntry->fAttributeSignature.fBufferData, kDSNAttrMetaNodeLocation ) == 0 )
											{
												/* Get the user location */
												*outUserLocation = (char *)calloc( pValueEntry->fAttributeValueData.fBufferLength + 1, sizeof( char ) );
												memcpy( *outUserLocation, pValueEntry->fAttributeValueData.fBufferData, pValueEntry->fAttributeValueData.fBufferLength );

												/* If we don't find duplicate users in the same node, we take the first one with
													a valid mail attribute */
												done = TRUE;
											}
											(void)dsCloseAttributeValueList( valueRef );
										}

										(void)dsDeallocAttributeEntry( inSearchNodeRef, pAttrEntry );
										pAttrEntry = NULL;

										(void)dsCloseAttributeList( attrListRef );
									}

									if ( pRecEntry != NULL )
									{
										(void)dsDeallocRecordEntry( inSearchNodeRef, pRecEntry );
										pRecEntry = NULL;
									}
								}
							}
							else
							{
								done = true;
								if ( uiRecCount > 1 )
								{
									syslog( LOG_ERR, "cram_md5_auth: Duplicate users %s found in directory.", inUserID );
								}
								dsStatus = eDSAuthInvalidUserName;
							}
						}
					} while ( (pContext != NULL) && (dsStatus == eDSNoErr) && (!done) );

					if ( pContext != NULL )
					{
						(void)dsReleaseContinueData( inSearchNodeRef, pContext );
						pContext = NULL;
					}
					(void)dsDataListDeallocate( inDirRef, pUserAttrType );
					pUserAttrType = NULL;
				}
				(void)dsDataListDeallocate( inDirRef, pUserRecType );
				pUserRecType = NULL;
			}
			(void)dsDataListDeAllocate( inDirRef, &tdlRecName, TRUE );
		}
		(void)dsDataBufferDeAllocate( inDirRef, pTDataBuff );
		pTDataBuff = NULL;
	}

	if ( pAcctName != NULL )
	{
		free( pAcctName );
		pAcctName = NULL;
	}

	return( dsStatus );

} /* sLook_up_user */


/* -----------------------------------------------------------------
	sValidateResponse ()
   ----------------------------------------------------------------- */

int sValidateResponse ( const char *inUserID, const char *inChallenge, const char *inResponse, const char *inAuthType, int validateType )
{
	int		        iResult			= -1;
	tDirStatus              dsStatus		= eDSNoErr;
	tDirReference		dirRef			= 0;
	tDirNodeReference	searchNodeRef           = 0;
	tDirNodeReference	userNodeRef		= 0;
	tDataBuffer		*pAuthBuff		= NULL;
	tDataBuffer		*pStepBuff		= NULL;
	tDataNode		*pAuthType		= NULL;
	char			*userLoc		= NULL;
	unsigned long		uiNameLen		= 0;
	unsigned long		uiChalLen		= 0;
	unsigned long		uiRespLen		= 0;
	unsigned long		uiBuffSzie		= 0;
	unsigned long		uiCurr			= 0;
	unsigned long		uiLen			= 0;
	int                     logErrors               = kLogErrors_disabled;
	
	if (kValidate_User == validateType) 
           logErrors = kLogErrors_enabled;


	if ( (inUserID == NULL) || (inChallenge == NULL) || (inResponse == NULL) || (inAuthType == NULL) )
	{
		return( -1 );
	}

	uiNameLen = strlen( inUserID );
	uiChalLen = strlen( inChallenge );
	uiRespLen = strlen( inResponse );

	uiBuffSzie = uiNameLen + uiChalLen + uiRespLen + 32;

	dsStatus = dsOpenDirService( &dirRef );
	if ( dsStatus == eDSNoErr )
	{
		dsStatus = sGet_search_node( dirRef, &searchNodeRef );
		if ( dsStatus == eDSNoErr )
		{
			dsStatus = sLook_up_user( dirRef, searchNodeRef, inUserID, &userLoc );
			if ( dsStatus == eDSNoErr )
			{
				dsStatus = sOpen_user_node( dirRef, userLoc, &userNodeRef );
				if ( dsStatus == eDSNoErr )
				{
					pAuthBuff = dsDataBufferAllocate( dirRef, uiBuffSzie );
					if ( pAuthBuff != NULL )
					{
						pStepBuff = dsDataBufferAllocate( dirRef, 256 );
						if ( pStepBuff != NULL )
						{
							pAuthType = dsDataNodeAllocateString( dirRef, inAuthType );
							if ( pAuthType != NULL )
							{
								/* User name */
								uiLen = uiNameLen;
								memcpy( &(pAuthBuff->fBufferData[ uiCurr ]), &uiLen, sizeof( unsigned long ) );
								uiCurr += sizeof( unsigned long );
								memcpy( &(pAuthBuff->fBufferData[ uiCurr ]), inUserID, uiLen );
								uiCurr += uiLen;

								if (kValidate_User == validateType) 
								{
									/* Challenge */
									uiLen = uiChalLen;
									memcpy( &(pAuthBuff->fBufferData[ uiCurr ]), &uiLen, sizeof( unsigned long ) );
									uiCurr += sizeof( unsigned long );
									memcpy( &(pAuthBuff->fBufferData[ uiCurr ]), inChallenge, uiLen );
									uiCurr += uiLen;
	
									/* Response */
									uiLen = uiRespLen;
									memcpy( &(pAuthBuff->fBufferData[ uiCurr ]), &uiLen, sizeof( unsigned long ) );
									uiCurr += sizeof( unsigned long );
									memcpy( &(pAuthBuff->fBufferData[ uiCurr ]), inResponse, uiLen );
									uiCurr += uiLen;
								}
								
								pAuthBuff->fBufferLength = uiCurr;

								dsStatus = dsDoDirNodeAuth( userNodeRef, pAuthType, true, pAuthBuff, pStepBuff, NULL );
								
                                                                if ( (kValidate_AuthMethod == validateType) && (eDSAuthFailed == dsStatus) )
                                                                        dsStatus = eDSAuthMethodNotSupported;
 
                                                                switch ( dsStatus )
								{
									case eDSNoErr:
										iResult = eDSNoErr;
										break;
			
									case eDSAuthNewPasswordRequired:
										iResult = eDSNoErr;
										break;
			
									case eDSAuthPasswordExpired:
										iResult = eDSNoErr;
										break;

									default:
										iResult = dsStatus;
										if (logErrors)
										    syslog( LOG_ERR, "cram_md5_auth: Authentication failed for user %s err=%d",inUserID,dsStatus); 
										break;
								}
								(void)dsDataNodeDeAllocate( dirRef, pAuthType );
								pAuthType = NULL;
							}
							else
							{
								if (logErrors) 
								    syslog( LOG_ERR, "cram_md5_auth: Authentication failed for user %s.  Unable to allocate memory.", inUserID );
								iResult = dsStatus;
							}
							(void)dsDataNodeDeAllocate( dirRef, pStepBuff );
							pStepBuff = NULL;
						}
						else
						{
							if (logErrors)
							    syslog( LOG_ERR, "cram_md5_auth: Authentication failed for user %s.  Unable to allocate memory.", inUserID );
							iResult = dsStatus;
						}
						(void)dsDataNodeDeAllocate( dirRef, pAuthBuff );
						pAuthBuff = NULL;
					}
					else
					{
						if (logErrors)
						    syslog( LOG_ERR, "cram_md5_auth: Authentication failed for user %s.  Unable to allocate memory.", inUserID );
						iResult = dsStatus;
					}
					(void)dsCloseDirNode( userNodeRef );
					userNodeRef = 0;
				}
				else
				{
				    if (logErrors)
					    syslog( LOG_ERR, "cram_md5_auth: Unable to open user directory node for user %s. (Open Directroy error: %d)", inUserID, dsStatus );
					iResult = dsStatus;
				}
			}
			else
			{
				if (logErrors)
				    syslog( LOG_ERR, "cram_md5_auth: Unable to find user %s. (Open Directroy error: %d)", inUserID, dsStatus );
				iResult = dsStatus;
			}
			(void)dsCloseDirNode( searchNodeRef );
			searchNodeRef = 0;
		}
		else
		{
			if (logErrors)
			    syslog( dsStatus, "cram_md5_auth: Unable to open directroy search node. (Open Directroy error: %d)", dsStatus );
			iResult = dsStatus;
		}
		(void)dsCloseDirService( dirRef );
		dirRef = 0;
	}
	else
	{
		if (logErrors)
		    syslog( LOG_ERR, "cram_md5_auth: Unable to open directroy. (Open Directroy error: %d)", dsStatus );
		iResult = dsStatus;
	}

	return( iResult );

} /* sValidateResponse */


int authenticate_cram_md5 ( const char *inUserID, const char *inChallenge, const char *inResponse  )
{
    return( sValidateResponse( inUserID, inChallenge, inResponse, kDSStdAuthCRAM_MD5, kValidate_User ) );
} 

int ds_supports_cram_md5 ( const char *inUserID )
{
    if (eDSAuthMethodNotSupported ==  sValidateResponse( inUserID, "", "", kDSStdAuthCRAM_MD5, kValidate_AuthMethod ) )
       return 0;

    return 1;
}
 
int ConvertBytesToHexChars( const void* inValue, unsigned long inValueLen, void *destPtr, unsigned long destLen, unsigned long *destCharCount)
{
    static const char* kHEXChars={ "0123456789abcdef" };
    unsigned char* theDataPtr = (unsigned char*) inValue;
    unsigned long copylen = inValueLen;    
    char *theString = (char *) destPtr;
    unsigned char temp;
    unsigned long count = 0;
    
    if (NULL == destPtr || destLen < 2)
    {	
    	return 0;
    }
      
    if ( (inValueLen * 2) > destLen)
        copylen = destLen / 2;
        
    for (count = 0; count < copylen; count++)
    {
        temp = *theDataPtr++;
        *theString++ = kHEXChars[temp >> 4];
        *theString++ = kHEXChars[temp & 0xF];
    }

    if (destCharCount != NULL)
        *destCharCount = copylen * 2;
        
    return copylen;
}
