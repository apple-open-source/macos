/*
 * Copyright (c) 2009 Apple Inc. All rights reserved.
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
#include <IOKit/IOReturn.h>
#include <IOKit/pwr_mgt/IOPMLib.h>
#include <IOKit/pwr_mgt/IOPMLibPrivate.h>
#include <stdlib.h>
#include <notify.h>
#include <stdio.h>
#include "PMTestLib.h"

/***
    cc -g -o ./AssertTimeouts AssertTimeouts.c -arch i386 -framework IOKit -framework CoreFoundation
    
    This tool exercises the settimeout machinery in PM configd by creating/scheduling and removing timers.
    The tool exercises both assertion release cleanup, and dead client process.
    
    This tool detects configd crashes or incorrect return values.

***/

#define DO_ITERATIONS                   10

//#define kMyTimeoutInterval              (CFTimeInterval)5

static CFTimeInterval kMyTimeoutInterval  = 5.0;

static CFTimeInterval kMyWaitForJudgement = 10.0;


CFStringRef SystemTimeoutActions[] = {
    NULL,
    kIOPMAssertionTimeoutActionLog,
    kIOPMAssertionTimeoutActionTurnOff,
    kIOPMAssertionTimeoutActionRelease
};

typedef struct {
    IOPMAssertionID     id;
    CFStringRef         action;
} FollowAssertion;

static int kActionsCount = sizeof(SystemTimeoutActions)/sizeof(CFStringRef);

static FollowAssertion *followerArray = NULL;
static int fEnd = 0;

int main(int argc, char *argv[])
{    
    IOPMAssertionID         assertion_id = 0;
    int                     didFork = 0;

    int                     notifyTokenTimeouts         = 0;
    int                     notifytokenOverallChange    = 0;
    int                     notifyTokenAnyChange        = 0;
    __block int             countTimeouts               = 0;
    __block int             countOverallChange          = 0;
    __block int             countAnyChange              = 0;

    if (argv[1]) {
        kMyTimeoutInterval = (CFTimeInterval)strtol(argv[1], NULL, 10);
        kMyWaitForJudgement = 2 * kMyTimeoutInterval;
    }


    PMTestInitialize("Assertion Timeouts: schedule and verify timeouts & their effects.", "com.apple.iokit.powermanagement");
    PMTestLog("Performing %d assert & wait for timeout cycles across all TimeoutActions.", DO_ITERATIONS);
    PMTestLog("Assertions will timeout after %0.02fs", kMyTimeoutInterval);
    PMTestLog("Test tool will check for timeouts after %0.02fs", (CFTimeInterval)kMyWaitForJudgement);
    
    
    
    int status;
    
    status = notify_register_dispatch(kIOPMAssertionTimedOutNotifyString, &notifyTokenTimeouts, dispatch_get_main_queue(), 
                             ^(int x){ countTimeouts++; });
    if (status != NOTIFY_STATUS_OK)
        PMTestFail("Notify status %d on XXX\n", status);
    
    status = notify_register_dispatch(kIOPMAssertionsAnyChangedNotifyString, &notifytokenOverallChange,dispatch_get_main_queue(), 
                            ^(int x){ countOverallChange++; });
    if (status != NOTIFY_STATUS_OK)
        PMTestFail("Notify status %d on YYY\n", status);

    status = notify_register_dispatch(kIOPMAssertionsChangedNotifyString, &notifyTokenAnyChange, dispatch_get_main_queue(), 
                             ^(int x) { countAnyChange++; });
    if (status != NOTIFY_STATUS_OK)
        PMTestFail("Notify status %d on ZZZ\n", status);
    
    
    /* 
     *
     * Create a whole bunch of assertions and watch them timeout.
     *
     */
    
    followerArray = (FollowAssertion *)calloc(sizeof(FollowAssertion), DO_ITERATIONS * kActionsCount);

    dispatch_async(dispatch_get_main_queue(),
       ^{
            IOReturn                ret = kIOReturnSuccess; 
            int                     i = 0;
            int                     j = 0;
            char                    failureString[200];
           
            for (i=0; i<DO_ITERATIONS; i++)
            {            
                for (j=0; j<kActionsCount; j++)
                {
                
                    CFStringRef nameForAssertion = CFStringCreateWithFormat(0, 0, CFSTR("TimeoutTest(%d) %@"), i,
                                                SystemTimeoutActions[j] ? SystemTimeoutActions[j] : CFSTR("DefaultAction"));
                                
                    followerArray[fEnd].action = SystemTimeoutActions[j]; 

                    ret = IOPMAssertionCreateWithDescription(kIOPMAssertionTypeNoDisplaySleep, 
                                                nameForAssertion,
                                                NULL, NULL, NULL,
                                                kMyTimeoutInterval,
                                                followerArray[fEnd].action,
                                                &followerArray[fEnd].id);
                    
                    ++fEnd;
                    
                    CFStringGetCString(nameForAssertion, failureString, sizeof(failureString), kCFStringEncodingUTF8);
                    
                    if(kIOReturnSuccess != ret) {
                        PMTestFail("CREATION: Error 0x%08x from IOPMAssertionCreateWithDescription(%s)\n", ret, 
                                   failureString);
                        break;
                    } else {
                        // PMTestLog("CREATED: <success> %ld ; %s\n", followerArray[fEnd].id, failureString);
                    }
                                   
                   CFRelease(nameForAssertion);
                }
            }
       });



    /* Wait for the assertions to timeout, then verify that they're in the state they should be.
     */
        
    dispatch_source_t   timer_source = dispatch_source_create(DISPATCH_SOURCE_TYPE_TIMER, 0, 0, dispatch_get_main_queue());

    dispatch_source_set_timer(timer_source, dispatch_time(DISPATCH_TIME_NOW, (uint64_t)kMyWaitForJudgement * NSEC_PER_SEC), DISPATCH_TIME_FOREVER, 0);
    
    dispatch_source_set_event_handler(timer_source, 
              ^{
                  int exitStatus = 1;
                  
                  
                  printf("Created a total of %d assertions.\n", fEnd);
                  printf("- AssertionTimeout notify fired %d times\n", countTimeouts);
                  printf("- AssertionOverallChange notify fired %d times\n", countOverallChange);
                  printf("- AssertionAnyChange notify fired %d times\n", countAnyChange);

                  if (countTimeouts < fEnd) {
                      PMTestFail("AssertionTimeout fired fewer times (%d) than assertions that timed out(%d).", countTimeouts, fEnd);
                  }
                  
                  if (countAnyChange < fEnd) {
                      PMTestFail("AssertionAnyChange fired fewer times (%d) than assertions that timed out(%d).", countAnyChange, fEnd);
                  }


                  
                  int k = 0;
                  for (k=0; k<fEnd; k++) {
                      
                      CFDictionaryRef   asrt = IOPMAssertionCopyProperties(followerArray[k].id);
                      CFStringRef       usedAction = followerArray[k].action ? followerArray[k].action : kIOPMAssertionTimeoutActionLog;
                      char              actionStr[200];
                      CFDateRef         timeoutDateKey = NULL;
                      CFDateRef         startDateKey = NULL;
                      CFTimeInterval    aliveInterval = 0.0;
                      int               level = -1;
                      
                      
                      snprintf(actionStr, sizeof(actionStr), "<UNKNOWN TYPE>");
                      CFStringGetCString(usedAction, actionStr, sizeof(actionStr), kCFStringEncodingUTF8);
                      if (asrt) {
                          timeoutDateKey = CFDictionaryGetValue(asrt, kIOPMAssertionTimedOutDateKey);
                          startDateKey = CFDictionaryGetValue(asrt, kIOPMAssertionCreateDateKey);
                        
                          if (!(timeoutDateKey && startDateKey)) {
                              PMTestFail("The dictionary for assertion %ld doesn't contain start and timeout dates.", followerArray[k].id);
                              CFShow(asrt);
                          } else {
                              aliveInterval = CFDateGetTimeIntervalSinceDate(timeoutDateKey, startDateKey);
                              
                              if ((aliveInterval <= (kMyTimeoutInterval - 1.0))
                              || (aliveInterval >= (kMyTimeoutInterval + 1.0))) {
                                  PMTestFail("Assertion %ld timed out at an incorrect time: timeout=%0.02f actual=%0.02f",
                                      followerArray[k].id, kMyTimeoutInterval, aliveInterval);
                              }
                              
                          }
                      
                          CFNumberRef lev = CFDictionaryGetValue(asrt, kIOPMAssertionLevelKey);
                          if (lev)
                              CFNumberGetValue(lev, kCFNumberIntType, &level);
                      }
/*                      
                      PMTestLog("CLEANUP STATUS: Assertion %d <Action=%s> assertion property dictionary %s%s%s",
                                 k, actionStr, asrt ? "exists":"is missing", 
                                timeoutDateKey ? " (timeout recorded)" : "",
                                (level == -1) ? "" : 
                                    ((level == kIOPMAssertionLevelOn)? " (Level=On)" : " (Level=Off)"));
*/
                      if (asrt && CFEqual(kIOPMAssertionTimeoutActionRelease, usedAction)) {
                          if (!timeoutDateKey)
                          {
                              PMTestFail("CLEANUP: Assertion %d doesn't have TimedOutDateKey for action %s",
                                         k, actionStr);
                          }
                          /* Good. This assertion doesn't exist, and we didn't expect it to.
                           */
                       }
                  }
                          
                  exit(exitStatus);
              });
    
    
    dispatch_resume(timer_source);
    
    dispatch_main();
                           
    return 0;
}
