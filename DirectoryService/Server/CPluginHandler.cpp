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
 * @header CPluginHandler
 */

#include <CoreFoundation/CFPlugIn.h>
#include <CoreFoundation/CFString.h>
#include <CoreFoundation/CFURL.h>
#include <CoreFoundation/CFPriv.h>

#include "CPluginHandler.h"
#include "CPlugInList.h"
#include "ServerControl.h"
#include "DirServiceMain.h"
#include "CString.h"
#include "COSUtils.h"
#include "CLog.h"
#include "CServerPlugin.h"

// --------------------------------------------------------------------------------
//	* Globals
// --------------------------------------------------------------------------------

DSEventSemaphore   	*gKickPluginHndlrThread	= nil;
CPluginHandler		*gPluginHandler = nil;


//--------------------------------------------------------------------------------------------------
//	* CPluginHandler()
//
//--------------------------------------------------------------------------------------------------

CPluginHandler::CPluginHandler ( void )
	: DSCThread(kTSPlugInHndlrThread )
{
	fThreadSignature = kTSPlugInHndlrThread;
	if ( gKickPluginHndlrThread == nil )
	{
		gKickPluginHndlrThread = new DSEventSemaphore();
		if ( gKickPluginHndlrThread == nil ) throw((sInt32)eMemoryAllocError);
	}
} // CPluginHandler



//--------------------------------------------------------------------------------------------------
//	* ~CPluginHandler()
//
//--------------------------------------------------------------------------------------------------

CPluginHandler::~CPluginHandler()
{
} // ~CPluginHandler



//--------------------------------------------------------------------------------------------------
//	* WakupHandlers() (static)
//
//--------------------------------------------------------------------------------------------------

void CPluginHandler::WakupHandlers ( void )
{
	gKickPluginHndlrThread->Signal();
} // WakupHandlers



//--------------------------------------------------------------------------------------------------
//	* StartThread()
//
//--------------------------------------------------------------------------------------------------

void CPluginHandler::StartThread ( void )
{
	if ( this == nil ) throw((sInt32)eMemoryError);

	this->Resume();

	SetThreadRunState( kThreadRun );		// Tell our thread it's running

} // StartThread



//--------------------------------------------------------------------------------------------------
//	* StopThread()
//
//--------------------------------------------------------------------------------------------------

void CPluginHandler::StopThread ( void )
{
	if ( this == nil ) throw((sInt32)eMemoryError);

	// Check that the current thread context is not our thread context
//	SignalIf_( this->IsCurrent() );

	SetThreadRunState( kThreadStop );		// Tell our thread to stop

} // StopThread



//--------------------------------------------------------------------------------------------------
//	* ThreadMain()
//
//--------------------------------------------------------------------------------------------------

long CPluginHandler::ThreadMain ( void )
{
	uInt32		uiPluginCnt		= 0;

	uiPluginCnt = LoadPlugins();
	if ( uiPluginCnt == 0 )
	{
		//SRVRLOG( kLogApplication, "*** WARNING: No Plugins loaded ***" );
		ERRORLOG( kLogApplication, "*** WARNING: No Plugins loaded ***" );
	}
	else
	{
		DBGLOG1( kLogApplication, "%d Plugins loaded or processed.", uiPluginCnt );

		DBGLOG( kLogApplication, "Initializing plugins." );
		gPlugins->InitPlugIns();
	}

	return( 0 );

} // ThreadMain


//--------------------------------------------------------------------------------------------------
//	* HandleMessage ()
//
//	/System/Library/PrivateFrameworks
//
//--------------------------------------------------------------------------------------------------

sInt32 CPluginHandler::LoadPlugins ( void )
{
	sInt32				status	= eDSNoErr;
	uInt32				uiCount	= 0;
	uInt32				uiMask	= kCFSystemDomainMask;
	CString				cSubPath( COSUtils::GetStringFromList( kAppStringsListID, kStrPlugInsFolder ) );
	CString				cOtherSubPath( COSUtils::GetStringFromList( kAppStringsListID, kStrOtherPlugInsFolder ) );
	CString				cPlugExt( COSUtils::GetStringFromList( kAppStringsListID, kStrPluginExtension ) );
	register CFIndex	i;
	CFArrayRef			aPaths	= nil;
	CFStringRef			sType	= nil;
	char				string	[ PATH_MAX ];


	//process the single configure plugin
	status = CServerPlugin::ProcessConfigurePlugin();
	if (status == eDSNoErr)
	{
		uiCount++;
	}
	
	//Retrieve all the Apple developed plugins first
	// Get the list of search paths.
	aPaths = ::CFCopySearchPathForDirectoriesInDomains( kCFLibraryDirectory, (CFSearchPathDomainMask)uiMask, true );
	if ( aPaths != nil )
	{
		i = ::CFArrayGetCount( aPaths );
		if ( i != 0 )
		{
			sType = ::CFStringCreateWithCString( kCFAllocatorDefault, cPlugExt.GetData(), kCFStringEncodingMacRoman );

			// I count down here because versions in the System directory is preferred
			// over versions in Local ( or user's home directory ).
			while ( i-- )
			{
				CFURLRef	urlPath = (CFURLRef)::CFArrayGetValueAtIndex( aPaths, i );
				CFStringRef	sBase, sPath;
				CFArrayRef	aBundles;

				// Append the subpath.
				sBase = ::CFURLCopyFileSystemPath( urlPath, kCFURLPOSIXPathStyle );
				sPath = ::CFStringCreateWithFormat( kCFAllocatorDefault, NULL, CFSTR( "%@/%s" ), sBase, cSubPath.GetData() );
				::CFRelease( sBase );
				sBase = nil;

				::CFStringGetCString( sPath, string, sizeof( string ), kCFStringEncodingMacRoman );
				DBGLOG( kLogApplication, "Checking for plugins in:" );
				DBGLOG1( kLogApplication, "  %s", string );

				// Convert it back into a CFURL.
				urlPath = ::CFURLCreateWithFileSystemPath( kCFAllocatorDefault, sPath, kCFURLPOSIXPathStyle, true );
				::CFRelease( sPath );
				sPath = nil;

				// Enumerate all the plugins in this system directory.
				aBundles = ::CFBundleCopyResourceURLsOfTypeInDirectory( urlPath, sType, NULL );
				::CFRelease( urlPath );
				urlPath = nil;

				if ( aBundles == nil )
				{
					continue;
				}

				for ( register CFIndex j = 0; j < ::CFArrayGetCount( aBundles ); ++j )
				{
					status = CServerPlugin::ProcessURL( (CFURLRef)::CFArrayGetValueAtIndex( aBundles, j ) );
					if ( status == eDSNoErr )
					{
						uiCount++;
					}
				}

				if ( aBundles != nil )
				{
					::CFRelease( aBundles );
					aBundles = nil;
				}
			}
		}
	}

	if ( sType != nil )
	{
		::CFRelease( sType );
		sType = nil;
	}

	if ( aPaths != nil )
	{
		::CFRelease( aPaths );
		aPaths = nil;
	}

	//Now retrieve any Third party developed plugins
	// Get the list of search paths.
	uiMask	= kCFLocalDomainMask;
	i = 0;
	aPaths = ::CFCopySearchPathForDirectoriesInDomains( kCFLibraryDirectory, (CFSearchPathDomainMask)uiMask, true );
	if ( aPaths != nil )
	{
		i = ::CFArrayGetCount( aPaths );
		if ( i != 0 )
		{
			sType = ::CFStringCreateWithCString( kCFAllocatorDefault, cPlugExt.GetData(), kCFStringEncodingMacRoman );

			//don't really expect more than one directory
			while ( i-- )
			{
				CFURLRef	urlPath = (CFURLRef)::CFArrayGetValueAtIndex( aPaths, i );
				CFStringRef	sBase, sPath;
				CFArrayRef	aBundles;

				// Append the subpath.
				sBase = ::CFURLCopyFileSystemPath( urlPath, kCFURLPOSIXPathStyle );
				sPath = ::CFStringCreateWithFormat( kCFAllocatorDefault, NULL, CFSTR( "%@/%s" ), sBase, cOtherSubPath.GetData() );
				::CFRelease( sBase );
				sBase = nil;

				::CFStringGetCString( sPath, string, sizeof( string ), kCFStringEncodingMacRoman );
				DBGLOG( kLogApplication, "Checking for plugins in:" );
				DBGLOG1( kLogApplication, "  %s", string );

				// Convert it back into a CFURL.
				urlPath = ::CFURLCreateWithFileSystemPath( kCFAllocatorDefault, sPath, kCFURLPOSIXPathStyle, true );
				::CFRelease( sPath );
				sPath = nil;

				// Enumerate all the plugins in this system directory.
				aBundles = ::CFBundleCopyResourceURLsOfTypeInDirectory( urlPath, sType, NULL );
				::CFRelease( urlPath );
				urlPath = nil;

				if ( aBundles == nil )
				{
					continue;
				}

				register CFIndex j = ::CFArrayGetCount( aBundles );
				while ( j-- )
				{
					status = CServerPlugin::ProcessURL( (CFURLRef)::CFArrayGetValueAtIndex( aBundles, j ) );
					if ( status == eDSNoErr )
					{
						uiCount++;
					}
				}

				if ( aBundles != nil )
				{
					::CFRelease( aBundles );
					aBundles = nil;
				}
			}
		}
	}

	if ( sType != nil )
	{
		::CFRelease( sType );
		sType = nil;
	}

	if ( aPaths != nil )
	{
		::CFRelease( aPaths );
		aPaths = nil;
	}

	return( uiCount );

} // LoadPlugins

