/*
	File:		ServiceInfo.h

	Contains:	

	Written by:	Kevin Arnold

	Copyright:	© 1997 - 2000 by Apple Computer, Inc., all rights reserved.

	Change History (most recent first):


*/
#ifndef _ServiceInfo_
#define _ServiceInfo_
#pragma once

#include "slp.h"
//#include "UException.h"
//#include "SLPArray.h"
//#include "LComparator.h"

#include "SLPRegistrar.h"

typedef unsigned short	LifeTime;


typedef struct ServiceLocationHeader{
			 UInt8	byte1;
			 UInt8	byte2;
			 UInt8	byte3;
			 UInt8	byte4;
			 UInt8	byte5;
			 UInt8	byte6;
			 UInt8	byte7;
			 UInt8	byte8;
			 UInt8	byte9;
			 UInt8	byte10;
			 UInt8	byte11;
			 UInt8	byte12;
			 UInt8	byte13;
			 UInt8	byte14;
			 UInt8	byte15;
			 UInt8	byte16;
} ServiceLocationHeader, *ServiceLocationHeaderPtr;

const long kExtHeaderLength = sizeof(ServiceLocationHeader)+4;

class ServiceInfo
{
public:
	ServiceInfo();
	~ServiceInfo();
	
	Boolean			SafeToUse( void ) { return this == mSelfPtr; };
	
	Boolean 		operator==( ServiceInfo& infoToCompare );
	Boolean			operator>( ServiceInfo& infoToCompare );
	
	unsigned long	GetSIRefNum( void ) { return mSIRefNum; };
	void			SetSIRefNum( const UInt32 siRefNum ) { mSIRefNum = siRefNum; };
	
	void			AddInterest( void );
	void			RemoveInterest( void );
	
	Boolean			IsTimeToReRegister( void );
	Boolean			IsTimeToExpire( void );
	void			UpdateLastRegistrationTimeStamp( void );
	
#ifdef USE_SA_ONLY_FEATURES
	OSStatus		RegisterWithAllDAs( void );
	OSStatus		RegisterWithDA( DAInfo* daToRegisterWith );
#endif //#ifdef USE_SA_ONLY_FEATURES	
	void			SetURL( const char* url, UInt16 urlLen );
	char*			GetURLPtr( void ) { return mURL; };
	CFStringRef		GetURLRef( void ) { return mURLRef; };
    
	void			SetServiceType( const char* serviceType, UInt16 serviceTypeLen );
	char*			PtrToServiceType( UInt16* serviceTypeLen );
	
	void			SetScope( const char* scope, UInt16 scopeLen );
	char*			GetScope( void ) { return mScope; };
	UInt16			GetScopeLen( void ) { return ( (mScope != NULL) ? ::strlen( mScope ) : 0 ); };
	
	void			AddAttribute( const char* attribute, UInt16 attributeLen );
	void			RemoveAllAttributes( void );
	char*			GetAttributeList( void ) { return mAttributeList; };
	UInt16			GetAttributeListLen( void ) { return ( (mAttributeList != NULL) ? ::strlen( mAttributeList ) : 0 ); };

	void			SetLifeTime( const LifeTime lifeTime ) { mLifeTime = lifeTime; };
	LifeTime		GetLifeTime( void ) { return mLifeTime; };
	
    void			SetTimeOfFirstRegistration( UInt32 timeOfFirstRegistration ) { mTimeOfFirstRegistration = timeOfFirstRegistration;  };
    UInt32			GetTimeOfFirstRegistration( void ) { return mTimeOfFirstRegistration; };
    
    void			SetTimeOfLastRegistrationUpdate( UInt32 timeOfLastRegistration ) { mTimeOfLastRegistrationUpdate = timeOfLastRegistration;  };
    UInt32			GetTimeOfLastRegistrationUpdate( void ) { return mTimeOfLastRegistrationUpdate; };
    
    void			SetTimeOfExpiration( UInt32 timeOfExpiration ) { mTimeOfExpiration = timeOfExpiration; };
    UInt32			GetTimeOfExpiration( void ) { return mTimeOfExpiration; };
    
    void			SetIPAddressRegisteredFrom( long ipRegisteredFrom ) { mIPRegisteredFrom = ipRegisteredFrom; };
    UInt32			GetIPAddressRegisteredFrom( void ) { return mIPRegisteredFrom; };
    
	Boolean			URLMatches( char* url, UInt16 urlLen );
//	Boolean			ServiceTypeMatches( const string& service );
	Boolean			ServiceTypeMatches( char* serviceType, UInt16 serviceTypeLen );
	Boolean			ScopeMatches( char* scope );
	Boolean			ScopeMatches( char* scope, UInt16 scopeLen );
//	Boolean			AttributesMatch( const string& attributes );

	UInt16			GetNumInterestedParties( void ) { return mNumInterestedParties; };
    
protected:
static unsigned long	mLatestAssignedSIRefNum;

	UInt32			mSIRefNum;
    UInt32			mTimeOfFirstRegistration;
    UInt32			mTimeOfLastRegistrationUpdate;
	UInt32			mTimeOfExpiration;
	UInt32			mTimeToRegister;
	LifeTime		mLifeTime;		// length of time that this is valid from mTimeOfLastRegistration
	char*			mURL;
    CFStringRef		mURLRef;
	char*			mServiceType;	// this can be different that what shows in mURL!
	char*			mAttributeList;
    long			mIPRegisteredFrom;
	UInt16			mNumInterestedParties;
	char*			mScope;
	UInt16			mNumScopes;
	ServiceInfo*	mSelfPtr;
};

#ifdef USE_SERVICE_INFO_COMPARATOR
class ServiceInfoComparator : public SLPComparator
{
public:
	ServiceInfoComparator();
	~ServiceInfoComparator();

	virtual SInt32		Compare(
								const void*			inItemOne,
								const void* 		inItemTwo,
								UInt32				inSizeOne,
								UInt32				inSizeTwo) const;

	virtual Boolean		IsEqualTo(
								const void*			inItemOne,
								const void* 		inItemTwo,
								UInt32				inSizeOne,
								UInt32				inSizeTwo) const;

protected:
};
#endif //#ifdef USE_SERVICE_INFO_COMPARATOR

// some helper functions
OSStatus MakeSLPServiceInfoAddedNotificationBuffer( ServiceInfo* service, UInt32* dataBufferLen, char** dataBuffer );
OSStatus MakeSLPServiceInfoRemovedNotificationBuffer( ServiceInfo* service, UInt32* dataBufferLen, char** dataBuffer );

OSStatus MakeServiceInfoFromNotificationBuffer( char* dataBuffer, UInt32 dataBufferLen, ServiceInfo** newServiceInfo );

void		CopyXIDFromRequestToCachedReply( ServiceLocationHeader* serviceReply, ServiceLocationHeader* originialRequest );

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
                                        ServiceInfo** newServiceInfo );

SLPReturnError ParseOutServiceRequest(	const char* buffer,
                                        UInt16 length,
                                        const char** serviceTypePtr,
                                        UInt16* serviceTypeLen,
                                        const char** scopeListPtr,
                                        UInt16* scopeListLen,
                                        const char** predicateListPtr,
                                        UInt16* predicateListLen );

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
                                    UInt16* attributesLen );
#endif
