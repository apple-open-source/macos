/*
 * Copyright (c) 2008 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
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
 *  PMTestLib.c
 *  SULeoGaia Verification
 *
 *  Created by Ethan Bold on 8/4/08.
 *  Copyright 2008 Apple. All rights reserved.
 *
 */


#include <IOKit/IOReturn.h>
#include "PMTestLib.h"
#include <CoreFoundation/CoreFoundation.h>

// Expect _TESTBOTS_OUTPUT_ to be defined
#ifndef _TESTBOTS_OUTPUT_
#define _TESTBOTS_OUTPUT_   5
#endif 

IOReturn PMTestInitialize(const char *testDecription, const char *bundleID)
{

#ifdef _TESTBOTS_OUTPUT_
    printf("\n");
    printf("PMTestInitialize: TestBots style output\n");
    printf("[TEST] %s\n", testDecription ? testDecription:"unspecified test");
    printf("  id = %s\n\n", bundleID ? bundleID:"unknown id");
#else
    /* XILog */
    char *XIconfig      = NULL;
    int XIecho          = true; 
    int XIxml           = false;
    char *XILogPath     = NULL; 
    
    gLogRef = XILogOpenLog(XILogPath, 
                "PM configd server connection", 
                "com.apple.iokit.ethan", XIconfig, XIxml, XIecho);
    if(gLogRef == NULL)
    {
        return kIOReturnError;
    }
#endif

    return kIOReturnSuccess;
}

void PMTestLog(char *fmt, ...)
{
    
    va_list ap;
    va_start(ap, fmt);
#ifdef _TESTBOTS_OUTPUT_
    vprintf(fmt, ap);
    printf("\n");
#else
    XILog(...);
#endif    
    va_end(ap);
    return;
}

void PMTestPass(char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
#ifdef _TESTBOTS_OUTPUT_
    printf("[PASS] ");
    vprintf(fmt, ap);
    printf("\n");
#endif // _TESTBOTS_OUTPUT_    
    va_end(ap);
    return;
    
}

void PMTestFail(char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
#ifdef _TESTBOTS_OUTPUT_
    printf("[FAIL] ");
    vprintf(fmt, ap);
    printf("\n");
#endif // _TESTBOTS_OUTPUT_    
    va_end(ap);
    return;
}

