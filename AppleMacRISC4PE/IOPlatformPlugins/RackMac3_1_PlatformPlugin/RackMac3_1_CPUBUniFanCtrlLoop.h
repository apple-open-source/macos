/*
 * Copyright (c) 2002-2003 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
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
/*
 * Copyright (c) 2002-2003 Apple Computer, Inc.  All rights reserved.
 *
 *
 */

#ifndef _RACKMAC3_1_CPUBUNIFANCTRLLOOP_H
#define _RACKMAC3_1_CPUBUNIFANCTRLLOOP_H

#include "IOPlatformPIDCtrlLoop.h"

class RackMac3_1_CPUBUniFanCtrlLoop : public IOPlatformPIDCtrlLoop
{

    OSDeclareDefaultStructors(RackMac3_1_CPUBUniFanCtrlLoop)

protected:

    IOPlatformControl * secondOutputControl;
    IOPlatformControl * thirdOutputControl;

    // overrides from OSObject superclass
    virtual bool init( void );
    virtual void free( void );
    virtual bool cacheMetaState( const OSDictionary * metaState );

public:

    // By setting a deadline and handling the deadlinePassed() callback, we get a periodic timer
    // callback that is funnelled through the platform plugin's command gate.

    virtual IOReturn initPlatformCtrlLoop(const OSDictionary *dict);
    virtual void deadlinePassed( void );
    virtual bool updateMetaState( void );
    virtual ControlValue calculateNewTarget( void ) const;
    virtual void sendNewTarget( ControlValue newTarget );
    virtual void sensorRegistered( IOPlatformSensor * aSensor );
    virtual void controlRegistered( IOPlatformControl * aControl );
};

#endif	// _RACKMAC3_1_CPUBUNIFANCTRLLOOP_H
