/*
 * Copyright (c) 2000-2001 Apple Computer, Inc. All Rights Reserved.
 * 
 * The contents of this file constitute Original Code as defined in and are
 * subject to the Apple Public Source License Version 1.2 (the 'License').
 * You may not use this file except in compliance with the License. Please obtain
 * a copy of the License at http://www.apple.com/publicsource and read it before
 * using this file.
 * 
 * This Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER EXPRESS
 * OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES, INCLUDING WITHOUT
 * LIMITATION, ANY WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR
 * PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT. Please see the License for the
 * specific language governing rights and limitations under the License.
 */


//
// yarrowseed - periodical to collect and seed entropy into /dev/random
//
#include "yarrowseed.h"
#include "MacYarrow_OSX.h"


//
// Constructor initializes the entropy generator and schedules itself
//
YarrowTimer::YarrowTimer(MachPlusPlus::MachServer &srv, const char *entropyFile)
    : MachServer::Timer(), server(srv)
{
    unsigned firstTimeout;
#if correct
    if (OSStatus err = yarrowServerInit(entropyFile, &firstTimeout))
        MacOSError::throwMe(err);
#else
    yarrowServerInit(entropyFile, &firstTimeout);
#endif
    server.setTimer(this, Time::Interval(firstTimeout / 1000.0));
}


/* 
 * Timeout event, the sole purpose of this class. Pass on to MacYarrow module.
 */
void YarrowTimer::action()
{
	unsigned nextTimeout = yarrowTimerEvent();
	scheduleTimer(nextTimeout);
}

void YarrowTimer::scheduleTimer(unsigned msFromNow)
{
    server.setTimer(this, Time::Interval(msFromNow / 1000.0));
}
