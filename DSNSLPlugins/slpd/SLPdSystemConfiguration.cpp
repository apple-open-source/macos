/*
 *  SLPdSystemConfiguration.cpp
 *  DSNSLPlugins
 *
 *  Created by Kevin Arnold on Thu Feb 21 2002.
 *  Copyright (c) 2002 __MyCompanyName__. All rights reserved.
 *
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

#include "SLPSystemConfiguration.h"
#include "SLPdSystemConfiguration.h"

EXPORT void InitializeSLPdSystemConfigurator( void )
{
    SLPdSystemConfiguration::TheSLPSC()->Initialize();
}


SLPSystemConfiguration* SLPdSystemConfiguration::TheSLPSC( CFRunLoopRef runLoopRef )
{
    if ( !msSLPSC )
    {
        msSLPSC = new SLPdSystemConfiguration( runLoopRef );
        msSLPSC->Initialize();
    }
    
    return msSLPSC;
}

void SLPdSystemConfiguration::FreeSLPSC( void )
{
    if ( msSLPSC )
        free( msSLPSC );
    
    msSLPSC = NULL;
}

SLPdSystemConfiguration::SLPdSystemConfiguration( CFRunLoopRef runLoopRef )
	: SLPSystemConfiguration( runLoopRef )
{
    SLP_LOG( SLP_LOG_CONFIG, "New SLPdSystemConfiguration created" );
}


SLPdSystemConfiguration::~SLPdSystemConfiguration()
{
    SLP_LOG( SLP_LOG_CONFIG, "SLPdSystemConfiguration deleted" );
}

void SLPdSystemConfiguration::HandleIPv4Notification( void )
{
    // delete the reg file
    SLP_LOG( SLP_LOG_CONFIG, "SLPdSystemConfiguration::HandleIPv4Notification, deleting reg file" );
    delete_regfile( SLPGetProperty("com.sun.slp.regfile") );
    
    CFRunLoopStop(mMainRunLoopRef);
}

void SLPdSystemConfiguration::HandleInterfaceNotification( void )
{
    // delete the reg file
    SLP_LOG( SLP_LOG_CONFIG, "SLPdSystemConfiguration::HandleInterfaceNotification, deleting reg file" );
    delete_regfile( SLPGetProperty("com.sun.slp.regfile") );
    
    CFRunLoopStop(mMainRunLoopRef);
}

