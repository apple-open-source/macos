/*
 *  CRFPlugin.cpp
 *  DSNSLPlugins
 *
 *  Created by Kevin Arnold on Thu Feb 28 2002.
 *  Copyright (c) 2002 Apple Computer. All rights reserved.
 *
 */

#include "CRFPlugin.h"

#include "CNSLHeaders.h"

#include "CRFPlugin.h"
#include "CRFServiceLookupThread.h"
#include "TGetCFBundleResources.h"

#define kCommandParamsID				128
#define kRecentsNameStrID				1
#define kFavoritesNameStrID				2

const CFStringRef	gBundleIdentifier = CFSTR("com.apple.DirectoryService.RecentsFavorites");
const char*			gProtocolPrefixString = "RecentsFavorites";
//96E5B874-2C87-11D6-A741-0003934FB010
extern "C" {
CFUUIDRef ModuleFactoryUUID = CFUUIDGetConstantUUIDWithBytes ( NULL, \
								0x96, 0xE5, 0xB8, 0x74, 0x2C, 0x87, 0x11, 0xD6, \
								0xA7, 0x41, 0x00, 0x03, 0x93, 0x4F, 0xB0, 0x10 );

}

static CDSServerModule* _Creator ( void )
{
	DBGLOG( "Creating new Recents Favorites Plugin\n" );
    return( new CRFPlugin );
}

CDSServerModule::tCreator CDSServerModule::sCreator = _Creator;

CRFPlugin::CRFPlugin( void )
    : CNSLPlugin()
{
	DBGLOG( "CRFPlugin::CRFPlugin\n" );
    mRecentServersFolderName = NULL;
    mFavoritesServersFolderName = NULL;
}

CRFPlugin::~CRFPlugin( void )
{
	DBGLOG( "CRFPlugin::~CRFPlugin\n" );
    
    if ( mRecentServersFolderName )
        ::CFRelease( mRecentServersFolderName );
    
    if ( mFavoritesServersFolderName )
        ::CFRelease( mFavoritesServersFolderName );
}

sInt32 CRFPlugin::InitPlugin( void )
{
    char				resBuff[256];
    SInt32				len;
    sInt32				siResult	= eDSNoErr;
	DBGLOG( "CRFPlugin::InitPlugin\n" );
    
    len = OurResources()->GetIndString( resBuff, kCommandParamsID, kRecentsNameStrID );
    
    if ( len > 0 )
    {
        BlockMove( resBuff, &resBuff[1], len+1 );
        resBuff[0] = 0x09;
        mRecentServersFolderName = ::CFStringCreateWithCString( NULL, resBuff, kCFStringEncodingUTF8 );
    }
    else
    {
        siResult = kNSLBadReferenceErr;
        DBGLOG( "CRFPlugin::InitPlugin couldn't load a resource Recent Servers folder name, len=%ld\n", len );
    }
    
    len = OurResources()->GetIndString( resBuff, kCommandParamsID, kFavoritesNameStrID );
    
    if ( len > 0 )
    {
        BlockMove( resBuff, &resBuff[1], len+1 );
        resBuff[0] = 0x09;
        mFavoritesServersFolderName = ::CFStringCreateWithCString( NULL, resBuff, kCFStringEncodingUTF8 );
    }
    else
    {
        siResult = kNSLBadReferenceErr;
        DBGLOG( "CRFPlugin::InitPlugin couldn't load a resource Favorites Servers folder name, len=%ld\n", len );
    }
    
    return siResult;
}

CFStringRef CRFPlugin::GetBundleIdentifier( void )
{
    return gBundleIdentifier;
}

// this is used for top of the node's path "NSL"
const char*	CRFPlugin::GetProtocolPrefixString( void )
{		
    return gProtocolPrefixString;
}

Boolean CRFPlugin::IsLocalNode( const char *inNode )
{
    Boolean result = false;
    
    return result;
}

#if SUPPORT_WRITE_ACCESS
sInt32 CRFPlugin::OpenRecord ( sOpenRecord *inData )
{
    sInt32					siResult			= eDSNoErr;		// by default we don't Open records
    const void*				dictionaryResult	= NULL;
    CNSLDirNodeRep*			nodeDirRep			= NULL;
	tDataNodePtr			pRecName			= NULL;
	tDataNodePtr			pRecType			= NULL;
    char				   *pNSLRecType			= NULL;
    
    DBGLOG( "CRFPlugin::OpenRecord called\n" );
    if( !::CFDictionaryGetValueIfPresent( mOpenDirNodeRefTable, (const void*)inData->fInNodeRef, &dictionaryResult ) )
    {
        DBGLOG( "CRFPlugin::OpenRecord called but we couldn't find the nodeDirRep!\n" );
        return eDSInvalidNodeRef;
    }
    
    if ( !::CFDictionaryContainsKey( mOpenDirNodeRefTable, (const void*)inData->fOutRecRef ) )
    {
        CFMutableDictionaryRef		newService = ::CFDictionaryCreateMutable( NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
        ThrowThisIfNULL_( newService, eDSAllocationFailed );
        
        ::CFDictionaryAddValue( mOpenDirNodeRefTable, (void*)inData->fOutRecRef, (void*)newService );
        
        nodeDirRep = (CNSLDirNodeRep*)dictionaryResult;
        
		pRecType = inData->fInRecType;
		ThrowThisIfNULL_( pRecType, eDSNullRecType );

		pRecName = inData->fInRecName;
		ThrowThisIfNULL_( pRecName, eDSNullRecName );

        pNSLRecType = CreateNSLTypeFromRecType( (char*)pRecType->fBufferData );
		ThrowThisIfNULL_( pNSLRecType, eDSInvalidRecordType );

        if ( pNSLRecType )
        {
            if ( getenv("NSLDEBUG") )
            {
                DBGLOG( "CRFPlugin::OpenRecord, CreateNSLTypeFromRecType returned pNSLRecType:%s\n", pNSLRecType );
                DBGLOG( "dictionary contents before:\n");
                CFShow( newService );
            }
            
            CFStringRef		keyRef, valueRef;
            
            // add node name
            keyRef = ::CFStringCreateWithCString( kCFAllocatorDefault, kDS1AttrLocation, kCFStringEncodingUTF8 );
            valueRef = nodeDirRep->GetNodeName();
            
            if ( !CFDictionaryContainsKey( newService, keyRef ) )
                ::CFDictionaryAddValue( newService, keyRef, valueRef );
                
            // add record name
            keyRef = ::CFStringCreateWithCString( kCFAllocatorDefault, kDSNAttrRecordName, kCFStringEncodingUTF8 );
            valueRef = ::CFStringCreateWithCString( kCFAllocatorDefault, (char*)pRecName->fBufferData, kCFStringEncodingUTF8 );
            if ( !CFDictionaryContainsKey( newService, keyRef ) )
                ::CFDictionaryAddValue( newService, keyRef, valueRef );

            ::CFRelease( keyRef );
            ::CFRelease( valueRef );
            
            // add record type
            keyRef = ::CFStringCreateWithCString( kCFAllocatorDefault, kDS1AttrServiceType, kCFStringEncodingUTF8 );
            valueRef = ::CFStringCreateWithCString( kCFAllocatorDefault, pNSLRecType, kCFStringEncodingUTF8 );

            if ( !CFDictionaryContainsKey( newService, keyRef ) )
                ::CFDictionaryAddValue( newService, keyRef, valueRef );

            ::CFRelease( keyRef );
            ::CFRelease( valueRef );
            
            if ( getenv("NSLDEBUG") )
            {
                DBGLOG( "dictionary contents after:\n");
                CFShow( newService );

                DBGLOG( "CRFPlugin::OpenRecord, finished intial creation of opened service dictionary\n" );
                if ( getenv( "NSLDEBUG" ) )
                    ::CFShow( newService );
            }
            
            free( pNSLRecType );
        }

    }
    else
    {
        // do we want to allow two openings of a record?  Or is this not possible as we should be getting a unique
        // ref assigned to this open?...
    }
    
    return( siResult );
}	// OpenRecord

sInt32 CRFPlugin::CloseRecord ( sCloseRecord *inData )
{
    sInt32					siResult			= eDSNoErr;		// by default we don't Close records

    DBGLOG( "CRFPlugin::CloseRecord called\n" );
    
    if ( ::CFDictionaryContainsKey( mOpenDirNodeRefTable, (const void*)inData->fInRecRef ) )
    {
        ::CFDictionaryRemoveValue( mOpenDirNodeRefTable, (void*)inData->fInRecRef );
    }
    else
    {
        DBGLOG( "CRFPlugin::CloseRecord called but the record wasn't found!\n" );
        return eDSRecordNotFound;
    }

    return( siResult );
}	// CloseRecord

sInt32 CRFPlugin::CreateRecord ( sCreateRecord *inData )
{
    sInt32					siResult			= eDSNoErr;		// by default we don't create records
    const void*				dictionaryResult	= NULL;

    DBGLOG( "CRFPlugin::CreateRecord called\n" );
	Try_
	{
        if( !::CFDictionaryGetValueIfPresent( mOpenDirNodeRefTable, (const void*)inData->fInNodeRef, &dictionaryResult ) )
        {
            DBGLOG( "CRFPlugin::CreateRecord called but we couldn't find the nodeDirRep!\n" );
            return eDSInvalidNodeRef;
        }
        
        if ( !::CFDictionaryContainsKey( mOpenDirNodeRefTable, (const void*)inData->fOutRecRef ) )
        {
            sOpenRecord		openRecData = { kOpenRecord, inData->fResult, inData->fInNodeRef, inData->fInRecType, inData->fInRecName, inData->fOutRecRef };
            siResult = OpenRecord( &openRecData );
        }
	}

	Catch_ ( err )
	{
		siResult = err;
	}
    
    return( siResult );
}	// CreateRecord

sInt32 CRFPlugin::DeleteRecord ( sDeleteRecord *inData )
{
    sInt32						siResult			= eDSNoErr;
    
    if ( inData->fInRecRef && ::CFDictionaryContainsKey( mOpenDirNodeRefTable, (void*)inData->fInRecRef ) )
    {
        CFMutableDictionaryRef serviceToDeregister = (CFMutableDictionaryRef)::CFDictionaryGetValue( mOpenDirNodeRefTable, (void*)inData->fInRecRef );
        siResult = DeregisterService( serviceToDeregister );
    }
    else
    {
        siResult = eDSInvalidRecordRef;
        DBGLOG( "CRFPlugin::FlushRecord called but with no value in fOurRecRef.\n" );
    }
    
    return( siResult );
}	// DeleteRecord

sInt32 CRFPlugin::FlushRecord ( sFlushRecord *inData )
{
    sInt32					siResult			= eDSNoErr;

	Try_
	{
        if ( inData->fInRecRef && ::CFDictionaryContainsKey( mOpenDirNodeRefTable, (void*)inData->fInRecRef ) )
        {
            CFMutableDictionaryRef		serviceToRegister = (CFMutableDictionaryRef)::CFDictionaryGetValue( mOpenDirNodeRefTable, (void*)inData->fInRecRef );
            DBGLOG( "CRFPlugin::FlushRecord calling RegisterService with the following service:\n" );
//            if ( getenv( "NSLDEBUG" ) )
//                ::CFShow( serviceToRegister );
                
            siResult = RegisterService( serviceToRegister );
//            ::CFRelease( serviceToRegister );
        }
        else
        {
            siResult = eDSInvalidReference;
            DBGLOG( "CRFPlugin::FlushRecord called but with no value in fOurRecRef.\n" );
        }
	}

	Catch_ ( err )
	{
		siResult = err;
	}

    return( siResult );
}	// DeleteRecord

sInt32 CRFPlugin::AddAttributeValue ( sAddAttributeValue *inData )
{
    sInt32					siResult			= eDSNoErr;

    DBGLOG( "CRFPlugin::AddAttributeValue called\n" );
	Try_
	{
        if ( inData->fInRecRef && ::CFDictionaryContainsKey( mOpenDirNodeRefTable, (void*)inData->fInRecRef ) )
        {
            CFMutableDictionaryRef		serviceToManipulate = (CFMutableDictionaryRef)::CFDictionaryGetValue( mOpenDirNodeRefTable, (void*)inData->fInRecRef );
    
            CFStringRef		keyRef, valueRef;
            CFTypeRef		existingValueRef = NULL;

            keyRef = ::CFStringCreateWithCString( kCFAllocatorDefault, inData->fInAttrType->fBufferData, kCFStringEncodingUTF8 );

            valueRef = ::CFStringCreateWithCString( kCFAllocatorDefault, inData->fInAttrValue->fBufferData, kCFStringEncodingUTF8 );

            if ( ::CFDictionaryGetValueIfPresent( serviceToManipulate, keyRef, &existingValueRef ) && existingValueRef && ::CFGetTypeID( existingValueRef ) == ::CFArrayGetTypeID() )
            {
                // this key is already represented by an array of values, just append this latest one
                ::CFArrayAppendValue( (CFMutableArrayRef)existingValueRef, valueRef );
            }
            else if ( existingValueRef && ::CFGetTypeID( existingValueRef ) == ::CFStringGetTypeID() )
            {
                // is this value the same as what we want to add?  If so skip, otherwise make it an array
                if ( ::CFStringCompare( (CFStringRef)existingValueRef, valueRef, 0 ) != kCFCompareEqualTo )
                {
                    // this key was represented by a string, we need to swap it with an new array with the two values
                    CFStringRef			oldStringRef = (CFStringRef)existingValueRef;
                    CFMutableArrayRef	newArrayRef = ::CFArrayCreateMutable( NULL, 0, &kCFTypeArrayCallBacks );
                    
                    ::CFArrayAppendValue( newArrayRef, oldStringRef );
                    ::CFArrayAppendValue( newArrayRef, valueRef );
                    
                    ::CFDictionaryRemoveValue(  serviceToManipulate, keyRef );
                    
                    ::CFDictionaryAddValue( serviceToManipulate, keyRef, newArrayRef );
                }
            }
            else
            {
                // nothing already there, we'll just add this string to the dictionary
                ::CFDictionaryAddValue( serviceToManipulate, keyRef, valueRef );
            }

            ::CFRelease( keyRef );
            ::CFRelease( valueRef );
        }
        else
        {
            siResult = eDSInvalidRecordRef;
            DBGLOG( "CRFPlugin::AddAttributeValue called but with no value in fOurRecRef.\n" );
        }
	}

	Catch_ ( err )
	{
		siResult = err;
	}

    return( siResult );
}

sInt32 CRFPlugin::RemoveAttribute ( sRemoveAttribute *inData )
{
    sInt32					siResult			= eDSNoErr;

	Try_
	{
        if ( inData->fInRecRef && ::CFDictionaryContainsKey( mOpenDirNodeRefTable, (void*)inData->fInRecRef ) )
        {
            CFMutableDictionaryRef		serviceToManipulate = serviceToManipulate;
    
            CFStringRef		keyRef;

            keyRef = ::CFStringCreateWithCString( kCFAllocatorDefault, inData->fInAttribute->fBufferData, kCFStringEncodingUTF8 );

            ::CFDictionaryRemoveValue( serviceToManipulate, keyRef );
            ::CFRelease( keyRef );
        }
        else
        {
            siResult = eDSInvalidRecordRef;
            DBGLOG( "CRFPlugin::RemoveAttribute called but with no value in fOurRecRef.\n" );
        }
	}

	Catch_ ( err )
	{
		siResult = err;
	}

    return( siResult );
}

sInt32 CRFPlugin::RemoveAttributeValue ( sRemoveAttributeValue *inData )
{
    sInt32					siResult			= eDSNoErr;

	Try_
	{
        if ( inData->fInRecRef && ::CFDictionaryContainsKey( mOpenDirNodeRefTable, (void*)inData->fInRecRef ) )
        {
            CFMutableDictionaryRef		serviceToManipulate = (CFMutableDictionaryRef)::CFDictionaryGetValue( mOpenDirNodeRefTable, (void*)inData->fInRecRef );
    
            CFStringRef			keyRef;
            CFPropertyListRef	valueRef;

            keyRef = ::CFStringCreateWithCString( kCFAllocatorDefault, inData->fInAttrType->fBufferData, kCFStringEncodingUTF8 );

            valueRef = (CFPropertyListRef)::CFDictionaryGetValue( serviceToManipulate, keyRef );
            
            if ( valueRef && ::CFGetTypeID( valueRef ) == ::CFArrayGetTypeID() )
            {
                if ( (UInt32)::CFArrayGetCount( (CFMutableArrayRef)valueRef ) > inData->fInAttrValueID )
                    ::CFArrayRemoveValueAtIndex( (CFMutableArrayRef)valueRef, inData->fInAttrValueID );
                else
                    siResult = eDSIndexOutOfRange;
            }
            else if ( valueRef && ::CFGetTypeID( valueRef ) == ::CFStringGetTypeID() )
            {
                ::CFDictionaryRemoveValue( serviceToManipulate, keyRef );
            }
            else
                siResult = eDSInvalidAttrValueRef;
                
            if ( keyRef )
                ::CFRelease( keyRef );
            
            if ( valueRef )
                ::CFRelease( valueRef );
        }
        else
        {
            siResult = eDSInvalidRecordRef;
            DBGLOG( "CRFPlugin::RemoveAttributeValue called but with no value in fOurRecRef.\n" );
        }
	}

	Catch_ ( err )
	{
		siResult = err;
	}

    return( siResult );
}

sInt32 CRFPlugin::SetAttributeValue ( sSetAttributeValue *inData )
{
    sInt32					siResult			= eDSNoErr;

	Try_
	{
        if ( inData->fInRecRef && ::CFDictionaryContainsKey( mOpenDirNodeRefTable, (void*)inData->fInRecRef ) )
        {
            CFMutableDictionaryRef		serviceToManipulate = (CFMutableDictionaryRef)::CFDictionaryGetValue( mOpenDirNodeRefTable, (void*)inData->fInRecRef );
    
            CFStringRef			keyRef, valueRef;
            CFMutableArrayRef	attributeArrayRef;

            keyRef = ::CFStringCreateWithCString( kCFAllocatorDefault, inData->fInAttrType->fBufferData, kCFStringEncodingUTF8 );
            valueRef = ::CFStringCreateWithCString( kCFAllocatorDefault, inData->fInAttrValueEntry->fAttributeValueData.fBufferData, kCFStringEncodingUTF8 );

            attributeArrayRef = (CFMutableArrayRef)::CFDictionaryGetValue( serviceToManipulate, keyRef );
            
            if ( attributeArrayRef && ::CFGetTypeID( attributeArrayRef ) == ::CFArrayGetTypeID() )
            {
                if ( (UInt32)::CFArrayGetCount( (CFMutableArrayRef)attributeArrayRef ) > inData->fInAttrValueEntry->fAttributeValueID )
                    ::CFArraySetValueAtIndex( (CFMutableArrayRef)attributeArrayRef, inData->fInAttrValueEntry->fAttributeValueID, valueRef );
                else
                    siResult = eDSIndexOutOfRange;
            }
            else if ( attributeArrayRef && ::CFGetTypeID( attributeArrayRef ) == ::CFStringGetTypeID() )
            {
                ::CFDictionaryRemoveValue( serviceToManipulate, keyRef );
                ::CFDictionarySetValue( serviceToManipulate, keyRef, valueRef );
            }
            else
                siResult = eDSInvalidAttrValueRef;


            ::CFRelease( keyRef );
            ::CFRelease( valueRef );
        }
        else
        {
            siResult = eDSInvalidRecordRef;
            DBGLOG( "CRFPlugin::SetAttributeValue called but with no value in fOurRecRef.\n" );
        }
	}

	Catch_ ( err )
	{
		siResult = err;
	}

    return( siResult );
}
#endif // SUPPORT_WRITE_ACCESS

sInt32 CRFPlugin::HandleNetworkTransition( sHeader *inData )
{
    sInt32					siResult			= eDSNoErr;
    
    ClearOutAllNodes();			// clear these out
    StartNodeLookup();			// and then start all over
    
    return ( siResult );
}

void CRFPlugin::NewNodeLookup( void )
{
	DBGLOG( "CRFPlugin::NewNodeLookup\n" );
    
    AddNode( mRecentServersFolderName );
    AddNode( mFavoritesServersFolderName );
    AddNode( kLocalManagedDataName );
}

void CRFPlugin::NewServiceLookup( char* serviceType, CNSLDirNodeRep* nodeDirRep )
{
	DBGLOG( "CRFPlugin::NewServicesLookup\n" );
    
    CRFServiceLookupThread* newLookup = new CRFServiceLookupThread( this, serviceType, nodeDirRep );
    
    // if we have too many threads running, just queue this search object and run it later
    if ( OKToStartNewSearch() )
        newLookup->Resume();
    else
        QueueNewSearch( newLookup );
}


Boolean CRFPlugin::OKToOpenUnPublishedNode( const char* nodeName )
{
    return false;
}

#if SUPPORT_WRITE_ACCESS
sInt32 CRFPlugin::RegisterService( CFDictionaryRef service )
{
    sInt32		status = eDSNoErr;
    CFStringRef	urlRef = NULL;
    CFStringRef	scopeRef = NULL;
    
	DBGLOG( "CRFPlugin::RegisterService\n" );
    if ( getenv("NSLDEBUG") )
        CFShow(service);
        
    if ( service && ::CFDictionaryGetValueIfPresent( service, CFSTR(kDSNAttrURL), (const void**)&urlRef) )
    {
        UInt32		scopePtrLength;
        char*		scopePtr = NULL;
        
        DBGLOG( "CRFPlugin::RegisterService, check for specified location to register in\n" );
        if ( ::CFDictionaryGetValueIfPresent( service, CFSTR(kDS1AttrLocation), (const void**)&scopeRef))
        {
        	if ( CFGetTypeID( scopeRef ) == CFArrayGetTypeID() )
            {
                scopeRef = (CFStringRef)::CFArrayGetValueAtIndex( (CFArrayRef)scopeRef, 1 );	// just get the first one for now
            }
                
            scopePtrLength = ::CFStringGetMaximumSizeForEncoding( ::CFStringGetLength( scopeRef ), kCFStringEncodingUTF8 ) + 1;
        	scopePtr = (char*)malloc( scopePtrLength );

            ::CFStringGetCString( scopeRef, scopePtr, scopePtrLength, kCFStringEncodingUTF8 );
        }
        else
        {
            DBGLOG( "CRFPlugin::RegisterService, no location specified, using empty scope for default\n" );
            scopePtr = (char*)malloc(1);
            scopePtr[0] = '\0';
        }
        
        UInt32		urlPtrLength = ::CFStringGetMaximumSizeForEncoding( ::CFStringGetLength( urlRef ), kCFStringEncodingUTF8 ) + 1;
        char*		urlPtr = (char*)malloc( urlPtrLength );
        
        ::CFStringGetCString( urlRef, urlPtr, urlPtrLength, kCFStringEncodingUTF8 );
        
        if ( urlPtr[0] != '\0' )
        {
            CFMutableStringRef	attributeRef = ::CFStringCreateMutable( NULL, 0 );
            char*		attributePtr = NULL;
            
            ::CFDictionaryApplyFunction( service, AddToAttributeList, attributeRef );
            
            CFIndex		attributePtrSize = ::CFStringGetMaximumSizeForEncoding( attributeRef, kCFStringEncodingUTF8 ) + 1;
			
            attributePtr = (char*)malloc( attributePtrSize );
            attributePtr[0] = '\0';
            
            ::CFStringGetCString( attributeRef, attributePtr, attributePtrSize, kCFStringEncodingUTF8 );
            
            if ( attributePtr && attributePtr[strlen(attributePtr)-1] == ',' )
                attributePtr[strlen(attributePtr)] = '\0';
                
            ::CFRelease( attributeRef );
            
            status = DoSLPRegistration( scopePtr, urlPtr, attributePtr );		// we should be putting together an attribute list from the other key/value pairs in service
            
            if ( attributePtr )
                free( attributePtr );
        }
            
        free( scopePtr );
        free( urlPtr );
    }
    else
        status = eDSNullAttribute;
        
    return status;
}

sInt32 CRFPlugin::DeregisterService( CFDictionaryRef service )
{
    sInt32		status = eDSNoErr;
    CFStringRef	urlRef = NULL;
    CFStringRef	scopeRef = NULL;
    
    if ( service && ::CFDictionaryGetValueIfPresent( service, CFSTR(kDSNAttrURL), (const void**)&urlRef) )
    {
        UInt32		scopePtrLength;
        char*		scopePtr = NULL;
        
        if ( ::CFDictionaryGetValueIfPresent( service, CFSTR(kDS1AttrLocation), (const void**)&scopeRef))
        {
        	if ( CFGetTypeID( scopeRef ) == CFArrayGetTypeID() )
            {
                scopeRef = (CFStringRef)::CFArrayGetValueAtIndex( (CFArrayRef)scopeRef, 1 );	// just get the first one for now
            }

        	scopePtrLength = ::CFStringGetMaximumSizeForEncoding( ::CFStringGetLength( scopeRef ), kCFStringEncodingUTF8 ) + 1;
        	scopePtr = (char*)malloc( scopePtrLength );

            ::CFStringGetCString( scopeRef, scopePtr, scopePtrLength, kCFStringEncodingUTF8 );
        }
        else
        {
            scopePtr = (char*)malloc(1);
            scopePtr[0] = '\0';
        }
        
        UInt32		urlPtrLength = ::CFStringGetMaximumSizeForEncoding( ::CFStringGetLength( urlRef ), kCFStringEncodingUTF8 ) + 1;
        char*		urlPtr = (char*)malloc( urlPtrLength );
        
        ::CFStringGetCString( urlRef, urlPtr, urlPtrLength, kCFStringEncodingUTF8 );
        
        if ( urlPtr[0] != '\0' )
            status = DoSLPDeregistration( scopePtr, urlPtr );
                        
        free( scopePtr );
        free( urlPtr );
    }
    else
        status = eDSNullAttribute;
        
    return status;
}

#endif // SUPPORT_WRITE_ACCESS