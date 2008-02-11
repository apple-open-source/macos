/*
 * Copyright (c) 1999 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * "Portions Copyright (c) 1999 Apple Computer, Inc.  All Rights
 * Reserved.  This file contains Original Code and/or Modifications of
 * Original Code as defined in and that are subject to the Apple Public
 * Source License Version 1.0 (the 'License').  You may not use this file
 * except in compliance with the License.  Please obtain a copy of the
 * License at http://www.apple.com/publicsource and read it before using
 * this file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License."
 * 
 * @APPLE_LICENSE_HEADER_END@
 */
/* 
 * Copyright (c) 1998 by Apple Computer, Inc.
 * Portions Copyright (c) 1988 by Sun Microsystems, Inc.
 * Portions Copyright (c) 1988 The Regents of the University of California.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */
 
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <pwd.h>
#include <netinet/in.h>
#include <rpc/types.h>
#include <rpc/xdr.h>
#include <rpc/rpc.h>
#include <rpcsvc/yp_prot.h>
#include <rpcsvc/ypclnt.h>
#include <rpcsvc/yppasswd.h>
#include <netdb.h>
#include <sys/socket.h>
#include <sys/file.h>
#include <errno.h>

#include <DirectoryService/DirServices.h>
#include <DirectoryService/DirServicesConst.h>
#include <DirectoryService/DirServicesTypes.h>
#include <DirectoryService/DirServicesUtils.h>

// password server can store 511 characters + a terminator.
#define kMaxPassword		512

#define SaySorryAndBail()	{status = -1; break;}

//-------------------------------------------------------------------------------------
//	ds_check_passwd
//-------------------------------------------------------------------------------------

int ds_check_passwd(char *uname, char *domain)
{
	char 						*p					= NULL;
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
	
	do
	{
		if (uname == NULL)
			SaySorryAndBail();
		
		printf("Checking password for %s.\n", uname);
		p = getpass("Password:");
		if ( p == NULL )
			SaySorryAndBail();
		
		status = dsOpenDirService( &dsRef );
		if ( status != eDSNoErr )
			SaySorryAndBail();
		
		tDataBuff = dsDataBufferAllocate( dsRef, 4096 );
		if (tDataBuff == NULL)
			SaySorryAndBail();
		
		if ( domain != NULL )
		{
			nodeName = dsBuildFromPath( dsRef, domain, "/" );
			if ( nodeName == NULL ) break;
			
			// find
			status = dsFindDirNodes( dsRef, tDataBuff, nodeName, eDSiExact, &nodeCount, &context );
		}
		else
		{
			// find on search node
			status = dsFindDirNodes( dsRef, tDataBuff, NULL, eDSSearchNodeName, &nodeCount, &context );
		}
		
		if ( status != eDSNoErr )
			SaySorryAndBail();
		
		if ( nodeCount < 1 )
			SaySorryAndBail();
		
		status = dsGetDirNodeName( dsRef, tDataBuff, 1, &nodeName );
		if (status != eDSNoErr)
			SaySorryAndBail();
		
		status = dsOpenDirNode( dsRef, nodeName, &nodeRef );
		dsDataListDeallocate( dsRef, nodeName );
		free( nodeName );
		nodeName = NULL;
		if (status != eDSNoErr)
			SaySorryAndBail();
		
		pRecName = dsBuildListFromStrings( dsRef, uname, NULL );
		pRecType = dsBuildListFromStrings( dsRef, kDSStdRecordTypeUsers, NULL );
		pAttrType = dsBuildListFromStrings( dsRef, kDSNAttrMetaNodeLocation, kDSNAttrRecordName, NULL );
	
		recCount = 1;
		status = dsGetRecordList( nodeRef, tDataBuff, pRecName, eDSExact, pRecType,
													pAttrType, 0, &recCount, &context );
		if ( status != eDSNoErr || recCount == 0 )
			SaySorryAndBail();
				
		status = dsGetRecordEntry( nodeRef, tDataBuff, 1, &attrListRef, &pRecEntry );
		if ( status != eDSNoErr )
			SaySorryAndBail();
		
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
		if ( status != eDSNoErr )
			SaySorryAndBail();
		
		pStepBuff = dsDataBufferAllocate( dsRef, 128 );
		
		pAuthType = dsDataNodeAllocateString( dsRef, kDSStdAuthNodeNativeClearTextOK );
		uiCurr = 0;
		
		// User name
		uiLen = strlen( pUserName );
		memcpy( &(tDataBuff->fBufferData[ uiCurr ]), &uiLen, sizeof( unsigned long ) );
		uiCurr += sizeof( unsigned long );
		memcpy( &(tDataBuff->fBufferData[ uiCurr ]), pUserName, uiLen );
		uiCurr += uiLen;
		
		// pw
		uiLen = strlen( p );
		memcpy( &(tDataBuff->fBufferData[ uiCurr ]), &uiLen, sizeof( unsigned long ) );
		uiCurr += sizeof( unsigned long );
		memcpy( &(tDataBuff->fBufferData[ uiCurr ]), p, uiLen );
		uiCurr += uiLen;
		
		tDataBuff->fBufferLength = uiCurr;
		
		status = dsDoDirNodeAuth( userNodeRef, pAuthType, 1, tDataBuff, pStepBuff, NULL );
		
	}
	while ( 0 );
	
	// clean up
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
	
	if ( status != eDSNoErr )
	{
		errno = EACCES;
		fprintf(stderr, "Sorry\n");
		exit(1);
	}
	
	return 0;
}




 
 
