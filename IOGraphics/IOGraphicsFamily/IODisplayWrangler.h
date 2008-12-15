/*
 * Copyright (c) 1998-2000 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * The contents of this file constitute Original Code as defined in and
 * are subject to the Apple Public Source License Version 1.1 (the
 * "License").  You may not use this file except in compliance with the
 * License.  Please obtain a copy of the License at
 * http://www.apple.com/publicsource and read it before using this file.
 * 
 * This Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */

#ifndef _IOKIT_IODISPLAYWRANGLER_H
#define _IOKIT_IODISPLAYWRANGLER_H

#include <IOKit/IOService.h>
#define IOFRAMEBUFFER_PRIVATE
#include <IOKit/graphics/IOFramebuffer.h>
#include <IOKit/graphics/IODisplay.h>

class IODisplayWrangler : public IOService
{
    OSDeclareDefaultStructors( IODisplayWrangler );

private:
    bool	fOpen;
    IOLock *	fMatchingLock;
    OSSet *	fFramebuffers;
    OSSet *	fDisplays;

    // from control panel: number of idle minutes before dimming
    UInt32	fMinutesToDim;
    // false: use minutesToDim unless in emergency situation
    bool	fDimCaptured;

    // ignore activity until time
    AbsoluteTime fIdleUntil;

    struct annoyance_event_t
    {
            UInt64 dim_time_secs;
            UInt64 wake_time_secs;
            UInt32 penalty;
    };
    
    struct annoyance_cap_t
    {
            UInt32 cutoff_time_secs;
            int cutoff_points;
    };
    
    struct annoyance_penalty_t
    {
            UInt32 time_secs;
            int penalty_points;
    };
    
    UInt64                fLastWakeTime_secs;
    UInt64                fLastDimTime_secs;    

    int                   fAnnoyanceEventArrayLength;
    annoyance_event_t   * fAnnoyanceEventArray;
    int                   fAnnoyanceEventArrayQHead;

    int                   fAnnoyanceCapsArrayLength;
    annoyance_cap_t     * fAnnoyanceCapsArray;

    int                   fAnnoyancePenaltiesArrayLength;
    annoyance_penalty_t * fAnnoyancePenaltiesArray;

    UInt32                fIdleTimeoutMin;
    UInt32                fIdleTimeoutMax;

private:

    virtual void initForPM( void );
    virtual IOReturn setAggressiveness( unsigned long, unsigned long );
    virtual bool activityTickle( unsigned long, unsigned long );
    virtual IOReturn setPowerState( unsigned long powerStateOrdinal, IOService* whatDevice );

    virtual unsigned long initialPowerStateForDomainState( IOPMPowerFlags domainState );

    virtual void makeDisplaysUsable( void );
    virtual void idleDisplays( void );
      
    static bool _displayHandler( void * target, void * ref,
                            IOService * newService );
    static bool _displayConnectHandler( void * target, void * ref,
                            IOService * newService );

    virtual bool displayHandler( OSSet * set, IODisplay * newDisplay);
    virtual bool displayConnectHandler( void * ref, IODisplayConnect * connect);

    virtual IODisplayConnect * getDisplayConnect(
		IOFramebuffer * fb, IOIndex connect );

    virtual IOReturn getConnectFlagsForDisplayMode(
		IODisplayConnect * connect,
		IODisplayModeID mode, UInt32 * flags );

public:
    
    static bool serverStart(void);
    virtual bool start(IOService * provider);

    static bool makeDisplayConnects( IOFramebuffer * fb );
    static void destroyDisplayConnects( IOFramebuffer * fb );
    virtual OSObject * copyProperty( const char * aKey) const;

    static IOReturn getFlagsForDisplayMode(
		IOFramebuffer * fb,
		IODisplayModeID mode, UInt32 * flags );
   
    // Adaptive Dimming methods
    virtual UInt32 calculate_idle_timer_period(int powerState);
    virtual void record_if_annoyance();
    virtual UInt32 calculate_penalty( UInt32 time_between_dim_and_wake_secs );
    virtual UInt64 calculate_latest_veto_till_time( UInt64 current_time_ns );
    virtual UInt64 calculate_earliest_time_idle_timeout_allowed( 
            UInt64 current_time_ns, UInt64 last_activity_secs, int powerState );
    virtual SInt32 nextIdleTimeout(AbsoluteTime currentTime, 
        AbsoluteTime lastActivity, unsigned int powerState);

private:
    void enqueueAnnoyance( UInt64 dim_time_secs, UInt64 wake_time_secs, UInt32 penalty );
    annoyance_event_t * getNthAnnoyance( int i );
    static UInt32 staticAnnoyanceEventArrayLength;
    static annoyance_event_t staticAnnoyanceEventArray[];
    static UInt32 staticAnnoyanceCapsArrayLength;
    static annoyance_cap_t staticAnnoyanceCapsArray[];
    static UInt32 staticAnnoyancePenaltiesArrayLength;
    static annoyance_penalty_t staticAnnoyancePenaltiesArray[];

public:
    virtual IOReturn setProperties( OSObject * properties );
    
};

void IODisplayUpdateNVRAM( IOService * entry, OSData * property );

#endif /* _IOKIT_IODISPLAYWRANGLER_H */
