/*
 *  SLPDAAdvertiser.cpp
 *  NSLPlugins
 *
 *  Created by root on Fri Sep 29 2000.
 *  Copyright (c) 2000 Apple Computer. All rights reserved.
 *
 */

/*
	File:		SLPDAAdvertiser.cp

	Contains:	A thread that will actively advertise the DA's presence
    
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
#include "SLPDefines.h"

#include "slpipc.h"
//#include "URLUtilities.h"
#include "SLPDAAdvertiser.h"

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
//        curDAAdvertiser->DeleteThread();
}

SLPDAAdvertiser::SLPDAAdvertiser( SAState* psa, Boolean isMainAdvertiser )
	: LThread(threadOption_Default)
{
	mServerState = psa;
    mRunForever = isMainAdvertiser;
	mSelfPtr = this;
}

SLPDAAdvertiser::~SLPDAAdvertiser()
{
	mSelfPtr = NULL;
    CLOSESOCKET(mSocket);
    SLP_LOG( SLP_LOG_DEBUG, "DA Advertiser has been killed" );
}

SLPInternalError SLPDAAdvertiser::Initialize( void )
{
    SLPInternalError		err = SLP_OK;

    mTimeToMakeNextAdvert = SDGetTime();
    
    mSocket = socket(AF_INET, SOCK_DGRAM, 0);

    if (mSocket < 0 || mSocket == SOCKET_ERROR)
    {
        SLP_LOG(SLP_LOG_DEBUG,"SLPDAAdvertiser: socket",SLP_NETWORK_INIT_FAILED);
        
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
            SLP_LOG(SLP_LOG_DEBUG,"SLPDAAdvertiser: set_multicast_sender_interf %s",slperror(err));
        }
    }
    
    return err;
}

void SLPDAAdvertiser::RestartAdvertisements( void )
{
    SLP_LOG( SLP_LOG_MSG, "DA Advertiser restarted" );
    mTimeToMakeNextAdvert = SDGetTime();
    SLP_LOG( SLP_LOG_DEBUG, "setting mTimeToMakeNextAdvert to %ld", mTimeToMakeNextAdvert );

}

#define	kNumDAAdvertsToMake						3
#define kNumAdditionalFirstTimeAdvertsToMake	6
void* SLPDAAdvertiser::Run()
{
    const char *	pcAttributeList = "";
    char*			advertMessage = NULL;
    int 			piOutSz = 0;
    Boolean			isFirstTime = true;
    
    SLP_LOG( SLP_LOG_MSG, "DA Advertiser is running" );
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
                SLP_LOG( SLP_LOG_DEBUG, "We are trying to advertise an empty scope list!" );
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
                if ( needToSetOverflow == SLP_TRUE )
                    SLP_LOG( SLP_LOG_DEBUG, "Sending out Truncated DAAdvert for scopelist: ", scopeListToAdvertise );
                else
                    SLP_LOG( SLP_LOG_DEBUG, "Sending out DAAdvert for scopelist: %s", scopeListToAdvertise );
                
                if (sendto(mSocket, advertMessage, piOutSz, 0, (struct sockaddr*) &mSockAddr_in, sizeof(struct sockaddr_in)) < 0)
                {
                    mslplog(SLP_LOG_DA,"SLPDAAdvertiser: multicast sendto",strerror(errno));
                }
                else
                    SLP_LOG( SLP_LOG_DA, "Unsolicited DA Advertisement Sent" );
                
                ::sleep( 3 );	// just wait a couple of seconds between advertisements
            }
        
            SLPFree(advertMessage);
            advertMessage = NULL;
            
            if ( scopeListToAdvertise )
                free( scopeListToAdvertise );
        
            mTimeToMakeNextAdvert = SDGetTime() + CONFIG_DA_HEART_BEAT;		// don't do this again for CONFIG_DA_HEART_BEAT or if someone resets mTimeToMakeNextAdvert
        }

bailTillNextTime:
        
        ::sleep ( 5 );		// we are just going to sleep and poll every 5 seconds to see if we need
                            // to start up again (DA state could have changed and instead of creating new
                            // threads, we'll do all our adverts here
                
        isFirstTime = false;
	} while (mRunForever);
    
	return NULL;
}
