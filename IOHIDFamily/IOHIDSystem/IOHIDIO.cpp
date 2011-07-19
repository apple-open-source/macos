/*
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
/* 	Copyright (c) 1992 NeXT Computer, Inc.  All rights reserved. 
 *
 * EventIO.m - Event System MiG interface for driver control and status.
 *
 * HISTORY
 * 2-April-92    Mike Paquette at NeXT 
 *      Created. 
 */

#include <IOKit/system.h>

#include <IOKit/hidsystem/IOHIDTypes.h>
#include <IOKit/hidsystem/IOHIDSystem.h>
#include <IOKit/hidsystem/IOHIDShared.h>

#include <IOKit/hidsystem/ev_private.h>	/* Per-machine configuration info */

/*
 * Additional kernel API to drivers using the Event Driver
 */
int EventCoalesceDisplayCmd( int cmd, int oldcmd );
int EventCoalesceDisplayCmd( int cmd, int oldcmd )
{
	static const char coalesce[4][4] = {
	    /* nop */  {EVNOP,  EVHIDE, EVSHOW, EVMOVE},
	    /* hide */ {EVHIDE, EVHIDE, EVNOP,  EVSHOW},
	    /* show */ {EVSHOW, EVNOP,  EVSHOW, EVSHOW},
	    /* move */ {EVMOVE, EVHIDE, EVSHOW, EVMOVE}
	};
	if ( cmd < EVLEVEL )	// coalesce EVNOP thru EVMOVE only
	    cmd = coalesce[oldcmd & 3][cmd & 3];
	return cmd;
}

