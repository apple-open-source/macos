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
 * @header CPluginHandler
 */

#include <CoreFoundation/CFPlugIn.h>
#include <CoreFoundation/CFString.h>
#include <CoreFoundation/CFURL.h>

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

#warning VERIFY the version string for the static plugins before each software release
//KW not clear we use the version string for anything yet in a static plugin
//static plugin name and version strings
static const char *sStaticPluginList[ kNumStaticPlugins ][ 2 ] =
{
	{ "Configure",	"1.7" },
	{ "NetInfo",	"1.7.4" },
	{ "LDAPv3",		"1.7.3" },
	{ "Search",		"1.7" }
};

//--------------------------------------------------------------------------------------------------
//	* CPluginHandler()
//
//--------------------------------------------------------------------------------------------------

CPluginHandler::CPluginHandler ( void ) : CInternalDispatchThread(kTSPlugInHndlrThread)
{
	fThreadSignature = kTSPlugInHndlrThread;
} // CPluginHandler



//--------------------------------------------------------------------------------------------------
//	* ~CPluginHandler()
//
//--------------------------------------------------------------------------------------------------

CPluginHandler::~CPluginHandler()
{
} // ~CPluginHandler


//--------------------------------------------------------------------------------------------------
//	* StartThread()
//
//--------------------------------------------------------------------------------------------------

void CPluginHandler::StartThread ( void )
{
	if ( this == nil ) throw((sInt32)eMemoryError);

	this->Resume();
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
//	* LoadPlugins ()
//
//--------------------------------------------------------------------------------------------------

sInt32 CPluginHandler::LoadPlugins ( void )
{
	sInt32				status	= eDSNoErr;
	uInt32				uiCount	= 0;
	CString				cSubPath( COSUtils::GetStringFromList( kAppStringsListID, kStrPlugInsFolder ) );
	CString				cOtherSubPath( COSUtils::GetStringFromList( kAppStringsListID, kStrOtherPlugInsFolder ) );
	CString				cPlugExt( COSUtils::GetStringFromList( kAppStringsListID, kStrPluginExtension ) );
	CFStringRef			sType	= nil;
	char				string	[ PATH_MAX ];

	//process the static plugin modules
    for (uInt32 iPlugin = 0; iPlugin < kNumStaticPlugins; iPlugin++)
    {
        status = CServerPlugin::ProcessStaticPlugin(	sStaticPluginList[iPlugin][0],
                                                        sStaticPluginList[iPlugin][1]);
        if (status == eDSNoErr)
        {
            uiCount++;
        }
    }
	
    //KW now we need to know which plugins are lazily loaded and which
    //are required now to be loaded
    
	//Retrieve all the Apple developed plugins
    sType = ::CFStringCreateWithCString( kCFAllocatorDefault, cPlugExt.GetData(), kCFStringEncodingMacRoman );

    CFURLRef	urlPath = 0;
    CFStringRef	sBase, sPath;
    CFArrayRef	aBundles;

    // Append the subpath.
    sBase = CFSTR( "/System/Library" );
    sPath = ::CFStringCreateWithFormat( kCFAllocatorDefault, NULL, CFSTR( "%@/%s" ), sBase, cSubPath.GetData() );
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

    if ( aBundles != nil )
    {
		register CFIndex bundleCount = CFArrayGetCount( aBundles );
        for ( register CFIndex j = 0; j < bundleCount; ++j )
        {
            status = CServerPlugin::ProcessURL( (CFURLRef)::CFArrayGetValueAtIndex( aBundles, j ) );
            if ( status == eDSNoErr )
            {
                uiCount++;
            }
			else
				SRVRLOG( kLogApplication, "\tError loading plugin, see DirectoryService.error.log for details" );
        }
    
        ::CFRelease( aBundles );
        aBundles = nil;
    }

	if ( sType != nil )
	{
		::CFRelease( sType );
		sType = nil;
	}

	//Now retrieve any Third party developed plugins
    sType = ::CFStringCreateWithCString( kCFAllocatorDefault, cPlugExt.GetData(), kCFStringEncodingMacRoman );

    // Append the subpath.
    sBase = CFSTR( "/Library" );
    sPath = ::CFStringCreateWithFormat( kCFAllocatorDefault, NULL, CFSTR( "%@/%s" ), sBase, cOtherSubPath.GetData() );
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

    if ( aBundles != nil )
    {
        register CFIndex j = ::CFArrayGetCount( aBundles );
        while ( j-- )
        {
            status = CServerPlugin::ProcessURL( (CFURLRef)::CFArrayGetValueAtIndex( aBundles, j ) );
            if ( status == eDSNoErr )
            {
                uiCount++;
            }
			else
				SRVRLOG( kLogApplication, "\tError loading 3rd party plugin, see DirectoryService.error.log for details" );
        }

        ::CFRelease( aBundles );
        aBundles = nil;
    }

	if ( sType != nil )
	{
		::CFRelease( sType );
		sType = nil;
	}

	return( uiCount );

} // LoadPlugins

