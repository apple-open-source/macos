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
#include "CPluginConfig.h"
#include "CNetInfoPlugin.h"
#include "CSearchPlugin.h"
#include "CLDAPv3Plugin.h"

#include <stdlib.h>	// for rand()

#include <CoreFoundation/CFPlugIn.h>
#include <CoreFoundation/CFString.h>
#include <CoreFoundation/CFURL.h>

extern CPluginConfig	   *gPluginConfig;

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

static void			_DebugLog		( const char *inFormat, va_list inArgs );

static SvrLibFtbl	_Callbacks = { CServerPlugin::_RegisterNode, CServerPlugin::_UnregisterNode, _DebugLog };

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
	return InternalRegisterNode( inToken, inNodeList, inNodeType, false );
}

sInt32 CServerPlugin::InternalRegisterNode ( const uInt32 inToken, tDataList *inNodeList, eDirNodeType inNodeType, bool isProxyRegistration )
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
	pPluginPtr = gPlugins->GetPlugInPtr( inToken, !isProxyRegistration );	// if this is a proxy registration (i.e. lazy loading), its ok to be nil
	if ( pPluginPtr == nil && !isProxyRegistration )
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
				gNodeList->AddNode( pNodeName, pNodeList, kDirNodeType, pPluginPtr, inToken );

				DBGLOG1( kLogPlugin, "Registered Directory Node %s", pNodeName );
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
				gNodeList->AddNode( pNodeName, pNodeList, kLocalHostedType, pPluginPtr, inToken );
				SRVRLOG1( kLogPlugin, "Registered Locally Hosted Node %s", pNodeName );
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
				gNodeList->AddNode( pNodeName, pNodeList, kDefaultNetworkNodeType, pPluginPtr, inToken );
				DBGLOG1( kLogPlugin, "Registered Default Network Node %s", pNodeName );
			}
			else
			{
				siResult = kNodeAlreadyRegistered;
			}
			bDone = true;
		}
		
		if ( !bDone && ( inNodeType & ( kLocalNodeType | kSearchNodeType | kConfigNodeType | kContactsSearchNodeType | kNetworkSearchNodeType | kDHCPLDAPv3NodeType ) ) )
		//specific node type added here - don't check for duplicates here
		//need to always be able to get in here regardless of mutexes ie. don't call IsPresent()
		{
			// Get our copy of the node list
			//this pNodeList either gets consumed by the AddNode OR we free it
			pNodeList = dsBuildFromPathPriv( pNodeName, (char *)gNodeList->GetDelimiter() );
			
			// Add the node to the node list
			siResult = gNodeList->AddNode( pNodeName, pNodeList, inNodeType, pPluginPtr, inToken );
			if (siResult == 0) //specific return from AddNode ie. not really a DS status
			{
				::dsDataListDeallocatePriv( pNodeList );
				free(pNodeList);
				pNodeList = nil;
				siResult = kNodeAlreadyRegistered;
			}
			else
			{
				SRVRLOG1( kLogPlugin, "Registered node %s", pNodeName );
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

sInt32 CServerPlugin::_UnregisterNode ( const uInt32 inToken, tDataList *inNode )
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
			DBGLOG1( kLogPlugin, "Unregistered node %s", nodePath );
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
	SvrLibFtbl stTemp	= _Callbacks;
	fPlugInSignature	= 0;
	fPlugInName			= nil;
}

// ----------------------------------------------------------------------------
//	* CServerPlugin()
//
// ----------------------------------------------------------------------------

CServerPlugin::CServerPlugin ( FourCharCode inSig, const char *inName ) : mInstance( NULL )
{
	SvrLibFtbl stTemp	= _Callbacks;
    
	fPlugInSignature	= inSig;
	fPlugInName			= nil;
	if ( inName != nil )
	{
		fPlugInName = strdup(inName);
	}
}


// ----------------------------------------------------------------------------
//	* CServerPlugin()
//
// ----------------------------------------------------------------------------

CServerPlugin::CServerPlugin ( CFPlugInRef		inThis,
								 CFUUIDRef		inFactoryID,
								 FourCharCode	inSig,
								 uInt32			inVers,
								 const char	   *inName )
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
		ERRORLOG( kLogPlugin, "*** Error in CServerPlugin:CFPlugInInstanceCreate -- spUnknown == NULL." );
		throw( (sInt32) 'ecom' );
	}

	spUnknown->QueryInterface( spUnknown,
								::CFUUIDGetUUIDBytes( kModuleInterfaceUUID ),
								(LPVOID *)(&mInstance) );

	// Now we are done with IUnknown
	spUnknown->Release( spUnknown );

	if ( mInstance == NULL )
	{
		ERRORLOG( kLogPlugin, "*** Error in CServerPlugin:QueryInterface -- mInstance == NULL." );
		throw( (sInt32) 'ecom' );
	}

	stTemp.fSignature = inSig;
	mInstance->linkLibFtbl( mInstance, &stTemp );

	if ( inName != nil )
	{
		fPlugInName = strdup(inName);
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
	CFStringRef		cfsOKToLoadPluginLazily	= nil;
	CFArrayRef		cfaLazyNodesToRegister	= nil;
	bool			loadPluginLazily	= false;
	uInt32			callocLength		= 0;
	
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
		cfsVersion = (CFStringRef)::CFDictionaryGetValue( plInfo, kPluginShortVersionStr );
		if ( cfsVersion == nil ) throw( (sInt32)ePluginVersionNotFound );

		callocLength = (uInt32) CFStringGetMaximumSizeForEncoding(CFStringGetLength(cfsVersion), kCFStringEncodingUTF8) + 1;
		pPIVersion = (char *)::calloc( 1, callocLength );
		if ( pPIVersion == nil ) throw( (sInt32)eMemoryError );

		// Convert it to a regular 'C' string 
		bGotIt = CFStringGetCString( cfsVersion, pPIVersion, callocLength, kCFStringEncodingUTF8 );
		if (bGotIt == false) throw( (sInt32)ePluginVersionNotFound );

		// Get the plugin configavail
		// if it does not exist then we use a default of "Not Available"
		cfsConfigAvail = (CFStringRef)::CFDictionaryGetValue( plInfo, kPluginConfigAvailStr );
		if (cfsConfigAvail)
		{
			callocLength = (uInt32) CFStringGetMaximumSizeForEncoding(CFStringGetLength(cfsConfigAvail), kCFStringEncodingUTF8) + 1;
			pPIConfigAvail = (char *)::calloc( 1, callocLength );
			if ( pPIConfigAvail == nil ) throw( (sInt32)eMemoryError );
			// Convert it to a regular 'C' string 
			bGotIt = CFStringGetCString( cfsConfigAvail, pPIConfigAvail, callocLength, kCFStringEncodingUTF8 );
			if (bGotIt == false) throw( (sInt32)ePluginConfigAvailNotFound );
		}
		else
		{
			pPIConfigAvail = strdup("Not Available");
		}

		// Get the plugin configfile
		// if it does not exist then we use a default of "Not Available"
		cfsConfigFile = (CFStringRef)::CFDictionaryGetValue( plInfo, kPluginConfigFileStr );
		if (cfsConfigFile)
		{
			callocLength = (uInt32) CFStringGetMaximumSizeForEncoding(CFStringGetLength(cfsConfigFile), kCFStringEncodingUTF8) + 1;
			pPIConfigFile = (char *)::calloc( 1, callocLength );
			if ( pPIConfigFile == nil ) throw( (sInt32)eMemoryError );
			// Convert it to a regular 'C' string 
			bGotIt = CFStringGetCString( cfsConfigFile, pPIConfigFile, callocLength, kCFStringEncodingUTF8 );
			if (bGotIt == false) throw( (sInt32)ePluginConfigFileNotFound );
		}
		else
		{
			pPIConfigFile = strdup("Not Available");
		}

		// Get the plugin name
		cfsName = (CFStringRef)::CFDictionaryGetValue( plInfo, kPluginNameStr );
		if ( cfsName == nil ) throw( (sInt32)ePluginNameNotFound );

		callocLength = (uInt32) CFStringGetMaximumSizeForEncoding(CFStringGetLength(cfsName), kCFStringEncodingUTF8) + 1;
		pPIName = (char *)::calloc( 1, callocLength );
		if ( pPIName == nil ) throw( (sInt32)eMemoryError );

		// Convert it to a regular 'C' string 
		bGotIt = CFStringGetCString( cfsName, pPIName, callocLength, kCFStringEncodingUTF8 );
		if (bGotIt == false) throw( (sInt32)ePluginNameNotFound );

		// Check for plugin handler
		if ( gPlugins == nil ) throw( (sInt32)ePluginHandlerNotLoaded );

		// Do we already have a plugin with this prefix registered?
		siResult = gPlugins->IsPresent( pPIName );
		if (siResult == eDSNoErr) throw( (sInt32)ePluginAlreadyLoaded );

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

	// In order to do load plugins lazily, we want to know a couple of things:
	//	1) Is this plugin configured to be loaded lazily?
	//		We require that the plugin have the CFString "YES" as a value of key kPluginLazyNodesToRegStr ("DSOKToLoadLazily") in the
	//		plugin's plist.
	//
	//	2) Is this plugin configured to be disabled?  If so, we want to also load it lazily.
	//
	//	3) Does this plugin have a list of nodes in its plist that it wants us to publish
	//		for it?  If so, we will register those nodes on the plugin's behalf.
	//		
	//		Registering nodes in the plist requires the following data:
	//		a CFArray with key name kPluginLazyNodesToRegStr ("DSNodesToRegister").
	//			Each item in this array is a CFDictionary that represents a node.  Here are the types in that dictionary:
	//				kPluginNodeToRegisterType ("DSNodeToRegisterType")		which is a CFNumberRef (kCFNumberSInt32Type) matching the eDirNodeType mask
	//				kPluginNodeToRegisterPath ("DSNodeToRegisterPath")		which is a CFArray of CFStrings that represent the path.  Index 0 is the root
	//											of the path (i.e. "/BSD/Local" would be [BSD,Local]
	//	If not #1, we will just load this plugin like normal.  This way legacy plugins and 3rd party
	//	plugins will behave no differently.

			cfsOKToLoadPluginLazily = (CFStringRef)::CFDictionaryGetValue( plInfo, kPluginOKToLoadLazilyStr );
			
			if ( cfsOKToLoadPluginLazily && ::CFGetTypeID( cfsOKToLoadPluginLazily ) == ::CFStringGetTypeID() && CFStringCompare( (CFStringRef)cfsOKToLoadPluginLazily, CFSTR("YES"), kCFCompareCaseInsensitive ) == kCFCompareEqualTo )
				loadPluginLazily = true;
				
			if ( !loadPluginLazily )
				loadPluginLazily = ( gPluginConfig->GetPluginState( pPIName ) == kInactive );	// if not active, load lazily as well
				
			if ( loadPluginLazily )
			{
				gPlugins->AddPlugIn( pPIName, pPIVersion, pPIConfigAvail, pPIConfigFile, fccPlugInSignature, cpPlugin, plgThis, cfuuidFactory, ulVers );
				SRVRLOG2( kLogApplication, "Plugin \"%s\", Version \"%s\", is set to load lazily.", pPIName, pPIVersion );

				// now we see if this plugin has any nodes it wants us to register on its behalf.
				cfaLazyNodesToRegister = (CFArrayRef)::CFDictionaryGetValue( plInfo, kPluginLazyNodesToRegStr );
				
				// This should either be a CFString with one node to register or a CFArray of CFStrings to register.
				
				if ( cfaLazyNodesToRegister && ::CFGetTypeID( cfaLazyNodesToRegister ) == ::CFArrayGetTypeID() )
				{
					CFIndex		numNodesToRegister = ::CFArrayGetCount( cfaLazyNodesToRegister );
					
					for ( CFIndex i=0; i<numNodesToRegister; i++ )
					{
						CFDictionaryRef		nodeDataRef		= (CFDictionaryRef)::CFArrayGetValueAtIndex( cfaLazyNodesToRegister, i );
						
						if ( nodeDataRef && ::CFGetTypeID( nodeDataRef ) == ::CFDictionaryGetTypeID() )
						{
							CFNumberRef		nodeTypeRef = (CFNumberRef)::CFDictionaryGetValue( nodeDataRef, kPluginNodeToRegisterType );
							CFArrayRef		nodePathRef = (CFArrayRef)::CFDictionaryGetValue( nodeDataRef, kPluginNodeToRegisterPath );
							
							if ( nodeTypeRef && ::CFGetTypeID( nodeTypeRef ) == ::CFNumberGetTypeID() && nodePathRef && ::CFGetTypeID( nodePathRef ) == ::CFArrayGetTypeID() )
							{
								CFIndex				numPieces = ::CFArrayGetCount( nodePathRef );
								tDataListPtr		nodeToRegisterList = ::dsDataListAllocatePriv();
								char				cBuf[1024];
								CFStringRef			curStringRef = nil;
								eDirNodeType		dirNodeType;
								
								if ( !nodeToRegisterList )
									throw( (sInt32)eMemoryError );
								
								for ( CFIndex j=0; j<numPieces; j++ )
								{
									curStringRef = (CFStringRef)::CFArrayGetValueAtIndex( nodePathRef, j );
									
									if ( curStringRef && ::CFGetTypeID( curStringRef ) == ::CFStringGetTypeID() )
									{
										if ( CFStringGetCString( curStringRef, cBuf, sizeof(cBuf), kCFStringEncodingUTF8 ) )
										{
											::dsAppendStringToListAllocPriv( nodeToRegisterList, cBuf );
										}
										else
										{
											ERRORLOG3( kLogPlugin, "*** Error in Plugin: %s, it has data too large (> %d bytes!) for node %d in key DSNodeToRegisterType or DSNodeToRegisterPath", sizeof(cBuf), pPIName, j );
											::dsDataListDeallocatePriv( nodeToRegisterList );
											free( nodeToRegisterList );
											nodeToRegisterList = nil;
											throw((sInt32)ePlugInInitError);
										}
									}
									else
									{
										ERRORLOG2( kLogPlugin, "*** Error in Plugin: %s, it has malformed plist data for node %d in key DSNodeToRegisterPath", pPIName, j );
										::dsDataListDeallocatePriv( nodeToRegisterList );
										free( nodeToRegisterList );
										nodeToRegisterList = nil;
										throw((sInt32)ePlugInInitError);
									}
								}
								
								if ( CFNumberGetValue( nodeTypeRef, kCFNumberSInt32Type, &dirNodeType ) )
								{
									CServerPlugin::InternalRegisterNode( fccPlugInSignature, nodeToRegisterList, dirNodeType, true );	// we are proxing the plugin
								}
								else
									ERRORLOG2( kLogPlugin, "*** Error in Plugin: %s, it has malformed plist data for node %d in key DSNodeToRegisterType", pPIName, i );

								::dsDataListDeallocatePriv( nodeToRegisterList );
								free( nodeToRegisterList );
								nodeToRegisterList = nil;
							}
							else
							{
								ERRORLOG2( kLogPlugin, "*** Error in Plugin: %s, it has malformed plist data for node %d in key DSNodeToRegisterType or DSNodeToRegisterPath", pPIName, i );
								throw((sInt32)ePlugInInitError);
							}
						}
						else
						{
							ERRORLOG2( kLogPlugin, "*** Error in Plugin: %s, it has malformed plist data for node %d in key DSNodesToRegister", pPIName, i );
							throw((sInt32)ePlugInInitError);
						}
					} // for i<numNodesToRegister
				}
				else if ( cfaLazyNodesToRegister )
				{
					ERRORLOG2( kLogPlugin, "*** Error in Plugin: %s, it has malformed plist data for key %s", pPIName, kPluginLazyNodesToRegStr );
					throw((sInt32)ePlugInInitError);
				}
			}
			else
			{
				cpPlugin = new CServerPlugin( plgThis, cfuuidFactory, fccPlugInSignature, ulVers, pPIName );
				cpPlugin->Validate( pPIVersion, fccPlugInSignature );
				gPlugins->AddPlugIn( pPIName, pPIVersion, pPIConfigAvail, pPIConfigFile, fccPlugInSignature, cpPlugin );
				SRVRLOG2( kLogApplication, "Plugin \"%s\", Version \"%s\", loaded successfully.", pPIName, pPIVersion );
			}
			
			DBGLOG3( kLogApplication, "Plugin \"%s\", Configure HI \"%s\", can be used to configure PlugIn with file \"%s\".", pPIName, pPIConfigAvail, pPIConfigFile );

			pPIName			= nil;
			pPIVersion		= nil;
			pPIConfigAvail	= nil;
			pPIConfigFile	= nil;
			cpPlugin		= nil;

			siResult = eDSNoErr;
		} // try

		catch( sInt32 err )
		{
			ERRORLOG1( kLogPlugin, "*** Error in Plugin path = %s.", path );
			ERRORLOG2( kLogPlugin, "*** Error attempting to load plugin %s.  Error = %d", pPIName, err );
		}
	}

	catch( sInt32 err )
	{
		siResult = err;
	}

	if ( pPIConfigAvail != nil )
	{
		free( pPIConfigAvail );
		pPIConfigAvail = nil;
	}

	if ( pPIConfigFile != nil )
	{
		free( pPIConfigFile );
		pPIConfigFile = nil;
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
//	* Initialize ()
//
// ----------------------------------------------------------------------------

sInt32 CServerPlugin::Initialize ( void )
{
	return( mInstance->initialize( mInstance ) );
} // Initialize


// --------------------------------------------------------------------------------
//	* Configure ()
// --------------------------------------------------------------------------------

sInt32 CServerPlugin::Configure ( void )
{
	//return( mInstance->configure( mInstance ) );
    return(eDSNoErr);
} // Configure


// ----------------------------------------------------------------------------
//	* SetPluginState()
//
// ----------------------------------------------------------------------------

sInt32 CServerPlugin::SetPluginState ( const uInt32 inState )
{
    return( mInstance->setPluginState( mInstance, inState ) );
}  // SetPluginState


// ----------------------------------------------------------------------------
//	* PeriodicTask ()
//
// ----------------------------------------------------------------------------

sInt32 CServerPlugin::PeriodicTask ( void )
{
	return( mInstance->periodicTask( mInstance ) );
} // PeriodicTask


// ----------------------------------------------------------------------------
//	* ProcessRequest()
//
// ----------------------------------------------------------------------------

sInt32 CServerPlugin::ProcessRequest ( void *inData )
{
        return( mInstance->processRequest( mInstance, inData ) );
}  // ProcessRequest


// --------------------------------------------------------------------------------
//	* Shutdown ()
// --------------------------------------------------------------------------------

sInt32 CServerPlugin::Shutdown ( void )
{
	//return( mInstance->shutdown( mInstance ) );
    return(eDSNoErr);
} // Shutdown


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
//	* ProcessStaticPlugin()
//
// ----------------------------------------------------------------------------

sInt32 CServerPlugin::ProcessStaticPlugin ( const char* inPluginName, const char* inPluginVersion )
{
	sInt32				siResult			= -1;
	CServerPlugin	   *cpPlugin			= nil;
	FourCharCode		fccPlugInSignature	= 0;

	try
	{
        fccPlugInSignature = ::rand();

        DBGLOG1( kLogApplication, "Processing <%s> plugin.", inPluginName );

        //need to decide which static plugin to create here
        if (strcmp(inPluginName,"Configure") == 0)
        {
            cpPlugin = new CConfigurePlugin( fccPlugInSignature, inPluginName );
        }
        else if (strcmp(inPluginName,"NetInfo") == 0)
        {
            cpPlugin = new CNetInfoPlugin( fccPlugInSignature, inPluginName );
        }
        else if (strcmp(inPluginName,"LDAPv3") == 0)
        {
            cpPlugin = new CLDAPv3Plugin( fccPlugInSignature, inPluginName );
        }
        else if (strcmp(inPluginName,"Search") == 0)
        {
            cpPlugin = new CSearchPlugin( fccPlugInSignature, inPluginName );
        }
        if ( cpPlugin != nil )
        {
            gPlugins->AddPlugIn( inPluginName, inPluginVersion, "Not Available", "Not Available", fccPlugInSignature, cpPlugin );

            cpPlugin->Validate( inPluginVersion, fccPlugInSignature );

            SRVRLOG2( kLogApplication, "Plugin <%s>, Version <%s>, processed successfully.", inPluginName, inPluginVersion );
        }
        else
        {
            ERRORLOG1( kLogApplication, "ERROR: <%s> plugin _FAILED_ to process.", inPluginName );
        }

        siResult = eDSNoErr;
	}

	catch( sInt32 err )
	{
		siResult = err;
        ERRORLOG2( kLogPlugin, "*** Error attempting to process <%s> plugin .  Error = %ld", inPluginName, err );
	}

	return( siResult );

} // ProcessStaticPlugin


