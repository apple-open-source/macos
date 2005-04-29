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

#include "IOHIDPrivate.h"
#include "IOHIKeyboard.h"
#include "IOHIDSystem.h"
#include <IOKit/IOService.h>
#include <IOKit/IOKitKeys.h>

void _DispatchKeyboardSpecialEvent(int key, bool down)
{
    AbsoluteTime            timeStamp;
    OSDictionary *          matchingDictionary  = 0;
    OSIterator *            iterator            = 0;
    IOHIKeyboard *          keyboard            = 0;
    unsigned                flags               = 0;
    IOHIDSystem *           hidSystem           = IOHIDSystem::instance();
    
    if (!hidSystem) return;
    
    matchingDictionary  = IOService::serviceMatching( "IOHIKeyboard" );
    
    if( matchingDictionary ) 
    {
        iterator = IOService::getMatchingServices( matchingDictionary );
        if( iterator )
        {
            while( (keyboard = (IOHIKeyboard*) iterator->getNextObject()) )
            {		
                flags |= keyboard->deviceFlags();
            }
            
            iterator->release();
        }
        
        matchingDictionary->release();
    }

    clock_get_uptime( &timeStamp );

    hidSystem->keyboardSpecialEvent(
                                    down ? NX_KEYDOWN : NX_KEYUP,
                                    flags,
                                    0,
                                    key,
                                    0,
                                    false,
                                    timeStamp);    
}

