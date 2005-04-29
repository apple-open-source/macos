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
 *  @header SLPDARegisterer
 */
 
#include "mslp_sd.h"
#include "slp.h"
#include "mslp.h"
#include "mslpd_store.h"
#include "mslp_dat.h"
#include "mslpd.h"
#include "slpipc.h"

#include "SLPDefines.h"
#include "URLUtilities.h"
#include "SLPDARegisterer.h"
#include "CNSLTimingUtils.h"

static SLPDARegisterer*	gRegisterer = NULL;

static void store_free(SAStore store);

struct RegCookie {
	SLPInternalError	errCode;
	Boolean		regFinished;
};

typedef struct RegCookie RegCookie;

void SLPHandleRegReport(
     SLPHandle       hSLP,
     SLPInternalError        errCode,
    void           *pvCookie);

void SLPHandleRegReport(
     SLPHandle       hSLP,
     SLPInternalError        errCode,
     void           *pvCookie)
{
    ((RegCookie*)pvCookie)->errCode = errCode;
    ((RegCookie*)pvCookie)->regFinished = true;
}

void InitializeSLPDARegisterer( SLPHandle serverState )
{
#ifdef ENABLE_SLP_LOGGING
    SLP_LOG( SLP_LOG_MSG, "InitializeSLPDARegisterer called" );
#endif
    if ( !gRegisterer )
    {
        gRegisterer = new SLPDARegisterer( serverState );
        
        if ( gRegisterer )
            gRegisterer->Resume();
    }
}

void RegisterAllServicesWithDA( SLPHandle serverState, struct sockaddr_in sinDA, const char *pcScopes )
{
#ifdef ENABLE_SLP_LOGGING
    SLP_LOG( SLP_LOG_MSG, "RegisterAllServicesWithDA called" );
#endif
    if ( !gRegisterer )
    {
        gRegisterer = new SLPDARegisterer( serverState );
        
        if ( gRegisterer )
            gRegisterer->Resume();

    }

    RegistrationObject*		newTask = new RegistrationObject();
    
    DAInfoObj*			newObj = new DAInfoObj;
    
    newObj->sinDA = sinDA;
    newObj->scopeList = safe_malloc( strlen(pcScopes)+1, pcScopes, strlen(pcScopes) );
    
    assert(newObj->scopeList);
    
    newTask->action = kRegisterAllServicesWithNewDA;
    newTask->data = newObj;
    
    gRegisterer->AddTask( newTask );
}

void RegisterAllServicesWithKnownDAs( SLPHandle serverState )
{
#ifdef ENABLE_SLP_LOGGING
    SLP_LOG( SLP_LOG_MSG, "RegisterAllServicesWithKnownDAs called" );
#endif
    if ( !gRegisterer )
    {
        gRegisterer = new SLPDARegisterer( serverState );
        
        if ( gRegisterer )
            gRegisterer->Resume();
    }
 
    gRegisterer->RegisterAllServices();
}

void RegisterNewService( RegData* newReg )
{
#ifdef ENABLE_SLP_LOGGING
    SLP_LOG( SLP_LOG_MSG, "RegisterNewService called, %s in (%s) with attributes: %s", newReg->mURLPtr, newReg->mScopeListPtr, newReg->mAttributeListPtr );
#endif    
    if ( gRegisterer )
    {
        RegistrationObject*		newTask = new RegistrationObject();
        
        newTask->action = kRegLocalService;
        newTask->data = newReg;
        
        gRegisterer->AddTask( newTask );
    }
    else
    {
#ifdef ENABLE_SLP_LOGGING
        SLP_LOG( SLP_LOG_DEBUG, "RegisterNewService ignoring service since our Registerer hasn't been created!" );
#endif
        delete( newReg );
    }
}

void DeregisterService( RegData* newReg )
{
#ifdef ENABLE_SLP_LOGGING
    SLP_LOG( SLP_LOG_MSG, "DeregisterService called, %s in (%s) with attributes: %s", newReg->mURLPtr, newReg->mScopeListPtr, newReg->mAttributeListPtr );
#endif    
    if ( gRegisterer )
    {
        RegistrationObject*		newTask = new RegistrationObject();
        
        newTask->action = kDeregLocalService;
        newTask->data = newReg;
        
        gRegisterer->AddTask( newTask );
    }
    else
    {
#ifdef ENABLE_SLP_LOGGING
        SLP_LOG( SLP_LOG_DEBUG, "DeregisterService ignoring service since our Registerer hasn't been created!" );
#endif
        delete( newReg );
    }
}

RegData::RegData( const char* urlPtr, UInt32 urlLen, const char* scopeListPtr, UInt32 scopeListLen, const char* attributeListPtr, UInt32 attributeListLen )
{
    if ( urlPtr && urlLen > 0 )
    {
        mURLPtr = (char*)malloc( urlLen + 1 );
        memcpy( mURLPtr, urlPtr, urlLen );
        mURLPtr[urlLen] = '\0';
    }
    else
        mURLPtr = NULL;
        
    if ( scopeListPtr  )
    {
        mScopeListPtr = (char*)malloc( scopeListLen + 1 );
        memcpy( mScopeListPtr, scopeListPtr, scopeListLen );
        mScopeListPtr[scopeListLen] = '\0';
    }
    else
    {
        mScopeListPtr = (char*)malloc( 1 );
        mScopeListPtr[0] = '\0';
    }
       
    if ( attributeListPtr )
    {
        mAttributeListPtr = (char*)malloc( attributeListLen + 1 );
        memcpy( mAttributeListPtr, attributeListPtr, attributeListLen );
        mAttributeListPtr[attributeListLen] = '\0';
    }
    else
    {
        mAttributeListPtr = (char*)malloc( 1 );
        mAttributeListPtr[0] = '\0';
    }
}

RegData::~RegData()
{
    if ( mURLPtr )
        free( mURLPtr );
    
    if ( mScopeListPtr )
        free ( mScopeListPtr );
        
    if ( mAttributeListPtr )
        free( mAttributeListPtr );
}

pthread_mutex_t	SLPDARegisterer::mQueueLock;
const CFStringRef kSLPDARegistererTaskSAFE_CFSTR = CFSTR("SLPDARegisterer Task");

CFStringRef SLPDARegistererCopyDesctriptionCallback ( const void *item )
{
    return kSLPDARegistererTaskSAFE_CFSTR;
}

Boolean SLPDARegistererEqualCallback ( const void *item1, const void *item2 )
{
    return item1 == item2;
}

SLPDARegisterer::SLPDARegisterer( SLPHandle serverState )
	: DSLThread()
{
	CFArrayCallBacks	callBack;
    
	mSLPSA = NULL;
    mServerState = (SAState*)serverState;
#ifdef ENABLE_SLP_LOGGING
    SLPLOG( SLP_LOG_NOTIFICATIONS, "SLPDARegisterer Created" );
#endif
    callBack.version = 0;
    callBack.retain = NULL;
    callBack.release = NULL;
    callBack.copyDescription = SLPDARegistererCopyDesctriptionCallback;
    callBack.equal = SLPDARegistererEqualCallback;

    mCanceled = false;
    mActionQueue = ::CFArrayCreateMutable ( NULL, 0, &callBack );
    
    pthread_mutex_init( &mQueueLock, NULL );
    mClearQueue = false;
    mRegFileNeedsProcessing = false;
    mRegisterAllServices = false;
}

SLPDARegisterer::~SLPDARegisterer()
{
#ifdef ENABLE_SLP_LOGGING
    SLP_LOG( SLP_LOG_MSG, "~SLPDARegisterer called" );
#endif    
    if ( mActionQueue )
        CFRelease( mActionQueue );
        
    mActionQueue = NULL;

    if ( mSLPSA )
        SLPClose( mSLPSA );
}

void SLPDARegisterer::AddTask( RegistrationObject* newTask )
{
    QueueLock();

    if ( mActionQueue )
    {
        ::CFArrayAppendValue( mActionQueue, newTask );
        
#ifdef ENABLE_SLP_LOGGING
        SLP_LOG( SLP_LOG_MSG, "SLPDARegisterer, Task #%d Added to Queue", CFArrayGetCount(mActionQueue) );
#endif
    }

    QueueUnlock();
}

void* SLPDARegisterer::Run()
{
    RegistrationObject*	task = NULL;
    
    while ( !mCanceled )
    {
        try {
            if ( mClearQueue )
            {
                QueueLock();
#ifdef ENABLE_SLP_LOGGING
                SLP_LOG( SLP_LOG_MSG, "SLPDARegisterer clearing queue" );
#endif
                mClearQueue = false;
                ::CFArrayRemoveAllValues( mActionQueue );
                QueueUnlock();
            }
            else
            {
                // grab next element off the queue and process
                QueueLock();
                if ( mActionQueue && ::CFArrayGetCount( mActionQueue ) > 0 )
                {
                    task = (RegistrationObject*)::CFArrayGetValueAtIndex( mActionQueue, 0 );		// grab the first one
                    ::CFArrayRemoveValueAtIndex( mActionQueue, 0 );
                    QueueUnlock();
                }
                else
                {
                    QueueUnlock();
                    
                    if ( mRegFileNeedsProcessing )
                    {
                        SAStore st;
    
                        SDLock( mServerState->pvMutex );
                        mRegFileNeedsProcessing = false;
#ifdef ENABLE_SLP_LOGGING
                        SLP_LOG( SLP_LOG_MSG, "SLPDARegisterer: process changed reg file" );
#endif    
                        if ( process_regfile( &st, SLPGetProperty("com.sun.slp.regfile") ) < 0 )
                        {
#ifdef ENABLE_SLP_LOGGING
                            SLP_LOG( SLP_LOG_ERR, "SLPDARegisterer: changed reg file unparsable, use old one" );
#endif
                        }
                        else
                        {
                            store_free( mServerState->store );              /* prevent leaks     */
                            mServerState->store = st;
#ifdef ENABLE_SLP_LOGGING
                            SLP_LOG( SLP_LOG_MSG, "SLPDARegisterer: read changed reg file, mServerState->store updated" );
#endif
                        }
            
                        if ( mServerState->store.size > 0 )
                        {
                            StartSLPUDPListener( mServerState );
                            StartSLPTCPListener( mServerState );
                        }
                        SDUnlock( mServerState->pvMutex );
                    }
                    else if ( mRegisterAllServices )
                    {
                        mRegisterAllServices = false;
#ifdef ENABLE_SLP_LOGGING
                        SLP_LOG( SLP_LOG_MSG, "SLPDARegisterer propigating all services" );
#endif
                        propogate_all_advertisements( (SAState*)mServerState );
                    }
                }
                
                if ( task )
                {
                    RegCookie	regCookie = {SLP_OK,0};
                    
                // handle task
                    if ( task->action == kRegLocalService && task->data )
                    {
                        RegData*		regData = (RegData*)task->data;
                        char			serviceType[255];
                        char*			ignore = NULL;
                        OSStatus		status = noErr;
                        
#ifdef ENABLE_SLP_LOGGING
                        SLP_LOG( SLP_LOG_MSG, "Handling a kRegLocalService task" );
#endif    
                        if ( !mSLPSA )
                        {
                            SLPSetProperty("com.sun.slp.isSA", "true");					// need to let the lib know that we are an SA
                            
                            status = (OSStatus)SLPOpen( "en", SLP_FALSE, &mSLPSA );
                        }
    
                        if ( regData->mURLPtr && regData->mAttributeListPtr )
                        {
                            if ( !IsURL( regData->mURLPtr, strlen(regData->mURLPtr), &ignore ) )
                            {
#ifdef ENABLE_SLP_LOGGING
                                SLP_LOG( SLP_LOG_DEBUG, "Tried to register a bad URL: %s", regData->mURLPtr );
#endif
                            }
                            else
                            {
                                GetServiceTypeFromURL( regData->mURLPtr, strlen(regData->mURLPtr), serviceType );
                                
                                status = (OSStatus)SLPReg( mSLPSA, regData->mURLPtr, CONFIG_INTERVAL_1, serviceType, regData->mAttributeListPtr, SLP_TRUE, SLPHandleRegReport, &regCookie );
                                
                                if (!list_subset(regData->mScopeListPtr,SLPGetProperty("net.slp.useScopes"))) 
                                {
                                    char		newScopeList[2*kMaxSizeOfParam];		// this should be big enough...
                                    
                                    sprintf( newScopeList, "%s,%s", SLPGetProperty("net.slp.useScopes"), regData->mScopeListPtr );
                            
                                    SLPSetProperty("net.slp.useScopes", newScopeList );		// update our scopelist
                                } 
                                
                                propogate_registration( mServerState, "en", serviceType, regData->mURLPtr, regData->mScopeListPtr, regData->mAttributeListPtr, CONFIG_INTERVAL_1 );
                                
                                mRegFileNeedsProcessing = true;
                            }
                        }
#ifdef ENABLE_SLP_LOGGING
                        else
                            SLP_LOG( SLP_LOG_DEBUG, "Tried to register a bad url/attribute combo" );
#endif                            
                        delete regData;
                    }
                    else if ( task->action == kDeregLocalService && task->data )
                    {
                        RegData*			regData = (RegData*)task->data;
                        char			serviceType[255];
                        char*			ignore = NULL;
                        OSStatus	 		status = noErr;
                        
#ifdef ENABLE_SLP_LOGGING
                        SLP_LOG( SLP_LOG_MSG, "Handling a kDeregLocalService task" );
#endif    
                        if ( regData->mURLPtr && regData->mScopeListPtr )
                        {
                            if ( !IsURL( regData->mURLPtr, strlen(regData->mURLPtr), &ignore ) )
                            {
#ifdef ENABLE_SLP_LOGGING
                                SLP_LOG( SLP_LOG_DEBUG, "Tried to deregister a bad URL: %s", regData->mURLPtr );
#endif
                            }
                            else
                            {
                                if ( !mSLPSA )
                                {
                                    SLPSetProperty("com.sun.slp.isSA", "true");					// need to let the lib know that we are an SA
                                    
                                    status = (OSStatus)SLPOpen( "en", SLP_FALSE, &mSLPSA );
                                }
            
                                GetServiceTypeFromURL( regData->mURLPtr, strlen(regData->mURLPtr), serviceType );
                                
                                status = (OSStatus)SLPDereg( mSLPSA, regData->mURLPtr, regData->mScopeListPtr, SLPHandleRegReport, &regCookie );
                                propogate_deregistration( mServerState, "en", serviceType, regData->mURLPtr, regData->mScopeListPtr, regData->mAttributeListPtr, CONFIG_INTERVAL_1 );
                                
                                mRegFileNeedsProcessing = true;				// we need to clear out the deregistered item
                                
                            }
                        }
#ifdef ENABLE_SLP_LOGGING
                        else
                            SLP_LOG( SLP_LOG_DEBUG, "Tried to deregister a bad url/scope combo" );
#endif                        
                        delete regData;
                    }
                    else if ( task->action == kRegisterAllServices )		// we might have multiple of these so we just keep setting this
                        mRegisterAllServices = true;						// and only process when we are done with the other stuff
                    else if ( task->action == kRegisterAllServicesWithNewDA )
                    {
                        DAInfoObj*	daObj = (DAInfoObj*)task->data;
                        
                        propogate_registrations( mServerState, daObj->sinDA, daObj->scopeList );
                        
						if ( daObj->scopeList )
							free( daObj->scopeList );
							
                        delete daObj;
                    }
                        
                    delete(task);	// this is just the struct object, if anything was in the data portion, it should have been freed above
                    task = NULL;
                }
                else
					SmartSleep(60*USEC_PER_SEC);
            }
        }
        
        catch ( int inErr )
        {
            SLP_LOG( SLP_LOG_ERR, "SLPDARegisterer caught an error" );
        }
    }
    
    return NULL;
}

static void store_free(SAStore store) {
  /* free the store, so that we can see where we leak in store reading */
  int i,j,k;
  if (store.size <= 0) return;
  
  for (i = 0; store.url[i] != NULL ; i++) {
    SLPFree(store.url[i]);
    SLPFree(store.srvtype[i]);
    SLPFree(store.attrlist[i]);
    SLPFree(store.lang[i]);
    SLPFree(store.scope[i]);
    if (store.tag[i]) {
      for (j=0;store.tag[i][j];j++) {
	SLPFree(store.tag[i][j]);
	if (store.values[i][j].pval) {
	  if (store.values[i][j].type == TYPE_OPAQUE ||
	      store.values[i][j].type == TYPE_STR)
	    for (k=0;k<store.values[i][j].numvals; k++)
	      SLPFree(store.values[i][j].pval[k].v_pc);
	  SLPFree(store.values[i][j].pval);
	}
      }
      SLPFree(store.values[i]);
      SLPFree(store.tag[i]);
    }
  }
  SLPFree(store.srvtype);
  SLPFree(store.scope);
  SLPFree(store.url);
  SLPFree(store.life);
  SLPFree(store.lang);
  SLPFree(store.tag);
  SLPFree(store.values);
  SLPFree(store.attrlist);
}    
