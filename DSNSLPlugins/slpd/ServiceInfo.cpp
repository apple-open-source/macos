/*
 *  ServiceInfo.cpp
 *  NSLPlugins
 *
 *  Created by imlucid on Wed Aug 23 2000.
 *  Copyright (c) 2000 Apple Computer. All rights reserved.
 *
 */
#include <stdio.h>
#include <stdlib.h>
//#include <LArray.h>
//#include <LArrayIterator.h>
#include <string.h>

#include "mslp_sd.h"
#include "slp.h"
#include "mslp.h"
#include "mslpd_store.h"
#include "mslp_dat.h"
#include "mslpd.h"

#include "ServiceInfo.h"
#include "SLPDefines.h"
#include "SLPComm.h"

#pragma mark -
#pragma mark - ServiceInfo
#pragma mark -
unsigned long ServiceInfo::mLatestAssignedSIRefNum = 0L;	// initalize this

ServiceInfo::ServiceInfo()
{
	mSIRefNum = ++mLatestAssignedSIRefNum;
	
	mNumInterestedParties = 1;	// whoever created us is interested!
	mScope = NULL;
	mAttributeList = NULL;
	mURL = NULL;
    mURLRef = NULL;
	mServiceType = NULL;
    mTimeOfFirstRegistration = 1;
    mTimeOfLastRegistrationUpdate = 1;
	mTimeOfExpiration = 1;		// init to non-zero so we don't do a divide by zero!
	mTimeToRegister = (UInt32)-1;
    mIPRegisteredFrom = 0;
	mSelfPtr = this;
}

ServiceInfo::~ServiceInfo()
{
	if ( mAttributeList != NULL )
	{
		free( mAttributeList );
		mAttributeList = NULL;
	}
	
	if ( mScope )
	{
		free( mScope );
		mScope = NULL;
	}
	
	if ( mURL != NULL )
	{
		free( mURL );
		mURL = NULL;
	}
	
    if ( mURLRef )
    {
        ::CFRelease( mURLRef );
        mURLRef = NULL;
    }
    
	if ( mServiceType )
	{
		free( mServiceType );
		mServiceType = NULL;
	}
}

#pragma mark -
#pragma mark public:
Boolean ServiceInfo::operator==( ServiceInfo& infoToCompare )
{
	Boolean urlsAreEqual = false;
	Boolean scopesAreEqual = false;
	
	if ( mURL && infoToCompare.mURL )
		urlsAreEqual = ( ::strcmp( mURL, infoToCompare.mURL ) == 0 );
		
	if ( urlsAreEqual )
		if ( mScope && infoToCompare.mScope )
			scopesAreEqual = ( ::strcmp( mScope, infoToCompare.mScope ) == 0 );

	return scopesAreEqual;
}

Boolean ServiceInfo::operator>( ServiceInfo& infoToCompare )
{
	Boolean urlIsGreater = false;
	int		compareValue = 0;
	
	if ( mURL && infoToCompare.mURL )
		compareValue = ( ::strcmp( mURL, infoToCompare.mURL ) > 0 );

	if ( compareValue == 0 && mScope && infoToCompare.mScope )
		urlIsGreater = ( ::strcmp( mScope, infoToCompare.mScope ) > 0 );
	else
		urlIsGreater = compareValue > 0;
		
	return urlIsGreater;
}

void ServiceInfo::AddInterest( void )
{
	mNumInterestedParties++;
}

void ServiceInfo::RemoveInterest( void )
{
#ifdef USE_EXCEPTIONS
	ThrowIfNULL_( mNumInterestedParties );
#endif
	if ( !mNumInterestedParties )
		return;
		
	mNumInterestedParties--;
	
	if ( mNumInterestedParties == 0 )
	{
		// this is the last object interested in us, time to go bye bye
		delete this;
	}
}

#pragma mark -

Boolean ServiceInfo::IsTimeToReRegister( void )
{
	Boolean		itsTime = false;
	UInt32		curTime = 0;
//	UInt32		timeBetweenRegisters = ((ServiceAgent::TheSA()->GetRegistrationLifetime())*100)/80;	// reregister in %80 of time
	
	curTime = GetCurrentTime();		// in seconds
	
	if ( curTime > mTimeToRegister )
		itsTime = true;

	return itsTime;
}

Boolean ServiceInfo::IsTimeToExpire( void )
{
	Boolean		itsTime = false;
	UInt32		curTime = 0;
//	UInt16		timeBetweenRegisters = ServiceAgent::TheSA()->GetDAHeartBeat();
	
	curTime = GetCurrentTime();

	if ( curTime > mTimeOfExpiration )
		itsTime = true;

	return itsTime;
}

void ServiceInfo::UpdateLastRegistrationTimeStamp( void )
{
	UInt32		curTime;

	curTime = GetCurrentTime();
	
	SetTimeOfLastRegistrationUpdate( curTime );
    mTimeOfExpiration = curTime + mLifeTime;
	mTimeToRegister = curTime + ((mLifeTime*80)/100);	// reregister in %80 of time
}

#pragma mark -
void ServiceInfo::SetURL( const char* url, UInt16 urlLen )
{
	if ( mURL != NULL )
		free( mURL );
	
	if ( url )
	{
        mURL = (char*)malloc( urlLen + 1 );
        ::memcpy( mURL, url, urlLen );
		mURL[urlLen] = '\0';
        
        if ( mURLRef )
            ::CFRelease( mURLRef );
            
//        mURLRef = ::CFStringCreateWithCString( NULL, mURL, CFStringGetSystemEncoding() );
        mURLRef = ::CFStringCreateWithCString( NULL, mURL, kCFStringEncodingUTF8 );
	}
	else
		mURL = NULL;
}

void ServiceInfo::SetServiceType( const char* serviceType, UInt16 serviceTypeLen )
{
#ifdef USE_EXCEPTIONS
	ThrowIfNULL_( serviceType );
#endif
	if ( !serviceType )
		return;
		
	if ( mServiceType )
		free( mServiceType );
	
	mServiceType = (char*)malloc( serviceTypeLen + 1 );
	::memcpy( mServiceType, serviceType, serviceTypeLen );
	mServiceType[serviceTypeLen] = '\0';
}

char* ServiceInfo::PtrToServiceType( UInt16* serviceTypeLen )
{
	if ( mServiceType == NULL )
		*serviceTypeLen = 0;
	else
		*serviceTypeLen = ::strlen( mServiceType );
		
	return mServiceType;
}

/*
char* ServiceInfo::PtrToServiceType( UInt16* serviceTypeLen )
{
	char*	serviceType = mURL;
	char*	eoServiceType;
	
	// now, how long is this?  basically the serviceType is everything up to the final :/ (or :portNum/).
	if ( mURL && (eoServiceType = ::strstr( mURL, ":/" )) != NULL )
		*serviceTypeLen = eoServiceType - serviceType;
	else if ( mURL )
	{
		eoServiceType = ::strstr( mURL, "/" );							// just look to the first forward slashe 
		
		while ( *eoServiceType != ':' && eoServiceType > serviceType )	// and walk back to the semi colon (port)
			eoServiceType--;
	
		if ( *eoServiceType == ':' )
			*serviceTypeLen = (eoServiceType - serviceType) -1;			// don't include the semi-colon
		else
			*serviceTypeLen = 0;
	}
	else
		*serviceTypeLen = 0;

	return serviceType;
}
*/
void ServiceInfo::SetScope( const char* scope, UInt16 scopeLen )
{
	if ( mScope )
		free( mScope );
	
	mScope = (char*)malloc( scopeLen + 1 );
#ifdef USE_EXCEPTIONS
	ThrowIfNULL_( mScope );
#endif
	if ( !mScope )
		return;
		
	::memcpy( mScope, scope, scopeLen );
	mScope[scopeLen] = '\0';
}

void ServiceInfo::AddAttribute( const char* attribute, UInt16 attributeLen )
{
	if ( attribute != NULL && *attribute != '\0' && attributeLen > 0 )
	{
		size_t newLength = attributeLen;
        
         if ( mAttributeList )
            newLength += ::strlen( mAttributeList );
		
		if ( mAttributeList != NULL )
			newLength += 1;	// this is for the comma
			
		char* newList = (char*)malloc( newLength +1 );
		
		if ( mAttributeList != NULL )
		{
			::strcpy( newList, mAttributeList );
			::strcat( newList, "," );
			
			free( mAttributeList );
		}
		else
            newList[0] = '\0';
            
		::strncat( newList, attribute, attributeLen );
		
		mAttributeList = newList;
	}
}

void ServiceInfo::RemoveAllAttributes( void )
{
	if ( mAttributeList != NULL )
	{
		free( mAttributeList );
		mAttributeList = NULL;
	}
}

Boolean ServiceInfo::URLMatches( char* url, UInt16 urlLen )
{
	Boolean		match = false;
	
	if ( urlLen > 0 && mURL )
		match = ( ::memcmp( url, mURL, (urlLen > strlen(mURL))?urlLen:strlen(mURL) ) == 0 );

	return match;
}

/*Boolean ServiceInfo::ServiceTypeMatches( const string& service )
{
	Boolean		match = false;
	
	if ( service.size() > 0 && mURL )
		match = ( ::strncmp( (Ptr)service.c_str(), mURL, max( service.size(), ::strlen(mURL) ) ) == 0 );

	return match;
}
*/
Boolean ServiceInfo::ServiceTypeMatches( char* serviceType, UInt16 serviceTypeLen )
{
	Boolean		match = false;
	
	if ( serviceTypeLen > 0 && mURL )
		match = ( ::memcmp( serviceType, mURL, serviceTypeLen ) == 0 && mURL[serviceTypeLen] == ':' );

	return match;
}

Boolean ServiceInfo::ScopeMatches( char* scope )
{
	Boolean match = false;
	
	if ( scope )
		match = ScopeMatches( scope, ::strlen( scope ) );
		
	return match;
}

Boolean ServiceInfo::ScopeMatches( char* scope, UInt16 scopeLen )
{
	Boolean		scopeMatch = false;
	
	if ( mScope && scope )
	{
		if ( ::memcmp( scope, mScope, (scopeLen>::strlen(mScope))?scopeLen:strlen(mScope) ) == 0 )
			scopeMatch = true;
	}
	
	return scopeMatch;
}

#pragma mark -
#pragma mark еее ServiceInfoComparator еее
#pragma mark -
#ifdef USE_SERVICE_INFO_COMPARATOR
ServiceInfoComparator::ServiceInfoComparator()
{
}
ServiceInfoComparator::~ServiceInfoComparator()
{
}

SInt32 ServiceInfoComparator::Compare(
	const void*		inItemOne,
	const void*		inItemTwo,
	UInt32			inSizeOne,
	UInt32			inSizeTwo) const
{
	SInt32			compareValue;
	ServiceInfo*	itemOne = (*(ServiceInfo**)inItemOne);
	ServiceInfo*	itemTwo = (*(ServiceInfo**)inItemTwo);
	UInt16			itemOneServiceTypeLen, itemTwoServiceTypeLen;
	Boolean			compareFinished = false;
	
	compareValue = ::memcmp( itemOne->PtrToServiceType( &itemOneServiceTypeLen ), itemTwo->PtrToServiceType( &itemTwoServiceTypeLen ), max(itemOneServiceTypeLen,itemTwoServiceTypeLen) );
	
	if ( compareValue == 0 )
	{
		compareValue = ::strcmp( itemOne->GetURLPtr(), itemTwo->GetURLPtr() );
	}
		
	return compareValue;
}

Boolean ServiceInfoComparator::IsEqualTo(
	const void*		inItemOne,
	const void*		inItemTwo,
	UInt32			inSizeOne,
	UInt32			inSizeTwo) const
{
	return (Compare(inItemOne, inItemTwo, inSizeOne, inSizeTwo) == 0);
}
								
#endif // #ifdef USE_SERVICE_INFO_COMPARATOR

#pragma mark -
#pragma mark some helper functions
OSStatus MakeSLPServiceInfoNotificationBuffer( SLPdMessageType messageType, ServiceInfo* service, UInt32* dataBufferLen, char** dataBuffer );

OSStatus MakeSLPServiceInfoAddedNotificationBuffer( ServiceInfo* service, UInt32* dataBufferLen, char** dataBuffer )
{
    return MakeSLPServiceInfoNotificationBuffer( kSLPRegisterURL, service, dataBufferLen, dataBuffer );
}

OSStatus MakeSLPServiceInfoRemovedNotificationBuffer( ServiceInfo* service, UInt32* dataBufferLen, char** dataBuffer )
{
    return MakeSLPServiceInfoNotificationBuffer( kSLPDeregisterURL, service, dataBufferLen, dataBuffer );
}

OSStatus MakeSLPServiceInfoNotificationBuffer( SLPdMessageType messageType, ServiceInfo* service, UInt32* dataBufferLen, char** dataBuffer )
{
    char*		curPtr = NULL;
    char*		url = service->GetURLPtr();
    char*		serviceType = NULL;
    char*		scope = service->GetScope();
    char*		attribute = service->GetAttributeList();
    
    UInt32		urlLen = 0;
    UInt32		serviceTypeLen = 0;
    UInt32		scopeLen = 0;
    UInt32		attributeLen = 0;
    LifeTime	lifeTime = service->GetLifeTime();
    UInt32		timeOfFirstRegistration = service->GetTimeOfFirstRegistration();
    UInt32		timeOfExpiration = service->GetTimeOfExpiration();
    UInt32		timeOfLastRegistration = service->GetTimeOfLastRegistrationUpdate();
    UInt32		ipAddressRegisteredFrom = service->GetIPAddressRegisteredFrom();
    
    OSStatus	status = noErr;
    
    if ( !url || !scope )
        status = -1;
    
    if ( !status )
    {
        urlLen = strlen(url);
        
        serviceType = service->PtrToServiceType(&(UInt16)serviceTypeLen);
        if ( serviceType )
            serviceTypeLen = strlen( serviceType );
            
        scopeLen = strlen(scope);
        attributeLen = service->GetAttributeListLen();
            
        *dataBufferLen = sizeof(SLPdMessageHeader) 
                        + sizeof(LifeTime)			// LifeTime
                        + sizeof(UInt32) 			// First Registration
                        + sizeof(UInt32) 			// Last Registration
                        + sizeof(UInt32)			// IP Address Registered From
                        + sizeof(UInt32)			// length of url
                        + urlLen					// url
                        + sizeof(UInt32)			// length of ServiceType
                        + serviceTypeLen			// serviceType
                        + sizeof(UInt32)			// length of scope
                        + scopeLen					// scope
                        + sizeof(UInt32)			// length of  attributes
                        + attributeLen				// attributes
                        + sizeof(UInt32); 			// Expiration
                        
        (*dataBuffer) = (char*)::malloc( *dataBufferLen );
    }
    
    if ( dataBuffer )
    {
        SLPdMessageHeader*	header = (SLPdMessageHeader*)(*dataBuffer);
        header->messageType = messageType;
        header->messageLength = *dataBufferLen;
        header->messageStatus = 0;

        curPtr = (char*)header + sizeof(SLPdMessageHeader);

        ::memcpy( curPtr, &lifeTime, sizeof(LifeTime) );
        curPtr += sizeof(LifeTime);

        ::memcpy( curPtr, &timeOfFirstRegistration, sizeof(UInt32) );
        curPtr += sizeof(UInt32);

        ::memcpy( curPtr, &timeOfExpiration, sizeof(UInt32) );
        curPtr += sizeof(UInt32);

        ::memcpy( curPtr, &ipAddressRegisteredFrom, sizeof(UInt32) );
        curPtr += sizeof(UInt32);

        ::memcpy( curPtr, &urlLen, sizeof(UInt32) );
        curPtr += sizeof(UInt32);

        ::memcpy( curPtr, url, urlLen );
        curPtr += urlLen;
        
        ::memcpy( curPtr, &serviceTypeLen, sizeof(UInt32) );
        curPtr += sizeof(UInt32);

        ::memcpy( curPtr, serviceType, serviceTypeLen );
        curPtr += serviceTypeLen;
        
        ::memcpy( curPtr, &scopeLen, sizeof(UInt32) );
        curPtr += sizeof(UInt32);

        ::memcpy( curPtr, scope, scopeLen );
        curPtr += scopeLen;
        
        ::memcpy( curPtr, &attributeLen, sizeof(UInt32) );
        curPtr += sizeof(UInt32);

        if ( attribute )
        {
            ::memcpy( curPtr, attribute, attributeLen );
            curPtr += attributeLen;
        }
        
        ::memcpy( curPtr, &timeOfLastRegistration, sizeof(UInt32) );
    }
    else
    {
        *dataBuffer = NULL;
        *dataBufferLen = 0;
    }

    return status;
}

const UInt32	kMinServiceInfoDataBufferSize = sizeof(SLPdMessageHeader) 
                                                + sizeof(UInt32)			// LifeTime
                                                + sizeof(UInt32) 			// First Registration
                                                + sizeof(UInt32) 			// Expiration
                                                + sizeof(UInt32)			// IP Address Registered From
                                                + sizeof(UInt32)			// length of url
                                                + sizeof(UInt32)			// length of ServiceType
                                                + sizeof(UInt32)			// length of scope
                                                + sizeof(UInt32);			// length of attributelist

OSStatus MakeServiceInfoFromNotificationBuffer( char* dataBuffer, UInt32 dataBufferLen, ServiceInfo** newServiceInfo )
{
    OSStatus		status = noErr;
    
    // first, is this buffer even big enough for a valid service info?
    if ( dataBuffer && dataBufferLen > kMinServiceInfoDataBufferSize )
    {
        char*			curPtr;
        UInt32			curDataLen;
        
        ServiceInfo*	newSI = new ServiceInfo();
        
        curPtr = dataBuffer + sizeof(SLPdMessageHeader);
        
        newSI->SetLifeTime( *((LifeTime*)curPtr) );
        curPtr += sizeof(LifeTime);
        
        newSI->SetTimeOfFirstRegistration( *((UInt32*)curPtr) );
        curPtr += sizeof(UInt32);

        newSI->SetTimeOfExpiration( *((UInt32*)curPtr) );
        curPtr += sizeof(UInt32);

        newSI->SetIPAddressRegisteredFrom( *((UInt32*)curPtr) );
        curPtr += sizeof(UInt32);

        curDataLen = *((UInt32*)curPtr);
        curPtr += sizeof(UInt32);
        
        newSI->SetURL( curPtr, curDataLen );
        curPtr += curDataLen;

        curDataLen = *((UInt32*)curPtr);
        curPtr += sizeof(UInt32);
        
        newSI->SetServiceType( curPtr, curDataLen );
        curPtr += curDataLen;
        
        curDataLen = *((UInt32*)curPtr);
        curPtr += sizeof(UInt32);
        
        newSI->SetScope( curPtr, curDataLen );
        curPtr += curDataLen;
        
        curDataLen = *((UInt32*)curPtr);
        curPtr += sizeof(UInt32);
        
        newSI->AddAttribute( curPtr, curDataLen );

        if ( dataBufferLen > (curPtr - dataBuffer) + curDataLen )		// if this is an old version, just skip
        {
            curPtr += curDataLen;
            
            newSI->SetTimeOfLastRegistrationUpdate( *((UInt32*)curPtr) );
        }
        
        *newServiceInfo = newSI;
    }
    else
        status = -1;		// something
    
    return status;
}

void CopyXIDFromRequestToCachedReply( ServiceLocationHeader* serviceReply, ServiceLocationHeader* originialRequest )
{
	*((UInt16*)&(serviceReply->byte11)) = *((UInt16*)&(originialRequest->byte11));		// copy the XID
}

// some helper functions
SLPReturnError CreateNewServiceInfo(	unsigned short lifeTime,
                                        const char* serviceURL,
                                        UInt16 serviceURLLen,
                                        const char* serviceType,
                                        UInt16 serviceTypeLen,
                                        const char* scope,
                                        UInt16 scopeLen,
                                        const char* attributes,
                                        UInt16 attributesLen,
                                        long ipRegisteredFrom,
                                        ServiceInfo** newServiceInfo )
{
	SLPReturnError	error = NO_ERROR;

	*newServiceInfo = new ServiceInfo();
	
#ifdef USE_EXCEPTIONS
	ThrowIfNULL_( *newServiceInfo );
#endif
	if ( !*newServiceInfo )
		return INTERNAL_ERROR;
		
	(*newServiceInfo)->SetLifeTime( lifeTime );
	(*newServiceInfo)->SetURL( serviceURL, serviceURLLen );
	(*newServiceInfo)->SetServiceType( serviceType, serviceTypeLen );
	(*newServiceInfo)->SetScope( scope, scopeLen );
	
	(*newServiceInfo)->AddAttribute( attributes, attributesLen );
	
    (*newServiceInfo)->SetIPAddressRegisteredFrom( ipRegisteredFrom );
    (*newServiceInfo)->SetTimeOfFirstRegistration( GetCurrentTime() );
    (*newServiceInfo)->UpdateLastRegistrationTimeStamp();
    
	return error;
}

/*SLPInternalError PullServiceTypeFromRequest( char* buffer, UInt16 length, string& service )
{
	return ( PullFromRequest( eServiceType, buffer, length, service ) );
}

SLPInternalError PullScopeFromRequest( char* buffer, UInt16 length, string& scope )
{
	return ( PullFromRequest( eScope, buffer, length, scope ) );
}

SLPInternalError PullAttributesFromRequest( char* buffer, UInt16 length, string& attributes )
{
	return ( PullFromRequest( eAttributes, buffer, length, attributes ) );
}

SLPInternalError PullFromRequest( RequestPiece piece, char* buffer, UInt16 length, string& subString )
{
	string serviceType, scope, attributes;
	
	SLPInternalError error = ParseOutServiceRequest( buffer, length, serviceType, scope, attributes );
	
	switch ( piece )
	{
		case eServiceType:
			subString = serviceType;
		break;
		
		case eScope:
			subString = scope;
		break;
		
		case eAttributes:
			subString = attributes;
		break;
		
		default:
			error = PROTOCOL_PARSE_ERROR;
		break;
	};
	
	return error;
}
*/
SLPReturnError ParseOutServiceRequest(	const char* buffer,
                                        UInt16 length,
                                        const char** serviceTypePtr,
                                        UInt16* serviceTypeLen,
                                        const char** scopeListPtr,
                                        UInt16* scopeListLen,
                                        const char** predicateListPtr,
                                        UInt16* predicateListLen )
{
	const char*		curPtr = buffer + GETHEADERLEN( buffer );	// pointing at length of prev response list
	UInt16			prListLen, slpSPILen;
	SLPReturnError	error = NO_ERROR;
	
	prListLen = *((UInt16*)curPtr);
	curPtr += prListLen + sizeof( prListLen );	// advance past the prev resp length and the whole list
	
	*serviceTypeLen = *((UInt16*)curPtr);
	curPtr += sizeof( *serviceTypeLen );
	
	*serviceTypePtr = curPtr;
		
	curPtr += *serviceTypeLen;				// advance past the serviceType
	
	*scopeListLen = *((UInt16*)curPtr);
	curPtr += sizeof( *scopeListLen );
	
	*scopeListPtr = curPtr;
		
	curPtr += *scopeListLen;				// advance past the scopeList
		
	*predicateListLen = *((UInt16*)curPtr);
	curPtr += sizeof( *predicateListLen );
	
	*predicateListPtr = curPtr;
		
	curPtr += *predicateListLen;				// advance past the predicateList
		
	// if we care about the SPI...
	slpSPILen = *((UInt16*)curPtr);
	curPtr += sizeof( slpSPILen );
/*	
	slpSPI = "";
	for ( int i=0; i<slpSPILen; i++ )
		slpSPI += curPtr[i];
*/	
	curPtr += slpSPILen;				// advance past the SPI
	
	if ( curPtr > buffer+length )
		error = PARSE_ERROR;	// we shouldn't have gone further than the length of the buffer!
		
	return error;
}
/*
SLPInternalError ParseOutServiceRequest(	char* buffer,
									UInt16 length,
									string& serviceType,
									string& scopeList,
									string& predicateList )
{
	char*		curPtr = buffer + GetSLPHeaderSize( (ServiceLocationHeader*)buffer );	// pointing at length of prev response list
	UInt16		messageLength = GetSLPLength( (ServiceLocationHeader*)buffer );
	UInt16		prListLen, serviceTypeLen, scopeListLen, predicateLen, slpSPILen;
	SLPInternalError	error = NO_ERROR;
	
	prListLen = *((UInt16*)curPtr);
	curPtr += prListLen + sizeof( prListLen );	// advance past the prev resp length and the whole list
	
	serviceTypeLen = *((UInt16*)curPtr);
	curPtr += sizeof( serviceTypeLen );
	
	serviceType = "";
	for ( int i=0; i<serviceTypeLen; i++ )
		serviceType += curPtr[i];
		
	curPtr += serviceTypeLen;				// advance past the serviceType
	
	scopeListLen = *((UInt16*)curPtr);
	curPtr += sizeof( scopeListLen );
	
	scopeList = "";
	for ( int i=0; i<scopeListLen; i++ )
		scopeList += curPtr[i];
		
	curPtr += scopeListLen;				// advance past the scopeList
		
	predicateLen = *((UInt16*)curPtr);
	curPtr += sizeof( predicateLen );
	
	predicateList = "";
	for ( int i=0; i<predicateLen; i++ )
		predicateList += curPtr[i];
		
	curPtr += predicateLen;				// advance past the predicateList
		
	// if we care about the SPI...
	slpSPILen = *((UInt16*)curPtr);
	curPtr += sizeof( slpSPILen );
    curPtr += slpSPILen;				// advance past the SPI
	
	if ( curPtr > buffer+length )
		error = PROTOCOL_PARSE_ERROR;	// we shouldn't have gone further than the length of the buffer!
		
	return error;
}
*/

Boolean TestForRegStylePacket( const char* buffer )
{
    // the way we are going to tell if this is from a Mac OS 9 -> 10.1.x system is that
    // at the same point in the file either we will be pointing at a URL Entry (valid Reg)
/*
4.3. URL Entries

      0                   1                   2                   3
      0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
     |   Reserved    |          Lifetime             |   URL Length  |
     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
*/	
    // or we will be pointing at a scope list segment:
/*
      0                   1                   2                   3
      0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
     |    Length of <scope-list>     |         <scope-list>          \
     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
*/

    // so we will just test that the first byte is zero and that the 2-3 bytes equal 1800 (the lifetime we used).
    // If this pans out then it is a dereg packet in the form of a registration packet
    Boolean		isRegStylePacket = false;
    
    if ( GETFUN( buffer ) == SRVREG )
        isRegStylePacket = true;
    else if ( *(buffer + GETHEADERLEN( buffer )) == '\0' && *((short*)(buffer + GETHEADERLEN( buffer ) + 1)) == 10800 )
        isRegStylePacket = true;
        
    return isRegStylePacket;
}

SLPReturnError ParseOutRegDereg(	const char* buffer,
							UInt16 length,
							UInt16* lifeTime,
							const char** serviceURL,
							UInt16* serviceURLLen,
							const char** serviceTypePtr,
							UInt16* serviceTypeLen,
							const char** scopeListPtr,
							UInt16* scopeListLen,
							const char** attributesPtr,
							UInt16* attributesLen )
{
	SLPReturnError	error = PARSE_ERROR;
	Boolean			isRegStylePacket = TestForRegStylePacket( buffer );			// Mac OS 9 -> 10.1.x were using the reg packet format for dereging
    
	if ( buffer && ( GETFUN( buffer ) == SRVREG || isRegStylePacket ) )
	{
		const char*		curPtr = buffer + GETHEADERLEN( buffer );	// pointing at length of prev response list
		const char*		eofRegPtr = NULL; 
		UInt16		messageLength = GETLEN( buffer );
		UInt16		slpSPILen, authBlockLen;
		UInt16		i;
		UInt8		numAuths;
		
        if ( isRegStylePacket && GETFUN( buffer ) != SRVREG )
            SLP_LOG( SLP_LOG_DEBUG, "Converting a deregistration packet from a Mac OS 9.1 -> 10.1 machine" );
            
		if ( messageLength > length )
		{
			SLP_LOG( SLP_LOG_ERR, "ParseOutRegDereg (SRVREG), message's length (%ld) says its longer than the message (%ld) itself!", messageLength, length );
			return PARSE_ERROR;
		}
		else if ( messageLength != length )
		{
			SLP_LOG( SLP_LOG_ERR, "ParseOutRegDereg (SRVREG), message's length (%ld) says its different than the message (%ld) itself!", messageLength, length );
			return PARSE_ERROR;
		}
		
		error = NO_ERROR;
		eofRegPtr =  buffer + messageLength;

		curPtr++;					// advance past the first Reserved bit
		
		*lifeTime = *((UInt16*)curPtr);
		curPtr += 2;				// advance past lifetime
		
		*serviceURLLen = *((UInt16*)curPtr);
		curPtr += 2;				// advance to url
		
		*serviceURL = curPtr;
		
		if ( curPtr > eofRegPtr )
			error = PARSE_ERROR;	// we shouldn't have gone further than the length of the buffer!
		
		if ( error == NO_ERROR )
		{
			curPtr += *serviceURLLen;		// advance past url to Auth Block part
			
			numAuths = *curPtr;
			curPtr++;					// advance to auth block
			
			for ( i=0; i<numAuths; i++ )
			{
				// if we care about the SPI...
				slpSPILen = *((UInt16*)curPtr);
				curPtr++;					// advance past # url auths
				
				curPtr += 2;			// advance past Block Structure Descriptor
				authBlockLen = *((UInt16*)curPtr);
				curPtr += authBlockLen;
				curPtr -= 2;			// minus our advance to the BSD
			}
		
			if ( curPtr > eofRegPtr )
				error = PARSE_ERROR;	// we shouldn't have gone further than the length of the buffer!
		}

		if ( error == NO_ERROR )
		{
			// Now we should be past the URLEntry portion...

			// and pointing at the service type string... we want to reg this REGARDLESS of url scheme! (SLPv2 8.3)
			*serviceTypeLen = *((UInt16*)curPtr);
			curPtr += 2;
			
			*serviceTypePtr = curPtr;
			curPtr += *serviceTypeLen;
			
			if ( curPtr > eofRegPtr )
				error = PARSE_ERROR;	// we shouldn't have gone further than the length of the buffer!
		}
		
		if ( error == NO_ERROR )
		{		
			// now we should be pointing at the scope list
			*scopeListLen = *((UInt16*)curPtr);
			curPtr += 2;
			
			*scopeListPtr = curPtr;
			curPtr += *scopeListLen;
			
			if ( curPtr > eofRegPtr )
				error = PARSE_ERROR;	// we shouldn't have gone further than the length of the buffer!
		}
		
		if ( error == NO_ERROR )
		{
			// ok we should be pointing to the attribute list length.
			*attributesLen = *((UInt16*)curPtr);
			curPtr += 2;
			
			*attributesPtr = curPtr;
			curPtr += *attributesLen;
			
			if ( curPtr > eofRegPtr )
				error = PARSE_ERROR;	// we shouldn't have gone further than the length of the buffer!
		}
	}
    else if ( GETFUN( buffer ) == SRVDEREG ) 
    {
		const char*		curPtr = buffer + GETHEADERLEN( buffer );	// pointing at length of prev response list
		const char*		eofRegPtr = NULL; 
		UInt16		messageLength = GETLEN( buffer );
		UInt16		slpSPILen, authBlockLen;
		UInt16		i;
		UInt8		numAuths;
		
        SLP_LOG( SLP_LOG_DEBUG, "Parsing a correctly formed deregistration packet from a non Mac OS 9.1 -> 10.1 machine" );
        
		if ( messageLength > length )
		{
			SLP_LOG( SLP_LOG_ERR, "ParseOutRegDereg (SRVDEREG), message's length (%ld) says its longer than the message (%ld) itself!", messageLength, length );
			return PARSE_ERROR;
		}
		else if ( messageLength != length )
		{
			SLP_LOG( SLP_LOG_ERR, "ParseOutRegDereg (SRVDEREG), message's length (%ld) says its different than the message (%ld) itself!", messageLength, length );
			return PARSE_ERROR;
		}
		
		error = NO_ERROR;
		eofRegPtr =  buffer + messageLength;
		
// ack!  In a service deregistration, the first info after the header is the scope list, THEN the URL Entry!
// we were parsing bogus info!
/*		curPtr++;					// advance past the first Reserved bit
		
		*lifeTime = *((UInt16*)curPtr);
		curPtr += 2;				// advance past lifetime
*/		
		if ( error == NO_ERROR )
		{		
			// now we should be pointing at the scope list
			*scopeListLen = *((UInt16*)curPtr);
			curPtr += 2;
			
			*scopeListPtr = curPtr;
			curPtr += *scopeListLen;
			
			if ( curPtr > eofRegPtr )
				error = PARSE_ERROR;	// we shouldn't have gone further than the length of the buffer!
		}
		
        // we should now be pointing at the URL-Entry block
        curPtr += 3;		// past reserved (1 byte) and Lifetime (2 bytes)
        
		*serviceURLLen = *((UInt16*)curPtr);
		curPtr += 2;				// advance to url
		
		*serviceURL = curPtr;
		
		if ( curPtr > eofRegPtr )
			error = PARSE_ERROR;	// we shouldn't have gone further than the length of the buffer!
		
		if ( error == NO_ERROR )
		{
			// now we return the service type
            *serviceTypePtr = *serviceURL;
            
            while ( memcmp( curPtr, ":/", 2 ) != 0 && curPtr < eofRegPtr )
                curPtr++;
            
            *serviceTypeLen = curPtr - *serviceTypePtr;
            
            if ( curPtr > eofRegPtr )
                error = PARSE_ERROR;	// we shouldn't have gone further than the length of the buffer!
        }
        
		if ( error == NO_ERROR )
		{
            curPtr = *serviceURL + *serviceURLLen;		// advance past url to Auth Block part
			
			numAuths = *curPtr;
			curPtr++;					// advance to auth block
			
			for ( i=0; i<numAuths; i++ )
			{
				// if we care about the SPI...
				slpSPILen = *((UInt16*)curPtr);
				curPtr++;					// advance past # url auths
				
				curPtr += 2;			// advance past Block Structure Descriptor
				authBlockLen = *((UInt16*)curPtr);
				curPtr += authBlockLen;
				curPtr -= 2;			// minus our advance to the BSD
			}
		
			if ( curPtr > eofRegPtr )
				error = PARSE_ERROR;	// we shouldn't have gone further than the length of the buffer!
		}

		if ( error == NO_ERROR )
		{
			// Now we should be past the URLEntry portion...
			// and we should be pointing to the attribute list length.
			*attributesLen = *((UInt16*)curPtr);
			curPtr += 2;
			
			*attributesPtr = curPtr;
			curPtr += *attributesLen;
			
			if ( curPtr > eofRegPtr )
				error = PARSE_ERROR;	// we shouldn't have gone further than the length of the buffer!
		}
    }
    else
    {
        error = PARSE_ERROR;
        SLP_LOG( SLP_LOG_MSG, "Parsing an incorrectly formed reg/dereg packet!" );
    }
	
	return error;
}




