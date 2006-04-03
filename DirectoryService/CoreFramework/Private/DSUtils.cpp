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
 * @header DSUtils
 */

#include "CLog.h"
#include "DSUtils.h"
#include "SharedConsts.h"
#include <DirectoryService/DirServicesConst.h>

#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stdio.h>
#include <mach/mach_time.h>	// for dsTimeStamp
#include <syslog.h>			// for syslog()



//--------------------------------------------------------------------------------------------------
//	Name:	dsDataBufferAllocatePriv
//
//--------------------------------------------------------------------------------------------------

tDataBufferPtr dsDataBufferAllocatePriv ( unsigned long inBufferSize )
{
	uInt32				size	= 0;
	tDataBufferPtr		outBuff	= nil;

	size = sizeof( tDataBufferPriv ) + inBufferSize;
	outBuff = (tDataBuffer *)::calloc( size + 1, sizeof( char ) );
	if ( outBuff != nil )
	{
		outBuff->fBufferSize = inBufferSize;
		outBuff->fBufferLength = 0;
	}

	return( outBuff );

} // dsDataBufferAllocatePriv


//--------------------------------------------------------------------------------------------------
//	Name:	dsDataBufferDeallocatePriv
//
//--------------------------------------------------------------------------------------------------

tDirStatus dsDataBufferDeallocatePriv ( tDataBufferPtr inDataBufferPtr )
{
	tDirStatus		tdsResult	= eDSNoErr;

	if ( inDataBufferPtr != nil )
	{
		free( inDataBufferPtr );
		inDataBufferPtr = nil;
	}
	else
	{
		tdsResult = eDSNullDataBuff;
	}

	return( tdsResult );

} // dsDataBufferDeallocatePriv



//--------------------------------------------------------------------------------------------------
//	Name:	dsDataListAllocatePriv
//
//--------------------------------------------------------------------------------------------------

tDataList* dsDataListAllocatePriv ( void )
{
	tDataList		   *outResult	= nil;

	outResult = (tDataList *)::calloc( sizeof( tDataList ), sizeof( char ) );

	return( outResult );

} // dsDataListAllocatePriv


//--------------------------------------------------------------------------------------------------
//	Name:	dsDataListDeallocatePriv
//
//--------------------------------------------------------------------------------------------------

tDirStatus dsDataListDeallocatePriv ( tDataListPtr inDataList  )
{
	tDirStatus			tdsResult	= eDSNoErr;
	tDataBufferPriv    *pPrivData	= nil;
	tDataBuffer		   *pTmpBuff	= nil;
	tDataBuffer		   *pDataBuff	= nil;

	if ( inDataList == nil )
	{
		return( eDSNullDataList );
	}

	if ( inDataList->fDataListHead != nil )
	{
		pDataBuff = inDataList->fDataListHead;
		
		inDataList->fDataListHead = nil;
		inDataList->fDataNodeCount = 0;
		
		pPrivData = (tDataBufferPriv *)pDataBuff;
		while ( pDataBuff != nil )
		{
			pTmpBuff = pDataBuff;

			if ( pPrivData != nil )
			{
				pDataBuff = pPrivData->fNextPtr;
				if ( pDataBuff != nil )
				{
					pPrivData = (tDataBufferPriv *)pDataBuff;
				}
			}
			else
			{
				pDataBuff = nil;
			}

			free( pTmpBuff );
			pTmpBuff = nil;
		}
	}

	return( tdsResult );

} // dsDataListDeallocatePriv


//--------------------------------------------------------------------------------------------------
//	Name:	dsGetPathFromListPriv
//
//--------------------------------------------------------------------------------------------------

char* dsGetPathFromListPriv ( tDataListPtr inDataList, const char *inDelimiter )
{
	char			   *outStr			= nil;
	uInt32				uiSafetyCntr	= 0;
	uInt32				uiStrLen		= 0;
	tDataNode		   *pCurrNode		= nil;
	tDataBufferPriv	   *pPrivData		= nil;
	char			   *prevStr			= nil;
	uInt32				cStrLen			= 256;
	char			   *nextStr			= nil;

	if ( (inDataList == nil) || (inDelimiter == nil) )
	{
		return( nil );
	}

	if ( (inDataList->fDataNodeCount == 0) || (inDataList->fDataListHead == nil) )
	{
		return( nil );
	}

        prevStr = (char *)calloc(cStrLen,sizeof(char));
	pCurrNode = inDataList->fDataListHead;
	while ( pCurrNode != nil )
	{
		// Append the delimiter
        strncat(prevStr,inDelimiter,strlen(inDelimiter));

        //check if there is more char buffer length required
		// Append the string
		pPrivData = (tDataBufferPriv *)pCurrNode;
        //check if there is more char buffer length required ie. look at the next string plus
        //the delimiter plus termination null plus pad of 4
        while (cStrLen < (1+strlen(prevStr)+pPrivData->fBufferLength+4))
        {
            nextStr = (char *)calloc(strlen(prevStr)+1,sizeof(char));
            strcpy(nextStr,prevStr);
            free(prevStr);
            cStrLen *= 2;
            prevStr = (char *)calloc(cStrLen,sizeof(char));
            strcpy(prevStr,nextStr);
            free(nextStr);
        }
        strncat(prevStr,(const char *)pPrivData->fBufferData,pPrivData->fBufferLength);

		pCurrNode = pPrivData->fNextPtr;

		uiSafetyCntr++;

		if ( uiSafetyCntr == inDataList->fDataNodeCount )
		{
			// Yes, we are done
			pCurrNode = nil;
		}
	}

    uiStrLen = strlen(prevStr);
	if ( uiStrLen != 0 )
	{
        outStr = (char *)calloc( uiStrLen + 1, sizeof(char));
        ::strcpy( outStr, prevStr );
	}

    free(prevStr);

	return( outStr );

} // dsGetPathFromListPriv


//--------------------------------------------------------------------------------------------------
//	Name:	dsBuildFromPathPriv
//
//--------------------------------------------------------------------------------------------------

tDataListPtr dsBuildFromPathPriv ( const char *inPathCString, const char *inPathSeparatorCString )
{
	const char		   *inStr		= nil;
	char			   *ptr			= nil;
	const char		   *inDelim		= nil;
	sInt32				delimLen	= 0;
	bool				done		= false;
	sInt32				len			= 0;
	tDataList		   *outDataList	= nil;
    char			   *cStr		= nil;

	if ( (inPathCString == nil) || (inPathSeparatorCString == nil) )
	{
		return( nil );
	}

	inStr = inPathCString;
	len = ::strlen( inStr );

   	inDelim = inPathSeparatorCString;
   	delimLen = ::strlen( inDelim );

	// Does the string == the delimiter
	if ( ::strcmp( inStr, inDelim ) == 0 )
	{
		return( nil );
	}

	outDataList = ::dsDataListAllocatePriv();
	if ( outDataList == nil )
	{
		return( nil );
	}

	ptr = strstr( inStr, inDelim );

	// Does the first char(s) == the delimiter
	if ( (ptr != nil) && (ptr == inStr) )
	{
		inStr += delimLen;
	}

	while ( !done && (*inStr != '\0') )
	{
		ptr = ::strstr( inStr, inDelim );
		if ( ptr == nil )
		{
			len = ::strlen( inStr );

            cStr = (char *)calloc(len + 1, sizeof(char));
            strncpy(cStr,inStr,len);

            ::dsAppendStringToListAllocPriv( outDataList, cStr );
            free(cStr);

            done = true;
		}
		else
		{
			len = ptr - inStr;

            cStr = (char *)calloc(len + 1, sizeof(char));
            strncpy(cStr,inStr,len);

            ::dsAppendStringToListAllocPriv( outDataList, cStr );
            free(cStr);
            
			inStr += len + delimLen;
		}
	}

	return( outDataList );

} // dsBuildFromPathPriv


//--------------------------------------------------------------------------------------------------
//	Name:	dsAppendStringToListAllocPriv
//
//--------------------------------------------------------------------------------------------------

tDirStatus dsAppendStringToListAllocPriv (	tDataList	   *inOutDataList,
											const char	   *inCString )
{
	tDirStatus			tdsResult		= eDSNoErr;
	const char		   *pInString		= inCString;
	tDataNodePtr		pNewNode		= nil;
	tDataNodePtr		pCurrNode		= nil;
	tDataBufferPriv    *pNewNodeData	= nil;
	tDataBufferPriv    *pCurNodeData	= nil;

	if ( inOutDataList == nil )
	{
		return( eDSNullDataList );
	}

	if ( inCString == nil )
	{
		return( eDSNullParameter );
	}

	if ( ((inOutDataList->fDataNodeCount != 0) && (inOutDataList->fDataListHead == nil)) ||
		 ((inOutDataList->fDataNodeCount == 0) && (inOutDataList->fDataListHead != nil)) )
	{
		return( eDSBadDataNodeFormat );
	}

	pNewNode = ::dsAllocListNodeFromStringPriv( pInString );
	if ( pNewNode == nil )
	{
		return( eMemoryAllocError );
	}

	if ( inOutDataList->fDataNodeCount == 0 )
	{
		inOutDataList->fDataListHead = pNewNode;
		pNewNodeData = (tDataBufferPriv *)pNewNode;
		pNewNodeData->fPrevPtr		= nil;
		pNewNodeData->fNextPtr		= nil;
		pNewNodeData->fScriptCode	= kASCIICodeScript;

		inOutDataList->fDataNodeCount++;
	}
	else
	{
		pCurrNode = ::dsGetLastNodePriv( inOutDataList->fDataListHead );
		if ( pCurrNode != nil )
		{
			// Get the current node's header and point it to the new
			pCurNodeData = (tDataBufferPriv *)pCurrNode;
			pCurNodeData->fNextPtr = pNewNode;

			// Get the new node's header and point it to the prevous end
			pNewNodeData = (tDataBufferPriv *)pNewNode;
			pNewNodeData->fPrevPtr	= pCurrNode;
			pNewNodeData->fNextPtr	= nil;

			// Set the script code to ASCII
			pNewNodeData->fScriptCode = kASCIICodeScript;

			inOutDataList->fDataNodeCount++;
		}
		else
		{
			tdsResult = eDSInvalidIndex;
		}
	}

	return( tdsResult );

} // dsAppendStringToListAllocPriv



//--------------------------------------------------------------------------------------------------
//	Name:	dsAllocListNodeFromStringPriv
//
//--------------------------------------------------------------------------------------------------

tDataNodePtr dsAllocListNodeFromStringPriv ( const char *inString )
{
	uInt32				nodeSize	= 0;
	uInt32				strLen		= 0;
	tDataNode		   *pOutNode	= nil;
	tDataBufferPriv	   *pPrivData	= nil;

	if ( inString != nil )
	{
		strLen = ::strlen( inString );
		nodeSize = sizeof( tDataBufferPriv ) + strLen + 1;
		pOutNode = (tDataNode *)::calloc( nodeSize, sizeof( char ) );
		if ( pOutNode != nil )
		{
			pOutNode->fBufferSize = nodeSize;
			pOutNode->fBufferLength = nodeSize;

			pPrivData = (tDataBufferPriv *)pOutNode;
			pPrivData->fBufferSize = strLen;
			pPrivData->fBufferLength = strLen;

			::strcpy( pPrivData->fBufferData, inString );
		}
	}

	return( pOutNode );

} // dsAllocListNodeFromStringPriv


//--------------------------------------------------------------------------------------------------
//	Name:	dsAllocListNodeFromBuffPriv
//
//--------------------------------------------------------------------------------------------------

tDataNodePtr dsAllocListNodeFromBuffPriv ( const void *inData, const uInt32 inSize )
{
	uInt32				nodeSize	= 0;
	tDataNode		   *pOutNode	= nil;
	tDataBufferPriv	   *pPrivData	= nil;

	if ( inData != nil )
	{
		nodeSize = sizeof( tDataBufferPriv ) + inSize + 1;
		pOutNode = (tDataNode *)::calloc( nodeSize, sizeof( char ) );
		if ( pOutNode != nil )
		{
			pOutNode->fBufferSize = nodeSize;
			pOutNode->fBufferLength = nodeSize;

			pPrivData = (tDataBufferPriv *)pOutNode;
			pPrivData->fBufferSize = inSize;
			pPrivData->fBufferLength = inSize;

			::memcpy( pPrivData->fBufferData, inData, inSize );
		}
	}

	return( pOutNode );

} // dsAllocListNodeFromBuffPriv


//--------------------------------------------------------------------------------------------------
//	Name:	dsGetThisNodePriv
//
//--------------------------------------------------------------------------------------------------

tDataNodePtr dsGetThisNodePriv ( tDataNode *inFirsNode, const unsigned long inIndex )
{
	uInt32				i			= 1;
	tDataNode		   *pCurrNode	= nil;
	tDataBufferPriv    *pPrivData	= nil;

	pCurrNode = inFirsNode;
	while ( pCurrNode != nil )
	{
		if ( i == inIndex )
		{
			break;
		}
		else
		{
			pPrivData = (tDataBufferPriv *)pCurrNode;
			pCurrNode = pPrivData->fNextPtr;
		}
		i++;
	}

	return( pCurrNode );

} // dsGetThisNodePriv


//--------------------------------------------------------------------------------------------------
//	Name:	dsGetLastNodePriv
//
//--------------------------------------------------------------------------------------------------

tDataNodePtr dsGetLastNodePriv ( tDataNode *inFirsNode )
{
	tDataNode		   *pCurrNode	= nil;
	tDataBufferPriv    *pPrivData	= nil;

	pCurrNode = inFirsNode;
	pPrivData = (tDataBufferPriv *)pCurrNode;

	while ( pPrivData->fNextPtr != nil )
	{
		pCurrNode = pPrivData->fNextPtr;
		pPrivData = (tDataBufferPriv *)pCurrNode;
	}

	return( pCurrNode );

} // dsGetLastNodePriv


//--------------------------------------------------------------------------------------------------
//	Name:	dsAppendStringToListPriv
//
//--------------------------------------------------------------------------------------------------

tDirStatus dsAppendStringToListPriv ( tDataListPtr inOutDataList, const char *inCString )
{
	tDirStatus			tdsResult		= eDSNoErr;
	const char		   *pInString		= inCString;
	tDataNodePtr		pNewNode		= nil;
	tDataNodePtr		pCurrNode		= nil;
	tDataBufferPriv    *pNewNodeData	= nil;
	tDataBufferPriv    *pCurNodeData	= nil;

	if ( inOutDataList == nil )
	{
		return( eDSNullDataList );
	}

	if ( inCString == nil )
	{
		return( eDSNullParameter );
	}

	if ( ((inOutDataList->fDataNodeCount != 0) && (inOutDataList->fDataListHead == nil)) ||
		 ((inOutDataList->fDataNodeCount == 0) && (inOutDataList->fDataListHead != nil)) )
	{
		return( eDSBadDataNodeFormat );
	}

	pNewNode = ::dsAllocListNodeFromStringPriv( pInString );
	if ( pNewNode == nil )
	{
		return( eMemoryAllocError );
	}

	if ( inOutDataList->fDataNodeCount == 0 )
	{
		inOutDataList->fDataListHead = pNewNode;
		pNewNodeData = (tDataBufferPriv *)pNewNode;
		pNewNodeData->fPrevPtr		= nil;
		pNewNodeData->fNextPtr		= nil;
		pNewNodeData->fScriptCode	= kASCIICodeScript;

		inOutDataList->fDataNodeCount++;
	}
	else
	{
		pCurrNode = ::dsGetLastNodePriv( inOutDataList->fDataListHead );
		if ( pCurrNode != nil )
		{
			// Get the current node's header and point it to the new
			pCurNodeData = (tDataBufferPriv *)pCurrNode;
			pCurNodeData->fNextPtr = pNewNode;

			// Get the new node's header and point it to the prevous end
			pNewNodeData = (tDataBufferPriv *)pNewNode;
			pNewNodeData->fPrevPtr	= pCurrNode;
			pNewNodeData->fNextPtr	= nil;

			// Set the script code to ASCII
			pNewNodeData->fScriptCode = kASCIICodeScript;

			inOutDataList->fDataNodeCount++;
		}
		else
		{
			tdsResult = eDSInvalidIndex;
		}
	}

	return( tdsResult );

} // dsAppendStringToListPriv


//--------------------------------------------------------------------------------------------------
//	Name:	dsDataListGetNodeCountPriv
//
//--------------------------------------------------------------------------------------------------

unsigned long dsDataListGetNodeCountPriv ( tDataListPtr inDataList )
{
	return( inDataList->fDataNodeCount );
} // dsDataListGetNodeCountPriv


//--------------------------------------------------------------------------------------------------
//	Name:	dsGetDataLengthPriv
//
//--------------------------------------------------------------------------------------------------

unsigned long dsGetDataLengthPriv ( tDataListPtr inDataList )
{
	bool				done		= false;
	unsigned long		outStrLen	= 0;
	tDataNodePtr		pCurrNode	= nil;
	tDataBufferPriv    *pPrivData	= nil;

	if ( inDataList != nil )
	{
		if ( inDataList->fDataListHead != nil )
		{
			pCurrNode = inDataList->fDataListHead;

			// Get the list total length
			while ( !done )
			{
				outStrLen += pCurrNode->fBufferLength;
				pPrivData = (tDataBufferPriv *)pCurrNode;
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

	return( outStrLen );

} // dsGetDataLengthPriv


//--------------------------------------------------------------------------------------------------
//	Name:	dsDataListGetNodePriv
// PLEASE note that this really returns a tDataBufferPriv
//--------------------------------------------------------------------------------------------------

tDirStatus dsDataListGetNodePriv ( tDataListPtr		inDataList,
									unsigned long	inNodeIndex,
									tDataNodePtr	*outDataListNode )
{
	uInt32				i			= 0;
	tDirStatus			tdsResult	= eDSNoErr;
	tDataNodePtr		pCurrNode	= nil;
	tDataBufferPriv    *pPrivData	= nil;

	*outDataListNode = nil;

	if ( inDataList != nil )
	{
		pCurrNode = inDataList->fDataListHead;

		// Get the list total length
		for ( i = 1; i < inNodeIndex; i++ )
		{
			pPrivData = (tDataBufferPriv *)pCurrNode;
			if ( pPrivData->fNextPtr == nil )
			{
				pCurrNode = nil;
				break;
			}
			else
			{
				pCurrNode = pPrivData->fNextPtr;
			}
		}

		if ( pCurrNode != nil )
		{
			*outDataListNode = pCurrNode;
		}
		else
		{
			tdsResult = eDSInvalDataList;
		}
	}

	return( tdsResult );

} // dsDataListGetNodePriv


//--------------------------------------------------------------------------------------------------
//	Name:	dsDataListGetNodeStringPriv
//
//--------------------------------------------------------------------------------------------------

char* dsDataListGetNodeStringPriv (	tDataListPtr	inDataList,
									unsigned long	inNodeIndex )
{
	uInt32				iSegment	= 0;
	tDataNodePtr		pCurrNode	= nil;
	tDataBufferPriv    *pPrivData	= nil;
	char			   *outSegStr	= nil;

	if ( ( inDataList != nil ) && ( inNodeIndex > 0 ) && ( inNodeIndex <= inDataList->fDataNodeCount ) )
	{
		pCurrNode = inDataList->fDataListHead;

		// Find the one we are interested in
		iSegment = 1;
		while ( (iSegment < inNodeIndex ) && ( pCurrNode != nil) )
		{
			pPrivData = (tDataBufferPriv *)pCurrNode;
			
			if ( pPrivData->fNextPtr == nil )
			{
				pCurrNode = nil;
			}
			else
			{
				pCurrNode = pPrivData->fNextPtr;
			}
			iSegment++;
		}

		if ( pCurrNode != nil )
		{
			pPrivData = (tDataBufferPriv *)pCurrNode;
			outSegStr = (char *) calloc(1, 1 + pPrivData->fBufferLength);
			memcpy(outSegStr, (const char *)pPrivData->fBufferData, pPrivData->fBufferLength);
		}
	}

	return( outSegStr );

} // dsDataListGetNodeStringPriv


//--------------------------------------------------------------------------------------------------
//	Name:	dsDeleteLastNodePriv
//
//--------------------------------------------------------------------------------------------------

tDirStatus dsDeleteLastNodePriv ( tDataListPtr inList )
{
	tDirStatus			result			= eDSNoErr;
	return( result );

} // dsDeleteLastNodePriv



//--------------------------------------------------------------------------------------------------
//	Name:	dsBuildListFromStringsPriv
//
//--------------------------------------------------------------------------------------------------

tDataList* dsBuildListFromStringsPriv ( const char *in1stCString, ... )
{
	tDirStatus			tdsResult	= eDSNoErr;
	tDataList		   *pOutList	= nil;
	const char		   *pStr		= nil;
	uInt32				nodeCount	= 0;
	tDataNodePtr		pCurrNode	= nil;
	tDataNodePtr		pPrevNode	= nil;
	tDataBufferPriv    *pPrivData	= nil;
	va_list				args;

	pOutList = ::dsDataListAllocatePriv();
	if ( pOutList == nil )
	{
		return( nil );
	}

	va_start( args, in1stCString );

	pStr = in1stCString;

	while ( (pStr != nil) && (tdsResult == eDSNoErr) )
	{
		// Node size is: struct size + string length + null term byte
		pCurrNode = ::dsAllocListNodeFromStringPriv ( pStr );
		if ( pCurrNode != nil )
		{
			// Increment the node list count
			nodeCount++;

			if ( pOutList->fDataNodeCount == 0 )
			{
				pOutList->fDataListHead = pCurrNode;
				pPrivData = (tDataBufferPriv *)pCurrNode;
				pPrivData->fPrevPtr		= nil;
				pPrivData->fNextPtr		= nil;
				pPrivData->fScriptCode	= kASCIICodeScript;

				pOutList->fDataNodeCount++;
			}
			else if ( pPrevNode != nil )
			{
				// Get the previous node's front pointer it to the next
				pPrivData = (tDataBufferPriv *)pPrevNode;
				pPrivData->fNextPtr = pCurrNode;

				// Get the current node's back pointer it to the prevous
				pPrivData = (tDataBufferPriv *)pCurrNode;
				pPrivData->fPrevPtr = pPrevNode;

				// Set the script code to ASCII
				pPrivData->fScriptCode = kASCIICodeScript;

				pOutList->fDataNodeCount++;
			}
			else
			{
				tdsResult = eMemoryAllocError;
			}

		}

		// Get the next string
		pStr = va_arg( args, char * );

		pPrevNode = pCurrNode;
		pCurrNode = nil;
	}

	va_end( args );

	pOutList->fDataNodeCount = nodeCount;

	return( pOutList );

} // dsBuildListFromStringsPriv


//--------------------------------------------------------------------------------------------------
//	Name:	dsDataListGetNodeAllocPriv
//
//--------------------------------------------------------------------------------------------------

tDirStatus dsDataListGetNodeAllocPriv ( const tDataList		   *inDataList,
										const unsigned long		inIndex,
										tDataNode			  **outDataNode )
{
	tDirStatus			tResult			= eDSNoErr;
	uInt32				uiLength		= 0;
	tDataBuffer		   *pOutDataNode	= nil;
	tDataNode		   *pCurrNode		= nil;
	tDataBufferPriv	   *pPrivData		= nil;


	LogAssert_( inDataList != nil );
	if ( inDataList == nil )
	{
		return( eDSNullDataList );
	}

	LogAssert_( inDataList->fDataListHead != nil );
	if ( inDataList->fDataListHead == nil )
	{
		return( eDSEmptyDataList );
	}

	pCurrNode = ::dsGetThisNodePriv( inDataList->fDataListHead, inIndex );
	if ( pCurrNode == nil )
	{
		return( eDSIndexOutOfRange );
	}

	if ( outDataNode == nil )
	{
		return( eDSNullTargetArgument );
	}

	pPrivData = (tDataBufferPriv *)pCurrNode;
	uiLength = pPrivData->fBufferLength;

	pOutDataNode = ::dsDataBufferAllocatePriv( uiLength + 1 );
	if ( pOutDataNode != nil )
	{
		::memcpy( pOutDataNode->fBufferData, pPrivData->fBufferData, uiLength );
		pOutDataNode->fBufferSize = uiLength;
		pOutDataNode->fBufferLength = uiLength;
		*outDataNode = pOutDataNode;
	}
	else
	{
		tResult = eMemoryAllocError;
	}

	return( tResult );

} // dsDataListGetNodeAllocPriv


//--------------------------------------------------------------------------------------------------
//	Name:	dsAuthBufferGetDataListAllocPriv
//
//--------------------------------------------------------------------------------------------------

tDataListPtr dsAuthBufferGetDataListAllocPriv ( tDataBufferPtr inAuthBuff )
{
	tDataListPtr pOutList = NULL;
	tDirStatus status = eDSNoErr;
	if (inAuthBuff == NULL)
	{
		LOG2( kStdErr, "*** DS NULL Error: File: %s. Line: %d.\n", __FILE__, __LINE__ );
		return NULL;
	}

	pOutList = dsDataListAllocatePriv();
	if (pOutList != NULL)
	{
		status = dsAuthBufferGetDataListPriv( inAuthBuff, pOutList );
		if (status != eDSNoErr)
		{
			dsDataListDeallocatePriv( pOutList );
			free( pOutList );
			pOutList = NULL;
		}
	}

	return pOutList;
} // dsAuthBufferGetDataListAllocPriv


//--------------------------------------------------------------------------------------------------
//	Name:	dsAuthBufferGetDataListPriv
//
//--------------------------------------------------------------------------------------------------

tDirStatus dsAuthBufferGetDataListPriv ( tDataBufferPtr inAuthBuff, tDataListPtr inOutDataList )
{
	char		   *pData			= nil;
	uInt32			itemLen			= 0;
	uInt32			offset			= 0;
	uInt32			buffSize		= 0;
	uInt32			buffLen			= 0;
	tDirStatus			tResult		= eDSNoErr;
	tDataNode		   *pCurrNode	= nil;
	tDataNode		   *pPrevNode	= nil;
	tDataBufferPriv    *pPrivData	= nil;

	if (inAuthBuff == NULL)
	{
		LOG2( kStdErr, "*** DS NULL Error: File: %s. Line: %d.\n", __FILE__, __LINE__ );
		return eDSNullDataBuff;
	}
	if ( inOutDataList == nil )
	{
		LOG2( kStdErr, "*** DS NULL Error: File: %s. Line: %d.\n", __FILE__, __LINE__ );
		return( eDSNullDataList );
	}

	// This could leak, but the client should not pass us a nonempty data list
	inOutDataList->fDataNodeCount = 0;
	inOutDataList->fDataListHead  = nil;

	pData		= inAuthBuff->fBufferData;
	buffSize	= inAuthBuff->fBufferSize;
	buffLen		= inAuthBuff->fBufferLength;

	if (buffLen > buffSize) throw( (sInt32)eDSInvalidBuffFormat );

	while ( (offset < buffLen) && (tResult == eDSNoErr) )
	{
		if (offset + sizeof( unsigned long ) > buffLen)
		{
			tResult = eDSInvalidBuffFormat;
			break;
		}
		::memcpy( &itemLen, pData, sizeof( unsigned long ) );
		pData += sizeof( unsigned long );
		offset += sizeof( unsigned long );
		if (itemLen + offset > buffLen)
		{
			tResult = eDSInvalidBuffFormat;
			break;
		}

		// Node size is: struct size + string length + null term byte
		pCurrNode = dsAllocListNodeFromBuffPriv( pData, itemLen );
		if ( pCurrNode != nil )
		{
			pData += itemLen;
			offset += itemLen;

			if ( inOutDataList->fDataNodeCount == 0 )
			{
				inOutDataList->fDataListHead = pCurrNode;
				pPrivData = (tDataBufferPriv *)pCurrNode;
				pPrivData->fPrevPtr		= nil;
				pPrivData->fNextPtr		= nil;
				pPrivData->fScriptCode	= kASCIICodeScript;

				inOutDataList->fDataNodeCount++;
			}
			else if ( pPrevNode != nil )
			{
				// Get the previous node's header and point it to the next
				pPrivData = (tDataBufferPriv *)pPrevNode;
				pPrivData->fNextPtr = pCurrNode;

				// Get the current node's header and point it to the prevous
				pPrivData = (tDataBufferPriv *)pCurrNode;
				pPrivData->fPrevPtr = pPrevNode;

				// Set the script code to ASCII
				pPrivData->fScriptCode = kASCIICodeScript;

				inOutDataList->fDataNodeCount++;
			}
			else
			{
				tResult = eMemoryAllocError;
				LOG3( kStdErr, "*** DSError: File: %s. Line: %d. Error = %d.\n", __FILE__, __LINE__, tResult );
			}
		}
		else
		{
			tResult = eMemoryAllocError;
			LOG3( kStdErr, "*** DSError: File: %s. Line: %d. Error = %d.\n", __FILE__, __LINE__, tResult );
		}

		pPrevNode = pCurrNode;
		pCurrNode = nil;
	}

	if ( tResult != eDSNoErr )
	{
		// clean up if there was an errors
		dsDataListDeallocatePriv(inOutDataList);
	}
	return( tResult );

} // dsAuthBufferGetDataListPriv


// --------------------------------------------------------------------------------
//	BinaryToHexConversion
// --------------------------------------------------------------------------------
void BinaryToHexConversion( const unsigned char *inBinary, unsigned long inLength, char *outHexStr )
{
    const unsigned char		   *sptr	= inBinary;
    char					   *tptr	= outHexStr;
    unsigned long 				index	= 0;
    char 						high;
	char						low;
    
    for ( index = 0; index < inLength; index++ )
    {
        high	= (*sptr & 0xF0) >> 4;
        low		= (*sptr & 0x0F);
        
        if ( high >= 0x0A )
            *tptr++ = (high - 0x0A) + 'A';
        else
            *tptr++ = high + '0';
            
        if ( low >= 0x0A )
            *tptr++ = (low - 0x0A) + 'A';
        else
            *tptr++ = low + '0';
            
        sptr++;
    }
    
    *tptr = '\0';
}


// --------------------------------------------------------------------------------
//	HexToBinaryConversion
// --------------------------------------------------------------------------------

void HexToBinaryConversion( const char *inHexStr, unsigned long *outLength, unsigned char *outBinary )
{
    unsigned char	   *tptr = outBinary;
    unsigned char		val;
    
    while ( *inHexStr && *(inHexStr+1) )
    {
        if ( *inHexStr >= 'A' )
            val = (*inHexStr - 'A' + 0x0A) << 4;
        else
            val = (*inHexStr - '0') << 4;
        
        inHexStr++;
        
        if ( *inHexStr >= 'A' )
            val += (*inHexStr - 'A' + 0x0A);
        else
            val += (*inHexStr - '0');
        
        inHexStr++;
        
        *tptr++ = val;
    }
    
    *outLength = (tptr - outBinary);
}

//--------------------------------------------------------------------------------------------------
//	Name:	dsTimestamp
//
//--------------------------------------------------------------------------------------------------
// Actually, [gettimeofday] is not so great, since it uses the calendar clock and has to call into the kernel. 
// mach_absolute_time() doesn't call into the kernel (on platforms where it doesn't have to...) 
// This is done for you by mach_absolute_time() and mach_timebase_info().

double dsTimestamp(void)
{
	static uint32_t	num		= 0;
	static uint32_t	denom	= 0;
	uint64_t		now;
	
	if (denom == 0) 
	{
		struct mach_timebase_info tbi;
		kern_return_t r;
		r = mach_timebase_info(&tbi);
		if (r != KERN_SUCCESS) 
		{
			syslog( LOG_ALERT, "Warning: mach_timebase_info FAILED! - error = %u\n", r);
			return 0;
		}
		else
		{
			num		= tbi.numer;
			denom	= tbi.denom;
		}
	}
	now = mach_absolute_time();
	
//	return (double)(now * (double)num / denom / NSEC_PER_SEC);	// return seconds
	return (double)(now * (double)num / denom / NSEC_PER_USEC);	// return microsecs
//	return (double)(now * (double)num / denom);	// return nanoseconds
}


tDirStatus dsGetAuthMethodEnumValue( tDataNode *inData, uInt32 *outAuthMethod )
{
	tDirStatus		siResult			= eDSNoErr;
	uInt32			uiNativeLen			= 0;
	char		   *authMethodPtr		= NULL;
	
	if ( inData == NULL )
	{
		*outAuthMethod = kAuthUnknownMethod;
		return eDSAuthMethodNotSupported;
	}
	
	authMethodPtr = (char *)inData->fBufferData;
	
	//DBGLOG1( kLogPlugin, "Using authentication method %s.", authMethodPtr );
	
	if ( ::strcmp( authMethodPtr, kDSStdAuthClearText ) == 0 )
	{
		// Clear text auth method
		*outAuthMethod = kAuthClearText;
	}
	else if ( ::strcmp( authMethodPtr, kDSStdAuthNodeNativeClearTextOK ) == 0 )
	{
		// Node native auth method
		*outAuthMethod = kAuthNativeClearTextOK;
	}
	else if ( ::strcmp( authMethodPtr, kDSStdAuthNodeNativeNoClearText ) == 0 )
	{
		// Node native auth method
		*outAuthMethod = kAuthNativeNoClearText;
	}
	else if ( ::strcmp( authMethodPtr, kDSStdAuthCrypt ) == 0 )
	{
		// Unix Crypt auth method
		*outAuthMethod = kAuthCrypt;
	}
	else if ( ::strcmp( authMethodPtr, kDSStdAuth2WayRandom ) == 0 )
	{
		// Two way random auth method
		*outAuthMethod = kAuth2WayRandom;
	}
	else if ( ::strcmp( authMethodPtr, kDSStdAuth2WayRandomChangePasswd ) == 0 )
	{
		// Two way random auth method
		*outAuthMethod = kAuth2WayRandomChangePass;
	}
	else if ( ::strcmp( authMethodPtr, kDSStdAuthSMB_NT_Key ) == 0 )
	{
		// Two way random auth method
		*outAuthMethod = kAuthSMB_NT_Key;
	}
	else if ( ::strcmp( authMethodPtr, kDSStdAuthSMB_LM_Key ) == 0 )
	{
		// Two way random auth method
		*outAuthMethod = kAuthSMB_LM_Key;
	}
	else if ( ::strcmp( authMethodPtr, kDSStdAuthSecureHash ) == 0 )
	{
		// secure hash method
		*outAuthMethod = kAuthSecureHash;
	}
	else if ( ::strcmp( authMethodPtr, kDSStdAuthReadSecureHash ) == 0 )
	{
		// secure hash method
		*outAuthMethod = kAuthReadSecureHash;
	}
	else if ( ::strcmp( authMethodPtr, kDSStdAuthWriteSecureHash ) == 0 )
	{
		// secure hash method
		*outAuthMethod = kAuthWriteSecureHash;
	}
	else if ( ::strcmp( authMethodPtr, kDSStdAuthSetPasswd ) == 0 )
	{
		// Admin set password
		*outAuthMethod = kAuthSetPasswd;
	}
	else if ( ::strcmp( authMethodPtr, kDSStdAuthSetPasswdAsRoot ) == 0 )
	{
		// Admin set password
		*outAuthMethod = kAuthSetPasswdAsRoot;
	}
	else if ( ::strcmp( authMethodPtr, kDSStdAuthChangePasswd ) == 0 )
	{
		// User change password
		*outAuthMethod = kAuthChangePasswd;
	}
	else if ( ::strcmp( authMethodPtr, kDSStdAuthAPOP ) == 0 )
	{
		// APOP auth method
		*outAuthMethod = kAuthAPOP;
	}
	else if ( ::strcmp( authMethodPtr, kDSStdAuthCRAM_MD5 ) == 0 )
	{
		// CRAM-MD5 auth method
		*outAuthMethod = kAuthCRAM_MD5;
	}
	else if ( ::strcmp( authMethodPtr, kDSStdAuthSetPolicy ) == 0 )
	{
		*outAuthMethod = kAuthSetPolicy;
	}
	else if ( ::strcmp( authMethodPtr, kDSStdAuthWithAuthorizationRef ) == 0 )
	{
		*outAuthMethod = kAuthWithAuthorizationRef;
	}
	else if ( ::strcmp( authMethodPtr, kDSStdAuthGetPolicy ) == 0 )
	{
		*outAuthMethod = kAuthGetPolicy;
	}
	else if ( ::strcmp( authMethodPtr, kDSStdAuthGetGlobalPolicy ) == 0 )
	{
		*outAuthMethod = kAuthGetGlobalPolicy;
	}
	else if ( ::strcmp( authMethodPtr, kDSStdAuthSetGlobalPolicy ) == 0 )
	{
		*outAuthMethod = kAuthSetGlobalPolicy;
	}
	else if ( ::strcmp( authMethodPtr, "dsAuthMethodSetPasswd:dsAuthNodeNativeCanUseClearText" ) == 0 )
	{
		*outAuthMethod = kAuthSetPasswdCheckAdmin;
	}
	else if ( ::strcmp( authMethodPtr, kDSStdAuthSetPolicyAsRoot ) == 0 )
	{
		*outAuthMethod = kAuthSetPolicyAsRoot;
	}
	else if ( ::strcmp( authMethodPtr, kDSStdAuthSetLMHash ) == 0 )
	{
		*outAuthMethod = kAuthSetLMHash;
	}
	else if ( ::strcmp( authMethodPtr, "dsAuthMethodStandard:dsAuthVPN_MPPEMasterKeys" ) == 0 ||
			  ::strcmp( authMethodPtr, "dsAuthMethodStandard:dsAuthVPN_PPTPMasterKeys" ) == 0 ||
			  ::strcmp( authMethodPtr, kDSStdAuthMPPEMasterKeys ) == 0 )
	{
		*outAuthMethod = kAuthVPN_PPTPMasterKeys;
	}
	else if ( ::strcmp( authMethodPtr, kDSStdAuthGetKerberosPrincipal ) == 0 )
	{
		*outAuthMethod = kAuthGetKerberosPrincipal;
	}
	else if ( ::strcmp( authMethodPtr, kDSStdAuthGetEffectivePolicy ) == 0 )
	{
		*outAuthMethod = kAuthGetEffectivePolicy;
	}
	else if ( ::strcmp( authMethodPtr, kDSStdAuthSetWorkstationPasswd ) == 0 )
	{
		*outAuthMethod = kAuthNTSetWorkstationPasswd;
	}
	else if ( ::strcmp( authMethodPtr, kDSStdAuthSMBWorkstationCredentialSessionKey ) == 0 ||
			  ::strcmp( authMethodPtr, kDSStdAuthSetNTHash ) == 0 )
	{
		*outAuthMethod = kAuthSMBWorkstationCredentialSessionKey;
	}
	else if ( ::strcmp( authMethodPtr, kDSStdAuthSMB_NT_UserSessionKey ) == 0 )
	{
		*outAuthMethod = kAuthSMB_NTUserSessionKey;
	}
	else if ( ::strcmp( authMethodPtr, kDSStdAuthGetUserName ) == 0 )
	{
		*outAuthMethod = kAuthGetUserName;
	}
    else if ( ::strcmp( authMethodPtr, kDSStdAuthSetUserName ) == 0 )
	{
		*outAuthMethod = kAuthSetUserName;
	}
    else if ( ::strcmp( authMethodPtr, kDSStdAuthGetUserData ) == 0 )
	{
		*outAuthMethod = kAuthGetUserData;
	}
    else if ( ::strcmp( authMethodPtr, kDSStdAuthSetUserData ) == 0 )
	{
		*outAuthMethod = kAuthSetUserData;
	}
    else if ( ::strcmp( authMethodPtr, kDSStdAuthDeleteUser ) == 0 )
	{
		*outAuthMethod = kAuthDeleteUser;
	}
	else if ( ::strcmp( authMethodPtr, kDSStdAuthNewUser ) == 0 )
	{
        *outAuthMethod = kAuthNewUser;
    }
	else if ( ::strcmp( authMethodPtr, kDSStdAuthNTLMv2 ) == 0 )
	{
		*outAuthMethod = kAuthNTLMv2;
	}
	else if ( ::strcmp( authMethodPtr, kDSStdAuthMSCHAP2 ) == 0 )
	{
		*outAuthMethod = kAuthMSCHAP2;
	}
	else if ( ::strcmp( authMethodPtr, kDSStdAuthDIGEST_MD5 ) == 0 )
	{
		*outAuthMethod = kAuthDIGEST_MD5;
	}
	else if ( ::strcmp( authMethodPtr, kDSStdAuth2WayRandom ) == 0 )
	{
		*outAuthMethod = kAuth2WayRandom;
	}
	else if ( ::strcmp( authMethodPtr, kDSStdAuth2WayRandomChangePasswd ) == 0 )
	{
		*outAuthMethod = kAuth2WayRandomChangePass;
	}
	else if ( ::strcmp( authMethodPtr, kDSStdAuthSMB_NT_Key ) == 0 )
	{
		*outAuthMethod = kAuthSMB_NT_Key;
	}
    else if ( ::strcmp( authMethodPtr, kDSStdAuthSMB_LM_Key ) == 0 )
	{
		*outAuthMethod = kAuthSMB_LM_Key;
	}
	else if ( ::strcmp( authMethodPtr, kDSStdAuthNewUserWithPolicy ) == 0 )
	{
        *outAuthMethod = kAuthNewUserWithPolicy;
    }
	else if ( ::strcmp( authMethodPtr, kDSStdAuthSetShadowHashWindows ) == 0 )
	{
        *outAuthMethod = kAuthSetShadowHashWindows;
    }
	else if ( ::strcmp( authMethodPtr, kDSStdAuthSetShadowHashSecure ) == 0 )
	{
        *outAuthMethod = kAuthSetShadowHashSecure;
    }
	else if ( ::strcmp( authMethodPtr, "dsAuthMethodStandard:dsAuthNTWithSessionKey" ) == 0 )
	{
        *outAuthMethod = kAuthNTSessionKey;
	}
	else
	{
		uiNativeLen	= ::strlen( kDSNativeAuthMethodPrefix );

		if ( ::strncmp( authMethodPtr, kDSNativeAuthMethodPrefix, uiNativeLen ) == 0 )
		{
			// User change password
			*outAuthMethod = kAuthNativeMethod;
		}
		else
		{
			*outAuthMethod = kAuthUnknownMethod;
			siResult = eDSAuthMethodNotSupported;
		}
	}
	
	return( siResult );
} // dsGetAuthMethodEnumValue


const char* dsGetPatternMatchName ( tDirPatternMatch inPatternMatchEnum )
{
	const char	   *outString   = nil;
	
	switch (inPatternMatchEnum)
	{
		case eDSNoMatch1:
			outString = "eDSNoMatch1";
			break;
			
		case eDSAnyMatch:
			outString = "eDSAnyMatch";
			break;
			
		case eDSExact:
			outString = "eDSExact";
			break;
			
		case eDSStartsWith:
			outString = "eDSStartsWith";
			break;
			
		case eDSEndsWith:
			outString = "eDSEndsWith";
			break;
			
		case eDSContains:
			outString = "eDSContains";
			break;
			
		case eDSLessThan:
			outString = "eDSLessThan";
			break;
			
		case eDSGreaterThan:
			outString = "eDSGreaterThan";
			break;
			
		case eDSLessEqual:
			outString = "eDSLessEqual";
			break;
			
		case eDSGreaterEqual:
			outString = "eDSGreaterEqual";
			break;
			
		case eDSWildCardPattern:
			outString = "eDSWildCardPattern";
			break;
			
		case eDSRegularExpression:
			outString = "eDSRegularExpression";
			break;
			
		case eDSCompoundExpression:
			outString = "eDSCompoundExpression";
			break;
			
		case eDSiExact:
			outString = "eDSiExact";
			break;
			
		case eDSiStartsWith:
			outString = "eDSiStartsWith";
			break;
			
		case eDSiEndsWith:
			outString = "eDSiEndsWith";
			break;
			
		case eDSiContains:
			outString = "eDSiContains";
			break;
			
		case eDSiLessThan:
			outString = "eDSiLessThan";
			break;
			
		case eDSiGreaterThan:
			outString = "eDSiGreaterThan";
			break;
			
		case eDSiLessEqual:
			outString = "eDSiLessEqual";
			break;
			
		case eDSiGreaterEqual:
			outString = "eDSiGreaterEqual";
			break;
			
		case eDSiWildCardPattern:
			outString = "eDSiWildCardPattern";
			break;
			
		case eDSiRegularExpression:
			outString = "eDSiRegularExpression";
			break;
			
		case eDSiCompoundExpression:
			outString = "eDSiCompoundExpression";
			break;
			
		case eDSLocalNodeNames:
			outString = "eDSLocalNodeNames";
			break;
			
		case eDSAuthenticationSearchNodeName:
			outString = "eDSAuthenticationSearchNodeName";
			break;
			
		case eDSConfigNodeName:
			outString = "eDSConfigNodeName";
			break;
			
		case eDSLocalHostedNodes:
			outString = "eDSLocalHostedNodes";
			break;
			
		case eDSContactsSearchNodeName:
			outString = "eDSContactsSearchNodeName";
			break;
			
		case eDSNetworkSearchNodeName:
			outString = "eDSNetworkSearchNodeName";
			break;
			
		case eDSDefaultNetworkNodes:
			outString = "eDSDefaultNetworkNodes";
			break;

		default:
			outString = "Pattern Match Unknown";
	}
	
	return(outString);
}