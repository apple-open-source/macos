/*
 *  SLPDARegisterer.h
 *  NSLPlugins
 *
 *  Created by karnold on Mon Mar 19 2001.
 *  Copyright (c) 2001 __CompanyName__. All rights reserved.
 *
 */

#ifndef _SLPDARegisterer_
#define _SLPDARegisterer_
#pragma once

#include "LThread.h"

enum ActionType {
    kRegLocalService,
    kDeregLocalService,
    kRegisterAllServices,
    kRegisterAllServicesWithNewDA
};
 
struct DAInfoObj
{
    struct sockaddr_in 	sinDA;
    char* 				scopeList;
};

class RegData
{
public:
                RegData( const char* urlPtr, UInt32 urlLen, const char* scopeListPtr, UInt32 scopeListLen, const char* attributeListPtr, UInt32 attributeListLen );
                ~RegData();
                
    char*		mURLPtr;
    char*		mScopeListPtr;
    char*		mScopeList;
    char*		mAttributeListPtr;
    UInt32		mURLLen;
    UInt32		mScopeListLen;
    UInt32		mAttributeListLen;
};

void RegisterNewService( RegData* newReg );
void DeregisterService( RegData* newReg );

struct RegistrationObject
{
    ActionType	action;
    void*		data;
};

class SLPDARegisterer : public LThread
{
public:
    SLPDARegisterer( SLPHandle serverState );
    ~SLPDARegisterer();
    
    virtual void*		Run();
    
    void				RegisterAllServices( void ) { mRegisterAllServices = true; }
    void				AddTask( RegistrationObject* newTask );
    
    static void			QueueLock( void ) { pthread_mutex_lock( &mQueueLock ); }
    static void			QueueUnlock( void ) { pthread_mutex_unlock( &mQueueLock ); }
    static pthread_mutex_t		mQueueLock;

protected:
	SAState*			mServerState;
	SLPHandle			mSLPSA;
    CFMutableArrayRef	mActionQueue;
    Boolean				mClearQueue;
    Boolean				mRegisterAllServices;
    Boolean				mRegFileNeedsProcessing;
    Boolean				mCanceled;
};

#endif