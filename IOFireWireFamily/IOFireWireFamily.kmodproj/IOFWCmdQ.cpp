/*
 * Copyright (c) 1998-2003 Apple Computer, Inc. All rights reserved.
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

#import <IOKit/firewire/IOFireWireController.h>

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

// executeQueue
//
//

bool IOFWCmdQ::executeQueue(bool all)
{
    IOFWCommand *cmd;
    
	cmd = fHead;
    
	while( cmd ) 
	{
        IOFWCommand *newHead;
        newHead = cmd->getNext();
        
		if( newHead )
            newHead->fQueuePrev = NULL;
        else
            fTail = NULL;
        
		fHead = newHead;

        cmd->fQueue = NULL;	// Not on this queue anymore
		cmd->startExecution();
        
		if(!all)
            break;
        
		cmd = newHead;
    }
	
    return fHead != NULL;	// ie. more to do
}

// checkProgress
//
//

void IOFWCmdQ::checkProgress( void )
{
    IOFWCommand *cmd;
    cmd = fHead;
    while(cmd) 
	{
        IOFWCommand *next;
        next = cmd->getNext();

		// see if this command has gone on for too long
		IOReturn status = cmd->checkProgress();
		if( status != kIOReturnSuccess )
		{
            cmd->complete( status );
        }
        cmd = next;
    }
}

// headChanged
//
//

void IOFWCmdQ::headChanged(IOFWCommand *oldHead)
{
    
}

