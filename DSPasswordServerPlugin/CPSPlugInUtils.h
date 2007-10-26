/*
 * Copyright (c) 2006 Apple Computer, Inc. All rights reserved.
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

#ifndef __CPSPlugInUtils_h__
#define __CPSPlugInUtils_h__	1

#include <DirectoryService/DirServices.h>
#include <DirectoryService/DirServicesUtils.h>
#include <DirectoryService/DirServicesConst.h>

#include <DirectoryServiceCore/CBuff.h>
#include <DirectoryServiceCore/CDataBuff.h>
#include <DirectoryServiceCore/CAttributeList.h>
#include <DirectoryServiceCore/SharedConsts.h>
#include <DirectoryServiceCore/PluginData.h>
#include <DirectoryServiceCore/CDSServerModule.h>
#include <DirectoryServiceCore/CSharedData.h>

#include <CoreFoundation/CoreFoundation.h>

#include <PasswordServer/AuthFile.h>
#include <PasswordServer/PSUtilitiesDefs.h>
#include "CPSPluginDefines.h"

#define kMoreHeaderReserveSize				(CAST_BLOCK * 5)

typedef struct BonjourServiceCBData {
	CFRunLoopRef runLoop;
	bool checking;
	SInt32 errorCode;
} BonjourServiceCBData;

typedef struct BonjourBrowserCBData {
	CFRunLoopRef runLoop;
	bool checking;
	SInt32 errorCode;
	CFMutableArrayRef serverArray;
} BonjourBrowserCBData;

__BEGIN_DECLS
SInt32 PWSErrToDirServiceError( PWServerError inError );
SInt32 PolicyErrToDirServiceError( int inPolicyError );
SInt32 SASLErrToDirServiceError( int inSASLError );

bool CheckServerVersionMin( int serverVers[], int reqMajor, int reqMinor, int reqBugFix, int reqTiny );

bool RSAPublicKeysEqual( const char *rsaKeyStr1, const char *rsaKeyStr2 );
long GetServerListFromBonjourForKeyHash( const char *inKeyHash, CFRunLoopRef inRunLoop, CFMutableArrayRef *outServerList );
void BrowseForPasswordServers( CFRunLoopRef inRunLoop );

__END_DECLS

#endif
