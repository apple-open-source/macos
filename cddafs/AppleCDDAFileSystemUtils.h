/*
 * Copyright (c) 2000-2008 Apple Inc. All rights reserved.
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

// AppleCDDAFileSystemUtils.h created by CJS on Sun 14-May-2000

#ifndef __APPLE_CDDA_FS_UTILS_H__
#define __APPLE_CDDA_FS_UTILS_H__

#ifndef __APPLE_CDDA_FS_DEFINES_H__
#include "AppleCDDAFileSystemDefines.h"
#endif

#include <sys/vnode.h>
#include <sys/attr.h>

#ifdef __cplusplus
extern "C" {
#endif


//-----------------------------------------------------------------------------
//	Function Prototypes - From AppleCDDAFileSystemUtils.c
//-----------------------------------------------------------------------------

int				InsertCDDANode 				( AppleCDDANodePtr newNodePtr,
											  vnode_t parentVNodePtr,
											  struct proc * theProcPtr );
errno_t			CreateNewCDDANode 			( mount_t mountPtr,
											  UInt32 nodeID,
											  enum vtype vNodeType,
											  vnode_t parentVNodePtr,
											  struct componentname * compNamePtr,
											  vnode_t * vNodeHandle );
int				DisposeCDDANode 			( vnode_t vNodePtr );
errno_t			CreateNewCDDAFile 			( mount_t mountPtr,
											  UInt32 nodeID,
											  AppleCDDANodeInfoPtr nodeInfoPtr,
											  vnode_t parentVNodePtr,
											  struct componentname * compNamePtr,
											  vnode_t * vNodeHandle );
errno_t			CreateNewXMLFile 			( mount_t mountPtr,
											  UInt32 xmlFileSize,
											  UInt8 * xmlData,
											  vnode_t parentVNodePtr,
											  struct componentname * compNamePtr,
											  vnode_t * vNodeHandle );
errno_t			CreateNewCDDADirectory 		( mount_t mountPtr,
											  UInt32 nodeID,
											  vnode_t * vNodeHandle );
boolean_t		IsAudioTrack 				( const SubQTOCInfoPtr trackDescriptorPtr );
UInt32			CalculateSize 				( const QTOCDataFormat10Ptr TOCDataPtr,
											  UInt32 trackDescriptorOffset,
											  UInt32 currentA2Offset );
SInt32			ParseTOC 					( mount_t mountPtr,
											  UInt32 numTracks );
int				GetTrackNumberFromName 		( const char * name,
											  UInt32 * trackNumber );

int				CalculateAttributeBlockSize	( struct attrlist * attrlist );
void			PackAttributesBlock			( struct attrlist * attrListPtr,
											  vnode_t vNodePtr,
											  void ** attrbufHandle,
											  void ** varbufHandle );


//-----------------------------------------------------------------------------
//	Function Prototypes - From AppleCDDAFileSystemUtilities.cpp
//-----------------------------------------------------------------------------


QTOCDataFormat10Ptr		CreateBufferFromIORegistry 	( mount_t mountPtr );
void					DisposeBufferFromIORegistry	( QTOCDataFormat10Ptr TOCDataPtr );


#ifdef __cplusplus
}
#endif

#endif // __APPLE_CDDA_FS_UTILS_H__
