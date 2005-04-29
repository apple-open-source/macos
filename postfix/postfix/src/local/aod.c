/*
 * Copyright (c) 2004 Apple Computer, Inc. All rights reserved.
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


#include "aod.h"

#include <string.h>
#include <stdbool.h>
#include <stdlib.h>
#include <syslog.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/sysctl.h>

#include <CoreFoundation/CFData.h>
#include <CoreFoundation/CFString.h>
#include <CoreFoundation/CFNumber.h>
#include <CoreFoundation/CFPropertyList.h>

#include <DirectoryService/DirServices.h>
#include <DirectoryService/DirServicesUtils.h>
#include <DirectoryService/DirServicesConst.h>

/* -----------------------------------------------------------------
	Prototypes 
   ----------------------------------------------------------------- */

static tDirStatus	sOpen_ds			( tDirReference *inOutDirRef );
static tDirStatus	sGet_search_node	( tDirReference inDirRef, tDirNodeReference *outSearchNodeRef );
static tDirStatus	sGet_user_attributes( tDirReference inDirRef, tDirNodeReference inSearchNodeRef, const char *inUserID, struct od_user_opts *inOutOpts );
static int			sVerify_version		( CFDictionaryRef inCFDictRef );
static void			sGet_mail_values	( char *inMailAttribute, struct od_user_opts *inOutOpts );
static void			sGet_acct_state		( CFDictionaryRef inCFDictRef, struct od_user_opts *inOutOpts );
static void			sGet_auto_forward	( CFDictionaryRef inCFDictRef, struct od_user_opts *inOutOpts );

/* -----------------------------------------------------------------
	Globals
   ----------------------------------------------------------------- */

char	gErrStr[ kONE_K_BUF ];

/* -----------------------------------------------------------------
	aodGetUserOptions ()
   ----------------------------------------------------------------- */

int aodGetUserOptions ( const char *inUserID, struct od_user_opts *inOutOpts )
{
	tDirStatus			dsStatus		= eDSNoErr;
	tDirReference		dirRef			= 0;
	tDirNodeReference	searchNodeRef	= 0;

	if ( (inUserID == NULL) || (inOutOpts == NULL) )
	{
		return( -1 );
	}

	memset( inOutOpts, 0, sizeof( struct od_user_opts ) );

	inOutOpts->fAcctState = eUnknownAcctState;
	inOutOpts->fIMAPLogin = eAcctDisabled;
	inOutOpts->fPOP3Login = eAcctDisabled;

	dsStatus = sOpen_ds( &dirRef );
	if ( dsStatus == eDSNoErr )
	{
		dsStatus = sGet_search_node( dirRef, &searchNodeRef );
		if ( dsStatus == eDSNoErr )
		{
			dsStatus = sGet_user_attributes( dirRef, searchNodeRef, inUserID, inOutOpts );
			(void)dsCloseDirNode( searchNodeRef );
		}
		(void)dsCloseDirService( dirRef );
	}

	return( dsStatus );

} /* aodGetUserOptions */


/* -----------------------------------------------------------------
   -----------------------------------------------------------------
   -----------------------------------------------------------------
	Static functions
   -----------------------------------------------------------------
   -----------------------------------------------------------------
   ----------------------------------------------------------------- */

/* -----------------------------------------------------------------
	sOpen_ds ()
   ----------------------------------------------------------------- */

tDirStatus sOpen_ds ( tDirReference *inOutDirRef )
{
	tDirStatus		dsStatus	= eDSNoErr;

	dsStatus = dsOpenDirService( inOutDirRef );

	return( dsStatus );

} /* sOpen_ds */


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
	sGet_user_attributes ()
   ----------------------------------------------------------------- */

tDirStatus sGet_user_attributes ( tDirReference inDirRef,
									tDirNodeReference inSearchNodeRef,
									const char *inUserID,
									struct od_user_opts *inOutOpts )
{
	char				   *p				= NULL;
	tDirStatus				dsStatus		= eMemoryAllocError;
	int						done			= FALSE;
	int						i				= 0;
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
				pUserAttrType = dsBuildListFromStrings( inDirRef, kDS1AttrMailAttribute, kDSNAttrRecordName, NULL );
				if ( pUserAttrType != NULL );
				{
					do {
						/* Get the user record(s) that matches the user id */
						dsStatus = dsGetRecordList( inSearchNodeRef, pTDataBuff, &tdlRecName, eDSiExact, pUserRecType,
													pUserAttrType, FALSE, &uiRecCount, &pContext );

						if ( dsStatus == eDSNoErr )
						{
							dsStatus = eDSInvalidName;
							/* do we have more than 1 match */
							if ( uiRecCount == 1 ) 
							{
								dsStatus = dsGetRecordEntry( inSearchNodeRef, pTDataBuff, 1, &attrListRef, &pRecEntry );
								if ( dsStatus == eDSNoErr )
								{
									/* Get the record name */
									(void)dsGetRecordNameFromEntry( pRecEntry, &pAcctName );

									if ( pAcctName != NULL )
									{
										if ( strlen( pAcctName ) < kONE_K_BUF )
										{
											strcpy( inOutOpts->fUserID, pAcctName );
										}
									}
									/* Get the attributes we care about for the record */
									for ( i = 1; i <= pRecEntry->fRecordAttributeCount; i++ )
									{
										dsStatus = dsGetAttributeEntry( inSearchNodeRef, pTDataBuff, attrListRef, i, &valueRef, &pAttrEntry );
										if ( (dsStatus == eDSNoErr) && (pAttrEntry != NULL) )
										{
											if ( strcasecmp( pAttrEntry->fAttributeSignature.fBufferData, kDS1AttrMailAttribute ) == 0 )
											{
												/* Only get the first attribute value */
												dsStatus = dsGetAttributeValue( inSearchNodeRef, pTDataBuff, 1, valueRef, &pValueEntry );
												if ( dsStatus == eDSNoErr )
												{
													/* Get the individual mail attribute values */
													sGet_mail_values( (char *)pValueEntry->fAttributeValueData.fBufferData, inOutOpts );

													/* If we don't find duplicate users in the same node, we take the first one with
														a valid mail attribute */
													done = true;
												}
											}
											else if ( strcasecmp( pAttrEntry->fAttributeSignature.fBufferData, kDSNAttrRecordName ) == 0 )
											{
												/* Only get the first attribute value */
												dsStatus = dsGetAttributeValue( inSearchNodeRef, pTDataBuff, 1, valueRef, &pValueEntry );
												if ( dsStatus == eDSNoErr )
												{
													/* Get the generated uid */
													if ( pValueEntry->fAttributeValueData.fBufferLength < kONE_K_BUF )
													{
														strncpy( inOutOpts->fRecName, pValueEntry->fAttributeValueData.fBufferData, pValueEntry->fAttributeValueData.fBufferLength );
													}
													else
													{
														strncpy( inOutOpts->fRecName, pValueEntry->fAttributeValueData.fBufferData, kONE_K_BUF - 1 );
													}
												}
											}

											if ( pValueEntry != NULL )
											{
												(void)dsDeallocAttributeValueEntry( inSearchNodeRef, pValueEntry );
												pValueEntry = NULL;
											}
										}
										if ( pAttrEntry != NULL )
										{
											(void)dsCloseAttributeValueList( valueRef );
											(void)dsDeallocAttributeEntry( inSearchNodeRef, pAttrEntry );
											pAttrEntry = NULL;
										}
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
									syslog( LOG_NOTICE, "Duplicate users %s found in directory.", inUserID );
								}
								inOutOpts->fUserID[ 0 ] = '\0';
								dsStatus = eDSUserUnknown;
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
	
	return( dsStatus );

} /* sGet_user_attributes */


/* -----------------------------------------------------------------
	sGet_mail_values ()
   ----------------------------------------------------------------- */

void sGet_mail_values ( char *inMailAttribute, struct od_user_opts *inOutOpts )
{
	int					iResult 	= 0;
	unsigned long		uiDataLen	= 0;
	CFDataRef			cfDataRef	= NULL;
	CFPropertyListRef	cfPlistRef	= NULL;
	CFDictionaryRef		cfDictRef	= NULL;

	if ( inMailAttribute != NULL )
	{
		uiDataLen = strlen( inMailAttribute );
		cfDataRef = CFDataCreate( NULL, (const UInt8 *)inMailAttribute, uiDataLen );
		if ( cfDataRef != NULL )
		{
			cfPlistRef = CFPropertyListCreateFromXMLData( kCFAllocatorDefault, cfDataRef, kCFPropertyListImmutable, NULL );
			if ( cfPlistRef != NULL )
			{
				if ( CFDictionaryGetTypeID() == CFGetTypeID( cfPlistRef ) )
				{
					cfDictRef = (CFDictionaryRef)cfPlistRef;
					iResult = sVerify_version( cfDictRef );
					if ( iResult == eNoErr )
					{
						sGet_acct_state( cfDictRef, inOutOpts );
					}
				}
				CFRelease( cfPlistRef );
			}
			CFRelease( cfDataRef );
		}
	}
} /* sGet_mail_values */


/* -----------------------------------------------------------------
	sVerify_version ()
   ----------------------------------------------------------------- */

int sVerify_version ( CFDictionaryRef inCFDictRef )
{
	int				iResult 	= 0;
	bool			bFound		= FALSE;
	CFStringRef		cfStringRef	= NULL;
	char		   *pValue		= NULL;

	bFound = CFDictionaryContainsKey( inCFDictRef, CFSTR( kXMLKeyAttrVersion ) );
	if ( bFound == true )
	{
		iResult = eInvalidDataType;

		cfStringRef = (CFStringRef)CFDictionaryGetValue( inCFDictRef, CFSTR( kXMLKeyAttrVersion ) );
		if ( cfStringRef != NULL )
		{
			if ( CFGetTypeID( cfStringRef ) == CFStringGetTypeID() )
			{
				iResult = eItemNotFound;

				pValue = (char *)CFStringGetCStringPtr( cfStringRef, kCFStringEncodingMacRoman );
				if ( pValue != NULL )
				{
					iResult = eWrongVersion;

					if ( strcasecmp( pValue, kXMLValueVersion ) == 0 )
					{
						iResult = eNoErr;
					}
				}
			}
		}
	}

	return( iResult );

} /* sVerify_version */


/* -----------------------------------------------------------------
	sGet_acct_state ()
   ----------------------------------------------------------------- */

void sGet_acct_state ( CFDictionaryRef inCFDictRef, struct od_user_opts *inOutOpts )
{
	bool			bFound		= FALSE;
	CFStringRef		cfStringRef	= NULL;
	char		   *pValue		= NULL;

	/* Default value */
	inOutOpts->fAcctState = eUnknownAcctState;

	bFound = CFDictionaryContainsKey( inCFDictRef, CFSTR( kXMLKeyAcctState ) );
	if ( bFound == true )
	{
		cfStringRef = (CFStringRef)CFDictionaryGetValue( inCFDictRef, CFSTR( kXMLKeyAcctState ) );
		if ( cfStringRef != NULL )
		{
			if ( CFGetTypeID( cfStringRef ) == CFStringGetTypeID() )
			{
				pValue = (char *)CFStringGetCStringPtr( cfStringRef, kCFStringEncodingMacRoman );
				if ( pValue != NULL )
				{
					if ( strcasecmp( pValue, kXMLValueAcctEnabled ) == 0 )
					{
						inOutOpts->fAcctState = eAcctEnabled;
					}
					else if ( strcasecmp( pValue, kXMLValueAcctDisabled ) == 0 )
					{
						inOutOpts->fAcctState = eAcctDisabled;
					}
					else if ( strcasecmp( pValue, kXMLValueAcctFwd ) == 0 )
					{
						sGet_auto_forward( inCFDictRef, inOutOpts );
					}
				}
			}
		}
	}
} /* sGet_acct_state */


/* -----------------------------------------------------------------
	sGet_auto_forward ()
   ----------------------------------------------------------------- */

void sGet_auto_forward ( CFDictionaryRef inCFDictRef, struct od_user_opts *inOutOpts )
{
	bool			bFound		= FALSE;
	CFStringRef		cfStringRef	= NULL;
	char		   *pValue		= NULL;

	bFound = CFDictionaryContainsKey( inCFDictRef, CFSTR( kXMLKeyAutoFwd ) );
	if ( bFound == true )
	{
		cfStringRef = (CFStringRef)CFDictionaryGetValue( inCFDictRef, CFSTR( kXMLKeyAutoFwd ) );
		if ( cfStringRef != NULL )
		{
			if ( CFGetTypeID( cfStringRef ) == CFStringGetTypeID() )
			{
				pValue = (char *)CFStringGetCStringPtr( cfStringRef, kCFStringEncodingMacRoman );
				if ( pValue != NULL )
				{
					if ( strlen( pValue ) < kONE_K_BUF )
					{
						inOutOpts->fAcctState = eAcctForwarded;
						inOutOpts->fPOP3Login = eAcctDisabled;
						inOutOpts->fIMAPLogin = eAcctDisabled;

						strcpy( inOutOpts->fAutoFwdAddr, pValue );
					}
				}
			}
		}
	}
} /* sGet_auto_forward */
