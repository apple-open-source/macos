/*
 * Copyright (c) 1998-2003 Apple Computer, Inc. All rights reserved.
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

// public
#import <IOKit/firewire/IOFireWireController.h>

// protected
#import <IOKit/firewire/IOFWWorkLoop.h>

// private
#import "IOFWQEventSource.h"

// createPendingQ
//
//

IOReturn IOFireWireController::createPendingQ( void )
{
    IOFWQEventSource *q;
    q = new IOFWQEventSource;
    fPendingQ.fSource = q;
    q->init(this);

	fWorkLoop->addEventSource(fPendingQ.fSource);
	
	return kIOReturnSuccess;
}

// destroyPendingQ
//
//

void IOFireWireController::destroyPendingQ( void )
{
	fWorkLoop->removeEventSource(fPendingQ.fSource);
    fPendingQ.fSource->release();
}

// headChanged
//
//

void IOFireWireController::pendingQ::headChanged(IOFWCommand *oldHead)
{
    if( fHead ) 
	{
        fSource->signalWorkAvailable();
    }
}
