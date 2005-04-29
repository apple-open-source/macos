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

#include "CPlugInList.h"
#include "CServerPlugin.h"
#include "CLauncher.h"
#include "DSUtils.h"
#include "PrivateTypes.h"
#include "SharedConsts.h"
#include "CPluginConfig.h"
#include "CLog.h"
#include "CNodeList.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>

#include <CoreFoundation/CoreFoundation.h>

extern CFRunLoopRef			gServerRunLoop;
extern DSMutexSemaphore    *gKerberosMutex;
extern CPluginConfig	   *gPluginConfig;
extern DSMutexSemaphore	   *gLazyPluginLoadingLock;
extern CPlugInList		   *gPlugins;
extern CNodeList		   *gNodeList;

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
	fTable		= nil;
	fTableTail  = nil;

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
	sInt32			siResult	= eDSInvalidPlugInConfigData;
	sTableData     *aTableEntry = nil;

	fMutex.Wait();

	try
	{
		if ( inName == nil )
		{
			return( eDSNullParameter );
		}

		aTableEntry = (sTableData *)calloc(1, sizeof(sTableData));
		if (fTableTail != nil)
		{
			fTableTail->pNext   = aTableEntry;
			fTableTail			= aTableEntry;
		}
		else
		{
			fTable = aTableEntry;
			fTableTail = aTableEntry;
		}
		fTableTail->pNext			= nil;
		fTableTail->fName			= inName;
		fTableTail->fVersion		= inVersion;
		fTableTail->fConfigAvail	= inConfigAvail;
		fTableTail->fConfigFile		= inConfigFile;
		fTableTail->fPluginPtr		= inPluginPtr;
		
		if ( inPluginRef )
		{
			fTableTail->fPluginRef = inPluginRef;
			CFRetain( fTableTail->fPluginRef );
		}
		
		if ( inCFuuidFactory )
		{
			fTableTail->fCFuuidFactory = inCFuuidFactory;
			CFRetain( fTableTail->fCFuuidFactory );
		}
		
		if ( inULVers )
			fTableTail->fULVers	= inULVers;
			
		fTableTail->fKey = inKey;
		ePluginState pluginState = gPluginConfig->GetPluginState( fTableTail->fName );
		
		fTableTail->fState = pluginState | kUninitialized;

		fPICount++;

		siResult = eDSNoErr;
	}

	catch( sInt32 err )
	{
		siResult = err;
	}

	fMutex.Signal();

	return( siResult );

} // AddPlugIn


void CPlugInList::LoadPlugin( sTableData *inTableEntry )
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
		if ( inTableEntry->fPluginPtr == nil)
		{
			inTableEntry->fPluginPtr = new CServerPlugin( inTableEntry->fPluginRef, inTableEntry->fCFuuidFactory, inTableEntry->fKey, inTableEntry->fULVers, inTableEntry->fName );
			
			ourPluginPtr = (CServerPlugin *)inTableEntry->fPluginPtr;

			if ( ourPluginPtr == NULL ) throw( (sInt32)eMemoryError );
			
			ourPluginPtr->Validate( inTableEntry->fVersion, inTableEntry->fKey );
			
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
		
						// provide the Kerberos Mutex to plugins that need it
						if (gKerberosMutex != NULL)
						{
							aHeader.fType			= kKerberosMutex;
							aHeader.fResult			= eDSNoErr;
							aHeader.fContextData	= (void *)gKerberosMutex;
							ourPluginPtr->ProcessRequest( (void*)&aHeader ); // don't handle return
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

			SRVRLOG2( kLogApplication, "Plugin \"%s\", Version \"%s\", loaded on demand successfully.", inTableEntry->fName, inTableEntry->fVersion );
		}
	}

	catch( sInt32 err )
	{
		SRVRLOG3( kLogApplication, "Plugin \"%s\", Version \"%s\", failed to load on demand (%d).", inTableEntry->fName, inTableEntry->fVersion, err );
	}
	
	gLazyPluginLoadingLock->Signal();
	
}


// ---------------------------------------------------------------------------
//	* InitPlugIns ()
//
// ---------------------------------------------------------------------------

void CPlugInList::InitPlugIns ( void )
{
	sTableData     *aTableEntry = nil;

	fMutex.Wait();

	aTableEntry = fTable;
	while ( aTableEntry != nil )
	{
		if ( (aTableEntry->fName != nil) && (aTableEntry->fPluginPtr != nil) )
		{
			try
			{
				//this constructor could throw
				CLauncher *cpLaunch = new CLauncher( (CServerPlugin *)aTableEntry->fPluginPtr );
				if ( cpLaunch != nil )
				{
					//this call could throw
					cpLaunch->StartThread();
				}
				DBGLOG2( kLogApplication, "Plugin \"%s\", Version \"%s\", activated successfully.", aTableEntry->fName, aTableEntry->fVersion );
			}
		
			catch( sInt32 err )
			{
				DBGLOG2( kLogApplication, "Plugin \"%s\", Version \"%s\", failed to launch initialization thread.", aTableEntry->fName, aTableEntry->fVersion );
			}
		}
		else if ( aTableEntry->fName != nil )
		{
			// if this plugin is supposed to be active, we should mark it as such, it still should be uninitialized.
			ePluginState		pluginState = gPluginConfig->GetPluginState( aTableEntry->fName );

			aTableEntry->fState = pluginState | kUninitialized;
			DBGLOG2( kLogApplication, "Plugin \"%s\", Version \"%s\", referenced to be loaded on demand successfully.", aTableEntry->fName, aTableEntry->fVersion );
		}
		aTableEntry = aTableEntry->pNext;
	}

	fMutex.Signal();

} // InitPlugIns


// ---------------------------------------------------------------------------
//	* IsPresent ()
//
// ---------------------------------------------------------------------------

sInt32 CPlugInList::IsPresent ( const char *inName )
{
	sInt32			siResult	= ePluginNameNotFound;
	sTableData     *aTableEntry = nil;

	fMutex.Wait();

	if ( inName == nil )
	{
		return( eDSNullParameter );
	}

	aTableEntry = fTable;
	while ( aTableEntry != nil )
	{
		if ( aTableEntry->fName != nil )
		{
			if ( ::strcmp( aTableEntry->fName, inName ) == 0 )
			{
				siResult = eDSNoErr;

				break;
			}
		}
		aTableEntry = aTableEntry->pNext;
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
	sInt32			siResult		= ePluginNameNotFound;
	sTableData     *aTableEntry		= nil;
	uInt32			curState		= kUnknownState;

	fMutex.Wait();

	if ( inName == nil )
	{
		return( eDSNullParameter );
	}

	aTableEntry = fTable;
	while ( aTableEntry != nil )
	{
		if ( aTableEntry->fName != nil )
		{
			if ( ::strcmp( aTableEntry->fName, inName ) == 0 )
			{
				curState = aTableEntry->fState;
				
				if ( (inState & kActive) && aTableEntry->fPluginPtr == NULL )
				{
					// This plugin has just been set to active but we haven't loaded it yet.
					//need to acquire mutexes in proper order
					fMutex.Signal();
					gNodeList->Lock();
					fMutex.Wait();
					LoadPlugin( aTableEntry );
					gNodeList->Unlock();
				}

				aTableEntry->fState = inState;

				if ( !( curState & inState ) && ( aTableEntry->fPluginPtr ) )
					aTableEntry->fPluginPtr->SetPluginState(inState);
				
				siResult = eDSNoErr;

				break;
			}
		}
		aTableEntry = aTableEntry->pNext;
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
	sInt32			siResult		= ePluginNameNotFound;
	sTableData     *aTableEntry		= nil;

	fMutex.Wait();

	if ( inName == nil )
	{
		return( eDSNullParameter );
	}

	aTableEntry = fTable;
	while ( aTableEntry != nil )
	{
		if ( aTableEntry->fName != nil )
		{
			if ( ::strcmp( aTableEntry->fName, inName ) == 0 )
			{
				*outState = aTableEntry->fState;

				siResult = eDSNoErr;

				break;
			}
		}
		aTableEntry = aTableEntry->pNext;
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
	uInt32			activeCount		= 0;
	sTableData     *aTableEntry		= nil;

	fMutex.Wait();

	aTableEntry = fTable;
	while ( aTableEntry != nil )
	{
		if ( aTableEntry->fName == nil )
		{
			if ( aTableEntry->fState & kActive )
			{
				activeCount++;
			}
		}
		aTableEntry = aTableEntry->pNext;
	}

	fMutex.Signal();

	return( activeCount );

} // GetActiveCount


// ---------------------------------------------------------------------------
//	* GetPlugInPtr ()
//
// ---------------------------------------------------------------------------

CServerPlugin* CPlugInList::GetPlugInPtr ( const char *inName, bool loadIfNeeded )
{
	CServerPlugin  *pResult			= nil;
	sTableData     *aTableEntry		= nil;

	fMutex.Wait();

	if ( inName == nil )
	{
		return( nil );
	}

	aTableEntry = fTable;
	while ( aTableEntry != nil )
	{
		if ( (aTableEntry->fName != nil) )
		{
			if ( ::strcmp( aTableEntry->fName, inName ) == 0 )
			{
				// if someone specifically asks for a node, even if not active, we should load the plugin
				// this can be for configure nodes, otherwise inactive nodes cannot be configured
				if ( (aTableEntry->fPluginPtr == NULL) && loadIfNeeded )
				{
					// This plugin hasn't been loaded it yet.  Load it if loadIfNeeded set
					//need to acquire mutexes in proper order
					fMutex.Signal();
					gNodeList->Lock();
					fMutex.Wait();
					LoadPlugin( aTableEntry );
					gNodeList->Unlock();
				}	

				pResult = aTableEntry->fPluginPtr;

				break;
			}
		}
		aTableEntry = aTableEntry->pNext;
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
	CServerPlugin  *pResult			= nil;
	sTableData     *aTableEntry		= nil;

	fMutex.Wait();

	aTableEntry = fTable;
	while ( aTableEntry != nil )
	{
		if ( (aTableEntry->fName != nil) )
		{
			if ( aTableEntry->fKey == inKey )
			{
				if (	aTableEntry->fPluginPtr == NULL
					&&	(gPluginConfig->GetPluginState(aTableEntry->fName) & kActive)
					&&	loadIfNeeded )
				{
					// This plugin hasn't been loaded it yet.  Don't load unless it is set as active
					//need to acquire mutexes in proper order
					fMutex.Signal();
					gNodeList->Lock();
					fMutex.Wait();
					LoadPlugin( aTableEntry );
					gNodeList->Unlock();
				}	

				pResult = aTableEntry->fPluginPtr;

				break;
			}
		}
		aTableEntry = aTableEntry->pNext;
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
	uInt32				tableIndex		= 0;
	sTableData		   *aTableEntry		= nil;

	fMutex.Wait();

	aTableEntry = fTable;
	while ( aTableEntry != nil )
	{
		if (tableIndex == *inIndex)
		{
			if ( (aTableEntry->fName != nil) && (aTableEntry->fPluginPtr != nil) )
			{
				pResult = aTableEntry->fPluginPtr;
				tableIndex++;
				break;
			}
			else
			{
				*inIndex = tableIndex + 1;	// keep looking for next loaded plugin
			}
		}
		tableIndex++;
		aTableEntry = aTableEntry->pNext;
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
	uInt32				tableIndex		= 0;
	sTableData		   *aTableEntry		= nil;

	fMutex.Wait();

	aTableEntry = fTable;
	while ( aTableEntry != nil )
	{
		if (tableIndex == inIndex)
		{
			break;
		}
		tableIndex++;
		aTableEntry = aTableEntry->pNext;
	}

	fMutex.Signal();

	return( aTableEntry );

} // GetPlugInInfo




