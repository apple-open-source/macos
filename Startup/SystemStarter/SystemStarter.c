/**
 * SystemStarter.c - System Starter driver
 * Wilfredo Sanchez | wsanchez@apple.com
 * $Apple$
 **
 * Copyright (c) 1999 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Portions Copyright (c) 1999 Apple Computer, Inc.  All Rights
 * Reserved.  This file contains Original Code and/or Modifications of
 * Original Code as defined in and that are subject to the Apple Public
 * Source License Version 1.1 (the "License").  You may not use this file
 * except in compliance with the License.  Please obtain a copy of the
 * License at http://www.apple.com/publicsource and read it before using
 * this file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON- INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 **/

#include <unistd.h>
#include <NSSystemDirectories.h>
#import  <CoreFoundation/CoreFoundation.h>
#include "main.h"
#include "Log.h"
#include "StartupItems.h"
#include "StartupDisplay.h"
#include "SystemStarter.h"

int system_starter ()
{
    DisplayContext aDisplayContext = (DisplayContext)0; /* state information */

    /**
     * Init the display context.
     **/
    if (!gVerboseFlag) aDisplayContext = initDisplayContext();

    displayStatus(aDisplayContext, CFSTR("Welcome to Macintosh."), 0.0);

    if (gDebugFlag && gNoRunFlag) sleep(1);

    /**
     * Get a list of Startup Items which are in /Local and /System.
     * We can't search /Network yet because the network isn't up.
     **/
    {
        CFMutableArrayRef      aWaitingList = StartupItemListCreateMutable(NSLocalDomainMask|NSSystemDomainMask);
        CFMutableDictionaryRef aStatusDict  = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks,
                                                                                 &kCFTypeDictionaryValueCallBacks);
        CFIndex aTotal = CFArrayGetCount(aWaitingList);
        CFIndex aCount = 0;

        while (aCount++ < aTotal)
          {
            CFDictionaryRef anItem = StartupItemListGetNext(aWaitingList, aStatusDict);

            if (anItem)
              {
                CFDictionaryRef aMessageDict =                  CFDictionaryGetValue(anItem,       CFSTR("Messages"));
                CFStringRef     aMessage     = (aMessageDict) ? CFDictionaryGetValue(aMessageDict, CFSTR("start"   )) : NULL;

		CFStringRef aLocalizedMessage = StartupItemCreateLocalizedString(anItem, aMessage);

		if (!aLocalizedMessage) aLocalizedMessage = aMessage ? CFRetain(aMessage) : CFRetain(CFSTR(""));

                displayStatus(aDisplayContext, aLocalizedMessage, (((float)aCount)/(((float)aTotal)+1.0)));

		CFRelease(aLocalizedMessage);

                StartupItemRun(anItem, aStatusDict);

                {
                    CFRange aRange  = {0, CFArrayGetCount(aWaitingList)};
                    CFIndex anIndex = CFArrayGetFirstIndexOfValue(aWaitingList, aRange, anItem);

                    if (anIndex >= 0)
                        CFArrayRemoveValueAtIndex(aWaitingList, anIndex);
                }
              }
            else
              {
                CFStringRef aDescription = CFCopyDescription(aWaitingList);

                warning(CFSTR("Some startup items failed to launch due to conflicts:\n%@\n"), aDescription);

                CFRelease(aDescription);

                break;
              }
          }

        CFRelease(aStatusDict);
        CFRelease(aWaitingList);
    }

    /**
     * Good-bye.
     **/
    displayStatus      (aDisplayContext, CFSTR("Startup complete."), 1.0);
    freeDisplayContext (aDisplayContext);

    return(0);
}
