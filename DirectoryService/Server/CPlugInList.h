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
 * @header CPlugInList
 */

#ifndef __CPlugInList_h__
#define __CPlugInList_h__	1

#include <CoreFoundation/CFPlugIn.h>

#include "PrivateTypes.h"
#include "DSMutexSemaphore.h"
#include "DSEventSemaphore.h"
#include "PluginData.h"

class	CServerPlugin;

const sInt32	kPlugInListNoErr			= 0;
const sInt32	kInvalidPlugInName			= -10128;
const sInt32	kMaxPlugInsLoaded			= -10129;
const sInt32	kPlugInFound				= -10130;
const sInt32	kPlugInNotFound				= -10131;

// Typedefs --------------------------------------------------------------------

class CPlugInList {
public:
typedef struct sTableData
{
   	const char			*fName;
   	const char			*fVersion;
    const char			*fConfigAvail;
    const char			*fConfigFile;
	CServerPlugin		*fPluginPtr;
	CFPlugInRef			fPluginRef;
	CFUUIDRef			fCFuuidFactory;
	uInt32				fULVers;
	FourCharCode		fKey;
	uInt32				fState;
} sTableData;

enum {
	kMaxPlugIns	= 128
};

public:
				CPlugInList			( void );
	virtual	   ~CPlugInList			( void );

	sInt32	   	AddPlugIn			(	const char 		*inName,
										const char 		*inVersion,
										const char 		*inConfigAvail,
										const char 		*inConfigFile,
										FourCharCode	 inKey,
										CServerPlugin	*inPlugin,
										CFPlugInRef 	 inPluginRef = NULL,
										CFUUIDRef		 inCFuuidFactory = NULL,
										uInt32			 inULVers = 0 );
										
	sInt32		DeletePlugIn		( const char *inName );

	void		LoadPlugin			( uInt32 tableIndex );
	void		InitPlugIns			( void );

	sInt32	 	IsPresent			( const char *inName );

	sInt32		GetState			( const char *inName, uInt32 *outState );
	sInt32		SetState			( const char *inName, const uInt32 inState );

	uInt32		GetPlugInCount		( void );
	uInt32		GetActiveCount		( void );

	sTableData*	GetPlugInInfo		( uInt32 inIndex );

CServerPlugin*	Next				( uInt32 *inIndex );

CServerPlugin* 	GetPlugInPtr		( const char *inName, bool loadIfNeeded = true );
CServerPlugin* 	GetPlugInPtr		( const uInt32 inKey, bool loadIfNeeded = true );

private:
	uInt32				fPICount;
	DSMutexSemaphore		fMutex;
	sTableData			fTable[ kMaxPlugIns ];
	DSEventSemaphore   	fWaitToInit;
};

#endif
