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
 *  @header SLPUDPListener
 *  A thread that will actively listen for communications via UDP for SLP requests
 */
 
#include <stdio.h>
#include <string.h>
#include <sys/un.h>
#include <syslog.h>

#include <DirectoryService/DirServicesTypes.h>

#include "mslp_sd.h"
#include "slp.h"
#include "mslp.h"
#include "mslpd_store.h"
#include "mslp_dat.h"
#include "mslpd.h"

#include "SLPComm.h"
#include "slpipc.h"
#include "SLPRegistrar.h"
#include "SLPUDPListener.h"
#include "CNSLTimingUtils.h"

static SLPUDPListener*	gUDPL = NULL;
static int				gUDPLRunning = 0;

int InitializeUDPListener( SAState* psa )
{
	OSStatus	status = SLP_OK;
	
	if ( !gUDPL )
    {
#ifdef ENABLE_SLP_LOGGING
        SLP_LOG( SLP_LOG_DEBUG, "creating a new UDP listener" );
#endif
        gUDPL = new SLPUDPListener( psa, &status );
        
        if ( !gUDPL )
            status = eMemoryAllocError;
    }
    
	return status;
}

// This is our C function wrapper to start this threaded object from the main mslp code
int StartSLPUDPListener( SAState* psa )
{
	OSStatus	status = SLP_OK;
	
	if ( !gUDPL )
        status = InitializeUDPListener( psa );
            
    if ( !status && !gUDPLRunning )
        gUDPL->Resume();
    
	return status;
}

void CancelSLPUDPListener( void )
{
    if ( gUDPL )
    {
        gUDPL->Cancel();
        gUDPL = NULL;
    }
}	

SLPUDPListener::SLPUDPListener( SAState* psa, OSStatus *status )
	: DSLThread()
{
	mServerState = psa;
    mCanceled = false;
	mSelfPtr = this;
	mNumBadDescriptors = 0;
}

SLPUDPListener::~SLPUDPListener()
{
	mSelfPtr = NULL;
#ifdef ENABLE_SLP_LOGGING
    SLP_LOG( SLP_LOG_DEBUG, "UDP listener has been killed" );
#endif
}

void SLPUDPListener::Cancel( void )
{
    mCanceled = true;
}

void* SLPUDPListener::Run()
{
    struct sockaddr_in	sinIn;
    int					err = 0;
	socklen_t			iSinInSz = sizeof(sinIn);    
	char* 				pcInBuf  = safe_malloc( RECVMTU, 0, 0 );

    assert( pcInBuf );
    gUDPLRunning = 1;
    
    /* block on receive */
    SLPUDPHandler* handler = new SLPUDPHandler( mServerState );
    if ( handler )
        handler->Resume();		// go off and handle this request

#ifdef ENABLE_SLP_LOGGING
    SLP_LOG( SLP_LOG_DEBUG, "UDP listener is running" );
#endif
    while (!mCanceled)
    {
// handle the connection from outside this function
        bzero( (char*)&sinIn, sizeof(sinIn) );
    
#ifdef ENABLE_SLP_LOGGING
        SLP_LOG( SLP_LOG_DEBUG, "SLPUDPListener:  calling recvfrom");
#endif
        if ( ( err = recvfrom( mServerState->sdUDP, pcInBuf, RECVMTU, 0, (struct sockaddr*)&sinIn, &iSinInSz ) ) < 0 )  
        {
            if ( mCanceled )
            {
                break;
            }
#ifdef ENABLE_SLP_LOGGING
            else if ( errno == EINTR )		// other wise just ignore and fall out
                SLP_LOG( SLP_LOG_DROP, "SLPUDPListener: recvfrom received EINTR");
#endif
            else if ( errno == EBADF )
			{
				mNumBadDescriptors++;
				
				if ( mNumBadDescriptors > kMaxNumFailures )
				{
					syslog( LOG_ERR, "slpd exiting due to an exorbitant amount of bad descriptors\n" );
					exit(0);
				}
				
				unsigned char ttl;
				char*	endPtr = NULL;
				u_char	loop = 1;	// enable
				
				mServerState->sdUDP = socket(AF_INET, SOCK_DGRAM, 0);		// bad file descriptor, try getting a new one

				ttl = (SLPGetProperty("net.slp.multicastTTL"))?(unsigned char) strtol(SLPGetProperty("net.slp.multicastTTL"),&endPtr,10):1400;
				err = setsockopt(mServerState->sdUDP, IPPROTO_IP, IP_MULTICAST_TTL, (char*)&ttl, sizeof(ttl));
				
				if (err < 0)
				{
#ifdef ENABLE_SLP_LOGGING
					mslplog(SLP_LOG_DEBUG,"SLPUDPListener: Could not set multicast TTL, %s",strerror(errno));
#endif
				}
				else
				{
					err = setsockopt( mServerState->sdUDP, IPPROTO_IP, IP_MULTICAST_LOOP, &loop, sizeof(loop) );

#ifdef ENABLE_SLP_LOGGING
					if (err < 0)
						mslplog(SLP_LOG_DEBUG,"SLPUDPListener: Could not set setsockopt, %s",strerror(errno));
#endif
				}
			}
#ifdef ENABLE_SLP_LOGGING
			else
            {
                SLP_LOG( SLP_LOG_DROP, "SLPUDPListener recvfrom: %s", strerror(errno) );
            }
#endif
        } 
        else if ( !mCanceled )
        {
            if ( err >= MINHDRLEN ) 
            {
#ifdef ENABLE_SLP_LOGGING
                SLP_LOG( SLP_LOG_MSG, "SLPUDPListener recvfrom, received %d bytes from: %s (%s)", err, inet_ntoa(sinIn.sin_addr), get_fun_str(GETFUN(pcInBuf)));
#endif                
                handler->AddUDPMessageToQueue( mServerState, pcInBuf, err, sinIn );
            }
#ifdef ENABLE_SLP_LOGGING
            else
            {
                SLP_LOG( SLP_LOG_DROP, "SLPUDPListener recvfrom, received %d bytes from: %s, ignoring as header is too small", err, inet_ntoa(sinIn.sin_addr) );
            }
#endif
        }
	}

    gUDPLRunning = 0;

	return NULL;
}

pthread_mutex_t	SLPUDPHandler::mQueueLock;

CFStringRef SLPUDPHandlerCopyDesctriptionCallback ( const void *item )
{
    return kSLPRAdminNotificationSAFE_CFSTR;
}

Boolean SLPUDPHandlerEqualCallback ( const void *item1, const void *item2 )
{
    return item1 == item2;
}

SLPUDPHandler::SLPUDPHandler(SAState* psa)
	: DSLThread()
{
	CFArrayCallBacks	callBack;
    
    callBack.version = 0;
    callBack.retain = NULL;
    callBack.release = NULL;
    callBack.copyDescription = SLPUDPHandlerCopyDesctriptionCallback;
    callBack.equal = SLPUDPHandlerEqualCallback;

    mCanceled = false;
    mServerState = psa;
    mUDPQueue = ::CFArrayCreateMutable ( NULL, 0, &callBack );
    
    pthread_mutex_init( &mQueueLock, NULL );
}

SLPUDPHandler::~SLPUDPHandler()
{
    if ( mUDPQueue )
        CFRelease( mUDPQueue );
        
    mUDPQueue = NULL;
}

void SLPUDPHandler::Cancel( void )
{
    mCanceled = true;
}

// delayed action
#define		kQueueAlertThreshold 250
void SLPUDPHandler::AddUDPMessageToQueue( SAState *psa, char* pcInBuf, int bufSize, struct sockaddr_in sinIn )
{
    UDPMessageObject*		udpMessage = new UDPMessageObject( psa, pcInBuf, bufSize, sinIn );
    
    QueueLock();
    if ( !mCanceled && mUDPQueue && udpMessage )
    {
#ifdef ENABLE_SLP_LOGGING
        long	queueCount = CFArrayGetCount(mUDPQueue);
        
        if ( queueCount < kQueueAlertThreshold )
            SLP_LOG( SLP_LOG_DEBUG, "AddUDPMessageToQueue, adding element #%d", queueCount );
        else
            SLP_LOG( SLP_LOG_DEBUG, "AddUDPMessageToQueue, adding element #%d to a Queue that is exceeding large!  (Requests may not be handled in a timely fashion)", queueCount );
#endif        
        ::CFArrayAppendValue( mUDPQueue, udpMessage );
    }
    QueueUnlock();
}

#define	kMinTimeToWaitToCheckForNewData		2
#define	kTimeToBump							4
#define	kMaxTimeBetweenNaps					10		// most requests timeout after 15 seconds so make sure we can process them in time
void* SLPUDPHandler::Run( void )
{
    UDPMessageObject* 	udpMessage = NULL;
	unsigned int		sleepTime = kMinTimeToWaitToCheckForNewData;
	
    while ( !mCanceled )
    {
        // grab next element off the queue and process
        QueueLock();
        if ( mUDPQueue && ::CFArrayGetCount( mUDPQueue ) > 0 )
        {
            sleepTime = kMinTimeToWaitToCheckForNewData;		// reset this
			udpMessage = (UDPMessageObject*)::CFArrayGetValueAtIndex( mUDPQueue, 0 );		// grab the first one
            ::CFArrayRemoveValueAtIndex( mUDPQueue, 0 );
            QueueUnlock();
        }
        else
        {
            QueueUnlock();
            SLPRegistrar::TheSLPR()->DoTimeCheckOnTTLs();		// not sure where a better place to do this would be... 

            DoPeriodicTasks();
            
            SmartSleep(sleepTime*USEC_PER_SEC);			// wait a sec for more data
			
			if ( sleepTime + kTimeToBump <= kMaxTimeBetweenNaps )
				sleepTime += kTimeToBump;
        }
        
        if ( udpMessage )
        {
            HandleMessage( udpMessage );
            delete udpMessage;
            udpMessage = NULL;
        }
    }
    
    return NULL;
}

static time_t lastprop = 0;

void SLPUDPHandler::DoPeriodicTasks(void)
{
    static long	modifiedRefreshInterval = SLPGetRefreshInterval() - kSecsToReregisterBeforeExpiration;			// we want this to be sometime BEFORE the server is going to expire our stuff!
    
    if (lastprop == 0)
    {
#ifdef ENABLE_SLP_LOGGING
        SLP_LOG( SLP_LOG_MSG, "DoPeriodicTasks first time, noting current time" );
#endif
        lastprop = time(NULL);
    }
    else if ( modifiedRefreshInterval + lastprop < time(NULL))
    {
        lastprop = time(NULL);
#ifdef ENABLE_SLP_LOGGING
        SLP_LOG( SLP_LOG_MSG, "DoPeriodicTasks, time to reregister our services" );
#endif
        RegisterAllServicesWithKnownDAs(mServerState);
    }
}

void SLPUDPHandler::HandleMessage( UDPMessageObject* udpMessage )
{
    if ( udpMessage )
	{
		int err = handle_udp(udpMessage->mServerState, udpMessage->mInBuf, udpMessage->mBufSize, udpMessage->mSinIn );
		
#ifdef ENABLE_SLP_LOGGING
		if ( err )
			SLP_LOG( SLP_LOG_MSG, "SLPUDPHandler received error from handle_udp: %d", err );
#endif
	}
}

#pragma mark -
UDPMessageObject::UDPMessageObject( SAState *psa, char* pcInBuf, int bufSize, struct sockaddr_in sinIn )
{
    mServerState = psa;
    mBufSize = bufSize;
    mInBuf = (char*)malloc(mBufSize);
    memcpy( mInBuf, pcInBuf, mBufSize );
    mSinIn = sinIn;
}

UDPMessageObject::~UDPMessageObject()
{
    if ( mInBuf )
        free( mInBuf );
}
