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
 * @header CServerPlugin
 * Interface for server-side plug-in wrapper class.
 */

#ifndef __CServerPlugin_h__
#define __CServerPlugin_h__	1

#include <CoreFoundation/CFPlugIn.h>
#include <CoreFoundation/CFString.h>

#include "ServerModule.h"
#include "PrivateTypes.h"

const	uInt32		kMaxPlugInAttributeStrLen	= 256;

//-----------------------------------------------------------------------------
//	* CServerPlugin Class Definition
//
//		- CServerPlugin is used to wrap CFPlugin type com.apple.DSServer.Module
//			instances for (hopefully) transparent use by C++.
//-----------------------------------------------------------------------------

class CServerPlugin {
public:
	static	sInt32	ProcessURL				( CFURLRef urlPlugin );
	static	sInt32	ProcessConfigurePlugin	( void );

public:
	/**** Instance methods. ****/
	// ctor and dtor.
				CServerPlugin	( void );
				CServerPlugin	( CFPlugInRef inThis, CFUUIDRef inFactoryID, FourCharCode inSig, uInt32 inVers, char *inName );
	virtual	   ~CServerPlugin	( void );

	// New methods.
		// Pass-thru functions to CFPlugin function table.
	virtual sInt32	Validate		( const char *inVersionStr, const uInt32 inSignature );
	virtual sInt32	Initialize		( void );
	virtual sInt32	PeriodicTask	( void );
	virtual sInt32	ProcessRequest	( void *inData );
	virtual sInt32	SetPluginState	( const uInt32 inState );

	char*			GetPluginName	( void );
	FourCharCode	GetSignature	( void );
	static sInt32	_RegisterNode	( const uInt32, tDataList *, eDirNodeType );
    
protected:
	FourCharCode	fPlugInSignature;
	char		   *fPlugInName;

private:
	// Instance data
	DSServerPlugin::ModuleFtbl	*mInstance;

	CFPlugInRef		fPlugInRef;
	uInt32			fPlugInVers;
};

#endif	// __CServerPlugin_h__
