/*
 * Copyright (c) 1999 Apple Computer, Inc. All rights reserved.
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
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/param.h>
#include <sys/socket.h>
#include <net/if.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <pwd.h>
#include <netdb.h>
#include <ctype.h>
#include <string.h>
#include <sys/dirent.h>

#include <DirectoryService/DirServices.h>
#include <DirectoryService/DirServicesConst.h>
#include <DirectoryService/DirServicesTypes.h>
#include <DirectoryService/DirServicesUtils.h>

// password server can store 511 characters + a terminator.
#define kMaxPassword		512

//-------------------------------------------------------------------------------------
//	ds_getpasswd
//-------------------------------------------------------------------------------------

void
ds_getpasswd(const char *loginUser, char *name, int isroot, int wasroot, int changePassOnSelf, char **old_clear, char **new_clear)
{
	int tries, len;
	char *p;
	static char obuf[kMaxPassword];
	static char nbuf[kMaxPassword];
	char prompt[MAXNAMLEN + 16];
	
	printf("Changing password for %s.\n", name);

	p = "";

	if (isroot == 0)
	{
		if ( changePassOnSelf )
		{
			strcpy( prompt, "Old password:" );
		}
		else
		{
			snprintf( prompt, sizeof(prompt), "password for %s:", loginUser );
		}
		
		p = getpass( prompt );
		snprintf( obuf, sizeof(obuf), "%s", p );
	}
	
	if ( wasroot == 0 )
	{
		tries = 0;
		nbuf[0] = '\0';
		for (;;)
		{
			p = getpass("New password:");
			if (!*p)
			{
				printf("Password unchanged.\n");
				exit(0);
			}
			
			tries++;
			len = strlen(p);
			
			snprintf( nbuf, sizeof(nbuf), "%s", p );
			if (!strcmp(nbuf, getpass("Retype new password:"))) break;
			
			printf("Mismatch; try again, EOF to quit.\n");
		}
	}
	
	*old_clear = obuf;
	*new_clear = nbuf;
}


//-------------------------------------------------------------------------------------
//	ds_passwd
//-------------------------------------------------------------------------------------

int
ds_passwd(char *uname, char *locn)
{
	tDirReference				dsRef				= 0;
    tDataBuffer				   *tDataBuff			= NULL;
    tDirNodeReference			nodeRef				= 0;
    long						status				= eDSNoErr;
    tContextData				context				= NULL;
	unsigned long				nodeCount			= 0;
	unsigned long				attrIndex			= 0;
	tDataList				   *nodeName			= NULL;
    tAttributeEntryPtr			pAttrEntry			= NULL;
	tDataList				   *pRecName			= NULL;
	tDataList				   *pRecType			= NULL;
	tDataList				   *pAttrType			= NULL;
	unsigned long				recCount			= 0;
	tRecordEntry		  	 	*pRecEntry			= NULL;
	tAttributeListRef			attrListRef			= 0;
	char					   *pUserLocation		= NULL;
	char					   *pUserName			= NULL;
	tAttributeValueListRef		valueRef			= 0;
	tAttributeValueEntry  	 	*pValueEntry		= NULL;
	tDataList				   *pUserNode			= NULL;
	tDirNodeReference			userNodeRef			= 0;
	tDataBuffer					*pStepBuff			= NULL;
	tDataNode				   *pAuthType			= NULL;
	unsigned long				uiCurr				= 0;
	unsigned long				uiLen				= 0;
	char 						*old_clear			= NULL;
	char						*new_clear			= NULL;
	int							isroot				= 0;
	int							wasroot				= 0;
	char						*loginUser			= NULL;
	int							changePassOnSelf	= 1;
	
	if (uname == NULL)
		return -1;
	
	loginUser = getlogin();
	if ( loginUser != NULL )
		changePassOnSelf = ( strcmp( loginUser, uname ) == 0 );
	
	status = dsOpenDirService( &dsRef );
	if (status != eDSNoErr)
		return status;
		
	do
    {
		if ( tDataBuff == NULL )
			tDataBuff = dsDataBufferAllocate( dsRef, 4096 );
		if (tDataBuff == NULL) break;
        
		if ( locn != NULL )
		{
			nodeName = dsBuildFromPath( dsRef, locn, "/" );
			if ( nodeName == NULL ) break;
			
			// find
			status = dsFindDirNodes( dsRef, tDataBuff, nodeName, eDSiExact, &nodeCount, &context );
        }
		else
		{
			// find on search node
			status = dsFindDirNodes( dsRef, tDataBuff, NULL, eDSSearchNodeName, &nodeCount, &context );
		}
		
		if (status != eDSNoErr) break;
		if ( nodeCount < 1 ) {
			status = eDSNodeNotFound;
			break;
		}
		
		if ( isroot == 1 )
		{
			// we already tried SetPasswordAsRoot and it didn't work
			// get the old (current) password and try again
			
			// getpass
			isroot = 0;
			wasroot = 1;
		}
		else
		if ( getuid() == 0 )
			isroot = 1;
		
		ds_getpasswd( loginUser, uname, isroot, wasroot, changePassOnSelf, &old_clear, &new_clear );
		
		status = dsGetDirNodeName( dsRef, tDataBuff, 1, &nodeName );
		if (status != eDSNoErr) continue;
		
		status = dsOpenDirNode( dsRef, nodeName, &nodeRef );
		dsDataListDeallocate( dsRef, nodeName );
		free( nodeName );
		nodeName = NULL;
		if (status != eDSNoErr) continue;
		
		pRecName = dsBuildListFromStrings( dsRef, uname, NULL );
		pRecType = dsBuildListFromStrings( dsRef, kDSStdRecordTypeUsers, NULL );
		pAttrType = dsBuildListFromStrings( dsRef, kDSNAttrMetaNodeLocation, kDSNAttrRecordName, NULL );

		recCount = 1;
		status = dsGetRecordList( nodeRef, tDataBuff, pRecName, eDSExact, pRecType,
													pAttrType, 0, &recCount, &context );
		if ( status != eDSNoErr ) break;
		if ( recCount == 0 ) {
			status = eDSAuthUnknownUser;
			break;
		}
		
		status = dsGetRecordEntry( nodeRef, tDataBuff, 1, &attrListRef, &pRecEntry );
		if ( status != eDSNoErr ) break;
		
		for ( attrIndex = 1; (attrIndex <= pRecEntry->fRecordAttributeCount) && (status == eDSNoErr); attrIndex++ )
		{
			status = dsGetAttributeEntry( nodeRef, tDataBuff, attrListRef, attrIndex, &valueRef, &pAttrEntry );
			if ( status == eDSNoErr && pAttrEntry != NULL )
			{
				if ( strcmp( pAttrEntry->fAttributeSignature.fBufferData, kDSNAttrMetaNodeLocation ) == 0 )
				{
					status = dsGetAttributeValue( nodeRef, tDataBuff, 1, valueRef, &pValueEntry );
					if ( status == eDSNoErr && pValueEntry != NULL )
					{
						pUserLocation = (char *) calloc( pValueEntry->fAttributeValueData.fBufferLength + 1, sizeof(char) );
						memcpy( pUserLocation, pValueEntry->fAttributeValueData.fBufferData, pValueEntry->fAttributeValueData.fBufferLength );
					}
				}
				else
				if ( strcmp( pAttrEntry->fAttributeSignature.fBufferData, kDSNAttrRecordName ) == 0 )
				{
					status = dsGetAttributeValue( nodeRef, tDataBuff, 1, valueRef, &pValueEntry );
					if ( status == eDSNoErr && pValueEntry != NULL )
					{
						pUserName = (char *) calloc( pValueEntry->fAttributeValueData.fBufferLength + 1, sizeof(char) );
						memcpy( pUserName, pValueEntry->fAttributeValueData.fBufferData, pValueEntry->fAttributeValueData.fBufferLength );
					}
				}
				
				if ( pValueEntry != NULL )
					dsDeallocAttributeValueEntry( dsRef, pValueEntry );
				pValueEntry = NULL;
				
				dsDeallocAttributeEntry( dsRef, pAttrEntry );
				pAttrEntry = NULL;
				dsCloseAttributeValueList( valueRef );
				valueRef = 0;
			}
		}
		
		pUserNode = dsBuildFromPath( dsRef, pUserLocation, "/" );
		status = dsOpenDirNode( dsRef, pUserNode, &userNodeRef );
		if ( status != eDSNoErr ) break;
		
		pStepBuff = dsDataBufferAllocate( dsRef, 128 );
		
		if ( isroot )
		{
			pAuthType = dsDataNodeAllocateString( dsRef, kDSStdAuthSetPasswdAsRoot );
			uiCurr = 0;
			
			// User name
			uiLen = strlen( pUserName );
			memcpy( &(tDataBuff->fBufferData[ uiCurr ]), &uiLen, sizeof( unsigned long ) );
			uiCurr += sizeof( unsigned long );
			memcpy( &(tDataBuff->fBufferData[ uiCurr ]), pUserName, uiLen );
			uiCurr += uiLen;
			
			// new pw
			uiLen = strlen( new_clear );
			memcpy( &(tDataBuff->fBufferData[ uiCurr ]), &uiLen, sizeof( unsigned long ) );
			uiCurr += sizeof( unsigned long );
			memcpy( &(tDataBuff->fBufferData[ uiCurr ]), new_clear, uiLen );
			uiCurr += uiLen;
			
			tDataBuff->fBufferLength = uiCurr;
			
			status = dsDoDirNodeAuth( userNodeRef, pAuthType, 1, tDataBuff, pStepBuff, NULL );
		}
		else
		if ( changePassOnSelf )
		{
			pAuthType = dsDataNodeAllocateString( dsRef, kDSStdAuthChangePasswd );
			uiCurr = 0;
			
			// User name
			uiLen = strlen( pUserName );
			memcpy( &(tDataBuff->fBufferData[ uiCurr ]), &uiLen, sizeof( unsigned long ) );
			uiCurr += sizeof( unsigned long );
			memcpy( &(tDataBuff->fBufferData[ uiCurr ]), pUserName, uiLen );
			uiCurr += uiLen;
			
			// old pw
			uiLen = strlen( old_clear );
			memcpy( &(tDataBuff->fBufferData[ uiCurr ]), &uiLen, sizeof( unsigned long ) );
			uiCurr += sizeof( unsigned long );
			memcpy( &(tDataBuff->fBufferData[ uiCurr ]), old_clear, uiLen );
			uiCurr += uiLen;
			
			// new pw
			uiLen = strlen( new_clear );
			memcpy( &(tDataBuff->fBufferData[ uiCurr ]), &uiLen, sizeof( unsigned long ) );
			uiCurr += sizeof( unsigned long );
			memcpy( &(tDataBuff->fBufferData[ uiCurr ]), new_clear, uiLen );
			uiCurr += uiLen;
			
			tDataBuff->fBufferLength = uiCurr;
			
			status = dsDoDirNodeAuth( userNodeRef, pAuthType, 1, tDataBuff, pStepBuff, NULL );
		}
		else
		{
			pAuthType = dsDataNodeAllocateString( dsRef, kDSStdAuthSetPasswd );
			uiCurr = 0;
			
			// User name
			uiLen = strlen( pUserName );
			memcpy( &(tDataBuff->fBufferData[ uiCurr ]), &uiLen, sizeof( unsigned long ) );
			uiCurr += sizeof( unsigned long );
			memcpy( &(tDataBuff->fBufferData[ uiCurr ]), pUserName, uiLen );
			uiCurr += uiLen;
			
			// new pw
			uiLen = strlen( new_clear );
			memcpy( &(tDataBuff->fBufferData[ uiCurr ]), &uiLen, sizeof( unsigned long ) );
			uiCurr += sizeof( unsigned long );
			memcpy( &(tDataBuff->fBufferData[ uiCurr ]), new_clear, uiLen );
			uiCurr += uiLen;
			
			// Authenticator name
			uiLen = strlen( loginUser );
			memcpy( &(tDataBuff->fBufferData[ uiCurr ]), &uiLen, sizeof( unsigned long ) );
			uiCurr += sizeof( unsigned long );
			memcpy( &(tDataBuff->fBufferData[ uiCurr ]), loginUser, uiLen );
			uiCurr += uiLen;
			
			// authenticator pw
			uiLen = strlen( old_clear );
			memcpy( &(tDataBuff->fBufferData[ uiCurr ]), &uiLen, sizeof( unsigned long ) );
			uiCurr += sizeof( unsigned long );
			memcpy( &(tDataBuff->fBufferData[ uiCurr ]), old_clear, uiLen );
			uiCurr += uiLen;
			
			tDataBuff->fBufferLength = uiCurr;
			
			status = dsDoDirNodeAuth( userNodeRef, pAuthType, 1, tDataBuff, pStepBuff, NULL );
		}
	}
	while ( isroot == 1 && status != eDSNoErr );
	
	/* old_clear and new_clear are statics (don't call free) */
	if (old_clear != NULL) {
		memset(old_clear, 0, strlen(old_clear));
	}
	if (new_clear != NULL) {
		memset(new_clear, 0, strlen(new_clear));
	}
    if (tDataBuff != NULL) {
		memset(tDataBuff, 0, tDataBuff->fBufferSize);
		dsDataBufferDeAllocate( dsRef, tDataBuff );
		tDataBuff = NULL;
	}
	
    if (pStepBuff != NULL) {
		dsDataBufferDeAllocate( dsRef, pStepBuff );
		pStepBuff = NULL;
	}
	if (pUserLocation != NULL ) {
		free(pUserLocation);
		pUserLocation = NULL;
	}
	if (pRecName != NULL) {
		dsDataListDeallocate( dsRef, pRecName );
		free( pRecName );
		pRecName = NULL;
	}
	if (pRecType != NULL) {
		dsDataListDeallocate( dsRef, pRecType );
		free( pRecType );
		pRecType = NULL;
	}
	if (pAttrType != NULL) {
		dsDataListDeallocate( dsRef, pAttrType );
		free( pAttrType );
		pAttrType = NULL;
	}
    if (nodeRef != 0) {
		dsCloseDirNode(nodeRef);
		nodeRef = 0;
	}
	if (dsRef != 0) {
		dsCloseDirService(dsRef);
		dsRef = 0;
	}

	if ( status != eDSNoErr ) {
		errno = EACCES;
		fprintf(stderr, "Sorry\n");
		exit(1);
	}
	
	return status;
}











