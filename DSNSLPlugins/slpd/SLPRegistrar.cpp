/*
	File:		SLPRegistrar.cp

	Contains:	class to keep track of all registered services

	Written by:	Kevin Arnold

	Copyright:	й 1997 - 2000 by Apple Computer, Inc., all rights reserved.

	Change History (most recent first):

 */
#include <stdio.h>
#include <string.h>
#include <sys/un.h>

#include "mslp_sd.h"
#include "slp.h"
#include "mslp.h"
#include "mslpd_store.h"
#include "mslp_dat.h"
#include "mslpd.h"
#include "mslpd_mask.h"

#include "mslpd_parse.h"

//#include "SLPDefines.h"

#include "slpipc.h"
//#include "URLUtilities.h"
#include "ServiceInfo.h"
#include "SLPRegistrar.h"

#include "SLPComm.h"

#ifdef USE_SLPREGISTRAR

SLPRegistrar* 	gsSLPRegistrar = NULL;
int				gTotalServices = 0;

void GetServiceTypeFromURL(	const char* readPtr,
							UInt32 theURLLength,
							char*	URLType );
                            
SLPReturnError HandleRegDereg( SLPBoolean isReg, const char* pcInBuf, int iInSz, struct sockaddr_in* sinIn );

long GetStatelessBootTime( void )
{
    return SLPRegistrar::TheSLPR()->GetStatelessBootTime();
}

long GetCurrentTime( void )
{
    struct	timeval curTime;
    
    if ( gettimeofday( &curTime, NULL ) != 0 )
        LOG_STD_ERROR_AND_RETURN( SLP_LOG_ERR, "call to gettimeofday returned error: %s", errno );
    
    return curTime.tv_sec;
}

SLPReturnError HandleRegistration( const char* pcInBuf, int iInSz, struct sockaddr_in* sinIn )
{
    return HandleRegDereg( SLP_TRUE, pcInBuf, iInSz, sinIn );
}

SLPReturnError HandleDeregistration( const char* pcInBuf, int iInSz, struct sockaddr_in* sinIn )
{
    return HandleRegDereg( SLP_FALSE, pcInBuf, iInSz, sinIn );
}

SLPReturnError HandleRegDereg( SLPBoolean isReg, const char* pcInBuf, int iInSz, struct sockaddr_in* sinIn )
{
    UInt16 			lifeTime;
    const char* 	serviceURL;
    UInt16 			serviceURLLen;
    const char* 	serviceTypePtr;
    UInt16 			serviceTypeLen;
    const char* 	scopeListPtr;
    char			scopeToRegIn[256] = {0};
    UInt16 			scopeListLen;
    const char* 	attributesPtr;
    UInt16 			attributesLen;
    
    ServiceInfo*	newSI = NULL;
    
    SLPReturnError	error = ParseOutRegDereg(	pcInBuf,
                                            iInSz, 
                                            &lifeTime, 
                                            &serviceURL, 
                                            &serviceURLLen, 
                                            &serviceTypePtr,
                                            &serviceTypeLen, 
                                            &scopeListPtr,
                                            &scopeListLen,
                                            &attributesPtr,
                                            &attributesLen );
    
    if ( error )
    {
        SLP_LOG( SLP_LOG_DROP, "Error in Parsing RegDereg header" );
    }
    
    if ( !error )
    {
        memcpy( scopeToRegIn, scopeListPtr, (scopeListLen<sizeof(scopeToRegIn))?scopeListLen:sizeof(scopeToRegIn)-1 );
        
        if ( !list_intersection( SLPGetProperty("com.apple.slp.daScopeList"), scopeToRegIn ) )
        {
            char	errorMsg[512];
            
            error = SCOPE_NOT_SUPPORTED;
            
            sprintf( errorMsg, "Service Agent: %s tried registering in an invalid scope (%s): %s", inet_ntoa(sinIn->sin_addr), scopeToRegIn, slperror(SLP_SCOPE_NOT_SUPPORTED) );
            SLP_LOG( SLP_LOG_DA, errorMsg );
        }
    }
    
    if ( !error )
    {
        error = CreateNewServiceInfo(	lifeTime, 
                                        serviceURL, 
                                        serviceURLLen, 
                                        serviceTypePtr, 
                                        serviceTypeLen, 
                                        scopeListPtr, 
                                        scopeListLen, 
                                        attributesPtr, 
                                        attributesLen, 
                                        sinIn->sin_addr.s_addr, 
                                        &newSI );
                                        
        if ( error )
            SLP_LOG( SLP_LOG_ERR, "CreateNewServiceInfo returned error" );
    }
    
    if ( !error )
    {    
        SLPRegistrar::Lock();
        
        if ( isReg )
        {
            static int counter = 1;
            
            SLP_LOG( SLP_LOG_MSG, "SLPRegistrar::TheSLPR()->RegisterService called for %dth time", counter++);
            
            SLPRegistrar::TheSLPR()->RegisterService( newSI );
        }
        else
        {
            SLPRegistrar::TheSLPR()->DeregisterService( newSI );
        }
           
        newSI->RemoveInterest();		// need to free this up
         
            
        SLPRegistrar::Unlock();
    }
    
    return error;
}

SLPReturnError DAHandleRequest( SAState *psa, struct sockaddr_in* sinIn, SLPBoolean viaTCP, Slphdr *pslphdr, const char *pcInBuf,
                            int iInSz, char **ppcOutBuf, int *piOutSz, int *piGot )
{
    int 				err;
    SLPReturnError 		result = NO_ERROR;
    SLPInternalError	iErr = SLP_OK;
    char*				previousResponderList = NULL;
    char* 				serviceType = NULL;
    char* 				scopeList = NULL;
    char* 				attributeList = NULL;
    UInt16				serviceTypeLen = 0;
    UInt16				scopeListLen = 0;
    UInt16				attributeListLen = 0;
    
    // first we need to parse this request
	if ((err = srvrqst_in(pslphdr, pcInBuf, iInSz, &previousResponderList, &serviceType, &scopeList, &attributeList))<0)
    {
        SLP_LOG( SLP_LOG_DROP,"DAHandleRequest: drop request due to parse in error" );
    }
    else
    {
        SLP_LOG( SLP_LOG_DEBUG, "DAHandleRequest: calling SLPRegistrar::TheSLPR()->CreateReplyToServiceRequest" );
        
        if ( !SDstrcasecmp(serviceType,"service:directory-agent") || !SDstrcasecmp(serviceType,"service:service-agent") )
        {
            if ( !SDstrcasecmp(serviceType,"service:directory-agent") )
            {
                if ( viaTCP )
                    SLP_LOG( SLP_LOG_DA, "received TCP service:directory-agent request from %s", inet_ntoa(sinIn->sin_addr) );
                else
                    SLP_LOG( SLP_LOG_DA, "received service:directory-agent request from %s", inet_ntoa(sinIn->sin_addr) );
            }
            else
            {
                SLP_LOG( SLP_LOG_SR, "service:service-agent from %s", inet_ntoa(sinIn->sin_addr) );
            }
            
            iErr = (SLPInternalError)store_request( psa, viaTCP, pslphdr, pcInBuf, iInSz, ppcOutBuf, piOutSz, piGot);
            
            if ( iErr )
                result = InternalToReturnError( iErr );
        }
        else
        {
	        SLP_LOG( SLP_LOG_SR, "service: %s request from %s", serviceType, inet_ntoa(sinIn->sin_addr) );

	        if ( serviceType )
	            serviceTypeLen = strlen( serviceType );
	            
	        if ( scopeList )
	            scopeListLen = strlen( scopeList );
	            
	        if ( attributeList )
	            attributeListLen = strlen( attributeList );

	        SLPRegistrar::Lock();

	        result = InternalToReturnError( SLPRegistrar::TheSLPR()->CreateReplyToServiceRequest(	
	                                                                        viaTCP,
	                                                                        pslphdr,
	                                                                        pcInBuf,
	                                                                        iInSz,
	                                                                        serviceType,
	                                                                        serviceTypeLen,
	                                                                        scopeList,
	                                                                        scopeListLen,
	                                                                        attributeList,
	                                                                        attributeListLen,
	                                                                        ppcOutBuf,
	                                                                        piOutSz ) );
	        *piGot = 1;
	        
	        SLPRegistrar::Unlock();
	        
	        if ( !*ppcOutBuf )
	            SLP_LOG( SLP_LOG_ERR, "*ppcOutBuf is NULL after parsing request from %s", inet_ntoa(sinIn->sin_addr) );
            else
                SLP_LOG( SLP_LOG_MSG, "Sending back a %ld size message to %s", *piOutSz, inet_ntoa(sinIn->sin_addr) );
	   }
   }
    
    if (serviceType) 
        SLPFree((void*)serviceType);
    
    if (previousResponderList) 
        SLPFree((void*)previousResponderList);
    
    if (scopeList) 
        SLPFree((void*)scopeList);
    
    if (attributeList) 
        SLPFree((void*)attributeList);
    
    return result;
}

pthread_mutex_t	SLPRegistrar::msObjectLock;
pthread_mutex_t	SLPRegistrar::msSLPRLock;

Boolean			gsLocksInitiailzed = false;

void TurnOnDA( void )
{
    SLP_LOG( SLP_LOG_RADMIN, "Setting property: \"com.apple.slp.isDA\" to true" );

    SLPSetProperty("com.apple.slp.isDA", "true");
    
    if ( gsSLPRegistrar )
        gsSLPRegistrar->SendRAdminSLPStarted();
}

void TurnOffDA( void )
{
    SLP_LOG( SLP_LOG_RADMIN, "Setting property: \"com.apple.slp.isDA\" to false" );

    SLPSetProperty("com.apple.slp.isDA", "false");
    
    if ( gsSLPRegistrar )
        gsSLPRegistrar->SendRAdminSLPStopped();
}

void InitSLPRegistrar( void )
{
    Boolean		previousNotificationsEnabled = false;
    
/*    if ( gsSLPRegistrar )
    {
        SLPRegistrar::Lock();
        previousNotificationsEnabled = gsSLPRegistrar->RAdminNotificationEnabled();			// keep track of this - we need a better solution for restarting
        delete gsSLPRegistrar;
        SLPRegistrar::Unlock();
    }
*/
    if ( !gsSLPRegistrar )
    {
        gsSLPRegistrar = new SLPRegistrar();
        gsSLPRegistrar->Initialize();
    }
    
    if ( previousNotificationsEnabled )
        gsSLPRegistrar->EnableRAdminNotification();
}

void TearDownSLPRegistrar( void )
{
    if ( gsSLPRegistrar )
    {
        gsSLPRegistrar->SetStatelessBootTime( 0 );			// this tells others that we are shutting down
        gsSLPRegistrar->DoDeregisterAllServices();
    }
}

void ResetStatelessBootTime( void )
{
    if ( gsSLPRegistrar )
    {
        struct timeval          currentTime;
        struct timezone         tz;
        
        gettimeofday( &currentTime, &tz ); 
        gsSLPRegistrar->SetStatelessBootTime( currentTime.tv_sec );		// number of seconds relative to 0h on 1 January 1970
    }
}

// CF Callback function prototypes
/*
CFStringRef SLPCliqueKeyCopyDesctriptionCallback ( const void *key );
Boolean SLPCliqueKeyEqualCallback ( const void *key1, const void *key2 );
CFHashCode SLPCliqueKeyHashCallback(const void *key);
*/
CFStringRef SLPCliqueValueCopyDesctriptionCallback ( const void *value );
Boolean SLPCliqueValueEqualCallback ( const void *value1, const void *value2 );
void SLPCliqueHandlerFunction(const void *inKey, const void *inValue, void *inContext);

CFStringRef SLPServiceInfoCopyDesctriptionCallback ( const void *item );
Boolean SLPServiceInfoEqualCallback ( const void *item1, const void *item2 );
CFHashCode SLPServiceInfoHashCallback(const void *key);
void SLPServiceInfoHandlerFunction(const void *inValue, void *inContext);
/*
CFStringRef SLPCliqueKeyCopyDesctriptionCallback ( const void *key )
{
//   SLPClique*		clique = (SLPClique*)value;
    
    return (CFStringRef)key;
}

Boolean SLPCliqueKeyEqualCallback ( const void *key1, const void *key2 )
{
//    SLPClique*		clique1 = (SLPClique*)::CFDictionaryGetValue( key1;
//    SLPClique*		clique2 = (SLPClique*)key2;
    
    return ( key1 == key2 || CFStringCompare( (CFStringRef)key1, (CFStringRef)key2, kCFCompareCaseInsensitive ) == kCFCompareEqualTo );
}

CFHashCode SLPCliqueKeyHashCallback(const void *key)
{
	long		value1, value2;			// we'll do this by grabbing the first 4 bytes of the string and adding them to the last 4 bytes...
    UInt8*		value1Ptr = (UInt8*)&value1;
    UInt8*		value2Ptr = (UInt8*)&value2;
    CFHashCode	returnCode;
    CFIndex		begin2;
    
    ::CFStringGetBytes ((CFStringRef) key, 
                        CFRangeMake(0,4), 
                        kCFStringEncodingUTF8, 
//                        CFStringGetSystemEncoding(), 
                        '?', 
                        false, 
                        value1Ptr, 
                        sizeof(value1), 
                        NULL );

    begin2 = CFStringGetLength((CFStringRef)key);
    begin2 > 4 ? begin2 = begin2-4 : begin2 = 0;
    
    ::CFStringGetBytes ((CFStringRef) key, 
                        CFRangeMake( begin2,4), 
                        kCFStringEncodingUTF8, 
//                        CFStringGetSystemEncoding(), 
                        '?', 
                        false, 
                        value2Ptr, 
                        sizeof(value2), 
                        NULL );

    returnCode = value1 + value2;
    
    return returnCode;		// just hash on the first 4 bytes of the string
}
*/
CFStringRef SLPCliqueValueCopyDesctriptionCallback ( const void *value )
{
   SLPClique*		clique = (SLPClique*)value;
    
    return clique->GetValueRef();
}

Boolean SLPCliqueValueEqualCallback ( const void *value1, const void *value2 )
{
    SLPClique*		clique1 = (SLPClique*)value1;
    SLPClique*		clique2 = (SLPClique*)value2;
    
    return ( clique1 == clique2 || CFStringCompare( clique1->GetValueRef(), clique2->GetValueRef(), kCFCompareCaseInsensitive ) == kCFCompareEqualTo );
}

void SLPCliqueHandlerFunction(const void *inKey, const void *inValue, void *inContext)
{
    SLPClique*					curClique = (SLPClique*)inValue;
    SLPCliqueHandlerContext*	context = (SLPCliqueHandlerContext*)inContext;
    
    switch ( context->message )
    {
        case kDeleteSelf:
            delete curClique;
        break;
        
        case kDeleteCliquesMatchingScope:
            if ( strcmp(curClique->CliqueScopePtr(), (char*)(context->dataPtr)) == 0 )
            {
                ::CFDictionaryRemoveValue( context->dictionary, curClique->GetKeyRef() );		// remove it from the dictionary
                delete curClique;
            }
        break;
        
        
        case kReportServicesToRAdmin:
            curClique->ReportAllServicesToRAdmin();
        break;
        
        case kUpdateCache:
		{
            SLP_LOG( SLP_LOG_DEBUG, "SLPRegistrar Updating Cache" );

            if ( curClique->CacheIsDirty() )
            {
                SLPInternalError status = curClique->UpdateCachedReplyForClique();
            
                if ( status == SLP_DA_BUSY_NOW )
                {
                    SLP_LOG( SLP_LOG_MSG, "SLPRegistrar Updating Cache received a DA_BUSY_NOW error, will postpone update" );
                }
			}
		}
        break;
        
        case kDoTimeCheckOnTTLs:
        {
            SLPServiceInfoHandlerContext	context = {kDoTimeCheckOnTTLs, curClique};
    
            ::CFSetApplyFunction( curClique->GetSetOfServiceInfosRef(), SLPServiceInfoHandlerFunction, &context );
        }
        break;
    };
}

#pragma mark еее Public Methods for SLPRegistrar еее
void SLPRegistrar::Lock( void ) 
{ 
    pthread_mutex_lock( &SLPRegistrar::msObjectLock ); 
}

SLPRegistrar::SLPRegistrar()
{
    mListOfSLPCliques = NULL;
	mOneOrMoreCliquesNeedUpdating = false;
	mLastTTLTimeCheck = 0;
	
	mAlreadyReadRegFile = false;
	mStatelessBootTime = 0;
    mRAdminNotifier = NULL;
	mSelfPtr = NULL;
}

SLPRegistrar::~SLPRegistrar()
{
	SLPCliqueHandlerContext	context = {mListOfSLPCliques, kDeleteSelf, this};
    
	mSelfPtr = NULL;
	
    Lock();

	if ( mListOfSLPCliques )
	{
		::CFDictionaryApplyFunction( mListOfSLPCliques, SLPCliqueHandlerFunction, &context );
        ::CFDictionaryRemoveAllValues( mListOfSLPCliques );
        ::CFRelease( mListOfSLPCliques );
        mListOfSLPCliques = NULL;
	}

    Unlock();
}

SLPRegistrar* SLPRegistrar::TheSLPR( void )
{
    return gsSLPRegistrar;
}

void SLPRegistrar::Initialize( void )
{
	if ( !gsLocksInitiailzed )
    {
        // mutex lock initialization
        pthread_mutex_init( &msObjectLock, NULL );
        pthread_mutex_init( &msSLPRLock, NULL );
        gsLocksInitiailzed = true;
    }
    
	// bootstamp initialization
    ResetStatelessBootTime();
    
	// database initialization
    CFDictionaryValueCallBacks	valueCallBack;
/*	CFDictionaryKeyCallBacks	keyCallBack;
    
    keyCallBack.version = 0;
    keyCallBack.retain = NULL;
    keyCallBack.release = NULL;
    keyCallBack.copyDescription = SLPCliqueKeyCopyDesctriptionCallback;
    keyCallBack.equal = SLPCliqueKeyEqualCallback;
    keyCallBack.hash = SLPCliqueKeyHashCallback;
//    keyCallBack.hash = NULL;
*/    
    valueCallBack.version = 0;
    valueCallBack.retain = NULL;
    valueCallBack.release = NULL;
    valueCallBack.copyDescription = SLPCliqueValueCopyDesctriptionCallback;
    valueCallBack.equal = SLPCliqueValueEqualCallback;
    
    mListOfSLPCliques = ::CFDictionaryCreateMutable( NULL, 0, &kCFCopyStringDictionaryKeyCallBacks, &valueCallBack);
    mRAdminNotifier = new SLPRAdminNotifier();
    if ( mRAdminNotifier )
    {
        mRAdminNotifier->Resume();
    }
    else
        SLP_LOG( SLP_LOG_ERR, "SLPRegistrar couldn't create its Notifier Thread!" );

	mSelfPtr = this;
}

#pragma mark -
void SLPRegistrar::EnableRAdminNotification( void )
{
	SLPSetProperty( "com.apple.slp.RAdminNotificationsEnabled", "true" );
    
    if ( !mRAdminNotifier )
    {
        mRAdminNotifier = new SLPRAdminNotifier();
        
        if ( mRAdminNotifier )
        {
            mRAdminNotifier->Resume();
        }
        else
            SLP_LOG( SLP_LOG_ERR, "SLPRegistrar couldn't create its Notifier Thread!" );
    }
    // now we should send all currently registered services back to ServerAdmin
    if ( mRAdminNotifier )
        mRAdminNotifier->RepostAllRegisteredData();
}

Boolean SLPRegistrar::IsServiceAlreadyRegistered( ServiceInfo* service )
{
    Boolean 	isAlreadyRegistered = false;
	
	if ( service )
	{
		SLPClique* clique = FindClique( service );
			
		if ( clique )
			isAlreadyRegistered = clique->IsServiceInfoInClique( service );
	}
	
	return isAlreadyRegistered;
}

SLPInternalError SLPRegistrar::RegisterService( ServiceInfo* service )
{
	
	SLPInternalError	status = SLP_OK;
	SLPClique*	clique = NULL;

#ifdef USE_EXCEPTIONS
	Try_
#endif
	{
		if ( service )
		{			
			clique = FindClique( service );
			
			if ( clique )
			{
				status = clique->AddServiceInfoToClique( service );
                
				if ( !status )
					mOneOrMoreCliquesNeedUpdating = true;
			}
			else
			{
#ifdef USE_EXCEPTIONS
				Try_
#endif
				{
					clique = new SLPClique();
#ifdef USE_EXCEPTIONS
					ThrowIfNULL_( clique );
#endif					
					if ( clique )
					{
						clique->Initialize( service );
						
						::CFDictionaryAddValue( mListOfSLPCliques, clique->GetKeyRef(), clique );
						
						status = clique->AddServiceInfoToClique( service );
						
						if ( !status )
							mOneOrMoreCliquesNeedUpdating = true;
					}
				}
				
#ifdef USE_EXCEPTIONS
				Catch_( inErr )
				{
					status = (SLPInternalError)inErr;
					if ( clique )
						delete clique;
				}
#endif
			}
		}
        else
            SLP_LOG( SLP_LOG_DA, "SLPRegistrar::RegisterService was passed a NULL service!");
	}
	
#ifdef USE_EXCEPTIONS
	Catch_ ( inErr )
	{
		status = (SLPInternalError)inErr;
	}
#endif		
	return status;
}

SLPInternalError SLPRegistrar::DeregisterService( ServiceInfo* service )
{
	
	SLPInternalError	status = SLP_OK;
	
#ifdef USE_EXCEPTIONS
	Try_
#endif
	{
		if ( service && service->SafeToUse() )
		{			
			SLPClique* clique = FindClique( service );
			
			if ( clique )
			{
				status = clique->RemoveServiceInfoFromClique( service );
			
				if ( clique->GetSetOfServiceInfosRef() == NULL )		// no more services, get rid of clique
				{
					::CFDictionaryRemoveValue( mListOfSLPCliques, clique->GetKeyRef() );
					
					delete clique;
				}
				
			}
			else
				status = SERVICE_NOT_REGISTERED;
		}
	}
	
#ifdef USE_EXCEPTIONS
	Catch_ ( inErr )
	{
        SLP_LOG( SLP_LOG_ERR, "SLPRegistrar::DeregisterService Caught_ inErr: %ld", (SLPInternalError)inErr );

		status = (SLPInternalError)inErr;
	}
#endif		
	SLP_LOG( SLP_LOG_DEBUG, "SLPRegistrar::DeregisterService finished, returning status: %d", status );

	return status;
}

SLPInternalError SLPRegistrar::RemoveScope( char* scope )
{
	SLPCliqueHandlerContext	context = {mListOfSLPCliques,kDeleteCliquesMatchingScope, scope};
    SLPInternalError				error = SLP_OK;
		
    Lock();

	if ( mListOfSLPCliques )
	{
    	::CFDictionaryApplyFunction( mListOfSLPCliques, SLPCliqueHandlerFunction, &context );
    }
    
    if ( RAdminNotificationEnabled() )
    {
        SendRAdminDeletedScope( scope );
    }

    Unlock();
    
    return error;
}

void SLPRegistrar::AddNotification( char* buffer, UInt32 bufSize )
{
    if ( mRAdminNotifier )
        mRAdminNotifier->AddNotificationToQueue( buffer, bufSize );
}

void SLPRegistrar::DoSendRAdminAllCurrentlyRegisteredServices( void )
{
    if ( mRAdminNotifier )
        mRAdminNotifier->RepostAllRegisteredData();
}

void SLPRegistrar::SendRAdminSLPStarted( void )
{
    UInt32		dataSendBufferLen;
    char*		dataSendBuffer = NULL;
    
    MakeSLPDAStatus( kSLPDARunning, &dataSendBufferLen, &dataSendBuffer  );

    AddNotification( dataSendBuffer, dataSendBufferLen );		// mRAdminNotifier takes control of buffer free
}

void SLPRegistrar::SendRAdminSLPStopped( void )
{
    UInt32		dataSendBufferLen;
    char*		dataSendBuffer = NULL;
    
    MakeSLPDAStatus( kSLPDANotRunning, &dataSendBufferLen, &dataSendBuffer  );

    AddNotification( dataSendBuffer, dataSendBufferLen );		// mRAdminNotifier takes control of buffer free
}

void SLPRegistrar::SendRAdminDeleteAllScopes( void )
{
    char*			pcScope = NULL;
    const char*		pcList = SLPGetProperty("com.apple.slp.daScopeList");
    int				offset=0;
    char			c;
    
    while( (pcScope = get_next_string(",",pcList,&offset,&c)) )
    {
        mslplog( SLP_LOG_RADMIN, "SendRAdminDeleteAllScopes removing scope: ", pcScope );
        RemoveScope( pcScope );
        
        free(pcScope);
    }
}

void SLPRegistrar::SendRAdminAllScopes( void )
{
    char*			pcScope = NULL;
    const char*		pcList = SLPGetProperty("com.apple.slp.daScopeList");
    int				offset=0;
    char			c;
    
    while( (pcScope = get_next_string(",",pcList,&offset,&c)) )
    {
        SendRAdminAddedScope( pcScope );
        
        free(pcScope);
    }
}

void SLPRegistrar::SendRAdminAddedScope( const char* newScope )
{
    UInt32		dataSendBufferLen = sizeof(SLPdMessageHeader) + strlen(newScope);
    char*		dataSendBuffer = (char*)malloc(dataSendBufferLen);
    
    SLPdMessageHeader*	header = (SLPdMessageHeader*)dataSendBuffer;
    header->messageType = kSLPAddScope;
    header->messageLength = dataSendBufferLen;
    header->messageStatus = 0;

    char* curPtr = (char*)header + sizeof(SLPdMessageHeader);

    ::memcpy( curPtr, newScope, strlen(newScope) );

    AddNotification( dataSendBuffer, dataSendBufferLen );		// mRAdminNotifier takes control of buffer free
}

void SLPRegistrar::SendRAdminDeletedScope( const char* oldScope )
{
    UInt32		dataSendBufferLen = sizeof(SLPdMessageHeader) + strlen(oldScope);
    char*		dataSendBuffer = (char*)malloc(dataSendBufferLen);
    
    SLPdMessageHeader*	header = (SLPdMessageHeader*)(dataSendBuffer);
    header->messageType = kSLPDeleteScope;
    header->messageLength = dataSendBufferLen;
    header->messageStatus = 0;

    char* curPtr = (char*)header + sizeof(SLPdMessageHeader);

    ::memcpy( curPtr, oldScope, strlen(oldScope) );

    AddNotification( dataSendBuffer, dataSendBufferLen );		// mRAdminNotifier takes control of buffer free
}

void SLPRegistrar::SendRAdminAllCurrentlyRegisteredServices( void )
{
	SLPCliqueHandlerContext	context = {mListOfSLPCliques, kReportServicesToRAdmin, NULL};
    
	Lock();
    
	if ( mListOfSLPCliques )
		::CFDictionaryApplyFunction( mListOfSLPCliques, SLPCliqueHandlerFunction, &context );
    
	Unlock();
}

void SLPRegistrar::DoDeregisterAllServices( void )
{
    if ( mRAdminNotifier )
        mRAdminNotifier->DeregisterAllRegisteredData();
}

void SLPRegistrar::DeregisterAllServices( void )
{
	SLPCliqueHandlerContext	context = {mListOfSLPCliques, kDeleteSelf, NULL};
    
    Lock();

	if ( mListOfSLPCliques )
	{
    	::CFDictionaryApplyFunction( mListOfSLPCliques, SLPCliqueHandlerFunction, &context );
        ::CFDictionaryRemoveAllValues( mListOfSLPCliques );

// don't get rid of mListOfSLPCliques, we could just be turning our selves off.  We can release it in our destructor
//        ::CFRelease( mListOfSLPCliques );
//        mListOfSLPCliques = NULL;
    }
    
    Unlock();
}

SLPInternalError SLPRegistrar::CreateReplyToServiceRequest(	
											SLPBoolean viaTCP,
                                            Slphdr *pslphdr,
                                            const char* originalHeader, 
											UInt16 originalHeaderLength,
											char* serviceType,
											UInt16 serviceTypeLen,
											char* scopeList,
											UInt16 scopeListLen,
											char* attributeList,
											UInt16 attributeListLen,
											char** returnBuffer, 
                                            int *piOutSz )
{
	
	SLPInternalError			error = SLP_OK;	// ok, set this as default
	UInt16				returnBufferLen;
	char*				tempPtr = NULL;
	char*				endPtr = NULL;
    int 				iMTU = strtol(SLPGetProperty("net.slp.MTU"),&endPtr,10);
    int 				iOverflow = 0;
	SLPClique*			matchingClique = NULL;

	SLP_LOG( SLP_LOG_DEBUG, "SLPRegistrar::CreateReplyToServiceRequest called" );
 
	matchingClique = FindClique( serviceType, serviceTypeLen, scopeList, scopeListLen, attributeList, attributeListLen );
	if ( returnBuffer && matchingClique )
	{
		if ( matchingClique->CacheIsDirty() )
			error = matchingClique->UpdateCachedReplyForClique();	// ok to call here as we are allowed to allocate memory
			
		if ( !error )
			SLP_LOG( SLP_LOG_DEBUG, "SLPRegistrar::CreateReplyToServiceRequest(), Copying Cached Return Data" );

        *piOutSz = matchingClique->GetCacheSize();
        SLP_LOG( SLP_LOG_DEBUG, "SLPRegistrar::CreateReplyToServiceRequest for %s in %s, *piOutSz: %ld", serviceType, scopeList, *piOutSz );
        
        if ( !viaTCP && *piOutSz > iMTU )	// just send 0 results and set the overflow bit
        {
            *piOutSz = GETHEADERLEN(originalHeader) + 4;
            iOverflow = 1;
            SLP_LOG( SLP_LOG_DEBUG, "SLPRegistrar::CreateReplyToServiceRequest(), sending back overflow message" );
        }
        
		if ( !error )
			tempPtr = safe_malloc(*piOutSz, 0, 0);
		
		assert( tempPtr );
		
		if ( !error )
		{
        	if ( iOverflow )
            {
                /* parse header out */
                SETVER(tempPtr,2);
                SETFUN(tempPtr,SRVRPLY);
                SETLEN(tempPtr,*piOutSz);
                SETLANG(tempPtr,pslphdr->h_pcLangTag);
                SETFLAGS(tempPtr,OVERFLOWFLAG);
            }
            else
                error = matchingClique->CopyCachedReturnData( tempPtr, matchingClique->GetCacheSize(), &returnBufferLen );
        }
        
		if ( !error )
			SETXID(tempPtr, pslphdr->h_usXID );
        
		if ( error )
			LOG_SLP_ERROR_AND_RETURN( SLP_LOG_ERR, "SLPRegistrar::CreateReplyToServiceRequest, CopyCachedReturnData returned error", error );
	
        if ( !error )
            *returnBuffer = tempPtr;
	}
	else if ( returnBuffer )
	{
		error = SLP_OK;		// we have nothing registered here yet, so just reply with no results
        
        SLP_LOG( SLP_LOG_DEBUG, "SLPRegistrar::CreateReplyToServiceRequest(), sending back empty result" );
        
        *piOutSz = GETHEADERLEN(originalHeader) + 4;
        tempPtr = safe_malloc(*piOutSz, 0, 0);
        
        assert( tempPtr );
        
        if ( tempPtr )
        {
            SETVER(tempPtr,2);
            SETFUN(tempPtr,SRVRPLY);
            SETLEN(tempPtr,*piOutSz);
            SETLANG(tempPtr,pslphdr->h_pcLangTag);
        }
        else
            error = SLP_MEMORY_ALLOC_FAILED;
            
        *returnBuffer = tempPtr;
	}
	else
	{
        SLP_LOG( SLP_LOG_DEBUG, "SLPRegistrar::CreateReplyToServiceRequest, returnBuffer is NULL!" );
	}

	return error;
}

#pragma mark -
#pragma mark -
SLPClique* SLPRegistrar::FindClique( ServiceInfo* service )
{
	
	UInt16				serviceTypeLen = strlen(service->PtrToServiceType( &serviceTypeLen ));
	SLPClique*	foundClique = FindClique( service->PtrToServiceType( &serviceTypeLen ), serviceTypeLen, service->GetScope(), service->GetScopeLen(), service->GetAttributeList(), service->GetAttributeListLen() );
	
	return foundClique;
}

void SLPRegistrar::UpdateDirtyCaches( void )
{
#ifdef USE_EXCEPTIONS
	Try_
#endif
	{
			
        SLPCliqueHandlerContext	context = {mListOfSLPCliques, kUpdateCache, NULL};
        
        Lock();
    
        if ( mListOfSLPCliques && mOneOrMoreCliquesNeedUpdating )
        {
            ::CFDictionaryApplyFunction( mListOfSLPCliques, SLPCliqueHandlerFunction, &context );
		}
        
        mOneOrMoreCliquesNeedUpdating = false;	// done
        
        Unlock();
	}
	
#ifdef USE_EXCEPTIONS
	Catch_( inErr )
	{
        SLP_LOG( SLP_LOG_DA, "SLPRegistrar::UpdateDirtyCaches Caught Err: %d", (SLPInternalError)inErr );
	}
#endif
}

#ifdef DO_TIME_CHECKS_ON_TTLS
SLPInternalError SLPRegistrar::DoTimeCheckOnTTLs( void )
{
	SLPInternalError		status = SLP_OK;
	
	// only check this every kTimeBetweenTTLTimeChecks ticks
	if ( GetCurrentTime() > mLastTTLTimeCheck + kTimeBetweenTTLTimeChecks )
	{
		// We have two basic functions we want to accomplish here:
		// 1) if we are a DA, then we need to dereg any services that have expired
		// 2) if we are not a DA, then we need to rereg any services that are close
		//		to expiring (assuming we have registered them with another DA)
        SLPCliqueHandlerContext	context = {mListOfSLPCliques, kDoTimeCheckOnTTLs, NULL};
        
        Lock();
    
        if ( mListOfSLPCliques && mOneOrMoreCliquesNeedUpdating )
        {
            ::CFDictionaryApplyFunction( mListOfSLPCliques, SLPCliqueHandlerFunction, &context );
		}

        Unlock();

		mLastTTLTimeCheck = GetCurrentTime();
	}
	
	return status;
}
#endif //#ifdef DO_TIME_CHECKS_ON_TTLS

#pragma mark еее Protected Methods for SLPRegistrar еее

SLPClique* SLPRegistrar::FindClique( 	char* serviceType,
                                                UInt16 serviceTypeLen,
                                                char* scopeList,
                                                UInt16 scopeListLen,
                                                char* attributeList,
                                                UInt16 attributeListLen )
{
	SLPClique*				curClique = NULL;
	CFStringRef				keyRef;
    char					keyString[512] = {0};
    
    if ( mListOfSLPCliques )
    {
        memcpy( keyString, serviceType, serviceTypeLen );
        memcpy( keyString+serviceTypeLen, "://", 3 );
        memcpy( keyString+serviceTypeLen+3, scopeList, scopeListLen );
        
//        keyRef = ::CFStringCreateWithCString( kCFAllocatorDefault, keyString, CFStringGetSystemEncoding() );
        keyRef = ::CFStringCreateWithCString( kCFAllocatorDefault, keyString, kCFStringEncodingUTF8 );
        assert( keyRef );
        
        if ( ::CFDictionaryGetCount( mListOfSLPCliques ) > 0 && ::CFDictionaryContainsKey( mListOfSLPCliques, keyRef ) )
            curClique = (SLPClique*)::CFDictionaryGetValue( mListOfSLPCliques, keyRef );
        else
        {
            SLP_LOG( SLP_LOG_DROP, "Unable to find SLPClique for request %s in %s.  Dictionary count: %d", serviceType, scopeList,  ::CFDictionaryGetCount( mListOfSLPCliques ) );
        }
    
        ::CFRelease( keyRef );
    }
    
	return curClique;
}

#pragma mark -
CFStringRef SLPServiceInfoCopyDesctriptionCallback ( const void *item )
{
    ServiceInfo*		serviceInfo = (ServiceInfo*)item;
    
    return serviceInfo->GetURLRef();
}

Boolean SLPServiceInfoEqualCallback ( const void *item1, const void *item2 )
{
    ServiceInfo*		serviceInfo1 = (ServiceInfo*)item1;
    ServiceInfo*		serviceInfo2 = (ServiceInfo*)item2;
    
    return ( serviceInfo1 == serviceInfo2 || *serviceInfo1 == *serviceInfo2 );
}

CFHashCode SLPServiceInfoHashCallback(const void *key)
{
    return 2;
//    return CFHash( key );
}

void SLPServiceInfoHandlerFunction(const void *inValue, void *inContext)
{
    ServiceInfo*					curServiceInfo = (ServiceInfo*)inValue;
    SLPServiceInfoHandlerContext*	context = (SLPServiceInfoHandlerContext*)inContext;
    SLPClique*						clique = (SLPClique*)context->dataPtr;
    
    switch ( context->message )
    {
        case kDeleteSelf:
            clique->RemoveServiceInfoFromClique( curServiceInfo );		// this will do the right clean up and notify RAdmin
//            curServiceInfo->RemoveInterest();
        break;
        
        case kUpdateCache:
            clique->AddToCachedReply( curServiceInfo );
        break;
        
        case kReportServicesToRAdmin:
            clique->NotifyRAdminOfChange( kServiceAdded, curServiceInfo );
        break;
        
        case kDoTimeCheckOnTTLs:
            if ( AreWeADirectoryAgent() )
            {
                if ( curServiceInfo->IsTimeToExpire() )
                {
                    SLP_LOG( SLP_LOG_EXP, "Service URL Expired: URL=%s, SCOPE= %s", curServiceInfo->GetURLPtr(), curServiceInfo->GetScope() );
                    clique->RemoveServiceInfoFromClique(curServiceInfo);
                }
            }
        break;
        
        default:
            SLP_LOG( SLP_LOG_DEBUG, "SLPServiceInfoHandlerFunction - received unhandled message type!" );
        break;
    };
}

#pragma mark еее Public Methods for SLPClique еее
SLPClique::SLPClique()
{
	mSelfPtr = NULL;
	mSetOfServiceInfos = NULL;
	mCliqueScope = NULL;
	mCliqueServiceType = NULL;
	mCachedReplyForClique = NULL;
	mCacheSize = 0;
	mLastRegDeregistrationTime = 0;
	mIsCacheDirty = true;
	mNotifyRAdminOfChanges = false;
    mKeyRef = NULL;
}

SLPClique::~SLPClique()
{
	
	if ( mSetOfServiceInfos )
	{
        SLPServiceInfoHandlerContext	context = {kDeleteSelf, this};

    	::CFSetApplyFunction( mSetOfServiceInfos, SLPServiceInfoHandlerFunction, &context );
        ::CFSetRemoveAllValues( mSetOfServiceInfos );
        ::CFRelease( mSetOfServiceInfos );
        mSetOfServiceInfos = NULL;
	}

	if ( mCliqueScope )
	{
		free( mCliqueScope );
		mCliqueScope = NULL;
	}

	if ( mCliqueServiceType )
	{
		free( mCliqueServiceType );
		mCliqueServiceType = NULL;
	}

	if ( mCachedReplyForClique )
	{
		free( mCachedReplyForClique );
		mCachedReplyForClique = NULL;
	}
    
    if ( mKeyRef )
    {
        CFRelease( mKeyRef );
        mKeyRef = NULL;
    }
}

void SLPClique::IntitializeInternalParams( ServiceInfo* exampleSI )
{
#ifdef USE_EXCEPTIONS
	ThrowIfNULL_( exampleSI );

	Try_
#else
	if ( !exampleSI || !exampleSI->GetScope() )
		return;
#endif
	{
#ifdef USE_EXCEPTIONS
		ThrowIfNULL_( exampleSI->GetScope() );
#endif
		mCliqueScope = (char*)malloc( exampleSI->GetScopeLen()+1 );			// scope for this clique
#ifdef USE_EXCEPTIONS
		ThrowIfNULL_( mCliqueScope );
#endif
		if ( mCliqueScope )
		{
			::strcpy( mCliqueScope, exampleSI->GetScope() );
	
			UInt16		serviceTypeLen;
			char*		serviceTypePtr = exampleSI->PtrToServiceType(&serviceTypeLen) ;
	
			mCliqueServiceType = (char*)malloc( serviceTypeLen+1 );
#ifdef USE_EXCEPTIONS
			ThrowIfNULL_( mCliqueServiceType );
#endif
			if ( mCliqueServiceType )
			{
				::memcpy( mCliqueServiceType, serviceTypePtr, serviceTypeLen );
				mCliqueServiceType[serviceTypeLen] = '\0';
				
				char		keyString[512];
				sprintf( keyString, "%s://%s", mCliqueServiceType, mCliqueScope );
		//        mKeyRef = ::CFStringCreateWithCString( kCFAllocatorDefault, keyString, CFStringGetSystemEncoding() );
				mKeyRef = ::CFStringCreateWithCString( kCFAllocatorDefault, keyString, kCFStringEncodingUTF8 );       
			}
		}
    }
	
#ifdef USE_EXCEPTIONS
	Catch_( inErr )
	{
		if ( mCliqueScope )
		{
			free( mCliqueScope );
			mCliqueScope = NULL;
		}
		
		if ( mCliqueServiceType )
		{
			free( mCliqueServiceType );
			mCliqueServiceType = NULL;
		}
		
		Throw_( inErr );
	}
#endif
}	

void SLPClique::Initialize( ServiceInfo* newService )
{
#ifdef USE_EXCEPTIONS
	ThrowIfNULL_( newService );
#endif
	if ( !newService )
		return;
		
	// database initialization
	CFSetCallBacks	callBack;
    
    callBack.version = 0;
    callBack.retain = NULL;
    callBack.release = NULL;
    callBack.copyDescription = SLPServiceInfoCopyDesctriptionCallback;
    callBack.equal = SLPServiceInfoEqualCallback;
    callBack.hash = SLPServiceInfoHashCallback;
//    callBack.hash = NULL;
    
    mSetOfServiceInfos = ::CFSetCreateMutable( NULL, 0, &callBack );

	IntitializeInternalParams( newService );
	
	mSelfPtr = this;
}
	
#pragma mark -
Boolean SLPClique::IsServiceInfoOKToJoinClique( ServiceInfo* service )
{
	
#ifdef USE_EXCEPTIONS
	ThrowIfNULL_( service );
	ThrowIfNULL_( service->GetScope() );
#endif
	if ( !service || !service->GetScope() )
		return false;
		
	UInt16		serviceTypeLen;
	
	Boolean		okToJoin = false;

	if ( this->ServiceTypeMatchesClique( service->PtrToServiceType( &serviceTypeLen ), serviceTypeLen ) )
	{
		if ( this->ScopeMatchesClique( service->GetScope(), ::strlen( service->GetScope() ) ) )		// now what if this is a scopeList?????
		{		
			okToJoin = true;
		}
	}
	
	return okToJoin;
}

Boolean SLPClique::IsServiceInfoInClique( ServiceInfo* service )
{
	
	Boolean			serviceFound = false;
    
    if ( mSetOfServiceInfos && service && ::CFSetGetCount( mSetOfServiceInfos ) > 0 )
        serviceFound = ::CFSetContainsValue( mSetOfServiceInfos, service );
		
	return serviceFound;
}

void SLPClique::NotifyRAdminOfChange( ChangeType type, ServiceInfo* service )
{
    if ( SLPRegistrar::TheSLPR()->RAdminNotificationEnabled() && AreWeADirectoryAgent() )
    {
        char*		dataSendBuffer = NULL;
        UInt32		dataSendBufferLen = 0;
        OSStatus	status = noErr;
        
        if ( type == kServiceAdded )
            status = MakeSLPServiceInfoAddedNotificationBuffer( service, &dataSendBufferLen, &dataSendBuffer );
        else
            status = MakeSLPServiceInfoRemovedNotificationBuffer( service, &dataSendBufferLen, &dataSendBuffer );
           
// let's just push this onto a queue for a central thread to process...
        SLPRegistrar::TheSLPR()->AddNotification( dataSendBuffer, dataSendBufferLen );		// mRAdminNotifier takes control of buffer free

        if ( status )
        {
            SLP_LOG( SLP_LOG_DROP, "Error notifing ServerAdmin of Service Change: %s", strerror(status) );
        }
    }
}

SLPInternalError SLPClique::AddServiceInfoToClique( ServiceInfo* service )
{
	
    SLPInternalError			status = SLP_OK;
    ServiceInfo*		siToNotifyWith = service;
    
    if ( IsServiceInfoInClique( service ) )
    {
        status = SERVICE_ALREADY_REGISTERED;
        siToNotifyWith = (ServiceInfo*)::CFSetGetValue( mSetOfServiceInfos, service );
    }
    else if ( mSetOfServiceInfos )
        CFSetAddValue( mSetOfServiceInfos, service );

    if ( !status )
    {
        SLP_LOG( SLP_LOG_REG, "New Service Registered, URL=%s. SCOPE=%s", service->GetURLPtr(), service->GetScope() );

        SLP_LOG( SLP_LOG_MSG, "Total Services: %d", ++gTotalServices );
    
	// increment the usage count for the serviceInfo
        service->AddInterest();

        mIsCacheDirty = true;
        
        mLastRegDeregistrationTime = GetCurrentTime();
    }
    else
    {
        SLP_LOG( SLP_LOG_MSG, "Duplicate Service, update timestamp, URL=%s, SCOPE=%s", service->GetURLPtr(), service->GetScope() );    
					
        // We need to update the reg time (in future, need to support partial registration!)
        //UpdateRegisteredServiceTimeStamp( service );
        siToNotifyWith->UpdateLastRegistrationTimeStamp();
    }
        
    NotifyRAdminOfChange( kServiceAdded, siToNotifyWith );
        	
	return SLP_OK;		// don't need to return error
}

SLPInternalError SLPClique::RemoveServiceInfoFromClique( ServiceInfo* service )
{
	
	ServiceInfo*	serviceInClique = NULL;		// the SI we are passed in may not be the exact object...
	SLPInternalError		status = SLP_OK;
	
    if ( IsServiceInfoInClique( service ) )
        serviceInClique = (ServiceInfo*)::CFSetGetValue( mSetOfServiceInfos, service );

    if ( serviceInClique )
    {
        ::CFSetRemoveValue( mSetOfServiceInfos, serviceInClique );
        
        {
            SLP_LOG( SLP_LOG_REG, "Service Deregistered, URL=%s, SCOPE=%s", service->GetURLPtr(), service->GetScope() );

            SLP_LOG( SLP_LOG_MSG, "Total Services: %d", --gTotalServices );
        }
        
        NotifyRAdminOfChange( kServiceRemoved, service );

		// decrement the usage count for the serviceInfo
		serviceInClique->RemoveInterest();
	
		mIsCacheDirty = true;
		mLastRegDeregistrationTime = GetCurrentTime();
    }
    else
    {
		status = SERVICE_NOT_REGISTERED;
	}
	
	return status;
}

SLPInternalError SLPClique::UpdateRegisteredServiceTimeStamp( ServiceInfo* service )
{
	SLPInternalError		status = SERVICE_NOT_REGISTERED;
	
	ServiceInfo*			serviceInClique = NULL;
	
    if ( IsServiceInfoInClique( service ) )
        serviceInClique = (ServiceInfo*)::CFSetGetValue( mSetOfServiceInfos, service );

    if ( serviceInClique )
    {
        serviceInClique->UpdateLastRegistrationTimeStamp();
        status = SLP_OK;
    }
    else
    {
        SLP_LOG( SLP_LOG_DA, "SLPClique::UpdateRegisteredServiceTimeStamp couldn't find service to update!, URL=%s, SCOPE=%s", service->GetURLPtr(), service->GetScope() );
    }
    
    return status;
}

Boolean SLPClique::ServiceTypeMatchesClique( char* serviceType, UInt16 serviceTypeLen )
{
#ifdef USE_EXCEPTIONS
    ThrowIfNULL_( mCliqueServiceType );
    ThrowIfNULL_( serviceType );
#endif
	if ( !mCliqueServiceType || !serviceType )
		return false;
		
    Boolean		match = false;
	
    if ( serviceTypeLen == ::strlen( mCliqueServiceType ) )
        match = ( ::memcmp( mCliqueServiceType, serviceType, serviceTypeLen ) == 0 );
		
    return match;
}

Boolean SLPClique::ScopeMatchesClique( char* scopePtr, UInt16 scopeLen )
{
#ifdef USE_EXCEPTIONS
    ThrowIfNULL_( mCliqueScope );
    ThrowIfNULL_( scopePtr );
#endif
	if ( !mCliqueScope || !scopePtr )
		return false;
		
    Boolean		match = false;
    
    if ( scopeLen == ::strlen( mCliqueScope ) )
        match = ( ::memcmp( mCliqueScope, scopePtr, scopeLen ) == 0 );
            
    return match;
}

#pragma mark -
SLPInternalError SLPClique::UpdateCachedReplyForClique( ServiceInfo* addNewServiceInfo )
{
    SLPInternalError		error = SLP_OK;
    
    if ( addNewServiceInfo )
    {
        {
            SLP_LOG( SLP_LOG_DEBUG, "SLPClique::UpdateCachedReplyForClique is calling AddToServiceReply for clique: SCOPE=%s, TYPE=%s", mCliqueScope, mCliqueServiceType );
        }

        // ok, just append the new one.
        AddToServiceReply( mNeedToUseTCP, addNewServiceInfo, NULL, 0, error, &mCachedReplyForClique );
                        
        if ( error == SLP_REPLY_TOO_BIG_FOR_PROTOCOL )
        {
            error = SLP_OK;
            mNeedToUseTCP = true;
            AddToServiceReply( mNeedToUseTCP, addNewServiceInfo, NULL, 0, error, &mCachedReplyForClique );	// its ok, add it now and TCP is set
        }
    }
    else
    {
        // this isn't just adding a new one, recalc from scratch
        {
            SLP_LOG( SLP_LOG_DEBUG, "SLPClique::UpdateCachedReplyForClique is rebuilding its cached data for clique: SCOPE=%s, TYPE=%s", mCliqueScope, mCliqueServiceType );
        }

        if ( mCachedReplyForClique )
            free( mCachedReplyForClique );
            
        mCachedReplyForClique = NULL;
        mNeedToUseTCP = false;			// reset this
        
        SLPServiceInfoHandlerContext	context = {kUpdateCache, this};

    	::CFSetApplyFunction( mSetOfServiceInfos, SLPServiceInfoHandlerFunction, &context );
    }
	
    if ( mCachedReplyForClique )
        mCacheSize = GETLEN( mCachedReplyForClique );
    else
        mCacheSize = 0;

    if ( !error )
        mIsCacheDirty = false;

    return error;
}

void SLPClique::AddToCachedReply( ServiceInfo* curSI )
{
    SLPInternalError		error = SLP_OK;
    
    AddToServiceReply( mNeedToUseTCP, curSI, NULL, 0, error, &mCachedReplyForClique );
                            
    if ( error == SLP_REPLY_TOO_BIG_FOR_PROTOCOL )
    {
        error = SLP_OK;
        mNeedToUseTCP = true;
        AddToServiceReply( mNeedToUseTCP, curSI, NULL, 0, error, &mCachedReplyForClique );	// its ok, add it now and TCP is set
    }
    else if ( error )
        mslplog( SLP_LOG_DEBUG, "ServiceAgent::UpdateCachedReplyForClique(), AddToServiceReply() returned:", strerror(error) );
}

UInt16 SLPClique::GetCacheSize()
{
	
    return mCacheSize;
}

SLPInternalError SLPClique::GetPtrToCacheWithXIDFromCurrentRequest( char* buffer, UInt16 length, char** serviceReply, UInt16* replyLength )
{
    SLPInternalError status = SLP_OK;
    
    if ( !mIsCacheDirty )
    {
        CopyXIDFromRequestToCachedReply( (ServiceLocationHeader*)buffer, (ServiceLocationHeader*)mCachedReplyForClique );
        
        *serviceReply = mCachedReplyForClique;
        *replyLength = mCacheSize;
    }
    else
        status = (SLPInternalError)DA_BUSY_NOW;
            
    return status;
}

#pragma mark -
void SLPClique::ReportAllServicesToRAdmin( void )
{
	SLPServiceInfoHandlerContext	context = {kReportServicesToRAdmin, this};

	if ( mSetOfServiceInfos )
		::CFSetApplyFunction( mSetOfServiceInfos, SLPServiceInfoHandlerFunction, &context );
}

SLPInternalError SLPClique::CopyCachedReturnData( char* returnBuffer, UInt16 maxBufferLen, UInt16* returnBufLen )
{
#ifdef USE_EXCEPTIONS
	ThrowIfNULL_( returnBuffer );
	ThrowIfNULL_( returnBufLen );
#endif
	if ( !returnBuffer || !returnBufLen )
		return SLP_INTERNAL_SYSTEM_ERROR;
		
	SLPInternalError		status = SLP_OK;
	
	if ( mCacheSize <= maxBufferLen )
	{
		memcpy( returnBuffer, mCachedReplyForClique, mCacheSize );
		*returnBufLen = mCacheSize;
	}
	else
	{
		// we have too much data,
		*returnBufLen = GETHEADERLEN(mCachedReplyForClique)+4;

		memcpy( returnBuffer, mCachedReplyForClique, *returnBufLen );
		
		SETLEN( returnBuffer, *returnBufLen );
		SETFLAGS( returnBuffer, OVERFLOWFLAG );
		
		*((UInt16*)(returnBuffer+GETHEADERLEN(returnBuffer))) = 0;					// set the error
		*((UInt16*)((char*)returnBuffer+GETHEADERLEN(returnBuffer)+2)) = 0;		// set the entry count to zero		
	}
		
	return status;
}

#pragma mark -
#pragma mark - еее SLPClique Protected Methods еее
#pragma mark -
#pragma mark Services in a file stuff
#pragma mark -

#pragma mark -

#pragma mark -
pthread_mutex_t	SLPRAdminNotifier::mQueueLock;

CFStringRef SLPRAdminNotifierCopyDesctriptionCallback ( const void *item )
{
    return CFSTR("SLP RAdmin Notification");
}

Boolean SLPRAdminNotifierEqualCallback ( const void *item1, const void *item2 )
{
    return item1 == item2;
}

SLPRAdminNotifier::SLPRAdminNotifier()
	: LThread(threadOption_Default)
{
	CFArrayCallBacks	callBack;
    
    LOG( SLP_LOG_NOTIFICATIONS, "SLPRAdminNotifier Created" );
    callBack.version = 0;
    callBack.retain = NULL;
    callBack.release = NULL;
    callBack.copyDescription = SLPRAdminNotifierCopyDesctriptionCallback;
    callBack.equal = SLPRAdminNotifierEqualCallback;

    mCanceled = false;
    mNotificationsQueue = ::CFArrayCreateMutable ( NULL, 0, &callBack );
    
    pthread_mutex_init( &mQueueLock, NULL );
    mClearQueue = false;
    mRepostAllScopes = false;
    mRepostAllData = false;
    mDeregisterAllData = false;
}

SLPRAdminNotifier::~SLPRAdminNotifier()
{
    if ( mNotificationsQueue )
        CFRelease( mNotificationsQueue );
        
    mNotificationsQueue = NULL;
}

void SLPRAdminNotifier::Cancel( void )
{
    mCanceled = true;
}

void* SLPRAdminNotifier::Run( void )
{
    NotificationObject*	notification = NULL;
    
    while ( !mCanceled )
    {
        if ( mClearQueue )
        {
            QueueLock();
            LOG( SLP_LOG_NOTIFICATIONS, "SLPRAdminNotifier clearing queue" );
            mClearQueue = false;
            ::CFArrayRemoveAllValues( mNotificationsQueue );
            QueueUnlock();
        }
        else if ( mRepostAllScopes )
        {
            mRepostAllScopes = false;
            LOG( SLP_LOG_NOTIFICATIONS, "SLPRAdminNotifier reposting all scopes" );
            SLPRegistrar::TheSLPR()->SendRAdminAllScopes();
        }
        else if ( mRepostAllData )
        {
            mRepostAllData = false;
            LOG( SLP_LOG_NOTIFICATIONS, "SLPRAdminNotifier reposting all currently registered services" );
            SLPRegistrar::TheSLPR()->SendRAdminAllScopes();
            SLPRegistrar::TheSLPR()->SendRAdminAllCurrentlyRegisteredServices();
        }
        else if ( mDeregisterAllData )
        {
            mDeregisterAllData = false;
            LOG( SLP_LOG_NOTIFICATIONS, "SLPRAdminNotifier deregistering all services" );
            SLPRegistrar::TheSLPR()->DeregisterAllServices();
        }
        else
        {
            // grab next element off the queue and process
            QueueLock();
            if ( mNotificationsQueue && ::CFArrayGetCount( mNotificationsQueue ) > 0 )
            {
                notification = (NotificationObject*)::CFArrayGetValueAtIndex( mNotificationsQueue, 0 );		// grab the first one
                ::CFArrayRemoveValueAtIndex( mNotificationsQueue, 0 );
                QueueUnlock();
            }
            else
            {
                QueueUnlock();
                sleep(1);
            }
            
            if ( notification )
            {
                SLP_LOG( SLP_LOG_NOTIFICATIONS, "SLPRAdminNotifier Sending %s Notification to ServerAdmin", PrintableHeaderType(notification->data) );
                
                SendNotification( notification );
                delete notification;
                notification = NULL;
            }
        }
    }
    
    return NULL;
}

// delayed action
void SLPRAdminNotifier::AddNotificationToQueue( char* buffer, UInt32 bufSize )
{
    NotificationObject*		newNotification = new NotificationObject( buffer, bufSize );
    
    QueueLock();
    if ( mNotificationsQueue )
    {
        ::CFArrayAppendValue( mNotificationsQueue, newNotification );
        
        SLP_LOG( SLP_LOG_DEBUG, "Notification Added to Queue" );
    }
    QueueUnlock();
}

// delayed action
void SLPRAdminNotifier::RepostAllRegisteredData( void )
{
    ClearQueue();
    RepostAllData();
}

// delayed action
void SLPRAdminNotifier::DeregisterAllRegisteredData( void )
{
    DeregisterAllData();
}

// immediate action
void SLPRAdminNotifier::SendNotification( NotificationObject*	notification )
{
    char*		ignoreReply = NULL;
    UInt32		ignoreReplyLen = 0;
    OSStatus	status = noErr;
    
    status = SendDataToSLPRAdmin( notification->data, notification->dataLen, &ignoreReply, &ignoreReplyLen );
    
    if ( ignoreReply && ((SLPdMessageHeader*)ignoreReply)->messageStatus == kSLPTurnOffRAdminNotifications )
        SLPRegistrar::TheSLPR()->DisableRAdminNotification();			// they don't want notifications!
        
    if ( ignoreReply )
        free( ignoreReply );
        
    if ( status )
    {
        SLP_LOG( SLP_LOG_DROP, "Error notifing ServerAdmin of Service Change: %s", strerror(status) );
    }
}

#pragma mark -
#if 0
Boolean IsLegalURLChar( const char theChar )
{
	return ( isprint( theChar ) && ( theChar != ' ' && theChar != '\"' && theChar != '<' && theChar != '>' && theChar != ',' ) );
}

Boolean	AllLegalURLChars( const char* theString, UInt32 theURLLength )
{

	Boolean	isLegal = true;
	
	if ( theString )				// make sure we have a string to examine
	{		
		for (SInt32 i = theURLLength - 1; i>=0 && isLegal; i--)
			isLegal = IsLegalURLChar( theString[i] );
	}
	else
		isLegal = false;
	
	return isLegal;
	
}

Boolean	IsURL( const char* theString, UInt32 theURLLength, char** svcTypeOffset )
{

	Boolean		foundURL=false;
	
	if ( AllLegalURLChars(theString, theURLLength) )
	{
		*svcTypeOffset = strstr(theString, ":/");			// look for the interesting tag chars
		if ( *svcTypeOffset != NULL)
			foundURL = true;
	}
	
	return foundURL;
	
}

void GetServiceTypeFromURL(	const char* readPtr,
							UInt32 theURLLength,
							char*	URLType )
{

	char*	curOffset = NULL;
	UInt16	typeLen;
	
	if ( IsURL( readPtr, theURLLength, &curOffset))
	{
		typeLen = curOffset - readPtr;
//		::BlockMove((unsigned char*)readPtr, URLType, typeLen);
        memcpy( URLType, (unsigned char*)readPtr, typeLen );
		URLType[typeLen] = '\0';
	}
	else
	{
		URLType[0] = '\0';				// nothing here to find
	}

}
#endif

void AddToServiceReply( Boolean usingTCP, ServiceInfo* serviceInfo, char* requestHeader, UInt16 length, SLPInternalError& error, char** replyPtr )
{
#pragma unused( length)
	char*	siURL = NULL;
    UInt16	siURLLen = 0;
	char*	newReply = NULL;
	char*	curPtr = NULL;
    char*	newReplyTemp = *replyPtr;
	UInt32	replyLength, newReplyLength;
    UInt16	xid;
	
	if ( newReplyTemp == NULL )
	{
        // this is a new reply, fill out empty header + room for error code and url entry count
		if ( requestHeader )
			replyLength = GETHEADERLEN(requestHeader)+4;
		else
			replyLength = HDRLEN+2+4;

		newReplyTemp = (char*)malloc( replyLength );
#ifdef USE_EXCEPTIONS
		ThrowIfNULL_( newReplyTemp );
#endif
		if ( !newReplyTemp )
			return;
			
		if ( requestHeader )
			xid = GETXID( requestHeader );
		else
			xid = 0;
   
        SETVER( newReplyTemp, SLP_VER );
        SETFUN( newReplyTemp, SRVRPLY );
        SETLEN( newReplyTemp, replyLength );
        SETFLAGS( newReplyTemp, 0 );
        SETXID( newReplyTemp, xid );
//        SETLANG( newReplyTemp, SLPGetProperty("net.slp.locale") );
    
        ServiceLocationHeader*	header = (ServiceLocationHeader*)newReplyTemp;
        *((UInt16*)&(header->byte13)) = 2;					// this is slightly bogus.  This determines how many bytes
                                                                            // are in the languageTag.  I'm hard coding to 'en'
    
        *((UInt16*)&(header->byte15)) = *((UInt16*)"en");

		*((UInt16*)(newReplyTemp+GETHEADERLEN(newReplyTemp))) = error;			// set the error
		*((UInt16*)((char*)newReplyTemp+GETHEADERLEN(newReplyTemp)+2)) = 0;		// set the entry count to zero
    }

	replyLength = GETLEN(newReplyTemp);
	
	if ( serviceInfo )
    {
        siURL = serviceInfo->GetURLPtr();
        siURLLen = strlen(siURL);
    }
	
	if ( serviceInfo )
    {
        UInt16 urlEntryLength = 1 + 2 + 2 + siURLLen + 1;				// reserved, lifetime, url len, url, # url authentication blocks
        
        newReplyLength = replyLength + urlEntryLength;
	}
    else
        newReplyLength = replyLength;
        
    if ( newReplyLength > MAX_REPLY_LENGTH )
    {
        SLP_LOG( SLP_LOG_MSG, "Size of reply exceeds maximum allowed by SLP, some services will be ignored." );
        return;
    }
    
	if ( !usingTCP )
	{
		// check the size to see if the new length is going to be bigger than our UDP Buffer size
		char*	endPtr = NULL;
		if ( newReplyLength > (UInt32)strtol(SLPGetProperty("net.slp.MTU"),&endPtr,10) )
		{
			SETFLAGS(newReplyTemp, OVERFLOWFLAG);
			error = SLP_REPLY_TOO_BIG_FOR_PROTOCOL;
			return;
		}
	}
	
	curPtr = newReplyTemp + GETHEADERLEN(newReplyTemp);
	*((UInt16*)curPtr) = (UInt16)error;	// now this is going to get overridden each time, do we care?
	
	curPtr += 2;	// advance past the error to the urlEntry count;

	if ( !serviceInfo )
    {
        *((UInt16*)curPtr) = 0;	// zero the urlEntry count;
        
        (*replyPtr) = newReplyTemp;
    }
    else
    {
        *((UInt16*)curPtr) += 1;	// increment the urlEntry count;
        
        newReply = (char*)malloc( newReplyLength );
        
#ifdef USE_EXCEPTIONS
        ThrowIfNULL_( newReply );
#endif
		if (!newReply )
			return;
			
        ::memcpy( newReply, newReplyTemp, replyLength );
        free( newReplyTemp );
        
        curPtr = newReply+replyLength;						// now we should be pointing at the end of old data, append new url entry
        
        *curPtr = 0;										// zero out the reserved bit
        curPtr++;
        
        *((UInt16*)curPtr) = serviceInfo->GetLifeTime();	// set lifetime
        curPtr += 2;
        
        *((UInt16*)curPtr) = siURLLen;					// set url length
        curPtr += 2;
        
        if ( siURL )
            ::memcpy( curPtr, siURL, siURLLen );
            
        curPtr += siURLLen;
        
        *curPtr = 0;						// this is for the url auth block (zero of them)
        
        SETLEN(newReply, newReplyLength);
        
        (*replyPtr) = newReply;
    }
}



#endif //#ifdef USE_SLPREGISTRAR
