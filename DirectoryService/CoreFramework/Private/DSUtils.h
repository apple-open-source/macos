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
 * @header DSUtils
 */

#ifndef __DSUtils_h__
#define __DSUtils_h__	1

#include "DirServicesTypes.h"
#include "PrivateTypes.h"

#ifdef __cplusplus
extern "C" {
#endif

tDataBufferPtr	dsDataBufferAllocatePriv		( unsigned long inBufferSize );
tDirStatus		dsDataBufferDeallocatePriv		( tDataBufferPtr inDataBufferPtr );
tDataListPtr	dsDataListAllocatePriv			( void );
tDirStatus		dsDataListDeallocatePriv		( tDataListPtr inDataList );
char*			dsGetPathFromListPriv			( tDataListPtr inDataList, const char *inDelimiter );
tDataListPtr	dsBuildFromPathPriv				( const char *inPathCString, const char *inPathSeparatorCString );
tDataListPtr	dsBuildListFromStringsPriv		( const char *in1stCString, ... );
tDirStatus		dsAppendStringToListAllocPriv	( tDataList *inOutDataList, const char *inCString );
tDataNodePtr	dsAllocListNodeFromStringPriv	( const char *inString );
tDataNodePtr	dsGetThisNodePriv				( tDataNode *inFirsNode, const unsigned long inIndex );
tDataNodePtr	dsGetLastNodePriv				( tDataNode *inFirsNode );
tDirStatus		dsAppendStringToListPriv		( tDataList *inDataList, const char *inCString );
tDirStatus		dsDeleteLastNodePriv			( tDataList *inDataList );
unsigned long	dsDataListGetNodeCountPriv		( tDataList *inDataList );
unsigned long	dsGetDataLengthPriv				( tDataList *inDataList );
tDirStatus		dsDataListGetNodePriv			( tDataList *inDataList, unsigned long inNodeIndex, tDataNodePtr *outDataListNode );
char*			dsDataListGetNodeStringPriv		( tDataListPtr inDataList, unsigned long inNodeIndex );
tDirStatus		dsDataListGetNodeAllocPriv		( const tDataList *inDataList, const unsigned long inNodeIndex, tDataNode **outDataNode );
tDataListPtr	dsAuthBufferGetDataListAllocPriv	( tDataBufferPtr inAuthBuff );
tDirStatus		dsAuthBufferGetDataListPriv		( tDataBufferPtr inAuthBuff, tDataListPtr inOutDataList );

#ifdef __cplusplus
}
#endif

#endif
