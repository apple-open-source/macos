/*
 * Copyright (c) 2007 Apple Inc. All rights reserved.
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

#include "BSDConfigureNode.h"
#include "NISNode.h"
#include "BSDPlugin.h"
#include "CSharedData.h"

DSEventSemaphore	gKickBSDRequests;

BSDPlugin::BSDPlugin( FourCharCode inSig, const char *inName ) : BaseDirectoryPlugin( inSig, inName )
{
	// This must be called before any other NISNode methods.
	NISNode::InitializeGlobals();
	
	fFlatFilesNode = CFStringCreateWithFormat( kCFAllocatorDefault, NULL, CFSTR("%@/%@"), fPluginPrefixCF, CFSTR("local") );
	fNISDomainName = NISNode::CopyDomainName();
	
	if ( DSIsStringEmpty(fNISDomainName) == false )
		fNISDomainNode = CFStringCreateWithFormat( kCFAllocatorDefault, NULL, CFSTR("%@/%s"), fPluginPrefixCF, fNISDomainName );
	else
		fNISDomainNode = NULL;
	
	fCustomCallReadConfig = 'read';
	fCustomCallWriteConfig = 'writ';
}

BSDPlugin::~BSDPlugin( void )
{
	UnregisterNode( fFlatFilesNode, kDirNodeType );
	UnregisterNode( fFlatFilesNode, kLocalHostedType );
	
	if (fNISDomainNode != NULL)
		UnregisterNode( fNISDomainNode, kDirNodeType );

	DSCFRelease( fNISDomainNode );
	DSCFRelease( fFlatFilesNode );
	DSFree( fNISDomainName );
}

SInt32 BSDPlugin::Initialize( void )
{
	RegisterNode( fFlatFilesNode, kDirNodeType );
	RegisterNode( fFlatFilesNode, kLocalHostedType );
	RegisterNode( fFlatFilesNode, kBSDNodeType );
	
	if ( fNISDomainNode != NULL )
		RegisterNode( fNISDomainNode, kDirNodeType );

	return BaseDirectoryPlugin::Initialize();
}

SInt32 BSDPlugin::SetPluginState( const UInt32 inState )
{
	BaseDirectoryPlugin::SetPluginState( inState );
	
	if (inState & kActive)
	{
		gKickBSDRequests.PostEvent();
	}

	return eDSNoErr;
}

SInt32 BSDPlugin::PeriodicTask( void )
{
	static	int	checkTime	= 0;
	
	// we check if NIS is available on our periodic task, it is a lightweight check in theory
	// we only do this ever 5 minutes
	if ( (++checkTime) >= 10 )
	{
		if ( fNISDomainNode != NULL )
			NISNode::IsNISAvailable();
		
		checkTime = 0;
	}
	
	return eDSNoErr;
}
	
CFDataRef BSDPlugin::CopyConfiguration( void )
{
	CFMutableDictionaryRef	currentStateRef	= CFDictionaryCreateMutable( NULL, 0, &kCFCopyStringDictionaryKeyCallBacks, 
																		 &kCFTypeDictionaryValueCallBacks );
	CFDataRef   			xmlData			= NULL;

	if ( currentStateRef == NULL )
		return NULL;
		
	if ( DSIsStringEmpty(fNISDomainName) == false )
	{
		CFStringRef	cfNISDomainName = CFStringCreateWithCString( kCFAllocatorDefault, fNISDomainName, kCFStringEncodingUTF8 );
		
		if ( cfNISDomainName != NULL )
			CFDictionarySetValue( currentStateRef, CFSTR(kDS1AttrLocation), cfNISDomainName );
		
		char *nisServers = NISNode::CopyNISServers( fNISDomainName );
		if ( nisServers != NULL )
		{
			CFStringRef cfNISServers = CFStringCreateWithCString( kCFAllocatorDefault, nisServers, kCFStringEncodingUTF8 );
			if ( cfNISServers != NULL )
			{
				CFDictionarySetValue( currentStateRef, CFSTR(kDSStdRecordTypeServer), cfNISServers );

				DSCFRelease( cfNISServers );
			}
			
			DSFree( nisServers );
		}
		
		DSCFRelease( cfNISDomainName );
	}
	
	// convert the dict into a XML blob
	xmlData = CFPropertyListCreateXMLData( kCFAllocatorDefault, currentStateRef );

	DSCFRelease( currentStateRef );
	
	return xmlData;
}

bool BSDPlugin::NewConfiguration( const char *inData, UInt32 inLength )
{
	if ( CheckConfiguration(inData, inLength) == false )
		return false;
	
	// no need to check these, already been done in CheckConfiguration
	CFDataRef			xmlData		= CFDataCreate( kCFAllocatorDefault, (UInt8 *)inData, inLength );
	CFDictionaryRef		newStateRef = (CFDictionaryRef) CFPropertyListCreateFromXMLData( kCFAllocatorDefault, xmlData, 
																						 kCFPropertyListImmutable, NULL );
	char				*pTemp		= NULL;
	char				*pTemp2		= NULL;
	const char			*pNewDomainName	= NULL;
	const char			*pServers	= NULL;
	
	fBasePluginMutex.WaitLock();

	UnregisterNode( fNISDomainNode, kDirNodeType );

	DSFree( fNISDomainName );
	DSCFRelease( fNISDomainNode );

	CFStringRef newDomainName = (CFStringRef) CFDictionaryGetValue( newStateRef, CFSTR(kDS1AttrLocation) );
	if ( newDomainName != NULL && CFStringGetLength(newDomainName) )
		pNewDomainName = BaseDirectoryPlugin::GetCStringFromCFString( newDomainName, &pTemp );

	CFStringRef	nisServersRef = (CFStringRef) CFDictionaryGetValue( newStateRef, CFSTR(kDSStdRecordTypeServer) );
	if ( nisServersRef != NULL )
		pServers = BaseDirectoryPlugin::GetCStringFromCFString( nisServersRef, &pTemp2 );
	
	if ( pNewDomainName != NULL )
	{
		fNISDomainName = strdup( pNewDomainName );
		fNISDomainNode = CFStringCreateWithFormat( kCFAllocatorDefault, NULL, CFSTR("%@/%s"), fPluginPrefixCF, fNISDomainName );
		
		NISNode::SetDomainAndServers( pNewDomainName, pServers );

		RegisterNode( fNISDomainNode, kDirNodeType );
	}
	else
	{
		NISNode::SetDomainAndServers( NULL, NULL );
	}
	
	fBasePluginMutex.SignalLock();
	
	DSFree( pTemp );
	DSFree( pTemp2 );
	DSCFRelease( newStateRef );
	DSCFRelease( xmlData );
		
	return true;
}

bool BSDPlugin::CheckConfiguration( const char *inData, UInt32 inLength )
{
	CFDataRef			xmlData			= CFDataCreate( kCFAllocatorDefault, (UInt8 *)inData, inLength );
	CFDictionaryRef		newStateRef		= NULL;
	bool				bReturn			= false;
	
	if ( xmlData != NULL )
	{
		newStateRef = (CFDictionaryRef) CFPropertyListCreateFromXMLData( kCFAllocatorDefault, xmlData, kCFPropertyListImmutable, NULL );
		
		if ( newStateRef != NULL && CFGetTypeID(newStateRef) == CFDictionaryGetTypeID() )
		{
			CFStringRef newDomainName = (CFStringRef) CFDictionaryGetValue( newStateRef, CFSTR(kDS1AttrLocation) );
			if ( newDomainName != NULL && CFGetTypeID(newDomainName) != CFStringGetTypeID() )
				goto failure;
			
			CFStringRef	nisServersRef = (CFStringRef) CFDictionaryGetValue( newStateRef, CFSTR(kDSStdRecordTypeServer) );
			if ( nisServersRef != NULL && CFGetTypeID(nisServersRef) != CFStringGetTypeID()  )
				goto failure;
			
			bReturn = true;
		}
	}
	
failure:
	DSCFRelease( xmlData );
	DSCFRelease( newStateRef );
		
	return bReturn;
}

tDirStatus BSDPlugin::HandleCustomCall( sBDPINodeContext *pContext, sDoPlugInCustomCall *inData )
{
	tDirStatus	siResult = eNotHandledByThisNode;

	return siResult;
}

bool BSDPlugin::IsConfigureNodeName( CFStringRef inNodeName )
{
	if ( CFStringCompare(fPluginPrefixCF, inNodeName, 0) == kCFCompareEqualTo )
		return true;
	
	return false;
}

BDPIVirtualNode *BSDPlugin::CreateNodeForPath( CFStringRef inPath, uid_t inUID, uid_t inEffectiveUID )
{
	BDPIVirtualNode *pNewNode = NULL;
	
	// configure node, let's return our configure node if it's just our prefix
	if ( CFStringCompare(fPluginPrefixCF, inPath, 0) == kCFCompareEqualTo )
	{
		pNewNode = new BSDConfigureNode( inPath, inUID, inEffectiveUID );
	}
	else if ( CFStringCompare(fFlatFilesNode, inPath, 0) == kCFCompareEqualTo )
	{
		pNewNode = new FlatFileNode( inPath, inUID, inEffectiveUID );
	}
	else if ( fNISDomainNode != NULL && CFStringCompare(fNISDomainNode, inPath, 0) == kCFCompareEqualTo && NISNode::IsNISAvailable() )
	{
		pNewNode = new NISNode( inPath, fNISDomainName, inUID, inEffectiveUID );
	}

	return pNewNode;
}
