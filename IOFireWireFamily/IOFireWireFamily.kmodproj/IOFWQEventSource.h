/*
 * Copyright (c) 1998-2002 Apple Computer, Inc. All rights reserved.
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
/*
	$Log: IOFWQEventSource.h,v $
	Revision 1.2  2002/09/25 00:27:20  niels
	flip your world upside-down

*/

#ifndef _IOKIT_IOFWQEVENTSOURCE_H
#define _IOKIT_IOFWQEVENTSOURCE_H

#import <IOKit/IOEventSource.h>

class IOFWCmdQ ;
class IOFireWireController ;

class IOFWQEventSource : public IOEventSource
{
    OSDeclareDefaultStructors(IOFWQEventSource)

protected:
    IOFWCmdQ *fQueue;
    virtual bool checkForWork();

public:
    bool init(IOFireWireController *owner);
    inline void signalWorkAvailable()	{IOEventSource::signalWorkAvailable();};
    inline void openGate()		{IOEventSource::openGate();};
    inline void closeGate()		{IOEventSource::closeGate();};
	inline bool inGate( void )  {return workLoop->inGate();};
};

#endif
