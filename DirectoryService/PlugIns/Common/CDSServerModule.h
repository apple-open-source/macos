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
 * @header CDSServerModule
 * Abstract, Singleton, Factory base class for server plugin.
 */

#ifndef __CDSServerModule_H__
#define __CDSServerModule_H__	1

#include "PluginData.h"
#include "PrivateTypes.h"

// ============================================================================
//	* CDSServerModule Abstract Base Class
// ============================================================================

class CDSServerModule
{
public:
	/**** Class (static) methods. ****/
	static CDSServerModule	*Instance	( void )
				{	if (!sInstance) sInstance = sCreator ();
					return sInstance; }

public:
	virtual sInt32	Validate			( const char *inVersionStr, const uInt32 inSignature );
	virtual sInt32	Initialize			( void );
	virtual sInt32	Configure			( void );
	virtual sInt32	SetPluginState		( const uInt32 inState );
	virtual sInt32	PeriodicTask		( void );
	virtual sInt32	ProcessRequest		( void *inData );
	virtual sInt32	Shutdown			( void );

protected:
	/**** Instance methods accessible only to class and subclasses. ****/
	// ctor and dtor are protected so only static methods can create
	// and destroy modules.
						CDSServerModule		( void );
	virtual			   ~CDSServerModule		( void );

protected:
	/**** Private typedefs. ****/
	typedef CDSServerModule			*(*tCreator) (void);

	/**** Class globals. ****/
	static CDSServerModule			*sInstance;
	static tCreator					sCreator;

	/**** Instance data. ****/
};

#endif	/* _CDSServerModule_H_ */
