/*
 *  SmartBatteryUserClient.c
 *  SULeoGaia Verification
 *
 *  Created by Ethan Bold on 8/4/08.
 *  Copyright 2008 Apple. All rights reserved.
 *
 *//*
 * Copyright (c) 2007 Apple Computer, Inc. All rights reserved.
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

#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/IOKitLib.h>
#include "PMTestLib.h"
#include <stdio.h>
#include <unistd.h>

#define     kBatteryManagerName                 "AppleSmartBatteryManager"

// The SMC may block the exclusive open by up to 3 seconds while it flushes
// outstanding SMBus I/O
#define     kMaxSecondsUCOperation              3.15

#define     kUCExclusiveIterationsCount           5
#define     kUCIterationsCount                    10

#define kMaxSimultaneousConnections             3

enum {
    kBatteryExclusiveAccessType = 1
};

io_connect_t                connectSmartBatteryManager(uint32_t options, IOReturn *outret);

int main(int argc, char *argv[]) {
    io_connect_t    manager_connect = IO_OBJECT_NULL;
    io_connect_t    manager[kMaxSimultaneousConnections];

    uint32_t    openfailure = 0;
    uint32_t    closefailure = 0;
    uint32_t    ucOpenedCount = 0;
    uint32_t    ucExclusiveOpenedCount = 0;
    IOReturn    connectreturn = 0;
    kern_return_t kernreturn = 0;
    CFAbsoluteTime      start_time = 0.0;
    CFAbsoluteTime      end_time = 0.0;
    CFTimeInterval      elapsed_time = 0.0;
    io_registry_entry_t IOREG_SmartBattery = IO_OBJECT_NULL;
    int                 simultaneousCount = 0;
    
    PMTestInitialize("SmartBatteryUserClient repetition test", "com.apple.iokit.smartbattery.repeater");

/* 
 * Make sure we can open a few simultaneous user clients
 */    
    for(simultaneousCount=0; simultaneousCount < kMaxSimultaneousConnections; simultaneousCount++) {
    
        manager[simultaneousCount] = connectSmartBatteryManager(0, &connectreturn);
        if (kIOReturnSuccess != connectreturn) {
            manager[simultaneousCount] = 0;
            PMTestFail("Failed to open non-exclusive user client #%d of %d. Status = 0x%08x", 
                simultaneousCount, kMaxSimultaneousConnections, connectreturn);
        } else {
            PMTestPass("Opened non-exclusive user client depth %d", simultaneousCount);
        }
    }
    
    IOREG_SmartBattery = IOServiceGetMatchingService( MACH_PORT_NULL,
                            IOServiceNameMatching(kBatteryManagerName) );
    if (IO_OBJECT_NULL == IOREG_SmartBattery) {
        PMTestLog("This machine does not support batteries. Skipping battery tests.");
        exit(0);
    }
    IOObjectRelease(IOREG_SmartBattery);
    
    for (simultaneousCount = kMaxSimultaneousConnections-1; simultaneousCount >= 0; simultaneousCount--)
    {
        if (!manager[simultaneousCount]) {
            PMTestLog("ODDITY: Trying to close connection %d - but NULL connection ID", simultaneousCount);
            continue;
        }
        connectreturn = IOServiceClose(manager[simultaneousCount]);
        if (kIOReturnSuccess != connectreturn) {
            PMTestFail("Failed to CLOSE non-exclusive user client #%d of %d. Status = 0x%08x", 
                    simultaneousCount, kMaxSimultaneousConnections, connectreturn);
        } else {
            PMTestPass("Closed user client at depth %d", simultaneousCount);
        }
    }
    
    while ( (ucOpenedCount < kUCIterationsCount) && (ucExclusiveOpenedCount < kUCExclusiveIterationsCount))
    {
/* 
 * Regular user client
 */    
        if (ucOpenedCount < kUCIterationsCount)
        {
            /* OPEN REGULAR */
            start_time = CFAbsoluteTimeGetCurrent();
            manager_connect = connectSmartBatteryManager(0, &connectreturn);
            if (MACH_PORT_NULL == manager_connect) 
            {
                PMTestFail("IOServiceOpen error 0x%08x opening %s", connectreturn, kBatteryManagerName);
                openfailure++;
            } else {
                end_time = CFAbsoluteTimeGetCurrent();
                
                elapsed_time = end_time - start_time;
                PMTestPass("User client opened successfully in %d.%02d seconds", (int)elapsed_time, (int)(100.0 * elapsed_time)%100);
                if (elapsed_time > kMaxSecondsUCOperation) {
                    PMTestFail("Error - opening user client took %d.%02d, exceeding %d.%02d", 
                                    (int)elapsed_time, (int)(100.0 * elapsed_time)%100,
                                    (int)kMaxSecondsUCOperation, (int)(100.0*kMaxSecondsUCOperation)%100);
                }
                
                /* CLOSE REGULAR */
                start_time = CFAbsoluteTimeGetCurrent();
                kernreturn = IOServiceClose(manager_connect);
                if (KERN_SUCCESS != kernreturn) {
                    PMTestFail("IOServiceClose error %d closing user client.", kernreturn);
                    closefailure++;
                } else {
                    end_time = CFAbsoluteTimeGetCurrent();
                    
                    elapsed_time = end_time - start_time;
                    PMTestPass("User client closed successfully in %d.%02d seconds", (int)elapsed_time, (int)(100.0 * elapsed_time)%100);
                    if (elapsed_time > kMaxSecondsUCOperation) {
                        PMTestFail("Error - closing user client took %d.%02d, exceeding %d.%02d", 
                                        (int)elapsed_time, (int)(100.0 * elapsed_time)%100,
                                        (int)kMaxSecondsUCOperation, (int)(100.0*kMaxSecondsUCOperation)%100);
                    }
                }
            }
            ucOpenedCount++;
        }

/*
 * Exclusive
 */
        
        if (ucExclusiveOpenedCount < kUCExclusiveIterationsCount)
        {
            /* OPEN EXCLUSIVE */
            start_time = CFAbsoluteTimeGetCurrent();
            manager_connect = connectSmartBatteryManager(kBatteryExclusiveAccessType, &connectreturn);
            if (MACH_PORT_NULL == manager_connect) 
            {
                //PMTestFail
                PMTestLog("IOServiceOpen error 0x%08x opening exclusive %s (This test requires root privileges; this may be a failure)", connectreturn, kBatteryManagerName);
                openfailure++;
            } else {
                end_time = CFAbsoluteTimeGetCurrent();
                
                elapsed_time = end_time - start_time;
                PMTestPass("User client EXCLUSIVE opened successfully in %d.%02d seconds", (int)elapsed_time, (int)(100.0 * elapsed_time)%100);
                if (elapsed_time > kMaxSecondsUCOperation) {
                    PMTestFail("Error - opening EXCLUSIVE user client took %d.%02d, exceeding %d.%02d", 
                                    (int)elapsed_time, (int)(100.0 * elapsed_time)%100,
                                    (int)kMaxSecondsUCOperation, (int)(100.0*kMaxSecondsUCOperation)%100);
                }
                /* CLOSE EXCLUSIVE */
                start_time = CFAbsoluteTimeGetCurrent();
                kernreturn = IOServiceClose(manager_connect);
                if (KERN_SUCCESS != kernreturn) {
                    PMTestFail("IOServiceClose error %d closing user client.", kernreturn);
                    closefailure++;
                } else {
                    end_time = CFAbsoluteTimeGetCurrent();
                    
                    elapsed_time = end_time - start_time;
                    PMTestPass("User client EXCLUSIVE closed successfully in %d.%02d seconds", (int)elapsed_time, (int)(100.0 * elapsed_time)%100);
                    if (elapsed_time > kMaxSecondsUCOperation) {
                        PMTestFail("Error - closing EXCLUSIVE user client took %d.%02d, exceeding %d.%02d", 
                                        (int)elapsed_time, (int)(100.0 * elapsed_time)%100,
                                        (int)kMaxSecondsUCOperation, (int)(100.0*kMaxSecondsUCOperation)%100);
                    }
                }
            }
            ucExclusiveOpenedCount++;
        }
        
    }

    PMTestLog("SmartBatteryUserClient test completed: opened %d clients and %d exclusive clients", 
                                        ucOpenedCount, ucExclusiveOpenedCount);

    if (openfailure == 0 && closefailure == 0)
    {
        PMTestLog("Success.");
    } else {
        if (openfailure) {
            PMTestLog("Test completed with %d failures opening the user client.", openfailure);
        }
        if (closefailure) {
            PMTestLog("Test completed with %d failures closing the user client.", closefailure);
        }
    }

    PMTestLog("The test is over.");

    return 0;
}

io_connect_t    connectSmartBatteryManager(uint32_t options, IOReturn *outret)
{
    io_service_t            smartbattman_entry = MACH_PORT_NULL;
    io_connect_t            manager_connect = MACH_PORT_NULL;
    IOReturn                ret;

    smartbattman_entry = IOServiceGetMatchingService( MACH_PORT_NULL,
                            IOServiceNameMatching(kBatteryManagerName) );


    if (MACH_PORT_NULL == smartbattman_entry) {
        return MACH_PORT_NULL;
    }

    ret = IOServiceOpen( smartbattman_entry,            /* service */
                         mach_task_self(),              /* owning task */
                         options,                       /* type - kBatteryExclusiveAccessType or not*/
                         &manager_connect);             /* connect */


    if (outret) *outret = ret;

    if(kIOReturnSuccess != ret) {
        return MACH_PORT_NULL;
    }
    
    IOObjectRelease(smartbattman_entry);
    return manager_connect;
}



