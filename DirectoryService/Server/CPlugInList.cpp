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

#include "CPlugInList.h"
#include "CServerPlugin.h"
#include "CLauncher.h"
#include "DSUtils.h"
#include "PrivateTypes.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

//Use of the table entries directly will need a Mutex if the order can change
//but likely will continue only to add on to the end of the table and
//not delete table entries so it is currently a non issue

// ---------------------------------------------------------------------------
//	* CPlugInList ()
//
// ---------------------------------------------------------------------------

CPlugInList::CPlugInList ( void )
{
	fPICount	= 0;

	::memset( fTable, 0, sizeof( fTable ) );

} // CPlugInList


// ---------------------------------------------------------------------------
//	* ~CPlugInList ()
//
// ---------------------------------------------------------------------------

CPlugInList::~CPlugInList ( void )
{
} // ~CPlugInList


// ---------------------------------------------------------------------------
//	* AddPlugIn ()
//
// ---------------------------------------------------------------------------

sInt32 CPlugInList::AddPlugIn ( const char		*inName,
								const char		*inVersion,
								const char		*inConfigAvail,
								const char		*inConfigFile,
								FourCharCode	 inKey,
								CServerPlugin	*inPluginPtr )
{
	sInt32		siResult	= kMaxPlugInsLoaded;
	uInt32		uiCntr		= 0;

	fMutex.Wait();

	try
	{
		if ( inName == nil )
		{
			return( kInvalidPlugInName );
		}

		// Go down tree to find parent node of new insertion node
		while ( uiCntr < kMaxPlugIns )
		{
			if ( fTable[ uiCntr ].fName == nil )
			{
				fTable[ uiCntr ].fName			= inName;
				fTable[ uiCntr ].fVersion		= inVersion;
				fTable[ uiCntr ].fConfigAvail	= inConfigAvail;
				fTable[ uiCntr ].fConfigFile	= inConfigFile;
				fTable[ uiCntr ].fPluginPtr		= inPluginPtr;
				fTable[ uiCntr ].fKey			= inKey;
                fTable[ uiCntr ].fState			= kInactive | kUninitalized;

				fPICount++;

				siResult = kPlugInListNoErr;
				break;
			}
			uiCntr++;
		}
	}

	catch( sInt32 err )
	{
		siResult = err;
	}

	fMutex.Signal();

	return( siResult );

} // AddPlugIn


// ---------------------------------------------------------------------------
//	* DeletePlugIn ()
//
// ---------------------------------------------------------------------------

sInt32 CPlugInList::DeletePlugIn ( const char *inName )
{
	sInt32		siResult	= kPlugInNotFound;
	uInt32		uiCntr		= 0;

	fMutex.Wait();

	try
	{
		if ( inName == nil )
		{
			return( kInvalidPlugInName );
		}

		// Go down tree to find parent node of new insertion node
		while ( uiCntr < kMaxPlugIns )
		{
			if ( fTable[ uiCntr ].fName != nil )
			{
				if ( ::strcmp( fTable[ uiCntr ].fName, inName ) == 0 )
				{
					//free( fTable[ uiCntr ].fName );
					fTable[ uiCntr ].fName			= nil;
					fTable[ uiCntr ].fVersion		= nil;
					fTable[ uiCntr ].fConfigAvail	= nil;
					fTable[ uiCntr ].fConfigFile	= nil;
					fTable[ uiCntr ].fPluginPtr		= nil;
					fTable[ uiCntr ].fKey			= 0;
					fTable[ uiCntr ].fState			= kUnknownState;

					fPICount--;

					siResult = kPlugInListNoErr;

					break;
				}
			}
			uiCntr++;
		}
	}

	catch( sInt32 err )
	{
		siResult = err;
	}

	fMutex.Signal();

	return( siResult );

} // DeletePlugIn


// ---------------------------------------------------------------------------
//	* InitPlugIns ()
//
// ---------------------------------------------------------------------------

void CPlugInList::InitPlugIns ( void )
{
	uInt32		uiCntr		= 0;

	fMutex.Wait();

	try
	{
		// Go down tree to find parent node of new insertion node
		while ( uiCntr < kMaxPlugIns )
		{
			if ( (fTable[ uiCntr ].fName != nil) && (fTable[ uiCntr ].fPluginPtr != nil) )
			{
				CLauncher *cpLaunch = new CLauncher( (CServerPlugin *)fTable[ uiCntr ].fPluginPtr );
				if ( cpLaunch != nil )
				{
					//this call could throw
					cpLaunch->StartThread();
				}
			}
			uiCntr++;
		}
	}

	catch( sInt32 err )
	{
	}

	fMutex.Signal();

} // InitPlugIns


// ---------------------------------------------------------------------------
//	* IsPresent ()
//
// ---------------------------------------------------------------------------

sInt32 CPlugInList::IsPresent ( const char *inName )
{
	sInt32		siResult	= kPlugInNotFound;
	uInt32		uiCntr		= 0;

	fMutex.Wait();

	try
	{
		if ( inName == nil )
		{
			return( kInvalidPlugInName );
		}

		// Go down tree to find parent node of new insertion node
		while ( uiCntr < kMaxPlugIns )
		{
			if ( fTable[ uiCntr ].fName != nil )
			{
				if ( ::strcmp( fTable[ uiCntr ].fName, inName ) == 0 )
				{
					siResult = kPlugInFound;

					break;
				}
			}
			uiCntr++;
		}
	}

	catch( sInt32 err )
	{
		siResult = err;
	}

	fMutex.Signal();

	return( siResult );

} // IsPresent


// ---------------------------------------------------------------------------
//	* SetState ()
//
// ---------------------------------------------------------------------------

sInt32 CPlugInList::SetState ( const char *inName, const uInt32 inState )
{
	sInt32		siResult	= kPlugInNotFound;
	uInt32		uiCntr		= 0;

	fMutex.Wait();

	try
	{
		if ( inName == nil )
		{
			return( kInvalidPlugInName );
		}

		// Go down tree to find parent node of new insertion node
		while ( uiCntr < kMaxPlugIns )
		{
			if ( fTable[ uiCntr ].fName != nil )
			{
				if ( ::strcmp( fTable[ uiCntr ].fName, inName ) == 0 )
				{
					fTable[ uiCntr ].fState = inState;

					siResult = kPlugInListNoErr;

					break;
				}
			}
			uiCntr++;
		}
	}

	catch( sInt32 err )
	{
		siResult = err;
	}

	fMutex.Signal();

	return( siResult );

} // SetState


// ---------------------------------------------------------------------------
//	* GetState ()
//
// ---------------------------------------------------------------------------

sInt32 CPlugInList::GetState ( const char *inName, uInt32 *outState )
{
	sInt32		siResult	= kPlugInNotFound;
	uInt32		uiCntr		= 0;

	fMutex.Wait();

	try
	{
		if ( inName == nil )
		{
			return( kInvalidPlugInName );
		}

		// Go down tree to find parent node of new insertion node
		while ( uiCntr < kMaxPlugIns )
		{
			if ( fTable[ uiCntr ].fName != nil )
			{
				if ( ::strcmp( fTable[ uiCntr ].fName, inName ) == 0 )
				{
					*outState = fTable[ uiCntr ].fState;

					siResult = kPlugInListNoErr;

					break;
				}
			}
			uiCntr++;
		}
	}

	catch( sInt32 err )
	{
		siResult = err;
	}

	fMutex.Signal();

	return( siResult );

} // GetState


// ---------------------------------------------------------------------------
//	* GetPlugInCount ()
//
// ---------------------------------------------------------------------------

uInt32 CPlugInList::GetPlugInCount ( void )
{
	return( fPICount );
} // GetPlugInCount



// ---------------------------------------------------------------------------
//	* GetActiveCount ()
//
// ---------------------------------------------------------------------------

uInt32 CPlugInList::GetActiveCount ( void )
{
	uInt32		siResult	= 0;
	uInt32		uiCntr		= 0;

	fMutex.Wait();

	try
	{
		// Go down tree to find parent node of new insertion node
		while ( uiCntr < kMaxPlugIns )
		{
			if ( fTable[ uiCntr ].fName == nil )
			{
				if ( fTable[ uiCntr ].fState & kActive )
				{
					siResult++;
				}
			}
			uiCntr++;
		}
	}

	catch( sInt32 err )
	{
		siResult = 0;
	}

	fMutex.Signal();

	return( siResult );

} // GetActiveCount


// ---------------------------------------------------------------------------
//	* GetPlugInPtr ()
//
// ---------------------------------------------------------------------------

CServerPlugin* CPlugInList::GetPlugInPtr ( const char *inName )
{
	CServerPlugin	   *pResult		= nil;
	uInt32				uiCntr		= 0;

	fMutex.Wait();

	try
	{
		if ( inName == nil )
		{
			return( nil );
		}

		// Go down tree to find parent node of new insertion node
		while ( uiCntr < kMaxPlugIns )
		{
			if ( (fTable[ uiCntr ].fName != nil) && (fTable[ uiCntr ].fPluginPtr != nil) )
			{
				if ( ::strcmp( fTable[ uiCntr ].fName, inName ) == 0 )
				{
					pResult = fTable[ uiCntr ].fPluginPtr;

					break;
				}
			}
			uiCntr++;
		}
	}

	catch( sInt32 err )
	{
		pResult = nil;
	}

	fMutex.Signal();

	return( pResult );

} // GetPlugInPtr


// ---------------------------------------------------------------------------
//	* GetPlugInPtr ()
//
// ---------------------------------------------------------------------------

CServerPlugin* CPlugInList::GetPlugInPtr ( const uInt32 inKey )
{
	CServerPlugin	   *pResult		= nil;
	uInt32				uiCntr		= 0;

	fMutex.Wait();

	try
	{
		// Go down tree to find parent node of new insertion node
		while ( uiCntr < kMaxPlugIns )
		{
			if ( (fTable[ uiCntr ].fName != nil) && (fTable[ uiCntr ].fPluginPtr != nil) )
			{
				if ( fTable[ uiCntr ].fKey == inKey )
				{
					pResult = fTable[ uiCntr ].fPluginPtr;

					break;
				}
			}
			uiCntr++;
		}
	}

	catch( sInt32 err )
	{
		pResult = nil;
	}

	fMutex.Signal();

	return( pResult );

} // GetPlugInPtr



// ---------------------------------------------------------------------------
//	* Next ()
//
// ---------------------------------------------------------------------------

CServerPlugin* CPlugInList::Next ( uInt32 *inIndex )
{
	CServerPlugin	   *pResult		= nil;
	uInt32				uiCntr		= *inIndex;

	fMutex.Wait();

	try
	{
		// Go down tree to find parent node of new insertion node
		while ( uiCntr < kMaxPlugIns )
		{
			if ( (fTable[ uiCntr ].fName != nil) && (fTable[ uiCntr ].fPluginPtr != nil) )
			{
				pResult = fTable[ uiCntr ].fPluginPtr;
				uiCntr++;
				break;
			}
			uiCntr++;
		}
	}

	catch( sInt32 err )
	{
		pResult = nil;
	}

	*inIndex = uiCntr;

	fMutex.Signal();

	return( pResult );

} // GetPlugInPtr




// ---------------------------------------------------------------------------
//	* GetPlugInInfo ()
//
// ---------------------------------------------------------------------------

CPlugInList::sTableData* CPlugInList::GetPlugInInfo ( uInt32 inIndex )
{
	sTableData		   *pResult		= nil;

	fMutex.Wait();

	try
	{
		// Go down tree to find parent node of new insertion node
		if ( inIndex < kMaxPlugIns )
		{
			pResult = &fTable[ inIndex ];
		}
	}

	catch( sInt32 err )
	{
		pResult = nil;
	}

	fMutex.Signal();

	return( pResult );

} // GetPlugInInfo




