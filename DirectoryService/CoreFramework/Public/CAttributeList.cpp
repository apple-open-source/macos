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
 * @header CAttributeList
 */

#include "CAttributeList.h"
#include "PrivateTypes.h"


//------------------------------------------------------------------------------------
//	* CAttributeList
//------------------------------------------------------------------------------------

CAttributeList::CAttributeList ( tDataListPtr inNodeList )
{
	fNodeList = inNodeList;
} // CAttributeList


//------------------------------------------------------------------------------------
//	* CAttributeList
//------------------------------------------------------------------------------------

CAttributeList::~CAttributeList ( void )
{
} // ~CAttributeList


//------------------------------------------------------------------------------------
//	* GetCount
//------------------------------------------------------------------------------------

uInt32 CAttributeList::GetCount ( void )
{
	if ( fNodeList != 0 )
	{
		return( fNodeList->fDataNodeCount );
	}

	return( 0 );

} // GetCount


//------------------------------------------------------------------------------------
//	* GetAttribute
//------------------------------------------------------------------------------------

sInt32 CAttributeList::GetAttribute ( uInt32 inIndex, char **outData )
{
	sInt32				result		= eDSNoErr;
	bool				done		= false;
	uInt32				i			= 0;
	tDataNodePtr		pCurrNode	= 0;
	tDataBufferPriv	   *pPrivData	= 0;

	if ( fNodeList == 0 )
	{
		result = eDSNullAttributeTypeList;
	}
	else
	{
		pCurrNode = fNodeList->fDataListHead;
		if ( inIndex > fNodeList->fDataNodeCount )
		{
			result = eDSAttrListError;
		}
	}

	if ( pCurrNode == 0 )
	{
		result = eDSAttrListError;
	}

	while ( (result == eDSNoErr) && !done )
	{
		pPrivData = (tDataBufferPriv *)pCurrNode;

		i++;
		if ( i == inIndex )
		{
			*outData = pPrivData->fBufferData;
			done = true;
		}

		if ( !done )
		{
			if ( pPrivData->fNextPtr != 0 )
			{
				pCurrNode = pPrivData->fNextPtr;
			}
			else
			{
				result = eDSAttrListError;
			}
		}
	}

	return( result );

} // GetAttribute
