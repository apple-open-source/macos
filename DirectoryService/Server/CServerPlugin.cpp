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
 */

#include "CServerPlugin.h"
#include "ServerControl.h"
#include "CNodeList.h"
#include "CPlugInList.h"
#include "CString.h"
#include "DSUtils.h"
#include "CLog.h"
#include "CConfigurePlugin.h"
#include "COSUtils.h"

#include <stdlib.h>	// for rand()

#include <CoreFoundation/CFPlugIn.h>
#include <CoreFoundation/CFString.h>
#include <CoreFoundation/CFURL.h>

using namespace DSServerPlugin;

// ----------------------------------------------------------------------------
//	* Private stuff
//	These are not declared statically in the class definition because I want
//	to hide the implementation details and reduce unrelated dependencies in
//	the class header.
// ----------------------------------------------------------------------------

// ----------------------------------------------------------------------------
//	* CServerPlugin Class Globals
//
//	These private typedefs, globals, and functions are not declared statically
//	in the class definition because I want to hide the implementation details
//	and reduce unrelated dependencies in the class header.
// ----------------------------------------------------------------------------

static sInt32		_UnregisterNode	( const uInt32, tDataList * );
static void			_DebugLog		( const char *inFormat, va_list inArgs );

static SvrLibFtbl	_Callbacks = { CServerPlugin::_RegisterNode, _UnregisterNode, _DebugLog };

const	sInt32	kNodeNotRegistered			= -8000;
const	sInt32	kNodeAlreadyRegistered		= -8001;
const	sInt32	kInvalidToken				= -8002;
const	sInt32	kNullNodeName				= -8003;
const	sInt32	kEmptyNodeName				= -8004;
const	sInt32	kServerError				= -8101;

// ----------------------------------------------------------------------------
//	* _RegisterNode()
//
// ----------------------------------------------------------------------------

sInt32 CServerPlugin::_RegisterNode ( const uInt32 inToken, tDataList *inNodeList, eDirNodeType inNodeType )
{
	sInt32			siResult	= eDSNoErr;
	tDataList	   *pNodeList	= nil;
	char		   *pNodeName	= nil;
	CServerPlugin  *pPluginPtr	= nil;
	bool			bDone		= false;

	if ( inNodeType == kUnknownNodeType )
	{
		return( kNodeNotRegistered );
	}
	
	if ( inToken == 0 )
	{
		return( kInvalidToken );
	}
	// Get the plugin pointer for this token
	//this call associates the pluginPtr with the input token thus preventing any plugin
	//from registering a node for another plugin unless that plugin knows the other plugin's token/signature
	pPluginPtr = gPlugins->GetPlugInPtr( inToken );
	if ( pPluginPtr == nil )
	{
		return( kInvalidToken );
	}
	
	if ( inNodeList == nil )
	{
		return( kNullNodeName );
	}

	if ( (inNodeList->fDataNodeCount == 0) || (inNodeList->fDataListHead == nil) )
	{
		return( kEmptyNodeName );
	}

	if ( gNodeList == nil )
	{
		return( kServerError );
	}

	// Create a path string
	pNodeName = ::dsGetPathFromListPriv( inNodeList, (char *)gNodeList->GetDelimiter() );
	if ( pNodeName != nil )
	{
		if ( inNodeType & kDirNodeType )
		{
			// Is this node registered - ie. bow out early if duplicate exists
			if ( gNodeList->IsPresent( pNodeName, kDirNodeType ) == false )
			{
				// Get our copy of the node list
				//this pNodeList either gets consumed by the AddNode OR we free it
				pNodeList = dsBuildFromPathPriv( pNodeName, (char *)gNodeList->GetDelimiter() );
				
				// Add the node to the node list
				gNodeList->AddNode( pNodeName, pNodeList, kDirNodeType, pPluginPtr );
			}
			else
			{
				siResult = kNodeAlreadyRegistered;
			}
			bDone = true;
		}
		
		if ( inNodeType & kLocalHostedType )
		{
			// Is this node registered - ie. bow out early if duplicate exists
			if ( gNodeList->IsPresent( pNodeName, kLocalHostedType ) == false )
			{
				// Get our copy of the node list
				//this pNodeList either gets consumed by the AddNode OR we free it
				pNodeList = dsBuildFromPathPriv( pNodeName, (char *)gNodeList->GetDelimiter() );
				
				// Add the node to the node list
				gNodeList->AddNode( pNodeName, pNodeList, kLocalHostedType, pPluginPtr );
			}
			else
			{
				siResult = kNodeAlreadyRegistered;
			}
			bDone = true;
		}
		
		if ( inNodeType & kDefaultNetworkNodeType )
		{
			// Is this node registered - ie. bow out early if duplicate exists
			if ( gNodeList->IsPresent( pNodeName, kDefaultNetworkNodeType ) == false )
			{
				// Get our copy of the node list
				//this pNodeList either gets consumed by the AddNode OR we free it
				pNodeList = dsBuildFromPathPriv( pNodeName, (char *)gNodeList->GetDelimiter() );
				
				// Add the node to the node list
				gNodeList->AddNode( pNodeName, pNodeList, kDefaultNetworkNodeType, pPluginPtr );
			}
			else
			{
				siResult = kNodeAlreadyRegistered;
			}
			bDone = true;
		}
		
		if ( !bDone && ( inNodeType & (kLocalNodeType | kSearchNodeType | kConfigNodeType | kContactsSearchNodeType | kNetworkSearchNodeType) ) )
		//specific node type added here - don't check for duplicates here
		//need to always be able to get in here regardless of mutexes ie. don't call IsPresent()
		{
			// Get our copy of the node list
			//this pNodeList either gets consumed by the AddNode OR we free it
			pNodeList = dsBuildFromPathPriv( pNodeName, (char *)gNodeList->GetDelimiter() );
			
			// Add the node to the node list
			siResult = gNodeList->AddNode( pNodeName, pNodeList, inNodeType, pPluginPtr );
			if (siResult == 0) //specific return from AddNode ie. not really a DS status
			{
				::dsDataListDeallocatePriv( pNodeList );
				free(pNodeList);
				pNodeList = nil;
				siResult = kNodeAlreadyRegistered;
			}
			else
			{
				siResult = eDSNoErr;
			}
		}

		free( pNodeName );
		pNodeName = nil;
	}
	else
	{
		siResult = kServerError;
	}

	return( siResult );

} // _RegisterNode


// ----------------------------------------------------------------------------
//	* _UnregisterNode()
//
// ----------------------------------------------------------------------------

static sInt32 _UnregisterNode ( const uInt32 inToken, tDataList *inNode )
{
	sInt32			siResult	= 0;
	char		   *nodePath	= nil;
	
	if ( inNode == nil )
	{
		return( kNullNodeName );
	}

	if ( (inNode->fDataNodeCount == 0) || (inNode->fDataListHead == nil) )
	{
		return( kEmptyNodeName );
	}

	if ( gNodeList == nil )
	{
		return( kServerError );
	}

	nodePath = ::dsGetPathFromListPriv( inNode, (char *)gNodeList->GetDelimiter() );
	if ( nodePath != nil )
	{
		if ( gNodeList->DeleteNode( nodePath ) != true )
		{
			siResult = kNodeNotRegistered;
			ERRORLOG1( kLogPlugin, "Can't unregister node %s since not registered", nodePath );
		}
		else
		{
			SRVRLOG1( kLogPlugin, "Unregistered node %s", nodePath );
		}
		free( nodePath );
		nodePath = nil;
	}

	return( siResult );

} // _UnregisterNode


// ----------------------------------------------------------------------------
//	* _DebugLog()
//
// ----------------------------------------------------------------------------

static void _DebugLog ( const char *inPattern, va_list args )
{
	CString		inLogStr( inPattern, args );

	DBGLOG( kLogPlugin, inLogStr.GetData() );

} // _DebugLog


// ----------------------------------------------------------------------------
//	* CServerPlugin()
//
// ----------------------------------------------------------------------------

CServerPlugin::CServerPlugin ( void ) : mInstance( NULL )
{
	SvrLibFtbl		stTemp		= _Callbacks;


}


// ----------------------------------------------------------------------------
//	* CServerPlugin()
//
// ----------------------------------------------------------------------------

CServerPlugin::CServerPlugin ( CFPlugInRef		inThis,
								 CFUUIDRef		inFactoryID,
								 FourCharCode	inSig,
								 uInt32			inVers,
								 char		   *inName )
	: mInstance( NULL )
{
	IUnknownVTbl	*spUnknown	= NULL;
	SvrLibFtbl		stTemp		= _Callbacks;

	fPlugInRef	= inThis;
	fPlugInVers	= inVers; //never used anywhere
	fPlugInName	= nil;

	spUnknown = (IUnknownVTbl *)::CFPlugInInstanceCreate( kCFAllocatorDefault,
														  inFactoryID,
														  kModuleTypeUUID );
	if ( spUnknown == NULL )
	{
		throw( (sInt32) 'ecom' );
	}

	spUnknown->QueryInterface( spUnknown,
								::CFUUIDGetUUIDBytes( kModuleInterfaceUUID ),
								(LPVOID *)(&mInstance) );

	// Now we are done with IUnknown
	spUnknown->Release( spUnknown );

	if ( mInstance == NULL )
	{
		throw( (sInt32) 'ecom' );
	}

	stTemp.fSignature = inSig;
	mInstance->linkLibFtbl( mInstance, &stTemp );

	if ( inName != nil )
	{
		fPlugInName = new char[ ::strlen( inName ) + 1 ];
		::strcpy( fPlugInName, inName );
	}

	fPlugInSignature = inSig;

} // CServerPlugin


// ----------------------------------------------------------------------------
//	* ~CServerPlugin()
//
// ----------------------------------------------------------------------------

CServerPlugin::~CServerPlugin ( void )
{
	if ( fPlugInName != nil )
	{
		delete( fPlugInName );
		fPlugInName = nil;
	}

	if ( mInstance != nil )
	{
		mInstance->Release( mInstance );
		mInstance = nil;
	}

} // ~CServerPlugin


// ----------------------------------------------------------------------------
//	* ProcessURL()
//
//		- Passed to _ProcessAllURLs() to validate each
//			bundle's property list and create the plugin object.
//
// ----------------------------------------------------------------------------

sInt32 CServerPlugin::ProcessURL ( CFURLRef inURLPlugin )
{
	sInt32			siResult			= -1;
	char		   *pPIVersion			= nil;
	char		   *pPIName				= nil;
	char		   *pPIConfigAvail 		= nil;
	char		   *pPIConfigFile 		= nil;
	bool			bGotIt				= false;
	unsigned char	path[ PATH_MAX ]	= "\0";
	uInt32			ulVers				= 0;
	CServerPlugin  *cpPlugin			= nil;
	FourCharCode	fccPlugInSignature	= 0;
	CFPlugInRef		plgThis				= ::CFPlugInCreate( kCFAllocatorDefault, inURLPlugin );
	CFStringRef		cfsVersion			= nil;
	CFStringRef		cfsName				= nil;
	CFStringRef		cfsConfigAvail 		= nil;
	CFStringRef		cfsConfigFile 		= nil;
	CFArrayRef		cfaFactories		= nil;
	CFUUIDRef		cfuuidFactory		= nil;
	CFBundleRef		bdlThis				= nil;
	CFDictionaryRef	plInfo				= nil;

	try
	{
		::CFURLGetFileSystemRepresentation( inURLPlugin, true, path, sizeof( path ) );
		if ( plgThis == nil ) throw ( (sInt32)eCFMGetFileSysRepErr );

		bdlThis	= ::CFPlugInGetBundle( plgThis );
		if ( bdlThis == nil ) throw ( (sInt32)eCFPlugInGetBundleErr );

		plInfo	= ::CFBundleGetInfoDictionary( bdlThis );
		if ( plInfo == nil ) throw ( (sInt32)eCFBndleGetInfoDictErr );

		ulVers	= ::CFBundleGetVersionNumber( bdlThis );

		if ( ::CFDictionaryGetValue( plInfo, kCFPlugInTypesKey ) == nil ) throw ( (sInt32)eCFBndleGetInfoDictErr );
		if ( ::CFDictionaryGetValue( plInfo, kCFPlugInFactoriesKey ) == nil ) throw ( (sInt32)eCFBndleGetInfoDictErr );

		// Get the plugin version
		cfsVersion = (CFStringRef)::CFDictionaryGetValue( plInfo, kPluginVersionStr );
		if ( cfsVersion == nil ) throw( (sInt32)ePluginVersionNotFound );

		pPIVersion = (char *)::malloc( kMaxPlugInAttributeStrLen );
		if ( pPIVersion == nil ) throw( (sInt32)eMemoryError );

		// Convert it to a regular 'C' string 
		bGotIt = CFStringGetCString( cfsVersion, pPIVersion, kMaxPlugInAttributeStrLen, kCFStringEncodingMacRoman );
		if (bGotIt == false) throw( (sInt32)ePluginVersionNotFound );

		// Get the plugin configavail
		// if it does not exist then we use a default of "Not Available"
		pPIConfigAvail = (char *)::malloc( kMaxPlugInAttributeStrLen );
		if ( pPIConfigAvail == nil ) throw( (sInt32)eMemoryError );
		cfsConfigAvail = (CFStringRef)::CFDictionaryGetValue( plInfo, kPluginConfigAvailStr );
		if (cfsConfigAvail)
		{
			// Convert it to a regular 'C' string 
			bGotIt = CFStringGetCString( cfsConfigAvail, pPIConfigAvail, kMaxPlugInAttributeStrLen, kCFStringEncodingMacRoman );
			if (bGotIt == false) throw( (sInt32)ePluginConfigAvailNotFound );
		}
		else
		{
			::strcpy(pPIConfigAvail, "Not Available");
		}

		// Get the plugin configfile
		// if it does not exist then we use a default of "Not Available"
		pPIConfigFile = (char *)::malloc( kMaxPlugInAttributeStrLen );
		if ( pPIConfigFile == nil ) throw( (sInt32)eMemoryError );
		cfsConfigFile = (CFStringRef)::CFDictionaryGetValue( plInfo, kPluginConfigFileStr );
		if (cfsConfigFile)
		{
			// Convert it to a regular 'C' string 
			bGotIt = CFStringGetCString( cfsConfigFile, pPIConfigFile, kMaxPlugInAttributeStrLen, kCFStringEncodingMacRoman );
			if (bGotIt == false) throw( (sInt32)ePluginConfigFileNotFound );
		}
		else
		{
			::strcpy(pPIConfigFile, "Not Available");
		}

		// Get the plugin name
		cfsName = (CFStringRef)::CFDictionaryGetValue( plInfo, kPluginNameStr );
		if ( cfsName == nil ) throw( (sInt32)ePluginNameNotFound );

		pPIName = (char *)::malloc( kMaxPlugInAttributeStrLen );
		if ( pPIName == nil ) throw( (sInt32)eMemoryError );

		// Convert it to a regular 'C' string 
		bGotIt = CFStringGetCString( cfsName, pPIName, kMaxPlugInAttributeStrLen, kCFStringEncodingMacRoman );
		if (bGotIt == false) throw( (sInt32)ePluginNameNotFound );

		// Check for plugin handler
		if ( gPlugins == nil ) throw( (sInt32)ePluginHandlerNotLoaded );

		// Do we already have a plugin with this prefix registered?
		siResult = gPlugins->IsPresent( pPIName );
		if (siResult != kPlugInNotFound) throw( (sInt32)ePluginNameNotFound );

#ifdef vDEBUB
	::printf( "Complete Info.plist is:\n" );
	::CFShow( plInfo );
	::fflush( stdout );
#endif

		cfaFactories = ::CFPlugInFindFactoriesForPlugInTypeInPlugIn( kModuleTypeUUID, plgThis );
		if ( cfaFactories == nil ) throw ( (sInt32)eNoPluginFactoriesFound );
		if (::CFArrayGetCount ( cfaFactories ) == 0) throw( (sInt32)eNoPluginFactoriesFound );

		cfuuidFactory = (CFUUIDRef)::CFArrayGetValueAtIndex( cfaFactories, 0 );

		try
		{
			fccPlugInSignature = ::rand();

			DBGLOG1( kLogApplication, "Loading plugin: \"%s\"", pPIName );

			cpPlugin = new CServerPlugin( plgThis, cfuuidFactory, fccPlugInSignature, ulVers, pPIName );
			if ( cpPlugin != nil )
			{
				gPlugins->AddPlugIn( pPIName, pPIVersion, pPIConfigAvail, pPIConfigFile, fccPlugInSignature, cpPlugin );

				cpPlugin->Validate( pPIVersion, fccPlugInSignature );

				SRVRLOG2( kLogApplication, "Plugin \"%s\", Version \"%s\", loaded successfully.", pPIName, pPIVersion );
				DBGLOG3( kLogApplication, "Plugin \"%s\", Configure HI \"%s\", can be used to configure PlugIn with file \"%s\".", pPIName, pPIConfigAvail, pPIConfigFile );

				pPIName			= nil;
				pPIVersion		= nil;
				pPIConfigAvail	= nil;
				pPIConfigFile	= nil;
				cpPlugin		= nil;
			}
			else
			{
				ERRORLOG1( kLogApplication, "ERROR: \"%s\" plugin _FAILED_ to loaded.", pPIName );
			}

			siResult = eDSNoErr;
		}

		catch( sInt32 err )
		{
			ERRORLOG1( kLogPlugin, "*** Error in Plugin path = %s.", path );
			ERRORLOG2( kLogPlugin, "*** Error attempting to load plugin %s.  Error = %ld", pPIName, err );
		}
	}

	catch( sInt32 err )
	{
		siResult = err;
	}

	if ( pPIVersion != nil )
	{
		free( pPIVersion );
		pPIVersion = nil;
	}

	if ( pPIName != nil )
	{
		free( pPIName );
		pPIName = nil;
	}

	if ( plgThis != nil )
	{
		::CFRelease( plgThis );
		plgThis = nil;
	}

	if ( cfaFactories != nil )
	{
		::CFRelease( cfaFactories );
		cfaFactories = nil;
	}

#if DEBUG
	// Putting these here to flush any debugging output.
	::fflush( stdout );
	::fflush( stderr );
#endif

	return( siResult );

} // ProcessURL


// ----------------------------------------------------------------------------
//	* Validate()
//
// ----------------------------------------------------------------------------

sInt32 CServerPlugin::Validate ( const char *inVersionStr, const uInt32 inSignature )
{
	return( mInstance->validate( mInstance, inVersionStr, inSignature ) );
} // Validate


// ----------------------------------------------------------------------------
//	* PeriodicTask ()
//
// ----------------------------------------------------------------------------

sInt32 CServerPlugin::PeriodicTask ( void )
{
	return( mInstance->periodicTask( mInstance ) );
} // PeriodicTask


// ----------------------------------------------------------------------------
//	* Initialize ()
//
// ----------------------------------------------------------------------------

sInt32 CServerPlugin::Initialize ( void )
{
	return( mInstance->initialize( mInstance ) );
} // Initialize


// ----------------------------------------------------------------------------
//	* ProcessRequest()
//
// ----------------------------------------------------------------------------

sInt32 CServerPlugin::ProcessRequest ( void *inData )
{
        return( mInstance->processRequest( mInstance, inData ) );
}  // ProcessRequest


// ----------------------------------------------------------------------------
//	* SetPluginState()
//
// ----------------------------------------------------------------------------

sInt32 CServerPlugin::SetPluginState ( const uInt32 inState )
{
    return( mInstance->setPluginState( mInstance, inState ) );
}  // SetPluginState


// ----------------------------------------------------------------------------
//	* GetPluginName()
//
// ----------------------------------------------------------------------------

char* CServerPlugin::GetPluginName ( void )
{
	return( fPlugInName );
}  // GetPluginName


// ----------------------------------------------------------------------------
//	* GetSignature()
//
// ----------------------------------------------------------------------------

FourCharCode CServerPlugin::GetSignature ( void )
{
	return( fPlugInSignature );
}  // GetSignature

// ----------------------------------------------------------------------------
//	* ProcessConfigurePlugin()
//
// ----------------------------------------------------------------------------

sInt32 CServerPlugin::ProcessConfigurePlugin ( void )
{
	sInt32			siResult			= -1;
	CConfigurePlugin  *cpPlugin			= nil;
	FourCharCode	fccPlugInSignature	= 0;

	try
	{
			fccPlugInSignature = ::rand();

			DBGLOG( kLogApplication, "Processing Configure plugin." );

			cpPlugin = new CConfigurePlugin( fccPlugInSignature, "Configure" );
			if ( cpPlugin != nil )
			{
				gPlugins->AddPlugIn( "Configure", COSUtils::GetStringFromList( kSysStringListID, kStrVersion ), "Not Available", "Not Available", fccPlugInSignature, cpPlugin );

				cpPlugin->Validate( COSUtils::GetStringFromList( kSysStringListID, kStrVersion ), fccPlugInSignature );

				SRVRLOG1( kLogApplication, "Plugin \"Configure\", Version \"%s\", processed successfully.", COSUtils::GetStringFromList( kSysStringListID, kStrVersion ) );

				cpPlugin		= nil;
			}
			else
			{
				ERRORLOG( kLogApplication, "ERROR: \"Configure\" plugin _FAILED_ to process." );
			}

			siResult = eDSNoErr;
	}

	catch( sInt32 err )
	{
		siResult = err;
        ERRORLOG1( kLogPlugin, "*** Error attempting to process Configure plugin .  Error = %ld", err );
	}

	return( siResult );

} // ProcessConfigurePlugin

