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
    bool        fOpen;
    IOLock *    fMatchingLock;
    OSSet *     fFramebuffers;
    OSSet *     fDisplays;

    // from control panel: number of idle minutes before going off
    UInt32      fMinutesToDim;
    // false: use minutesToDim unless in emergency situation
    bool        fDimCaptured;
    bool        fAnnoyed;

    unsigned long fPendingPowerState;

    // ignore activity until time
    AbsoluteTime  fIdleUntil;
    // annoyed wake until time
    AbsoluteTime  fAnnoyanceUntil;

    AbsoluteTime  fDimInterval;
    AbsoluteTime  fOffInterval[2];

private:

    virtual void initForPM( void );
    virtual IOReturn setAggressiveness( unsigned long, unsigned long );
    virtual bool activityTickle( unsigned long, unsigned long );
    virtual IOReturn setPowerState( unsigned long powerStateOrdinal, IOService* whatDevice );

    virtual unsigned long initialPowerStateForDomainState( IOPMPowerFlags domainState );
      
    static bool _displayHandler( void * target, void * ref,
                            IOService * newService, IONotifier * notifier );
    static bool _displayConnectHandler( void * target, void * ref,
                            IOService * newService, IONotifier * notifier );

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
    static void activityChange( IOFramebuffer * fb );
    static unsigned long getDisplaysPowerState(void);

    virtual OSObject * copyProperty( const char * aKey) const;

    static IOReturn getFlagsForDisplayMode(
                IOFramebuffer * fb,
                IODisplayModeID mode, UInt32 * flags );
   
    // Adaptive Dimming methods
    virtual SInt32 nextIdleTimeout(AbsoluteTime currentTime, 
        AbsoluteTime lastActivity, unsigned int powerState);

public:
    virtual IOReturn setProperties( OSObject * properties );
    
};

void IODisplayUpdateNVRAM( IOService * entry, OSData * property );

#endif /* _IOKIT_IODISPLAYWRANGLER_H */
