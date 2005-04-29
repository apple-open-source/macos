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
 *  @header SLPDAAdvertiser
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
#include "SLPDefines.h"

#include "slpipc.h"
#include "SLPDAAdvertiser.h"
#include "CNSLTimingUtils.h"

static SLPDAAdvertiser* gDAAdvertiser = NULL;

// This is our C function wrapper to start this threaded object
int StartSLPDAAdvertiser( SAState* psa )
{
	SLPInternalError			status = SLP_OK;
	Boolean				isMainAdvertiser = false;
    SLPDAAdvertiser*	newAdvertiser = NULL;
    
    if ( gDAAdvertiser )
    {
        gDAAdvertiser->RestartAdvertisements();
    }
    else
    {
        isMainAdvertiser = true;
        newAdvertiser = new SLPDAAdvertiser( psa, isMainAdvertiser );
        
        status = newAdvertiser->Initialize();
        
        if ( status )
        {
            delete newAdvertiser;
            newAdvertiser = NULL;
        }
        else
            newAdvertiser->Resume();
    
        gDAAdvertiser = newAdvertiser;
    }
    
	return status;
}

void StopSLPDAAdvertiser( void )
{
    SLPDAAdvertiser* 	curDAAdvertiser = gDAAdvertiser;
    
    gDAAdvertiser = NULL;
    
    if ( curDAAdvertiser )
        curDAAdvertiser->SetRunForever( false );				// if it is asleep, it will go away eventually
}

SLPDAAdvertiser::SLPDAAdvertiser( SAState* psa, Boolean isMainAdvertiser )
	: DSLThread()
{
	mServerState = psa;
    mRunForever = isMainAdvertiser;
	mSelfPtr = this;
}

SLPDAAdvertiser::~SLPDAAdvertiser()
{
	mSelfPtr = NULL;
    CLOSESOCKET(mSocket);
#ifdef ENABLE_SLP_LOGGING
    SLP_LOG( SLP_LOG_DEBUG, "DA Advertiser has been killed" );
#endif
}

SLPInternalError SLPDAAdvertiser::Initialize( void )
{
    SLPInternalError		err = SLP_OK;

    mTimeToMakeNextAdvert = SDGetTime();
    
    mSocket = socket(AF_INET, SOCK_DGRAM, 0);

    if (mSocket < 0 || mSocket == SOCKET_ERROR)
    {
#ifdef ENABLE_SLP_LOGGING
        SLP_LOG(SLP_LOG_DEBUG,"SLPDAAdvertiser: socket",SLP_NETWORK_INIT_FAILED);
#endif        
        err = SLP_NETWORK_INIT_FAILED;
    }
    else
    {
        memset(&mSockAddr_in,0,sizeof mSockAddr_in);
        mSockAddr_in.sin_family = AF_INET;
        mSockAddr_in.sin_port   = htons(SLP_PORT);
        
        mSockAddr_in.sin_addr.s_addr = SLP_MCAST;						// I don't think it makes sense for us to not multicast  as a DA
        if ((err = set_multicast_sender_interf(mSocket)) != SLP_OK) 
        {
            CLOSESOCKET(mSocket);
#ifdef ENABLE_SLP_LOGGING
            SLP_LOG(SLP_LOG_DEBUG,"SLPDAAdvertiser: set_multicast_sender_interf %s",slperror(err));
#endif
        }
    }
    
    return err;
}

void SLPDAAdvertiser::RestartAdvertisements( void )
{
#ifdef ENABLE_SLP_LOGGING
    SLP_LOG( SLP_LOG_MSG, "DA Advertiser restarted" );
#endif
    mTimeToMakeNextAdvert = SDGetTime();
#ifdef ENABLE_SLP_LOGGING
    SLP_LOG( SLP_LOG_DEBUG, "setting mTimeToMakeNextAdvert to %ld", mTimeToMakeNextAdvert );
#endif
}

#define	kNumDAAdvertsToMake						3
#define kNumAdditionalFirstTimeAdvertsToMake	6
void* SLPDAAdvertiser::Run()
{
    const char *	pcAttributeList = "";
    char*			advertMessage = NULL;
    int 			piOutSz = 0;
    Boolean			isFirstTime = true;
    
#ifdef ENABLE_SLP_LOGGING
    SLP_LOG( SLP_LOG_MSG, "DA Advertiser is running" );
#endif
    do
    {
        if ( AreWeADirectoryAgent() && mTimeToMakeNextAdvert < SDGetTime() )
        {
            int			numDAAdvertsToMake = kNumDAAdvertsToMake;
            char*		scopeListToAdvertise = (char*)malloc( strlen(SLPGetProperty("com.apple.slp.daScopeList")) + 1 );// start out with a copy
            SLPBoolean	needToSetOverflow = SLP_FALSE;
            
            if ( SLPGetProperty("com.apple.slp.daAttributeList") )
                pcAttributeList = SLPGetProperty("com.apple.slp.daAttributeList");
                
            if ( SLPGetProperty("com.apple.slp.daScopeList" ) )
                strcpy( scopeListToAdvertise, SLPGetProperty("com.apple.slp.daScopeList") );
            else
            {
#ifdef ENABLE_SLP_LOGGING
                SLP_LOG( SLP_LOG_DEBUG, "We are trying to advertise an empty scope list!" );
#endif
                scopeListToAdvertise[0] = '\0';
            }
                
            if ( mRunForever && isFirstTime )
                numDAAdvertsToMake += kNumAdditionalFirstTimeAdvertsToMake;
                
            if ( SLPGetProperty("com.apple.slp.daPrunedScopeList") && SLPGetProperty("com.apple.slp.daPrunedScopeList") != "" )
            {
                strcpy( scopeListToAdvertise, SLPGetProperty("com.apple.slp.daPrunedScopeList") );
                needToSetOverflow = SLP_TRUE;
            }
            else if ( SLPGetProperty("com.apple.slp.daScopeList") )
                strcpy( scopeListToAdvertise, SLPGetProperty("com.apple.slp.daScopeList") );
            else
                goto bailTillNextTime;		// no Scope list!
                
            advertMessage = MakeDAAdvertisementMessage( NULL, mServerState->pcDAURL, scopeListToAdvertise, pcAttributeList, GetStatelessBootTime(), &piOutSz );            
            if ( needToSetOverflow == SLP_TRUE )
                SETFLAGS(advertMessage,(unsigned char) OVERFLOWFLAG);	// we want clients to make a TCP connection

            SETFLAGS(advertMessage,(unsigned char) MCASTFLAG);		// we are multicasting this
            
            for ( int i=0; i<numDAAdvertsToMake; i++ )
            {
#ifdef ENABLE_SLP_LOGGING
                if ( needToSetOverflow == SLP_TRUE )
                    SLP_LOG( SLP_LOG_DEBUG, "Sending out Truncated DAAdvert for scopelist: ", scopeListToAdvertise );
                else
                    SLP_LOG( SLP_LOG_DEBUG, "Sending out DAAdvert for scopelist: %s", scopeListToAdvertise );
#endif                
                if (sendto(mSocket, advertMessage, piOutSz, 0, (struct sockaddr*) &mSockAddr_in, sizeof(struct sockaddr_in)) < 0)
                {
#ifdef ENABLE_SLP_LOGGING
                    mslplog(SLP_LOG_DA,"SLPDAAdvertiser: multicast sendto",strerror(errno));
#endif
                }
#ifdef ENABLE_SLP_LOGGING
                else
                    SLP_LOG( SLP_LOG_DA, "Unsolicited DA Advertisement Sent" );
#endif                
                SmartSleep( 3*USEC_PER_SEC );	// just wait a few of seconds between advertisements
            }
        
            SLPFree(advertMessage);
            advertMessage = NULL;
            
            if ( scopeListToAdvertise )
                free( scopeListToAdvertise );
        
            mTimeToMakeNextAdvert = SDGetTime() + CONFIG_DA_HEART_BEAT;		// don't do this again for CONFIG_DA_HEART_BEAT or if someone resets mTimeToMakeNextAdvert
        }

bailTillNextTime:
        
        SmartSleep ( 5*USEC_PER_SEC );		// we are just going to sleep and poll every 5 seconds to see if we need
                            // to start up again (DA state could have changed and instead of creating new
                            // threads, we'll do all our adverts here
                
        isFirstTime = false;
	} while (mRunForever);
    
	return NULL;
}
