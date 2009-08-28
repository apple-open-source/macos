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
#include <OpenDirectory/OpenDirectory.h>
#include <DirectoryService/DirServices.h>
#include <DirectoryService/DirServicesUtils.h>
#include <DirectoryService/DirServicesConst.h>
#include <DirectoryService/DirServicesConstPriv.h>

#include <membership.h>
#include <membershipPriv.h>

#include "dserr.h"

enum { kLogErrors_disabled = 0, kLogErrors_enabled = 1 };
enum { kValidate_AuthMethod = 0, kValidate_User = 1 };
	
static tDirReference		dirRef		= 0;
static tDirNodeReference	searchNodeRef   = 0;
static char			userExists[1024] = "";

/* forward declarations for internal functions */
#if 0
tDirStatus _od_auth_open_user_node(tDirReference inDirRef, const char *inUserLoc, 
                                   tDirNodeReference *outUserNodeRef);
tDirStatus _od_auth_get_search_node(tDirReference inDirRef,
							        tDirNodeReference *outSearchNodeRef)
tDirStatus _od_auth_lookup_user(tDirReference inDirRef,
						        tDirNodeReference inSearchNodeRef,
						        const char *inUserID,
						        char **outUserLocation);
int _od_auth_validate_response(const char *inUserID, const char *inChallenge, 
                               const char *inResponse, const char *inAuthType, 
							   int validateType);
int _od_auth_bytes_to_hex_chars(const void* inValue,  UInt32 inValueLen, 
                               void *destPtr, UInt32 destLen, 
							   UInt32 *destCharCount);

#endif

int _od_auth_check_user_exists(const char *inUserID);
static int od_auth_configure_directory_references(void);

/* -----------------------------------------------------------------
   PUBLIC FUNCTIONS
   ----------------------------------------------------------------- */

/* -----------------------------------------------------------------
    int od_auth_check_user_exists( const char* userName );
    
	Verify existance of user in Directory Service database
	
	ARGS:
		username (IN) username as a UTF8 string	

	RETURNS:
		0 = failed
		1 = user exists

   ----------------------------------------------------------------- */
int od_auth_check_user_exists(const char* userName)
{
    return( _od_auth_check_user_exists( userName ) );
     
     return kFailed;
}

/* -----------------------------------------------------------------
    int od_auth_check_plain_password()
    
	ARGS:
		username (IN) username as a UTF8 string	
		password (IN) password as a UTF8 string

	RETURNS: int
		0 = success
		1 = failed

   ----------------------------------------------------------------- */
int od_auth_check_plain_password(const char* userName, const char* password)
{
    if (CHECKPW_SUCCESS == checkpw(userName, password))
        return kAuthenticated;
     
     return kFailed;
}

/* -----------------------------------------------------------------
	int od_auth_supports_cram_md5()
	
	Checks whether CRAM-MD5 is available for the user-id.
	
	RETURNS:
		0 = not available
		1 = available
   ----------------------------------------------------------------- */
int od_auth_supports_cram_md5(const char *inUserID)
{
	int iResult = _od_auth_validate_response( inUserID, "", "", 
	                                          kDSStdAuthCRAM_MD5, 
											  kValidate_AuthMethod);
    if (eDSAuthMethodNotSupported == iResult)
       return 0;

    return 1;
}
 
/* -----------------------------------------------------------------
	int od_auth_create_crammd5_challenge()
	
	Validate CRAM-MD5 Response based on user-id and challenge.
	
	RETURNS:
		0 = failed
		1 = success
   ----------------------------------------------------------------- */
int od_auth_create_crammd5_challenge(char *outChallenge, int destsize)
{
    int numchars = 0;
    char randombytes[ 32 ];
    UInt32 destChars = 0;
		
    if (NULL != outChallenge)
        outChallenge[0] = 0;

    if (destsize < 5) // just in case it changes type or becomes too small
        return 0;
    
    if (0 == RAND_bytes(randombytes, sizeof(randombytes) ) )
        return 0;
    
    // test routine for ConvertBytesToHexChars
    // numchars = testChallenge(m, &destChars);

    numchars = _od_auth_bytes_to_hex_chars( randombytes,  sizeof(randombytes), outChallenge, (UInt32) destsize - 1, &destChars);
    if (numchars != sizeof(randombytes) || destChars >= destsize) // failed to convert all the bytes or converted too many bytes
        numchars = destChars = 0; 
        
    outChallenge[destChars] = 0; // c string terminate
    return numchars;
}

/* -----------------------------------------------------------------
	int od_auth_check_crammd5_response()
	
	Validate CRAM-MD5 Response based on user-id and challenge.
	
	RETURNS:
		0 = failed
		1 = authenticated
   ----------------------------------------------------------------- */
int od_auth_check_crammd5_response(const char *inUserID, const char *inChallenge, 
                                   const char *inResponse)
{
	int iResult =  _od_auth_validate_response( inUserID, inChallenge, inResponse, 
									           kDSStdAuthCRAM_MD5, 
											   kValidate_User );
    return( iResult );
} 

/* -----------------------------------------------------------------
	int _od_auth_bytes_to_hex_chars()
	
	Converts the input byte array to an ASCII string using  
	hexadecimal character encoding.
	
	RETURNS:
		0 = conversion failed
		n = (success) number of bytes copied from source array
   ----------------------------------------------------------------- */
int _od_auth_bytes_to_hex_chars( const void* inValue, UInt32 inValueLen, 
                                void *destPtr, UInt32 destLen, 
								UInt32 *destCharCount)
{
    static const char* kHEXChars={ "0123456789abcdef" };
    unsigned char* theDataPtr = (unsigned char*) inValue;
    UInt32 copylen = inValueLen;    
    char *theString = (char *) destPtr;
    unsigned char temp;
    UInt32 count = 0;
    
    if (NULL == destPtr || destLen < 2) {	
    	return 0;
    }
      
    if ( (inValueLen * 2) > destLen)
        copylen = destLen / 2;
        
    for (count = 0; count < copylen; count++) {
        temp = *theDataPtr++;
        *theString++ = kHEXChars[temp >> 4];
        *theString++ = kHEXChars[temp & 0xF];
    }

    if (destCharCount != NULL)
        *destCharCount = copylen * 2;
        
    return copylen;
}


/* -----------------------------------------------------------------
   Private Functions
   ----------------------------------------------------------------- */


/* -----------------------------------------------------------------
	_od_auth_open_user_node ()
   ----------------------------------------------------------------- */

tDirStatus _od_auth_open_user_node (  tDirReference inDirRef, 
                                      const char *inUserLoc, 
									  tDirNodeReference *outUserNodeRef )
{
	tDirStatus		dsStatus	= eMemoryAllocError;
	tDataList	   *pUserNode	= NULL;

	pUserNode = dsBuildFromPath( inDirRef, inUserLoc, "/" );
	if ( pUserNode != NULL ) {

		dsStatus = dsOpenDirNode( inDirRef, pUserNode, outUserNodeRef );

		(void) dsDataListDeallocate( inDirRef, pUserNode );
		free( pUserNode );
		pUserNode = NULL;

	}

	return( dsStatus );

} /* _od_auth_open_user_node */



/* -----------------------------------------------------------------
	_od_auth_get_search_node ()
   ----------------------------------------------------------------- */

tDirStatus _od_auth_get_search_node ( tDirReference inDirRef,
							 tDirNodeReference *outSearchNodeRef )
{
	tDirStatus		dsStatus	= eMemoryAllocError;
	UInt32 uiCount		= 0;
	tDataBuffer	   *pTDataBuff	= NULL;
	tDataList	   *pDataList	= NULL;

	pTDataBuff = dsDataBufferAllocate( inDirRef, 8192 );
	if ( pTDataBuff != NULL )
	{
		dsStatus = dsFindDirNodes( inDirRef, pTDataBuff, NULL, 
		                           eDSSearchNodeName, &uiCount, NULL );
		if ( dsStatus == eDSNoErr ) {
			dsStatus = eDSNodeNotFound;
			if ( uiCount == 1 ) {
				dsStatus = dsGetDirNodeName( inDirRef, pTDataBuff, 1, &pDataList );
				if ( dsStatus == eDSNoErr ) {
					dsStatus = dsOpenDirNode( inDirRef, pDataList, outSearchNodeRef );
				}

				if ( pDataList != NULL ) {
					(void)dsDataListDeallocate( inDirRef, pDataList );

					free( pDataList );
					pDataList = NULL;
				}
			}
		}

		(void) dsDataBufferDeAllocate( inDirRef, pTDataBuff );
		pTDataBuff = NULL;
	}

	return( dsStatus );

} /* _od_auth_get_search_node */


/* -----------------------------------------------------------------
	_od_auth_lookup_user ()
   ----------------------------------------------------------------- */

tDirStatus _od_auth_lookup_user ( tDirReference inDirRef,
						  tDirNodeReference inSearchNodeRef,
						  const char *inUserID,
						  char **outUserLocation )
{
	tDirStatus				dsStatus		= eMemoryAllocError;
	int						done			= FALSE;
	char				   *pAcctName		= NULL;
	UInt32					uiRecCount		= 0;
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
			if ( pUserRecType != NULL ) {
				pUserAttrType = dsBuildListFromStrings( inDirRef, kDSNAttrMetaNodeLocation, NULL );
				if ( pUserAttrType != NULL ) {
					do {
						/* Get the user record(s) that matches the name */
						dsStatus = dsGetRecordList( inSearchNodeRef, pTDataBuff, &tdlRecName, eDSiExact, pUserRecType,
													pUserAttrType, FALSE, &uiRecCount, &pContext );

						if ( dsStatus == eDSNoErr ) {
							dsStatus = eDSInvalidName;
							if ( uiRecCount == 1 ) {
								dsStatus = dsGetRecordEntry( inSearchNodeRef, pTDataBuff, 1, &attrListRef, &pRecEntry );
								if ( dsStatus == eDSNoErr ) {
									/* Get the record name */
									(void)dsGetRecordNameFromEntry( pRecEntry, &pAcctName );
			
									dsStatus = dsGetAttributeEntry( inSearchNodeRef, pTDataBuff, attrListRef, 1, &valueRef, &pAttrEntry );
									if ( (dsStatus == eDSNoErr) && (pAttrEntry != NULL) ) {
										dsStatus = dsGetAttributeValue( inSearchNodeRef, pTDataBuff, 1, valueRef, &pValueEntry );
										if ( (dsStatus == eDSNoErr) && (pValueEntry != NULL) ) {
											if ( strcmp( pAttrEntry->fAttributeSignature.fBufferData, kDSNAttrMetaNodeLocation ) == 0 ) {
												/* Get the user location */
												*outUserLocation = (char *)calloc( pValueEntry->fAttributeValueData.fBufferLength + 1, sizeof( char ) );
												memcpy( *outUserLocation, pValueEntry->fAttributeValueData.fBufferData, pValueEntry->fAttributeValueData.fBufferLength );

												/* If we don't find duplicate users in the same node, we take the first one with
													a valid mail attribute */
												done = TRUE;
											}
											(void) dsCloseAttributeValueList( valueRef );
										}

										(void)dsDeallocAttributeEntry( inSearchNodeRef, pAttrEntry );
										pAttrEntry = NULL;

										(void)dsCloseAttributeList( attrListRef );
									}

									if ( pRecEntry != NULL ) {
										(void) dsDeallocRecordEntry( inSearchNodeRef, pRecEntry );
										pRecEntry = NULL;
									}
								}
							}
							else {
								done = true;
								if ( uiRecCount > 1 ) {
									syslog( LOG_ERR, "cram_md5_auth: Duplicate users %s found in directory.", inUserID );
								}
								dsStatus = eDSAuthInvalidUserName;
							}
						}
					} while ( (pContext != NULL) && (dsStatus == eDSNoErr) && (!done) );

					if ( pContext != NULL ) {
						(void) dsReleaseContinueData( inSearchNodeRef, pContext );
						pContext = NULL;
					}

					(void)dsDataListDeallocate( inDirRef, pUserAttrType );
					if (pUserAttrType != NULL) {
						free(pUserAttrType);
						pUserAttrType = NULL;
					}
				}

				(void)dsDataListDeallocate( inDirRef, pUserRecType );
				if (pUserRecType != NULL) {
					free(pUserRecType);
					pUserRecType = NULL;
				}
			}

			(void)dsDataListDeallocate( inDirRef, &tdlRecName );
			if (tdlRecName.fDataListHead != NULL) {
				free(tdlRecName.fDataListHead);
				tdlRecName.fDataListHead = NULL;
			}
		}

		(void)dsDataBufferDeAllocate( inDirRef, pTDataBuff );
		pTDataBuff = NULL;
	}

	if ( pAcctName != NULL ) {
		free( pAcctName );
		pAcctName = NULL;
	}

	if ( pValueEntry != NULL ) {
		free( pValueEntry );
		pValueEntry = NULL;
	}

	return( dsStatus );

} /* _od_auth_lookup_user */


int
od_auth_configure_directory_references()
{
	tDirStatus              dsStatus		= eDSNoErr;
	
	if (dirRef != 0 || searchNodeRef != 0) {
		if (dsVerifyDirRefNum(dirRef) != eDSNoErr) {
			dsCloseDirNode(searchNodeRef);
			searchNodeRef = 0;

			dsCloseDirService(dirRef);
			dirRef = 0;
		}
	}

	if (dirRef == 0) {
		dsStatus = dsOpenDirService( &dirRef );
		if (! IS_EXPECTED_DS_ERROR(dsStatus)) {
			syslog( LOG_ERR, "od_auth: Unable to open directory. (Open Directory error: %d)", dsStatus );
			goto done;
		}
	}

	if (searchNodeRef == 0) {
		dsStatus = _od_auth_get_search_node( dirRef, &searchNodeRef );
		if (! IS_EXPECTED_DS_ERROR(dsStatus)) {
			syslog( LOG_ERR, "od_auth: Unable to open directory search node. (Open Directory error: %d)", dsStatus );
			goto done;
		}
	}

done:
	return dsStatus;
}

/* -----------------------------------------------------------------
	_od_auth_validate_response ()
   ----------------------------------------------------------------- */

int _od_auth_validate_response ( const char *inUserID, const char *inChallenge, 
                                 const char *inResponse, const char *inAuthType, 
								 int validateType )
{
	int		        iResult			= -1;
	tDirStatus              dsStatus		= eDSNoErr;
	tDirNodeReference	userNodeRef		= 0;
	tDataBuffer		*pAuthBuff		= NULL;
	tDataBuffer		*pStepBuff		= NULL;
	tDataNode		*pAuthType		= NULL;
	char			*userLoc		= NULL;
	UInt32				uiNameLen		= 0;
	UInt32				uiChalLen		= 0;
	UInt32				uiRespLen		= 0;
	UInt32				uiBuffSzie		= 0;
	UInt32				uiCurr			= 0;
	UInt32				uiLen			= 0;
	int                     logErrors               = kLogErrors_disabled;
	
	if (kValidate_User == validateType) 
           logErrors = kLogErrors_enabled;

	if ( (inUserID == NULL) || (inChallenge == NULL) || (inResponse == NULL) || (inAuthType == NULL) ) {
		return( -1 );
	}

	uiNameLen = strlen( inUserID );
	uiChalLen = strlen( inChallenge );
	uiRespLen = strlen( inResponse );

	uiBuffSzie = uiNameLen + uiChalLen + uiRespLen + 32;

	if (od_auth_configure_directory_references() != eDSNoErr) {
		iResult = eDSInvalidReference;
		goto done;
	}

	dsStatus = _od_auth_lookup_user( dirRef, searchNodeRef, inUserID, &userLoc );
	if ( dsStatus == eDSNoErr ) {
		dsStatus = _od_auth_open_user_node( dirRef, userLoc, &userNodeRef );
		if ( dsStatus == eDSNoErr ) {
			pAuthBuff = dsDataBufferAllocate( dirRef, uiBuffSzie );
			if ( pAuthBuff != NULL ) {
				pStepBuff = dsDataBufferAllocate( dirRef, 256 );
				if ( pStepBuff != NULL ) {
					pAuthType = dsDataNodeAllocateString( dirRef, inAuthType );
					if ( pAuthType != NULL ) {
						/* User name */
						uiLen = uiNameLen;
						memcpy( &(pAuthBuff->fBufferData[ uiCurr ]), &uiLen, sizeof( UInt32 ) );
						uiCurr += sizeof( UInt32 );
						memcpy( &(pAuthBuff->fBufferData[ uiCurr ]), inUserID, uiLen );
						uiCurr += uiLen;

						if (kValidate_User == validateType) {
							/* Challenge */
							uiLen = uiChalLen;
							memcpy( &(pAuthBuff->fBufferData[ uiCurr ]), &uiLen, sizeof( UInt32 ) );
							uiCurr += sizeof( UInt32 );
							memcpy( &(pAuthBuff->fBufferData[ uiCurr ]), inChallenge, uiLen );
							uiCurr += uiLen;
	
							/* Response */
							uiLen = uiRespLen;
							memcpy( &(pAuthBuff->fBufferData[ uiCurr ]), &uiLen, sizeof( UInt32 ) );
							uiCurr += sizeof( UInt32 );
							memcpy( &(pAuthBuff->fBufferData[ uiCurr ]), inResponse, uiLen );
							uiCurr += uiLen;
						}
						
						pAuthBuff->fBufferLength = uiCurr;

						dsStatus = dsDoDirNodeAuth( userNodeRef, pAuthType, true, pAuthBuff, pStepBuff, NULL );
						
                                                                if ( (kValidate_AuthMethod == validateType) && (eDSAuthFailed == dsStatus) )
                                                                        dsStatus = eDSAuthMethodNotSupported;
 
						switch ( dsStatus )	{
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
					else {
						if (logErrors) 
						    syslog( LOG_ERR, "cram_md5_auth: Authentication failed for user %s.  Unable to allocate memory.", inUserID );
						iResult = dsStatus;
					}

					(void) dsDataNodeDeAllocate( dirRef, pStepBuff );
					pStepBuff = NULL;
				}
				else {
					if (logErrors)
					    syslog( LOG_ERR, "cram_md5_auth: Authentication failed for user %s.  Unable to allocate memory.", inUserID );
					iResult = dsStatus;
				}
				(void)dsDataNodeDeAllocate( dirRef, pAuthBuff );
				pAuthBuff = NULL;
			}
			else {
				if (logErrors)
				    syslog( LOG_ERR, "cram_md5_auth: Authentication failed for user %s.  Unable to allocate memory.", inUserID );
				iResult = dsStatus;
			}
			(void)dsCloseDirNode( userNodeRef );
			userNodeRef = 0;
		}
		else {
		    if (logErrors)
			    syslog( LOG_ERR, "cram_md5_auth: Unable to open user directory node for user %s. (Open Directory error: %d)", inUserID, dsStatus );
			iResult = dsStatus;
		}
	}
	else {
		if (logErrors)
		    syslog( LOG_ERR, "cram_md5_auth: Unable to find user %s. (Open Directory error: %d)", inUserID, dsStatus );
		iResult = dsStatus;
	}


done:
	if (userLoc != NULL) {
		free(userLoc);
		userLoc = NULL;
	}
	return( iResult );

} /* _od_auth_validate_response */

/* -----------------------------------------------------------------
	_od_auth_check_user_exists ()
   ----------------------------------------------------------------- */

int _od_auth_check_user_exists(const char *inUserID)
{
	int		        iResult			= 0;
	tDirStatus              dsStatus		= eDSNoErr;
	tDirNodeReference	userNodeRef		= 0;
	char			*userLoc		= NULL;
	UInt32				uiNameLen		= 0;
	UInt32				uiBuffSzie		= 0;
	int                     logErrors               = kLogErrors_disabled;
	
	if ( (inUserID == NULL) ) {
		return( -1 );
	}

	/* This is a hack to work around the fact that we're calling into
	 * DS/OD multiple times for the same user instead of making a single
	 * call and retaining the results.  Doing this hack is easy, doing the
	 * correct fix is difficult.  See <rdar://problem/6786738> "extra work
	 * is being performed during DIGEST-MD5 auth" for the details.
	 * All this code does is ask: "If the last time this function was
	 * called we found that this user exists, then assume the user
	 * still exists" -- that should be a safe assumption. */
	if (*userExists != (char)0 && strcmp(userExists, inUserID) == 0) {
		return 1; // user exists
	}
   
	uiNameLen = strlen( inUserID );

	uiBuffSzie = uiNameLen + 32;

	if (od_auth_configure_directory_references() != eDSNoErr) {
		iResult = eDSInvalidReference;
		goto done;
	}

	dsStatus = _od_auth_lookup_user( dirRef, searchNodeRef, inUserID, &userLoc );
	if ( dsStatus == eDSNoErr )	{
		dsStatus = _od_auth_open_user_node( dirRef, userLoc, &userNodeRef );
		if ( dsStatus == eDSNoErr ) {
			iResult = 1; // user exists

			(void)dsCloseDirNode( userNodeRef );
			userNodeRef = 0;
		}
		else {
		    if (logErrors)
			    syslog( LOG_ERR, "_od_auth_check_user_exists: Unable to open user directory node for user %s. (Open Directory error: %d)", inUserID, dsStatus );
			iResult = dsStatus;
		}
	}
	else {
		if (logErrors)
		    syslog( LOG_ERR, "_od_auth_check_user_exists: Unable to find user %s. (Open Directory error: %d)", inUserID, dsStatus );
		iResult = dsStatus;
	}

done:

	if (userLoc != NULL) {
		free(userLoc);
		userLoc = NULL;
	}

	if (iResult == 1) {
		/* user exists, so cache that result */
		if (strlcpy(userExists, inUserID, sizeof(userExists)) >= sizeof(userExists)) {
			/* something went wrong, user id specified was too long, don't cache it */
			*userExists = (char)0;
		}
	}

	return( iResult );

} /* _od_auth_check_user_exists */
