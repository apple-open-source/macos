/*
 *  SLPDALocator.cpp
 *  NSLPlugins
 *
 *  Created by Kevin Arnold on Thu Oct 05 2000.
 *  Copyright (c) 2000 Apple Computer, Inc. All rights reserved.
 *
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
//#include "mslpd.h"


//#include "slpipc.h"
#include "LThread.h"
#include "SLPDALocator.h"

static SLPDALocator* 	gDALocator = NULL;
static pthread_mutex_t	gLock = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t	gQueuedDALock = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t	gTheSLPDALLock = PTHREAD_MUTEX_INITIALIZER;
//static Boolean			gLockInitialized = false;

void WakeDALocator(CFRunLoopTimerRef timer, void *info);

// This is our C function wrapper to start this threaded object
SLPInternalError StartSLPDALocator( void* daadvert_callback, SLPHandle serverState )
{
	SLPInternalError	status = SLP_OK;
    SLP_LOG( SLP_LOG_DEBUG, "StartSLPDALocator called." );
	
/*    if (!gLockInitialized)
    {
        gLockInitialized = true;
        pthread_mutex_init( &gLock, NULL );
        
        pthread_mutex_init( &gQueuedDALock, NULL );
    }
*/    
    LockGlobalDATable();
    status = SLPDALocator::TheSLPDAL()->Initialize( daadvert_callback, serverState );
    
    if ( !status && !SLPDALocator::TheSLPDAL()->IsRunning() )
    {
        SLP_LOG( SLP_LOG_DEBUG, "SLPDALocator isn't running yet, calling Resume" );
        SLPDALocator::TheSLPDAL()->Start();
    }
    else
    {
        SLP_LOG( SLP_LOG_DEBUG, "SLPDALocator can't call Resume, status is %d, IsRunning returned %d", status, SLPDALocator::TheSLPDAL()->IsRunning() );
    }
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
    SLPDALocator* 	curDAAdvertiser = gDALocator;
    
    gDALocator = NULL;
    
    if ( curDAAdvertiser )
        curDAAdvertiser->Cancel();
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
        SLP_LOG( SLP_LOG_DEBUG, "creating a new DA Locator thread" );
        
        gDALocator = new SLPDALocator();
    }

    ::pthread_mutex_unlock( &gTheSLPDALLock );

    return gDALocator;
}

SLPDALocator::SLPDALocator()
	: LThread(threadOption_Default)
{
    mServerState = NULL;
    mDACallback = NULL;
    mDATable = NULL;
    mIsRunning = false;
    mLookupInProgress = false;
    mTableReset = false;
    mCanceled = false;
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
    
    SLP_LOG( SLP_LOG_DEBUG, "DA Locator has been killed" );
}

void SLPDALocator::KillSLPDALocator( void )
{
	void* volatile		result	= NULL;
	this->DeleteThread(result);
}

SLPInternalError SLPDALocator::Initialize( void* daadvert_callback, SLPHandle serverState )
{
    SLP_LOG( SLP_LOG_DEBUG, "Initialize called with callback: 0x%x, serverState: 0x%x", daadvert_callback, serverState );
    
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
    // mServerState - got all that?
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
		SLP_LOG( SLP_LOG_MSG, "SLPDALocator::Kick, %d DAs in list to remove.",mDATable->iSize );
	
		int i;
		for (i = 0; i < mDATable->iSize; i++) 
		{
		SLPFree(mDATable->pDAE[i].pcScopeList);
		mDATable->pDAE[i].pcScopeList = NULL;
		}
		mDATable->iSize = 0;
		
		UnlockGlobalDATable();
	
		if ( mRunLoopRef && !mLookupInProgress )
			CFRunLoopStop( mRunLoopRef );
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
        sleep(2);
        
        mDALookupHasntHadAChanceToFindADAYet = false;    
    }
    
    return mDATable;
}

void SLPDALocator::Start( void )
{
    mIsRunning = true;
    this->Resume();
}

void* SLPDALocator::Run()
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
    
    //  first we are supposed to wait some random time between 0-3 seconds before starting
    // our initial DA Discovery (SLP 2608 Sec. 12.2.1) but we are going to make it 2
    const char*	pcScopes = "";
    long 		sleepValue = (random()*2)/LONG_MAX;
    SLPInternalError	err = SLP_OK;
    
    mIsRunning = true;
    mRunLoopRef = CFRunLoopGetCurrent();

    CFRunLoopTimerContext 	c = {0, mRunLoopRef, NULL, NULL, NULL};
    
    sleep( sleepValue );
    
    while ( !mCanceled )
    {
        mTableReset = false;
        mLookupInProgress = true;
        
        if ( Initialize() == SLP_OK )
		{
			// do this every time
			SLP_LOG( SLP_LOG_DEBUG,"SLPDALocator starting active DA discovery");
			
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
				if (err != SLP_NETWORK_TIMED_OUT )
				{
					mslplog(SLP_LOG_MSG,"SLPDALocator could not do DA discovery",slperror(err));
				}
				else
				{
					SLP_LOG( SLP_LOG_DEBUG,"SLPDALocator found no DAs");
				}
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
    
        if ( !mTableReset )
        {
            CFRunLoopTimerRef 			timer = CFRunLoopTimerCreate(NULL, CFAbsoluteTimeGetCurrent() + CONFIG_DA_FIND, 0, 0, 0, WakeDALocator, (CFRunLoopTimerContext*)&c);
            CFRunLoopAddTimer(mRunLoopRef, timer, kCFRunLoopDefaultMode);
    
            SLP_LOG( SLP_LOG_DEBUG, "SLPDALocator going to sleep, 0x%x\n", (void*)this );
            CFRunLoopRun();
            SLP_LOG( SLP_LOG_DEBUG, "SLPDALocator waking from sleep, 0x%x\n", (void*)this );
    
            CFRelease( timer );
        }
    }
    
	return NULL;
}

void WakeDALocator(CFRunLoopTimerRef timer, void *info) 
{
    SLP_LOG( SLP_LOG_DEBUG, "WakeDALocator called\n" );
    CFRunLoopStop( (CFRunLoopRef)info );
}

void SLPDALocator::LocateAndAddDA( long addrOfDA )
{
    if ( !mServerState )
    {
        SLP_LOG( SLP_LOG_DEBUG, "SLPDALocator::LocateAndAddDA, no mServerState yet, we'll just add this to a queue to be processed later" );
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
		int         			iMTU = strtol(SLPGetProperty("net.slp.MTU"),&endPtr,10);
        int						iSize = strtol(SLPGetProperty("net.slp.MTU"),&endPtr,10);
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
    
        SLP_LOG( SLP_LOG_DEBUG, "SLPDALocator::LocateAndAddDA going to ask DA:%s its info", inet_ntoa(sin.sin_addr) );

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
                SLP_LOG( SLP_LOG_DA, "get_reply could not get_da_results from [%s]...: %s",inet_ntoa(sin.sin_addr), slperror(err) );
                
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
    //                    last_one(err, ALL_DONE,pvUser,(SLPHandle)puas,pvCallback,cbt);	  
                        SLP_LOG(SLP_LOG_DEBUG, "get_reply overflow, tcp failed from [%s] when locating and adding the DA...: %s",inet_ntoa(sin.sin_addr), slperror(err));
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
    int         			iMTU = strtol(SLPGetProperty("net.slp.MTU"),&endPtr,10);
    int						iSize = strtol(SLPGetProperty("net.slp.MTU"),&endPtr,10);
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

    SLP_LOG( SLP_LOG_DEBUG, "SLPDALocator::LocateAndAddDA going to ask DA:%s its info", inet_ntoa(sin.sin_addr) );

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
            SLP_LOG( SLP_LOG_DA, "get_reply could not get_da_results from [%s]...: %s",inet_ntoa(sin.sin_addr), slperror(err) );
            
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
//                    last_one(err, ALL_DONE,pvUser,(SLPHandle)puas,pvCallback,cbt);	  
                    SLP_LOG(SLP_LOG_DEBUG, "get_reply overflow, tcp failed from [%s]...: %s",inet_ntoa(sin.sin_addr), slperror(err));
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


