/*
 * Copyright (c) 2000 Apple Computer, Inc. All rights reserved.
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

// AppleCDDAFileSystemUtils.h created by CJS on Sun 14-May-2000

#ifndef __APPLE_CDDA_FS_UTILS_H__
#define __APPLE_CDDA_FS_UTILS_H__

#ifndef __APPLE_CDDA_FS_DEFINES_H__
#include "AppleCDDAFileSystemDefines.h"
#endif

#include <sys/attr.h>

#ifdef __cplusplus
extern "C" {
#endif


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	Function Prototypes - From AppleCDDAFileSystemUtils.c
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

int				InsertCDDANode 				( AppleCDDANodePtr newNodePtr,
											  struct vnode * parentVNodePtr,
											  struct proc * theProcPtr );
int				CreateNewCDDANode 			( struct mount * mountPtr,
											  UInt32 nodeID,
											  struct proc * procPtr,
											  struct vnode ** vNodeHandle );
int				DisposeCDDANode 			( struct vnode * vNodePtr,
											  struct proc * theProcPtr );
int				CreateNewCDDAFile 			( struct mount * mountPtr,
											  UInt32 nodeID,
											  AppleCDDANodeInfoPtr nodeInfoPtr,
											  struct proc * procPtr,
											  struct vnode ** vNodeHandle );
int				CreateNewXMLFile 			( struct mount * mountPtr,
											  struct proc * procPtr,
											  UInt32 xmlFileSize,
											  UInt8 * xmlData,
											  struct vnode ** vNodeHandle );
int				CreateNewCDDADirectory 		( struct mount * mountPtr,
											  const char * name,
											  UInt32 nodeID,
											  struct proc * procPtr,
											  struct vnode ** vNodeHandle );
boolean_t		IsAudioTrack 				( const SubQTOCInfoPtr trackDescriptorPtr );
UInt32			CalculateSize 				( const QTOCDataFormat10Ptr TOCDataPtr,
											  UInt32 trackDescriptorOffset,
											  UInt32 currentA2Offset );
SInt32			ParseTOC 					( struct mount * mountPtr,
											  UInt32 numTracks,
											  UInt32 xmlFileSize,
											  UInt8 * xmlData,
											  struct proc * theProc );
int				GetTrackNumberFromName 		( const char * name,
											  UInt32 * trackNumber );

int				CalculateAttributeBlockSize	( struct attrlist * attrlist );
void			PackAttributesBlock			( struct attrlist * attrListPtr,
											  struct vnode * vNodePtr,
											  void ** attrbufHandle,
											  void ** varbufHandle );


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	Function Prototypes - From AppleCDDAFileSystemUtilities.cpp
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ


QTOCDataFormat10Ptr		CreateBufferFromIORegistry 	( struct mount * mountPtr );
void					DisposeBufferFromIORegistry	( QTOCDataFormat10Ptr TOCDataPtr );


#ifdef __cplusplus
}
#endif

#endif // __APPLE_CDDA_FS_UTILS_H__
