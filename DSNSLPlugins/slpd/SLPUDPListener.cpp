/*
	File:		SLPUDPListener.cp

	Contains:	A thread that will actively listen for communications via UDP for SLP requests
    
    TEMP: Currently we are also handling the TCP communications here too...
    
	Written by:	Kevin Arnold

	Copyright:	© 2000 by Apple Computer, Inc., all rights reserved.

	Change History (most recent first):


*/
#include <stdio.h>
#include <string.h>
#include <sys/un.h>
//#include <Carbon/Carbon.h>

#include "mslp_sd.h"
#include "slp.h"
#include "mslp.h"
#include "mslpd_store.h"
#include "mslp_dat.h"
#include "mslpd.h"
//#include "SLPDefines.h"

#include "slpipc.h"
//#include "URLUtilities.h"
#include "SLPRegistrar.h"
#include "SLPUDPListener.h"

static SLPUDPListener*	gUDPL = NULL;
static int				gUDPLRunning = 0;

int InitializeUDPListener( SAState* psa )
{
	OSStatus	status = SLP_OK;
	
	if ( !gUDPL )
    {
        SLP_LOG( SLP_LOG_DEBUG, "creating a new UDP listener" );
        gUDPL = new SLPUDPListener( psa, &status );
        
        if ( !gUDPL )
            status = memFullErr;
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
	: LThread(threadOption_Default)
{
	mServerState = psa;
    mCanceled = false;
	mSelfPtr = this;
}

SLPUDPListener::~SLPUDPListener()
{
	mSelfPtr = NULL;
    SLP_LOG( SLP_LOG_DEBUG, "UDP listener has been killed" );
}

void SLPUDPListener::Cancel( void )
{
    mCanceled = true;
}

void* SLPUDPListener::Run()
{
    struct sockaddr_in	sinIn;
    int					err = 0,iSinInSz = sizeof(sinIn);    
	char* 				pcInBuf  = safe_malloc( RECVMTU, 0, 0 );

    assert( pcInBuf );
    gUDPLRunning = 1;
    
    /* block on receive */
    SLPUDPHandler* handler = new SLPUDPHandler( mServerState );
    if ( handler )
        handler->Resume();		// go off and handle this request

    SLP_LOG( SLP_LOG_DEBUG, "UDP listener is running" );
    while (!mCanceled)
    {
// handle the connection from outside this function
        bzero( (char*)&sinIn, sizeof(sinIn) );
    
        SLP_LOG( SLP_LOG_DEBUG, "SLPUDPListener:  calling recvfrom");
        if ( ( err = recvfrom( mServerState->sdUDP, pcInBuf, RECVMTU, 0, (struct sockaddr*)&sinIn, &iSinInSz ) ) < 0 )  
        {
            if ( mCanceled )
            {
                break;
            }
            else if ( errno == EINTR )		// other wise just ignore and fall out
                SLP_LOG( SLP_LOG_DROP, "mslpd handle_udp recvfrom received EINTR");
            else
            {
                SLP_LOG( SLP_LOG_DROP, "SLPUDPListener recvfrom: %s", strerror(errno) );
            }
        } 
        else if ( !mCanceled )
        {
            if ( err >= MINHDRLEN ) 
            {
                SLP_LOG( SLP_LOG_MSG, "SLPUDPListener recvfrom, received %d bytes from: %s (%s)", err, inet_ntoa(sinIn.sin_addr), get_fun_str(GETFUN(pcInBuf)));
                
                handler->AddUDPMessageToQueue( mServerState, pcInBuf, err, sinIn );
            }
            else
            {
                SLP_LOG( SLP_LOG_DROP, "SLPUDPListener recvfrom, received %d bytes from: %s, ignoring as header is too small", err, inet_ntoa(sinIn.sin_addr) );
            }
        }
	}

    gUDPLRunning = 0;

	return NULL;
}

pthread_mutex_t	SLPUDPHandler::mQueueLock;

CFStringRef SLPUDPHandlerCopyDesctriptionCallback ( const void *item )
{
    return CFSTR("SLP RAdmin Notification");
}

Boolean SLPUDPHandlerEqualCallback ( const void *item1, const void *item2 )
{
    return item1 == item2;
}

SLPUDPHandler::SLPUDPHandler(SAState* psa)
	: LThread(threadOption_Default)
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
        long	queueCount = CFArrayGetCount(mUDPQueue);
        
        if ( queueCount < kQueueAlertThreshold )
            SLP_LOG( SLP_LOG_DEBUG, "AddUDPMessageToQueue, adding element #%d", queueCount );
        else
            SLP_LOG( SLP_LOG_DEBUG, "AddUDPMessageToQueue, adding element #%d to a Queue that is exceeding large!  (Requests may not be handled in a timely fashion)", queueCount );
        
        ::CFArrayAppendValue( mUDPQueue, udpMessage );
    }
    QueueUnlock();
}

void* SLPUDPHandler::Run( void )
{
    UDPMessageObject* udpMessage = NULL;

    while ( !mCanceled )
    {
        // grab next element off the queue and process
        QueueLock();
        if ( mUDPQueue && ::CFArrayGetCount( mUDPQueue ) > 0 )
        {
            udpMessage = (UDPMessageObject*)::CFArrayGetValueAtIndex( mUDPQueue, 0 );		// grab the first one
            ::CFArrayRemoveValueAtIndex( mUDPQueue, 0 );
            QueueUnlock();
        }
        else
        {
            QueueUnlock();
            SLPRegistrar::TheSLPR()->DoTimeCheckOnTTLs();		// not sure where a better place to do this would be... 

            DoPeriodicTasks();
            
            sleep(1);			// wait a sec for more data
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
        SLP_LOG( SLP_LOG_MSG, "DoPeriodicTasks first time, noting current time" );
        lastprop = time(NULL);
    }
    else if ( modifiedRefreshInterval + lastprop < time(NULL))
    {
        lastprop = time(NULL);
        SLP_LOG( SLP_LOG_MSG, "DoPeriodicTasks, time to reregister our services" );
        RegisterAllServicesWithKnownDAs(mServerState);
    }
}

void SLPUDPHandler::HandleMessage( UDPMessageObject* udpMessage )
{
    if ( udpMessage )
	{
		int err = handle_udp(udpMessage->mServerState, udpMessage->mInBuf, udpMessage->mBufSize, udpMessage->mSinIn );
		
		if ( err )
			SLP_LOG( SLP_LOG_MSG, "SLPUDPHandler received error from handle_udp: %d", err );
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
