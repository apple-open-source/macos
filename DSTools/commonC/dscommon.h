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

/*!
 * @header dscommon
 * Record access methods via the DirectoryService API.
 */


#ifndef _dscommon_h_
#define _dscommon_h_	1

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <signal.h>
#include <curses.h>
#include <sys/types.h>
#include <sys/param.h>
#include <unistd.h>
#include <termios.h>
#include <pwd.h>

#include <DirectoryService/DirectoryService.h>

#ifdef __cplusplus
extern "C" {
#endif


#pragma mark -
#pragma mark Text Input Routines

void				intcatch					(   int dontcare);

char*				read_passphrase				(   const char *prompt,
													int from_stdin);

#pragma mark -
#pragma mark DS API Support Routines

bool				singleAttributeValueMissing	(   tDirReference inDSRef,
													tDirNodeReference inDSNodeRef,
													char* inRecordType,
													char* inAttributeType,
													char* inAttributeValue,
													SInt32 *outResult,
													bool inVerbose);
char*				createNewuid				(   tDirReference inDSRef,
													tDirNodeReference inDSNodeRef,
													bool inVerbose);
char*				createNewgid				(   tDirReference inDSRef,
													tDirNodeReference inDSNodeRef,
													bool inVerbose);
char*				createNewGUID				(   bool inVerbose);
SInt32				addRecordParameter			(   tDirReference inDSRef,
													tDirNodeReference inDSNodeRef,
													tRecordReference inRecordRef,
													char* inAttrType,
													char* inAttrName,
													bool inVerbose);
tRecordReference	createAndOpenRecord			(   tDirReference inDSRef,
													tDirNodeReference inDSNodeRef,
													char* inRecordName,
													char* inRecordType,
													SInt32 *outResult,
													bool inVerbose);
SInt32				getAndOutputRecord			(   tDirReference inDSRef,
													tDirNodeReference inDSNodeRef,
													char* inRecordName,
													char* inRecordType,
													bool inVerbose);
tDirNodeReference   getNodeRef					(   tDirReference inDSRef,
													char* inNodename,
													char* inUsername,
													char* inPassword,
													bool inVerbose);
char*				getSingleRecordAttribute	(   tDirReference inDSRef,
													tDirNodeReference inDSNodeRef,
													char* inRecordName,
													char* inRecordType,
													char* inAttributeType,
													SInt32 *outResult,
													bool inVerbose);
tRecordReference	openRecord					(   tDirReference inDSRef,
													tDirNodeReference inDSNodeRef,
													char* inRecordName,
													char* inRecordType,
													SInt32 *outResult,
													bool inVerbose);
bool				UserIsMemberOfGroup			(   tDirReference inDSRef,
													tDirNodeReference inDSNodeRef,
													const char* shortName,
													const char* groupName );

#ifdef __cplusplus
}
#endif

#endif	// _dscommon_h_
