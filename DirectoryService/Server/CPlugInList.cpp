/*
 * Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
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

/*!
 * @header CPlugInList
 */

#include "CPlugInList.h"
#include "CServerPlugin.h"
#include "CLauncher.h"
#include "DSUtils.h"
#include "PrivateTypes.h"
#include "SharedConsts.h"
#include "CPluginConfig.h"
#include "CLog.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>

extern CFRunLoopRef			gServerRunLoop;
extern CPluginConfig	   *gPluginConfig;
extern DSMutexSemaphore	   *gLazyPluginLoadingLock;
extern CPlugInList		   *gPlugins;

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
								CServerPlugin	*inPluginPtr,
								CFPlugInRef		 inPluginRef,
								CFUUIDRef		 inCFuuidFactory,
								uInt32			 inULVers )
{
	sInt32		siResult	= kMaxPlugInsLoaded;
	uInt32		tableIndex		= 0;

	fMutex.Wait();

	try
	{
		if ( inName == nil )
		{
			return( kInvalidPlugInName );
		}

		// Go down tree to find parent node of new insertion node
		while ( tableIndex < kMaxPlugIns )
		{
			if ( fTable[ tableIndex ].fName == nil )
			{
				fTable[ tableIndex ].fName			= inName;
				fTable[ tableIndex ].fVersion		= inVersion;
				fTable[ tableIndex ].fConfigAvail	= inConfigAvail;
				fTable[ tableIndex ].fConfigFile	= inConfigFile;
				fTable[ tableIndex ].fPluginPtr		= inPluginPtr;
				
				if ( inPluginRef )
				{
					fTable[ tableIndex ].fPluginRef		= inPluginRef;
					CFRetain( fTable[ tableIndex ].fPluginRef );
				}
				
				if ( inCFuuidFactory )
				{
					fTable[ tableIndex ].fCFuuidFactory	= inCFuuidFactory;
					CFRetain( fTable[ tableIndex ].fCFuuidFactory );
				}
				
				if ( inULVers )
					fTable[ tableIndex ].fULVers	= inULVers;
					
				fTable[ tableIndex ].fKey			= inKey;
				ePluginState		pluginState = gPluginConfig->GetPluginState( fTable[ tableIndex ].fName );
				
				fTable[ tableIndex ].fState = pluginState | kUninitialized;

				fPICount++;

				siResult = kPlugInListNoErr;
				break;
			}
			tableIndex++;
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
	uInt32		tableIndex		= 0;

	fMutex.Wait();

	try
	{
		if ( inName == nil )
		{
			return( kInvalidPlugInName );
		}

		// Go down tree to find parent node of new insertion node
		while ( tableIndex < kMaxPlugIns )
		{
			if ( fTable[ tableIndex ].fName != nil )
			{
				if ( ::strcmp( fTable[ tableIndex ].fName, inName ) == 0 )
				{
					//free( fTable[ tableIndex ].fName );
					fTable[ tableIndex ].fName			= nil;
					fTable[ tableIndex ].fVersion		= nil;
					fTable[ tableIndex ].fConfigAvail	= nil;
					fTable[ tableIndex ].fConfigFile	= nil;
					fTable[ tableIndex ].fPluginPtr		= nil;
					fTable[ tableIndex ].fKey			= 0;
					fTable[ tableIndex ].fState			= kUnknownState;
					
					if ( fTable[ tableIndex ].fPluginRef )
						CFRelease( fTable[ tableIndex ].fPluginRef );
					fTable[ tableIndex ].fPluginRef = nil;
					
					if ( fTable[ tableIndex ].fCFuuidFactory )
						CFRelease( fTable[ tableIndex ].fCFuuidFactory );
					fTable[ tableIndex ].fCFuuidFactory = nil;
					
					fTable[ tableIndex ].fULVers		= 0;
					
					fPICount--;

					siResult = kPlugInListNoErr;

					break;
				}
			}
			tableIndex++;
		}
	}

	catch( sInt32 err )
	{
		siResult = err;
	}

	fMutex.Signal();

	return( siResult );

} // DeletePlugIn

void CPlugInList::LoadPlugin( uInt32 tableIndex )
{
	bool			done		= false;
	sInt32			siResult 	= eDSNoErr;
	uInt32			uiCntr		= 0;
	uInt32			uiAttempts	= 100;
	uInt32			uiWaitTime 	= 1;
	sHeader			aHeader;
	ePluginState	pluginState	= kUnknownState;
	CServerPlugin  *ourPluginPtr= nil;
	
	gLazyPluginLoadingLock->Wait();
	
	try
	{
		fTable[ tableIndex ].fPluginPtr = new CServerPlugin( fTable[ tableIndex ].fPluginRef, fTable[ tableIndex ].fCFuuidFactory, fTable[ tableIndex ].fKey, fTable[ tableIndex ].fULVers, fTable[ tableIndex ].fName );
		
		ourPluginPtr = (CServerPlugin *)fTable[ tableIndex ].fPluginPtr;

		if ( ourPluginPtr == NULL ) throw( (sInt32)eMemoryError );
		
		ourPluginPtr->Validate( fTable[ tableIndex ].fVersion, fTable[ tableIndex ].fKey );
		
		if ( gPlugins != nil )
		{
			while ( !done )
			{
				uiCntr++;
	
				// Attempt to initialize it
				siResult = ourPluginPtr->Initialize();
				if ( ( siResult != eDSNoErr ) && ( uiCntr == 1 ) )
				{
					ERRORLOG3( kLogApplication, "Attempt #%l to initialize plug-in %s failed.\n  Will retry initialization at most 100 times every %l second.", uiCntr, ourPluginPtr->GetPluginName(), uiWaitTime );
				}
				
				if ( siResult == eDSNoErr )
				{
					DBGLOG2( kLogApplication, "Initialization of plug-in %s succeeded with #%l attempt.", ourPluginPtr->GetPluginName(), uiCntr );
	
					gPlugins->SetState( ourPluginPtr->GetPluginName(), kInitialized );
	
					//provide the CFRunLoop to the plugins that need it
					if (gServerRunLoop != NULL)
					{
						aHeader.fType			= kServerRunLoop;
						aHeader.fResult			= eDSNoErr;
						aHeader.fContextData	= (void *)gServerRunLoop;
						siResult = ourPluginPtr->ProcessRequest( (void*)&aHeader ); //don't handle return
					}
	
					pluginState = gPluginConfig->GetPluginState( ourPluginPtr->GetPluginName() );
					if ( pluginState == kInactive )
					{
						siResult = ourPluginPtr->SetPluginState( kInactive );
						if ( siResult == eDSNoErr )
						{
							SRVRLOG1( kLogApplication, "Plug-in %s state is now inactive.", ourPluginPtr->GetPluginName() );
					
							gPlugins->SetState( ourPluginPtr->GetPluginName(), kInactive );
						}
						else
						{
							ERRORLOG2( kLogApplication, "Unable to set %s plug-in state to inactive.  Received error %l.", ourPluginPtr->GetPluginName(), siResult );
						}
					}
					else
					{
						siResult = ourPluginPtr->SetPluginState( kActive );
						if ( siResult == eDSNoErr )
						{
							SRVRLOG1( kLogApplication, "Plug-in %s state is now active.", ourPluginPtr->GetPluginName() );
		
							gPlugins->SetState( ourPluginPtr->GetPluginName(), kActive );
						}
						else
						{
							ERRORLOG2( kLogApplication, "Unable to set %s plug-in state to active.  Received error %l.", ourPluginPtr->GetPluginName(), siResult );
						}
					}
					
					done = true;
				}
	
				if ( !done )
				{
					// We will try this 100 times before we bail
					if ( uiCntr == uiAttempts )
					{
						ERRORLOG2( kLogApplication, "%l attempts to initialize plug-in %s failed.\n  Setting plug-in state to inactive.", uiCntr, ourPluginPtr->GetPluginName() );
	
						gPlugins->SetState( ourPluginPtr->GetPluginName(), kInactive | kFailedToInit );
	
						siResult = ourPluginPtr->SetPluginState( kInactive );
	
						done = true;
					}
					else
					{
						fWaitToInit.Wait( uiWaitTime * kMilliSecsPerSec );
					}
				}
			}
		}

		SRVRLOG2( kLogApplication, "Plugin \"%s\", Version \"%s\", loaded on demand successfully.", fTable[ tableIndex ].fName, fTable[ tableIndex ].fVersion );
	}

	catch( sInt32 err )
	{
		SRVRLOG3( kLogApplication, "Plugin \"%s\", Version \"%s\", failed to load on demand (%d).", fTable[ tableIndex ].fName, fTable[ tableIndex ].fVersion, err );
	}
	
	gLazyPluginLoadingLock->Signal();
	
}


// ---------------------------------------------------------------------------
//	* InitPlugIns ()
//
// ---------------------------------------------------------------------------

void CPlugInList::InitPlugIns ( void )
{
	uInt32		tableIndex		= 0;

	fMutex.Wait();

	// Go down tree to find parent node of new insertion node
	while ( tableIndex < kMaxPlugIns )
	{
		if ( (fTable[ tableIndex ].fName != nil) && (fTable[ tableIndex ].fPluginPtr != nil) )
		{
			try
			{
				//this constructor could throw
				CLauncher *cpLaunch = new CLauncher( (CServerPlugin *)fTable[ tableIndex ].fPluginPtr );
				if ( cpLaunch != nil )
				{
					//this call could throw
					cpLaunch->StartThread();
				}
				DBGLOG2( kLogApplication, "Plugin \"%s\", Version \"%s\", activated successfully.", fTable[ tableIndex ].fName, fTable[ tableIndex ].fVersion );
			}
		
			catch( sInt32 err )
			{
				DBGLOG2( kLogApplication, "Plugin \"%s\", Version \"%s\", failed to launch initialization thread.", fTable[ tableIndex ].fName, fTable[ tableIndex ].fVersion );
			}
		}
		else if ( fTable[ tableIndex ].fName != nil )
		{
			// if this plugin is supposed to be active, we should mark it as such, it still should be uninitialized.
			ePluginState		pluginState = gPluginConfig->GetPluginState( fTable[ tableIndex ].fName );

			fTable[ tableIndex ].fState = pluginState | kUninitialized;
			DBGLOG2( kLogApplication, "Plugin \"%s\", Version \"%s\", referenced to be loaded on demand successfully.", fTable[ tableIndex ].fName, fTable[ tableIndex ].fVersion );
		}
		tableIndex++;
	}

	fMutex.Signal();

} // InitPlugIns


// ---------------------------------------------------------------------------
//	* IsPresent ()
//
// ---------------------------------------------------------------------------

sInt32 CPlugInList::IsPresent ( const char *inName )
{
	sInt32		siResult		= kPlugInNotFound;
	uInt32		tableIndex		= 0;

	fMutex.Wait();

	try
	{
		if ( inName == nil )
		{
			return( kInvalidPlugInName );
		}

		// Go down tree to find parent node of new insertion node
		while ( tableIndex < kMaxPlugIns )
		{
			if ( fTable[ tableIndex ].fName != nil )
			{
				if ( ::strcmp( fTable[ tableIndex ].fName, inName ) == 0 )
				{
					siResult = kPlugInFound;

					break;
				}
			}
			tableIndex++;
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
	sInt32		siResult		= kPlugInNotFound;
	uInt32		tableIndex		= 0;
	uInt32		curState		= kUnknownState;

	fMutex.Wait();

	try
	{
		if ( inName == nil )
		{
			return( kInvalidPlugInName );
		}

		// Go down tree to find parent node of new insertion node
		while ( tableIndex < kMaxPlugIns )
		{
			if ( fTable[ tableIndex ].fName != nil )
			{
				if ( ::strcmp( fTable[ tableIndex ].fName, inName ) == 0 )
				{
					curState = fTable[ tableIndex ].fState;
					
					if ( (inState & kActive) && fTable[ tableIndex ].fPluginPtr == NULL )
					{
						// This plugin has just been set to active but we haven't loaded it yet.
						LoadPlugin( tableIndex );
					}

					fTable[ tableIndex ].fState = inState;

					if ( !( curState & inState ) && ( fTable[ tableIndex ].fPluginPtr ) )
						fTable[ tableIndex ].fPluginPtr->SetPluginState(inState);
					
					siResult = kPlugInListNoErr;

					break;
				}
			}
			tableIndex++;
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
	sInt32		siResult		= kPlugInNotFound;
	uInt32		tableIndex		= 0;

	fMutex.Wait();

	try
	{
		if ( inName == nil )
		{
			return( kInvalidPlugInName );
		}

		// Go down tree to find parent node of new insertion node
		while ( tableIndex < kMaxPlugIns )
		{
			if ( fTable[ tableIndex ].fName != nil )
			{
				if ( ::strcmp( fTable[ tableIndex ].fName, inName ) == 0 )
				{
					*outState = fTable[ tableIndex ].fState;

					siResult = kPlugInListNoErr;

					break;
				}
			}
			tableIndex++;
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
	uInt32		siResult		= 0;
	uInt32		tableIndex		= 0;

	fMutex.Wait();

	try
	{
		// Go down tree to find parent node of new insertion node
		while ( tableIndex < kMaxPlugIns )
		{
			if ( fTable[ tableIndex ].fName == nil )
			{
				if ( fTable[ tableIndex ].fState & kActive )
				{
					siResult++;
				}
			}
			tableIndex++;
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

CServerPlugin* CPlugInList::GetPlugInPtr ( const char *inName, bool loadIfNeeded )
{
	CServerPlugin	   *pResult			= nil;
	uInt32				tableIndex		= 0;

	fMutex.Wait();

	try
	{
		if ( inName == nil )
		{
			return( nil );
		}

		// Go down tree to find parent node of new insertion node
		while ( tableIndex < kMaxPlugIns )
		{
			if ( (fTable[ tableIndex ].fName != nil) )
			{
				if ( ::strcmp( fTable[ tableIndex ].fName, inName ) == 0 )
				{
					// if someone specifically asks for a node, even if not active, we should load the plugin
					// this can be for configure nodes, otherwise inactive nodes cannot be configured
					if ( (fTable[ tableIndex ].fPluginPtr == NULL) && loadIfNeeded )
					{
						// This plugin hasn't been loaded it yet.  Load it if loadIfNeeded set
						LoadPlugin( tableIndex );
					}	

					pResult = fTable[ tableIndex ].fPluginPtr;

					break;
				}
			}
			tableIndex++;
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

CServerPlugin* CPlugInList::GetPlugInPtr ( const uInt32 inKey, bool loadIfNeeded )
{
	CServerPlugin	   *pResult			= nil;
	uInt32				tableIndex		= 0;

	fMutex.Wait();

	try
	{
		// Go down tree to find parent node of new insertion node
		while ( tableIndex < kMaxPlugIns )
		{
			if ( (fTable[ tableIndex ].fName != nil) )
			{
				if ( fTable[ tableIndex ].fKey == inKey )
				{
					if (	fTable[ tableIndex ].fPluginPtr == NULL
						&&	(gPluginConfig->GetPluginState(fTable[ tableIndex ].fName) & kActive)
						&&	loadIfNeeded )
					{
						// This plugin hasn't been loaded it yet.  Don't load unless it is set as active
						LoadPlugin( tableIndex );
					}	

					pResult = fTable[ tableIndex ].fPluginPtr;

					break;
				}
			}
			tableIndex++;
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
	CServerPlugin	   *pResult			= nil;
	uInt32				tableIndex		= *inIndex;

	fMutex.Wait();

	try
	{
		// Go down tree to find parent node of new insertion node
		while ( tableIndex < kMaxPlugIns )
		{
			if ( (fTable[ tableIndex ].fName != nil) && (fTable[ tableIndex ].fPluginPtr != nil) )
			{
				pResult = fTable[ tableIndex ].fPluginPtr;
				tableIndex++;
				break;
			}
			tableIndex++;
		}
	}

	catch( sInt32 err )
	{
		pResult = nil;
	}

	*inIndex = tableIndex;

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




