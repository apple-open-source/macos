/*
	File:		SLPTCPListener.cp

	Contains:	A thread that will actively listen for communications via TCP for SLP requests
    
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
#include "SLPTCPListener.h"

static SLPTCPListener* gTCPL = NULL;
static int				gTCPLRunning = 0;

int InitializeTCPListener( SAState* psa )
{
	OSStatus	status = SLP_OK;
	
	if ( !gTCPL )
    {
        SLP_LOG( SLP_LOG_DEBUG, "creating a new TCP listener" );
        gTCPL = new SLPTCPListener( psa, &status );
        
        if ( !gTCPL )
            status = memFullErr;
    }
    
    return status;
}

// This is our C function wrapper to start this threaded object from the main mslp code
int StartSLPTCPListener( SAState* psa )
{
	OSStatus	status = SLP_OK;
	
	if ( !gTCPL )
        InitializeTCPListener( psa );
        
    if ( !status && !gTCPLRunning )
        gTCPL->Resume();
    
	return status;
}

void CancelSLPTCPListener( void )
{
    if ( gTCPL )
    {
        gTCPL->Cancel();
        gTCPL = NULL;
    }
}	

SLPTCPListener::SLPTCPListener( SAState* psa, OSStatus *status )
	: LThread(threadOption_Default)
{
	mServerState = psa;

    mCanceled = false;
	mSelfPtr = this;
}

SLPTCPListener::~SLPTCPListener()
{
	mSelfPtr = NULL;
    SLP_LOG( SLP_LOG_DEBUG, "TCP listener has been killed" );
}

void SLPTCPListener::Cancel( void )
{
    mCanceled = true;
}

void* SLPTCPListener::Run()
{
	int					iSinInSz = sizeof(struct sockaddr_in);
    struct sockaddr_in	sinIn;
    OSStatus			status;
    
    gTCPLRunning = 1;
    /* block on receive */
    SLP_LOG( SLP_LOG_DEBUG, "TCP listener is running" );

    while (!mCanceled)
    {
        SOCKET				sdRqst = accept(mServerState->sdTCP,(struct sockaddr*)&sinIn,&iSinInSz);
		
        if ( mCanceled )
        {
            break;
        }
        else if ( sdRqst == SOCKET_ERROR || sdRqst < 0 ) 
        {
            SLP_LOG(SLP_LOG_MSG, "SLPTCPListener accept: %s", strerror(errno));
            sleep(1);
        } 
        else
        {
            SLP_LOG( SLP_LOG_MSG, "SLPTCPListener recvfrom, accepted a connction from: %s", inet_ntoa(sinIn.sin_addr));
            
            TCPHandlerThread*	newTCPConnection = new TCPHandlerThread(&status);
            
            if ( newTCPConnection )
            {
                newTCPConnection->Initialize( sdRqst, mServerState, sinIn );
                newTCPConnection->Resume();
            }
        }
	}

    gTCPLRunning = 0;
	return NULL;
}

TCPHandlerThread::TCPHandlerThread( OSStatus *status )
    : LThread(threadOption_Default)
{
}

TCPHandlerThread::~TCPHandlerThread()
{
    CLOSESOCKET(mRequestSD);
}

void TCPHandlerThread::Initialize( SOCKET newRequest, SAState* serverState, struct sockaddr_in sinIn )
{
    mRequestSD = newRequest;
    mServerState = serverState;
    mSinIn = sinIn;
}

void* TCPHandlerThread::Run()
{
    handle_tcp( mServerState, mRequestSD, mSinIn );   /* for now ignore return code */
    
    return NULL;
}

