/*
	File:		SLPRegistrar.h

	Contains:	Class to deal with all registered services, cached requests and db access

	Written by:	Kevin Arnold

	Copyright:	© 1997 - 2000 by Apple Computer, Inc., all rights reserved.

	Change History (most recent first):

*/

#define USE_SLPREGISTRAR
#ifdef USE_SLPREGISTRAR

#ifndef _SLPRegistrar_
#define _SLPRegistrar_
#pragma once

#include <pthread.h>	// for pthread_*_t
#include <unistd.h>		// for _POSIX_THREADS

#include <CoreServices/CoreServices.h>

#include "LThread.h"
//#include "SimpleList.h"

#define DO_TIME_CHECKS_ON_TTLS	1
#define kTimeBetweenTTLTimeChecks 5*60	// seconds - five minutes

class ServiceAgent;
class ServiceInfo;
#ifdef USE_SA_ONLY_FEATURES
class DAInfo;
#endif // #ifdef USE_SA_ONLY_FEATURES
class SLPClique;
class SLPRAdminNotifier;

typedef enum {
        kDeleteSelf,
        kDeleteCliquesMatchingScope,
        kReportServicesToRAdmin,
        kUpdateCache,
        kDoTimeCheckOnTTLs
} SLPHandlerMessageType;

typedef struct SLPCliqueHandlerContext {
    CFMutableDictionaryRef		dictionary;
    SLPHandlerMessageType		message;
    void*						dataPtr;
} SLPCliqueHandlerContext;

typedef struct SLPServiceInfoHandlerContext {
    SLPHandlerMessageType		message;
    void*						dataPtr;
} SLPServiceInfoHandlerContext;

enum ChangeType 
{
    kServiceAdded,
    kServiceRemoved
};

class SLPRegistrar
{
public:
friend class ServiceAgent;										// <06>

	SLPRegistrar();
	~SLPRegistrar();
	
    static SLPRegistrar* SLPRegistrar::TheSLPR( void );
    
	void		Initialize( void );
	void		LoadFromRegFile( Boolean waitForKosherFileIfNeeded=false );
	
	Boolean		SafeToUse( void ) { return this == mSelfPtr; };

	Boolean		IsServiceAlreadyRegistered( ServiceInfo* service );
	
	SLPInternalError	RegisterService( ServiceInfo* service );
	SLPInternalError	DeregisterService( ServiceInfo* service );
	
    SLPInternalError	RemoveScope( char* scope );
    
    Boolean		RAdminNotificationEnabled( void ) { return SLPGetProperty("com.apple.slp.RAdminNotificationsEnabled") && !SDstrcasecmp(SLPGetProperty("com.apple.slp.RAdminNotificationsEnabled"),"true") ; };
    void		EnableRAdminNotification( void );
    void		DisableRAdminNotification( void ) { SLPSetProperty( "com.apple.slp.RAdminNotificationsEnabled", "false" ); };
    
    void		AddNotification( char* buffer, UInt32 bufSize );
    void		DoSendRAdminAllCurrentlyRegisteredServices( void );		// this creates a thread to call the below method
    
    void		SendRAdminSLPStarted( void );
    void		SendRAdminSLPStopped( void );
    
    void		SendRAdminDeleteAllScopes( void );
    void		SendRAdminAllScopes( void );
    void		SendRAdminAddedScope( const char* newScope );
    void		SendRAdminDeletedScope( const char* oldScope );
    void		SendRAdminAllCurrentlyRegisteredServices( void );
    
    void		DoDeregisterAllServices( void );
    void		DeregisterAllServices( void );
    
	SLPInternalError	SaveRegisteredServicesToFile( Boolean deleteServicesWhenDone = false );

// this one allocates the reply buffer
	SLPInternalError	CreateReplyToServiceRequest(	
											SLPBoolean viaTCP,
                                            Slphdr *pslphdr,
                                            const char* originalHeader, 
											UInt16 originalHeaderength,
											char* serviceType,
											UInt16 serviceTypeLen,
											char* scopeList,
											UInt16 scopeListLen,
											char* attributeList,
											UInt16 attributeListLen,
											char** returnBuffer, 
                                            int *piOutSz );

	SLPClique*	FindClique( ServiceInfo* service );

	void		UpdateDirtyCaches( void );
	
    long		GetStatelessBootTime( void ) { return mStatelessBootTime; };
    void		SetStatelessBootTime( long newBootTime ) { mStatelessBootTime = newBootTime; };
    
#ifdef DO_TIME_CHECKS_ON_TTLS
	SLPInternalError	DoTimeCheckOnTTLs( void );
#endif //#ifdef DO_TIME_CHECKS_ON_TTLS	
    static void	Lock( void );
    static void	Unlock( void ) { pthread_mutex_unlock( &msObjectLock ); };
    
    static	pthread_mutex_t	msObjectLock;
    static	pthread_mutex_t	msSLPRLock;		// use this in our TheSLPR() call
	
protected:	
	SLPInternalError	SaveDAStampToKosherFile( UInt32 timeStamp );
	SLPInternalError	ReadDAStampFromKosherFile( Boolean waitForKosherFileIfNeeded );
	SLPInternalError	DeleteKosherFile( void );
	SLPInternalError	DeleteRegisteredServicesFiles( void );

	SLPInternalError	ReadRegisteredServicesFromFile( void );
	
	SLPClique*	FindClique( 
                    char* serviceType,
                    UInt16 serviceTypeLen,
                    char* scopeList,
                    UInt16 scopeListLen,
                    char* attributeList,
                    UInt16 attributeListLen );
							
private:
	CFMutableDictionaryRef	mListOfSLPCliques;
	long					mLastTTLTimeCheck;
    long					mStatelessBootTime;
	Boolean					mAlreadyReadRegFile;
	Boolean					mOneOrMoreCliquesNeedUpdating;
	SLPRAdminNotifier*		mRAdminNotifier;
	SLPRegistrar*			mSelfPtr;
};

// The SLPClique class is a logical grouping of all services of a particular service
// type inside a particular scope.
class SLPClique
{
public:
    
	SLPClique();
	~SLPClique();
	
	void		Initialize( ServiceInfo* newService );
	
	void		IntitializeInternalParams( ServiceInfo* exampleSI );
	
	Boolean		SafeToUse( void ) { return this == mSelfPtr; };
	
    CFStringRef	GetKeyRef( void ) { return mKeyRef; };
    CFStringRef GetValueRef( void ) { return mKeyRef; };
    
    CFSetRef	GetSetOfServiceInfosRef( void ) { return mSetOfServiceInfos; };
    
	Boolean		IsServiceInfoOKToJoinClique( ServiceInfo* service );
	Boolean		IsServiceInfoInClique( ServiceInfo* service );
	
	SLPInternalError	AddServiceInfoToClique( ServiceInfo* service );
	SLPInternalError	RemoveServiceInfoFromClique( ServiceInfo* service );
	
	SLPInternalError	UpdateRegisteredServiceTimeStamp( ServiceInfo* service );
	
	Boolean		ServiceTypeMatchesClique( char* serviceType, UInt16 serviceTypeLen );
	Boolean		ScopeMatchesClique( char* scopePtr, UInt16 scopeLen );
	
	Boolean		CacheIsDirty( void ) { return mIsCacheDirty; };
	SLPInternalError	UpdateCachedReplyForClique( ServiceInfo* addNewServiceInfo=NULL );
	UInt16		GetCacheSize();
	
	SLPInternalError	GetPtrToCacheWithXIDFromCurrentRequest( char* buffer, UInt16 length, char** serviceReply, UInt16* replyLength );
	
    void		ReportAllServicesToRAdmin( void );
#ifdef USE_SA_ONLY_FEATURES
	SLPInternalError	RegisterServicesWithDA( DAInfo* daToRegisterWith );
#endif //#ifdef USE_SA_ONLY_FEATURES	
//	ServiceInfoListObj*	GetListOfServiceInfos( void ) { return mListOfServiceInfos; };
	
	char*		CliqueScopePtr( void ) { return mCliqueScope; };
	char*		CliqueServiceTypePtr( void ) { return mCliqueServiceType; };
	SLPInternalError	CopyCachedReturnData( char* returnBuffer, UInt16 maxBufferLen, UInt16* returnBufLen );

	SLPInternalError	SaveServicesToFile( void );
    void		AddToCachedReply( ServiceInfo* curSI );
    void		NotifyRAdminOfChange( ChangeType type, ServiceInfo* service );

protected:
	void		ClearRegisteredServicesFile( void );			// this just deletes the file
	Boolean		GetRegFileSpec( FSSpec* regFile );
	
	SLPInternalError	OpenRegisteredServicesFile( const FSSpec& fileToOpen, short* refNum );	// this will create the file if needed
	SLPInternalError	CloseRegisteredServicesFile( short refNum);

	SLPInternalError	SaveCurrentTimeStamp( short refNum );
	SLPInternalError	SaveRegisteredServiceToFile( ServiceInfo* serviceToSave, short refNum );
	Boolean		LoadListOfServiceInfosIfNecessary();
	SLPInternalError	ReadInListOfServiceInfosFromFile( const FSSpec& fileToReadIn );
	
private:
	CFMutableSetRef	mSetOfServiceInfos;
//	LArray*		mListOfServiceInfos;	// this is a sorted array of Service Infos belonging
										// to this clique.
	char*		mCliqueScope;			// scope for this clique
	char*		mCliqueServiceType;
	char*		mCachedReplyForClique;
    CFStringRef	mKeyRef;
    CFStringRef	mValueRef;
	UInt32		mLastRegDeregistrationTime;
	UInt32		mCacheSize;
	Boolean		mIsCacheDirty;
	Boolean		mNeedToUseTCP;
    Boolean		mNotifyRAdminOfChanges;
	SLPClique*	mSelfPtr;
};

class NotificationObject;

class SLPRAdminNotifier : public LThread
{
public:
    SLPRAdminNotifier();
    ~SLPRAdminNotifier();
    
    void				Cancel( void );
    virtual	void*		Run ( void );
    
    void				RepostAllRegisteredData( void );
    void				DeregisterAllRegisteredData( void );
    
    void				AddNotificationToQueue( char* buffer, UInt32 bufSize );
    
    static void			QueueLock( void ) { pthread_mutex_lock( &mQueueLock ); }
    static void			QueueUnlock( void ) { pthread_mutex_unlock( &mQueueLock ); }
    static pthread_mutex_t		mQueueLock;
protected:
    void				SendNotification( NotificationObject* notification );
    
    void				ClearQueue( void ) { mClearQueue = true; }
    void				RepostAllScopes( void ) { mRepostAllScopes = true; };
    void				RepostAllData( void ) { mRepostAllData = true; }
    void				DeregisterAllData( void ) { mDeregisterAllData = true; }

private:    
    CFMutableArrayRef	mNotificationsQueue;
    Boolean				mClearQueue;
    Boolean				mRepostAllScopes;
    Boolean				mRepostAllData;
    Boolean				mDeregisterAllData;
    
    Boolean				mCanceled;
};

class NotificationObject
{
public:
    NotificationObject( char* inData, UInt32 inDataLen ) { data = inData; dataLen = inDataLen; };
    ~NotificationObject() { if (data) free(data); };
    
    char*			data;
    UInt32			dataLen;
};

SLPInternalError GetSLPRegFolderSpec( FSSpec* regFolderSpec );
void AddToServiceReply( Boolean usingTCP, ServiceInfo* serviceInfo, char* requestHeader, UInt16 length, SLPInternalError& error, char** replyPtr );

#endif // #ifndef _SLPRegistrar_

#endif //#ifdef USE_SLPREGISTRAR
