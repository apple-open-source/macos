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
 *  @header SLPTCPListener
 *  A thread that will actively listen for communications via TCP for SLP requests
 */

#include <stdio.h>
#include <string.h>
#include <sys/un.h>

#include <DirectoryService/DirServicesTypes.h>

#include "mslp_sd.h"
#include "slp.h"
#include "mslp.h"
#include "mslpd_store.h"
#include "mslp_dat.h"
#include "mslpd.h"

#include "slpipc.h"
#include "SLPTCPListener.h"
#include "CNSLTimingUtils.h"

static SLPTCPListener* gTCPL = NULL;
static int				gTCPLRunning = 0;

int InitializeTCPListener( SAState* psa )
{
	OSStatus	status = SLP_OK;
	
	if ( !gTCPL )
    {
#ifdef ENABLE_SLP_LOGGING
        SLP_LOG( SLP_LOG_DEBUG, "creating a new TCP listener" );
#endif
        gTCPL = new SLPTCPListener( psa, &status );
        
        if ( !gTCPL )
            status = eMemoryAllocError;
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
	: DSLThread()
{
	mServerState = psa;

    mCanceled = false;
	mSelfPtr = this;
}

SLPTCPListener::~SLPTCPListener()
{
	mSelfPtr = NULL;
#ifdef ENABLE_SLP_LOGGING
    SLP_LOG( SLP_LOG_DEBUG, "TCP listener has been killed" );
#endif
}

void SLPTCPListener::Cancel( void )
{
    mCanceled = true;
}

void* SLPTCPListener::Run()
{
	socklen_t			iSinInSz = sizeof(struct sockaddr_in);
    struct sockaddr_in	sinIn;
    OSStatus			status;
    
    gTCPLRunning = 1;
    /* block on receive */
#ifdef ENABLE_SLP_LOGGING
    SLP_LOG( SLP_LOG_DEBUG, "TCP listener is running" );
#endif

    while (!mCanceled)
    {
        SOCKET				sdRqst = accept(mServerState->sdTCP,(struct sockaddr*)&sinIn,&iSinInSz);
		
        if ( mCanceled )
        {
            break;
        }
        else if ( sdRqst == SOCKET_ERROR || sdRqst < 0 ) 
        {
#ifdef ENABLE_SLP_LOGGING
            SLP_LOG(SLP_LOG_MSG, "SLPTCPListener accept: %s", strerror(errno));
#endif
            SmartSleep(1*USEC_PER_SEC);
        } 
        else
        {
#ifdef ENABLE_SLP_LOGGING
            SLP_LOG( SLP_LOG_MSG, "SLPTCPListener recvfrom, accepted a connction from: %s", inet_ntoa(sinIn.sin_addr));
#endif            
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
    : DSLThread()
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
