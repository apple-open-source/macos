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
 *  @header SLPDALocator
 */

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <limits.h>

#include "mslp_sd.h"
#include "slp.h"
#include "mslp.h"
#include "mslp_dat.h"    /* Definitions for mslp_dat             */
#include "mslplib.h"     /* Definitions specific to the mslplib  */
#include "mslplib_opt.h" /* Definitions for optional msg support */
#include "mslpd_store.h"
#include "mslp_dat.h"

#include "SLPDALocator.h"
#include "CNSLTimingUtils.h"

static SLPDALocator* 	gDALocator = NULL;
static pthread_mutex_t	gLock = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t	gQueuedDALock = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t	gTheSLPDALLock = PTHREAD_MUTEX_INITIALIZER;

#include <syslog.h>
void StartDALocator(CFRunLoopTimerRef timer, void *info);
void* RunWrapper ( void *arg );

// This is our C function wrapper to start this threaded object
SLPInternalError StartSLPDALocator( void* daadvert_callback, CFRunLoopRef runLoop, SLPHandle serverState )
{
	SLPInternalError	status = SLP_OK;
#ifdef ENABLE_SLP_LOGGING
    SLP_LOG( SLP_LOG_DEBUG, "StartSLPDALocator called." );
#endif
    LockGlobalDATable();
    status = SLPDALocator::TheSLPDAL()->Initialize( daadvert_callback, serverState );

	if ( runLoop )
		SLPDALocator::TheSLPDAL()->SetRunLoop( runLoop );
	
    if ( !status && !SLPDALocator::TheSLPDAL()->IsRunning() )
    {
#ifdef ENABLE_SLP_LOGGING
        SLP_LOG( SLP_LOG_DEBUG, "SLPDALocator isn't running yet, calling Start" );
#endif
        SLPDALocator::TheSLPDAL()->Start();
    }
#ifdef ENABLE_SLP_LOGGING
    else
    {
        SLP_LOG( SLP_LOG_DEBUG, "SLPDALocator can't call Resume, status is %d, IsRunning returned %d", status, SLPDALocator::TheSLPDAL()->IsRunning() );
    }
#endif
    UnlockGlobalDATable();
    
    return status;
}

void KickSLPDALocator( void )
{
    if ( gDALocator )
        gDALocator->Kick();
}

void StopSLPDALocator( void )
{
    ::pthread_mutex_lock( &gTheSLPDALLock );
    SLPDALocator* 	curDAAdvertiser = gDALocator;
    
    gDALocator = NULL;
    
    if ( curDAAdvertiser && curDAAdvertiser->IsLookupInProgress() )
	{
		curDAAdvertiser->DeleteSelfWhenFinished();
	}
	else if ( curDAAdvertiser )
	{
		delete( curDAAdvertiser );
	}
    ::pthread_mutex_unlock( &gTheSLPDALLock );
}

int GlobalDATableCreationCompleted( void )
{
    if ( gDALocator && gDALocator->FinishedFirstLookup() )
        return true;
    else
        return false;
}

DATable* GetGlobalDATable( void )
{
    DATable*	globalTable = NULL;
    
    if ( gDALocator )
        globalTable = gDALocator->GetDATable();
        
    return globalTable;
}

DATable* GetGlobalDATableForRequester( void )
{
    DATable*	globalTable = NULL;
    
    if ( gDALocator )
        globalTable = gDALocator->GetDATableForRequester();
        
    return globalTable;
}

void LocateAndAddDA( long addrOfDA )
{
    SLPDALocator::TheSLPDAL()->LocateAndAddDA( addrOfDA );
}

void LockGlobalDATable( void )
{
    ::pthread_mutex_lock( &gLock );
}

void UnlockGlobalDATable( void )
{
    ::pthread_mutex_unlock( &gLock );
}

SLPDALocator* SLPDALocator::TheSLPDAL( void )
{
    ::pthread_mutex_lock( &gTheSLPDALLock );
    
    if ( !gDALocator )
    {
#ifdef ENABLE_SLP_LOGGING
        SLP_LOG( SLP_LOG_DEBUG, "Setting up a new DA Locator" );
#endif        
        gDALocator = new SLPDALocator();
    }

    ::pthread_mutex_unlock( &gTheSLPDALLock );

    return gDALocator;
}

SLPDALocator::SLPDALocator()
{
    mServerState = NULL;
    mDACallback = NULL;
    mDATable = NULL;
    mIsRunning = false;
    mLookupInProgress = false;
    mTableReset = false;
    mDeleteSelfWhenFinished = false;
    mRunLoopRef = NULL;
    mDATableInitialized = false;
    mInitialDALookupStillPending = true;
    mDALookupHasntHadAChanceToFindADAYet = true;
    
    mQueuedDAsToLookup = NULL;
    mNumQueuedDAsToLookup = 0;
    mSocket = 0;
	mSelfPtr = this;
}

SLPDALocator::~SLPDALocator()
{
	if ( mDATable )
        dat_delete( mDATable );
        
    if ( mServerState )
        SLPClose( mServerState );
    
    CLOSESOCKET(mSocket);
    mSocket = 0;

	if ( mTimer )
	{
		if ( CFRunLoopTimerIsValid( mTimer ) )
			CFRunLoopTimerInvalidate( mTimer );
		CFRelease( mTimer );
	}
	mTimer = NULL;
    
#ifdef ENABLE_SLP_LOGGING
    SLP_LOG( SLP_LOG_DEBUG, "DA Locator has been killed" );
#endif
}

SLPInternalError SLPDALocator::Initialize( void* daadvert_callback, SLPHandle serverState )
{
#ifdef ENABLE_SLP_LOGGING
    SLP_LOG( SLP_LOG_DEBUG, "Initialize called with callback: 0x%x, serverState: 0x%x", daadvert_callback, serverState );
#endif    
    if ( !mServerState )
        mServerState = serverState;
    
    if ( !mDACallback )
        mDACallback = daadvert_callback;
   
	return SLP_OK;
}

SLPInternalError SLPDALocator::Initialize( void )
{
    SLPInternalError		err = SLP_OK;

    // ALERT, we must initialize our DATable first before calling SLPOpen!
    // this is because SLPOpen will then call us to get the DATable to fill into
    // mServerState
	if ( !mDATableInitialized )
    {
        mDATable = dat_init();
        mDATableInitialized = true;
    }
    
  	if ( !mServerState )
        SLPOpen( "en", SLP_FALSE, &mServerState );		// should we just do this at the start of each search  and  return any errors?

    if ( !mSocket )
    {
        mSocket = socket(AF_INET, SOCK_DGRAM, 0);
    
        if (mSocket < 0 || mSocket == SOCKET_ERROR)
        {
            mSocket = 0;
			LOG_SLP_ERROR_AND_RETURN(SLP_LOG_ERR,"SLPDALocator: socket",SLP_NETWORK_INIT_FAILED);
            
            err = SLP_NETWORK_INIT_FAILED;
        }
        else if ( !OnlyUsePreConfiguredDAs() )
        {
            memset(&mSockAddr_in,0,sizeof mSockAddr_in);
            mSockAddr_in.sin_family = AF_INET;
            mSockAddr_in.sin_port   = htons(SLP_PORT);
            
            mSockAddr_in.sin_addr.s_addr = SLP_MCAST;
            
            if ((err = set_multicast_sender_interf(mSocket)) != SLP_OK) 
            {
                CLOSESOCKET(mSocket);
				mSocket = 0;
                LOG_SLP_ERROR_AND_RETURN(SLP_LOG_DEBUG,"SLPDALocator: set_multicast_sender_interf",err);
            }
        }
    }
    
    return err;
}

void SLPDALocator::Kick( void )
{
    if ( mDATable )		// if this isn't intialized we don't want to do anything
	{
		LockGlobalDATable();
		mTableReset = true;
		mInitialDALookupStillPending = true;
		mDALookupHasntHadAChanceToFindADAYet = true;
#ifdef ENABLE_SLP_LOGGING
		SLP_LOG( SLP_LOG_MSG, "SLPDALocator::Kick, %d DAs in list to remove.",mDATable->iSize );
#endif	
		int i;
		for (i = 0; i < mDATable->iSize; i++) 
		{
			SLPFree(mDATable->pDAE[i].pcScopeList);
			mDATable->pDAE[i].pcScopeList = NULL;
		}
		mDATable->iSize = 0;
		
		UnlockGlobalDATable();

		CFRunLoopTimerSetNextFireDate( mTimer, CFAbsoluteTimeGetCurrent()+0 );

	}
}

DATable* SLPDALocator::GetDATable( void )
{
    return mDATable;
}

DATable* SLPDALocator::GetDATableForRequester( void )
{
    while ( mDALookupHasntHadAChanceToFindADAYet )
    {
        SmartSleep(2*USEC_PER_SEC);
        
        mDALookupHasntHadAChanceToFindADAYet = false;    
    }
    
    return mDATable;
}

void SLPDALocator::SetRunLoop( CFRunLoopRef runLoop )
{
	mRunLoopRef = runLoop;
}

void SLPDALocator::Start( void )
{
    if ( mRunLoopRef )		// If we haven't been given a runloop ref yet, just ignore this call to start us.
	{
		mIsRunning = true;
	
		CFRunLoopTimerContext 		c = {0, this, NULL, NULL, NULL};
	
		mTimer = CFRunLoopTimerCreate(NULL, CFAbsoluteTimeGetCurrent() + CONFIG_DA_INITIAL_DELAY, CONFIG_DA_FIND, 0, 0, StartDALocator, (CFRunLoopTimerContext*)&c);
		CFRunLoopAddTimer(mRunLoopRef, mTimer, kCFRunLoopDefaultMode);
	}
}


//void* SLPDALocator::Run()
void SLPDALocator::DoLookup( void )
{
    /*
    *  discover DAs actively
    *  each time one is found, local registration is immediately forward
    *
    *  NOTE:  This is very simple-minded.  If there are many DAs or any of
    *  them are slow - the forwarding will take too long and the active
    *  discovery will time out before all DAs are found.  A better way to
    *  do this would be to simply go through all DAs after this round and
    *  forward to them sequentially.
    */
    
#ifdef TRACK_FUNCTION_TIMES
	CFAbsoluteTime	startTime = CFAbsoluteTimeGetCurrent();
	syslog( LOG_ERR, "(%s) SLPDALocator::DoLookup started\n", "SLP" );
#endif
    //  first we are supposed to wait some random time between 0-3 seconds before starting
    // our initial DA Discovery (SLP 2608 Sec. 12.2.1) but we are going to make it 2
    const char*	pcScopes = "";
    long 		sleepValue = (random()*2)/LONG_MAX;
    SLPInternalError	err = SLP_OK;
    
	CFRunLoopTimerSetNextFireDate( mTimer, CFAbsoluteTimeGetCurrent()+CONFIG_DA_FIND );

    mIsRunning = true;
	if ( !mRunLoopRef )
		mRunLoopRef = CFRunLoopGetCurrent();
    
    sleep( sleepValue*USEC_PER_SEC );
    
	mTableReset = false;
	
	if ( Initialize() == SLP_OK )
	{
		// do this every time
#ifdef ENABLE_SLP_LOGGING
		SLP_LOG( SLP_LOG_DEBUG,"SLPDALocator starting active DA discovery");
#endif		
		::pthread_mutex_lock( &gQueuedDALock );
		if ( mNumQueuedDAsToLookup && !mTableReset )		// need to clear these up
		{
			for ( int i=0; i< mNumQueuedDAsToLookup && !mTableReset; i++ )
			{
				LocateAndAddDA(mQueuedDAsToLookup[i]);
			}
			
			mNumQueuedDAsToLookup = 0;
			free( mQueuedDAsToLookup );
			mQueuedDAsToLookup = NULL;
		}
		::pthread_mutex_unlock( &gQueuedDALock );
		
		char*	endPtr = NULL;

		if ( !OnlyUsePreConfiguredDAs() && (( err = active_da_discovery(	(SLPHandle)mServerState,
											DADISCMSEC,
											mSocket, 
											strtol(SLPGetProperty("net.slp.MTU"),&endPtr,10),
											mSockAddr_in, 
											pcScopes,
											(void *)mDATable,
											(void *)mDACallback,
											SLPDAADVERT_CALLBACK) ) < 0) )
		{
#ifdef ENABLE_SLP_LOGGING
			if (err != SLP_NETWORK_TIMED_OUT )
			{
				mslplog(SLP_LOG_MSG,"SLPDALocator could not do DA discovery",slperror(err));
			}
			else
			{
				SLP_LOG( SLP_LOG_DEBUG,"SLPDALocator found no DAs");
			}
#endif
		}
		
		CLOSESOCKET(mSocket);
		mSocket = 0;
	}
	else if ( mServerState )
	{
		SLPClose( mServerState );
		mServerState = NULL;
	}
	// now we sleep
	
	mInitialDALookupStillPending = false;
	mDALookupHasntHadAChanceToFindADAYet = false;
	mLookupInProgress = false;
	
	if ( mDeleteSelfWhenFinished )	// we were canceled but in the midst of processing
		delete this;

#ifdef TRACK_FUNCTION_TIMES
	syslog( LOG_ERR, "(%s) SLPDALocator::DoLookup took %f seconds\n", "SLP", CFAbsoluteTimeGetCurrent() - startTime );
#endif
}

static bool				gThreadAttrsSet = false;
static pthread_attr_t	_DefaultAttrs;
static pthread_key_t	_NameKey;
static pthread_key_t	_ObjectKey;

void StartDALocator(CFRunLoopTimerRef timer, void *info) 
{
#ifdef TRACK_FUNCTION_TIMES
	CFAbsoluteTime	startTime = CFAbsoluteTimeGetCurrent();
#endif
    ::pthread_mutex_lock( &gTheSLPDALLock );
    SLPDALocator*		daLocator = (SLPDALocator*)info;
	
#ifdef ENABLE_SLP_LOGGING
	SLP_LOG( SLP_LOG_DEBUG, "StartDALocator timer function called\n" );
#endif	
	if ( daLocator )
		daLocator->LookupInProgress();
		
    ::pthread_mutex_unlock( &gTheSLPDALLock );

	if ( !gThreadAttrsSet )
	{
		gThreadAttrsSet = true;
		::pthread_attr_init( &_DefaultAttrs );
		::pthread_key_create(&_NameKey, NULL);
		::pthread_key_create(&_ObjectKey, NULL);
		::pthread_attr_setdetachstate( &_DefaultAttrs, PTHREAD_CREATE_DETACHED);
	}
	
	// Currently detaching so threads don't stick around.
	pthread_t	tNew = NULL;

    ::pthread_create( &tNew, &_DefaultAttrs, RunWrapper, (void *)daLocator );

#ifdef TRACK_FUNCTION_TIMES
	syslog( LOG_ERR, "(%s) StartDALocator took %f seconds\n", "SLP", CFAbsoluteTimeGetCurrent() - startTime );
#endif
}

void* RunWrapper ( void *arg )
{
	SLPDALocator	*oMe = (SLPDALocator *)arg;

	::pthread_setspecific( _ObjectKey, oMe );
	::pthread_setspecific( _NameKey, "SLPDALocator" );

	// Execute the the lookup on the thread.
	oMe->DoLookup();

	return NULL;

} // _RunWrapper

void SLPDALocator::LocateAndAddDA( long addrOfDA )
{
    if ( !mServerState )
    {
#ifdef ENABLE_SLP_LOGGING
        SLP_LOG( SLP_LOG_DEBUG, "SLPDALocator::LocateAndAddDA, no mServerState yet, we'll just add this to a queue to be processed later" );
#endif
        ::pthread_mutex_lock( &gQueuedDALock );
        // we haven't been initialized yet, just add these to a queue to be read at that time
        long			newQueueLength = mNumQueuedDAsToLookup*sizeof(long) + sizeof(long);
        long*			newQueue = (long*)malloc( newQueueLength );
        
        if ( mQueuedDAsToLookup )
        {
            memcpy( newQueue, mQueuedDAsToLookup, mNumQueuedDAsToLookup*sizeof(long) );
            free( mQueuedDAsToLookup );
        }
            
        newQueue[mNumQueuedDAsToLookup++] = addrOfDA;
        mQueuedDAsToLookup = newQueue;

        ::pthread_mutex_unlock( &gQueuedDALock );
    }
    else
    {
        char*					endPtr = NULL;
		int         			iMTU = (SLPGetProperty("net.slp.MTU"))?strtol(SLPGetProperty("net.slp.MTU"),&endPtr,10):1400;
        int						iSize = iMTU;
        char					*pcRecvBuf = safe_malloc(RECVMTU,0,0);
        char					*pcSendBuf = safe_malloc(iMTU,0,0);
        int						len    = 0;   /* This records the reply length. */
        int						iLast  = 0;   /* used to for ending async callbacks */    
        struct sockaddr_in		sin;
        SLPInternalError				err;
        
        memset(&sin, 0, sizeof(struct sockaddr_in));
        sin.sin_port = htons(SLP_PORT);
        sin.sin_family = AF_INET;
    
        // add daOption->daList[i] to our list of DAs
        sin.sin_addr.s_addr = addrOfDA;
    
#ifdef ENABLE_SLP_LOGGING
        SLP_LOG( SLP_LOG_DEBUG, "SLPDALocator::LocateAndAddDA going to ask DA:%s its info", inet_ntoa(sin.sin_addr) );
#endif
        if (!(err = generate_srvrqst(pcSendBuf,&iMTU,"en","", "service:directory-agent",""))) 
        {
            if ((err = get_unicast_result(
                                            MAX_UNICAST_WAIT,
                                            mSocket, 
                                            pcSendBuf, 
                                            iSize, 
                                            pcRecvBuf,
                                            RECVMTU, 
                                            &len, 
                                            sin)) != SLP_OK) 
            {
#ifdef ENABLE_SLP_LOGGING
                SLP_LOG( SLP_LOG_DA, "get_reply could not get_da_results from [%s]...: %s",inet_ntoa(sin.sin_addr), slperror(err) );
#endif                
                SLPFree(pcRecvBuf);
                pcRecvBuf = NULL;
            }
            else
            {
                if (GETFLAGS(pcRecvBuf) & OVERFLOWFLAG) 
                { /* the result overflowed ! */
                    SLPFree(pcRecvBuf);
                    pcRecvBuf = NULL;   
                    
                    // set the port to use the SLP port
                    sin.sin_port   = htons(SLP_PORT);
                    if ((err=get_tcp_result(pcSendBuf,iSize, sin, &pcRecvBuf,&len)) != SLP_OK) 
                    {
                        SLPFree(pcRecvBuf);
                        pcRecvBuf = NULL;

#ifdef ENABLE_SLP_LOGGING
                        SLP_LOG(SLP_LOG_DEBUG, "get_reply overflow, tcp failed from [%s] when locating and adding the DA...: %s",inet_ntoa(sin.sin_addr), slperror(err));
#endif
                    }
                }
            }
            /* evokes the callback once */
            if ( !err )
                err = process_reply(pcSendBuf, pcRecvBuf, len, &iLast, (void *)mDATable, (SLPHandle)mServerState, (void *)mDACallback, SLPSRVURL_CALLBACK);
        }
        
        if ( pcRecvBuf )
            SLPFree(pcRecvBuf);
            
        if ( pcSendBuf )
            SLPFree(pcSendBuf);
    }
}

void SLPDALocator::AskDAForScopeSponserInfo( long addrOfDA )
{
	char*					endPtr = NULL;
    int         			iMTU = (SLPGetProperty("net.slp.MTU"))?strtol(SLPGetProperty("net.slp.MTU"),&endPtr,10):1400;
    int						iSize = iMTU;
    char					*pcRecvBuf = safe_malloc(RECVMTU,0,0);
    char					*pcSendBuf = safe_malloc(iMTU,0,0);
    int						len    = 0;   /* This records the reply length. */
    int						iLast  = 0;   /* used to for ending async callbacks */    
    struct sockaddr_in		sin;
    SLPInternalError				err;
    
    memset(&sin, 0, sizeof(struct sockaddr_in));
    sin.sin_port = htons(SLP_PORT);
    sin.sin_family = AF_INET;

    // add daOption->daList[i] to our list of DAs
    sin.sin_addr.s_addr = addrOfDA;

#ifdef ENABLE_SLP_LOGGING
    SLP_LOG( SLP_LOG_DEBUG, "SLPDALocator::LocateAndAddDA going to ask DA:%s its info", inet_ntoa(sin.sin_addr) );
#endif
    if (!(err = generate_srvrqst(pcSendBuf,&iMTU,"en","", "service:com.apple.slp.defaultRegistrationScope",""))) 
    {
        if ((err = get_unicast_result(
                                        MAX_UNICAST_WAIT,
                                        mSocket, 
                                        pcSendBuf, 
                                        iSize, 
                                        pcRecvBuf,
                                        RECVMTU, 
                                        &len, 
                                        sin)) != SLP_OK) 
        {
#ifdef ENABLE_SLP_LOGGING
            SLP_LOG( SLP_LOG_DA, "get_reply could not get_da_results from [%s]...: %s",inet_ntoa(sin.sin_addr), slperror(err) );
#endif            
            SLPFree(pcRecvBuf);
            pcRecvBuf = NULL;
        }
        else
        {
            if (GETFLAGS(pcRecvBuf) & OVERFLOWFLAG) 
            { /* the result overflowed ! */
                SLPFree(pcRecvBuf);
                pcRecvBuf = NULL;   
                
                // set the port to use the SLP port
                sin.sin_port   = htons(SLP_PORT);
                if ((err=get_tcp_result(pcSendBuf,iSize, sin, &pcRecvBuf,&len)) != SLP_OK) 
                {
                    SLPFree(pcRecvBuf);
                    pcRecvBuf = NULL;

#ifdef ENABLE_SLP_LOGGING
                    SLP_LOG(SLP_LOG_DEBUG, "get_reply overflow, tcp failed from [%s]...: %s",inet_ntoa(sin.sin_addr), slperror(err));
#endif
                }
            }
        }
        /* evokes the callback once */
        if ( !err )
            err = process_reply(pcSendBuf, pcRecvBuf, len, &iLast, (void *)mDATable, (SLPHandle)mServerState, (void *)mDACallback, SLPDAADVERT_CALLBACK);
    }
    
    if ( pcRecvBuf )
        SLPFree(pcRecvBuf);
        
    if ( pcSendBuf )
        SLPFree(pcSendBuf);
}
