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
 * @header CPlugInList
 */

#ifndef __CPlugInList_h__
#define __CPlugInList_h__	1

#include "PrivateTypes.h"
#include "DSMutexSemaphore.h"
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
   	const char		   *fName;
   	const char		   *fVersion;
    const char		   *fConfigAvail;
    const char		   *fConfigFile;
	CServerPlugin	   *fPluginPtr;
	FourCharCode		fKey;
	uInt32		fState;
} sTableData;

enum {
	kMaxPlugIns	= 128
};

public:
				CPlugInList			( void );
	virtual	   ~CPlugInList			( void );

	sInt32	   	AddPlugIn			( const char *inName, const char *inVersion, const char *inConfigAvail, const char *inConfigFile, FourCharCode inKey, CServerPlugin *inPlugin );
	sInt32		DeletePlugIn		( const char *inName );

	void		InitPlugIns			( void );

	sInt32	 	IsPresent			( const char *inName );

	sInt32		GetState			( const char *inName, uInt32 *outState );
	sInt32		SetState			( const char *inName, const uInt32 inState );

	uInt32		GetPlugInCount		( void );
	uInt32		GetActiveCount		( void );

	sTableData*	GetPlugInInfo		( uInt32 inIndex );

CServerPlugin*	Next				( uInt32 *inIndex );

CServerPlugin* 	GetPlugInPtr		( const char *inName );
CServerPlugin* 	GetPlugInPtr		( const uInt32 inKey );

private:
	uInt32				fPICount;
	DSMutexSemaphore		fMutex;
	sTableData			fTable[ kMaxPlugIns ];
};

#endif
