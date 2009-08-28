/*
 * Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
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
 *  XXXXXXXXXXXX.c
 *  Automated Power Management tests
 *
 *  Created by Ethan Bold on 7/25/08.
 *  Copyright 2008 Apple. All rights reserved.
 *
 */

#include <CoreFoundation/CoreFoundation.h>
#include <SystemConfiguration/SCValidation.h>
#include <IOKit/IOKitLib.h>
#include <IOKit/pwr_mgt/IOPMLibPrivate.h>

#include <XILog/XILog.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>

main(int argc, char *argv[])
{
    /* XILog */
    char *XIconfig      = NULL;
    int XIecho          = true; 
    int XIxml           = false;
    char *XILogPath     = NULL; 

    /* Custom log file may be defined on command line */
    if (argc > 2) {
        XILogPath = argv[1];
    }

    // 
    // XILog Initialization
    //
     XILogRef logRef = XILogOpenLog(XILogPath, "PMAssertions", "com.apple.iokit.ethan", XIconfig, XIxml, XIecho);
     if(logRef == NULL)
     {
         fprintf(stderr,"Couldn't create log: %s", XILogPath ? XILogPath : "(NULL)");
         exit(-1);
     }
     
     
     
     
     
     
    //
    // XILog Cleanup
    // 
    XILogEndTestCase(logRef, kXILogTestPassOnErrorLevel);
}



