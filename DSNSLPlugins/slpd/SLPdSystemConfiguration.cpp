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
 *  @header SLPdSystemConfiguration
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
#ifdef ENABLE_SLP_LOGGING
    SLP_LOG( SLP_LOG_CONFIG, "New SLPdSystemConfiguration created" );
#endif
}


SLPdSystemConfiguration::~SLPdSystemConfiguration()
{
#ifdef ENABLE_SLP_LOGGING
    SLP_LOG( SLP_LOG_CONFIG, "SLPdSystemConfiguration deleted" );
#endif
}

void SLPdSystemConfiguration::HandleIPv4Notification( void )
{
    // delete the reg file
#ifdef ENABLE_SLP_LOGGING
    SLP_LOG( SLP_LOG_CONFIG, "SLPdSystemConfiguration::HandleIPv4Notification, deleting reg file" );
#endif
    delete_regfile( SLPGetProperty("com.sun.slp.regfile") );
    
    CFRunLoopStop(mMainRunLoopRef);
}

void SLPdSystemConfiguration::HandleInterfaceNotification( void )
{
    // delete the reg file
#ifdef ENABLE_SLP_LOGGING
    SLP_LOG( SLP_LOG_CONFIG, "SLPdSystemConfiguration::HandleInterfaceNotification, deleting reg file" );
#endif
    delete_regfile( SLPGetProperty("com.sun.slp.regfile") );
    
    CFRunLoopStop(mMainRunLoopRef);
}
