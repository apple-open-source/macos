//
//  main.c
//  powerassertions-timeouts.c
//
//  Created by Ethan Bold on 3/23/14.
//
//


#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/IOReturn.h>
#include <IOKit/pwr_mgt/IOPMLib.h>
#include <IOKit/pwr_mgt/IOPMLibPrivate.h>
#include <stdlib.h>
#include <notify.h>
#include <stdio.h>

/***

 This tool exercises the settimeout machinery in PM configd by creating/scheduling and removing timers.
 The tool exercises both assertion release cleanup, and dead client process.

 This tool detects configd crashes or incorrect return values.

 ***/

static const int DO_ITERATIONS              = 10;
static CFTimeInterval kMyTimeoutInterval    = 2.0;
static CFTimeInterval kMyWaitForJudgement   = 4.0;

CFStringRef SystemTimeoutActions[] = {
    NULL,
    kIOPMAssertionTimeoutActionLog,
    kIOPMAssertionTimeoutActionTurnOff,
    kIOPMAssertionTimeoutActionRelease
};

static int kActionsCount = sizeof(SystemTimeoutActions)/sizeof(CFStringRef);


typedef struct {
    IOPMAssertionID     id;
    CFStringRef         action;
} FollowAssertion;

typedef struct {
    FollowAssertion     *followers;
    int                 fCount;
    int                 timeouts;
    int                 systemwide;
    int                 any;
} Results;

Results result;

static void install_notify_listeners(void);
static void create_assertions(void);
static void inspect_results_delayed(void);

int main(int argc, char *argv[])
{

    printf("Executing powerassertions-timeouts: schedule and verify timeouts & their effects.\n");
    printf("Performing %d assert & wait for timeout cycles across all TimeoutActions.\n", DO_ITERATIONS);
    printf("Assertions will timeout after %0.02fs\n", kMyTimeoutInterval);
    printf("Test tool will check for timeouts after %0.02fs\n", (CFTimeInterval)kMyWaitForJudgement);


    install_notify_listeners();
    create_assertions();
    inspect_results_delayed();

    dispatch_main();
    return 0;
}


static void install_notify_listeners(void)
{
    int notifyxx;
    int status;
    IOReturn ret;

    dispatch_queue_t        handlerq = dispatch_get_main_queue();


    ret = IOPMAssertionNotify(kIOPMAssertionsAnyChangedNotifyString, kIOPMNotifyRegister);
    if (kIOReturnSuccess != ret) {
        printf("anychange error 0x%08x\n",ret);
    }
    ret = IOPMAssertionNotify(kIOPMAssertionsChangedNotifyString, kIOPMNotifyRegister);
    if (kIOReturnSuccess != ret) {
        printf("changed error 0x%08x\n",ret);
    }
    ret = IOPMAssertionNotify(kIOPMAssertionTimedOutNotifyString, kIOPMNotifyRegister);
    if (kIOReturnSuccess != ret) {
        printf("timedout error 0x%08x\n",ret);
    }


    status = notify_register_dispatch(kIOPMAssertionTimedOutNotifyString,
                                      &notifyxx,
                                      handlerq,
                                      ^(int x){ result.timeouts++; });
    if (status != NOTIFY_STATUS_OK) {
        printf("[FAIL] %s:%d Notify status %d on XXX\n", __FILE__, __LINE__, status);
    }
    status = notify_register_dispatch(kIOPMAssertionsAnyChangedNotifyString,
                                      &notifyxx,
                                      handlerq,
                                      ^(int x){ result.any++; });
    if (status != NOTIFY_STATUS_OK) {
        printf("[FAIL] Notify status %d on YYY\n", status);
    }

    status = notify_register_dispatch(kIOPMAssertionsChangedNotifyString,
                                      &notifyxx,
                                      handlerq,
                                      ^(int x) { result.systemwide++; });
    if (status != NOTIFY_STATUS_OK) {
        printf("[FAIL] Notify status %d on ZZZ\n", status);
    }
}


static void create_assertions(void)
{
    result.followers = (FollowAssertion *)calloc(sizeof(FollowAssertion), DO_ITERATIONS * kActionsCount);

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
                   CFStringRef name;
                   name = CFStringCreateWithFormat(0, 0,
                                                   CFSTR("TimeoutTest(%d) %@\n"),
                                                   i,
                                                   SystemTimeoutActions[j] ? SystemTimeoutActions[j] : CFSTR("DefaultAction"));

                   CFStringGetCString(name,
                                      failureString,
                                      sizeof(failureString),
                                      kCFStringEncodingUTF8);

                   result.followers[result.fCount].action = SystemTimeoutActions[j];

                   ret = IOPMAssertionCreateWithDescription(kIOPMAssertNetworkClientActive,
                                                            name,
                                                            NULL, NULL, NULL,
                                                            kMyTimeoutInterval,
                                                            result.followers[result.fCount].action,
                                                            &result.followers[result.fCount].id);
                   ++result.fCount;


                   if(kIOReturnSuccess != ret) {
                       printf("[FAIL] CREATION: Error 0x%08x from IOPMAssertionCreateWithDescription(%s)\n", ret,
                       failureString);
                       break;
                   }

                   CFRelease(name);
               }
           }
       });
}





static void inspect_results_delayed(void)
{

    /* Wait for the assertions to timeout, then verify that they're in the state they should be.
     */

    dispatch_source_t   timer_source = dispatch_source_create(DISPATCH_SOURCE_TYPE_TIMER, 0, 0,
                                                              dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0));

    dispatch_source_set_timer(timer_source, dispatch_time(DISPATCH_TIME_NOW, (uint64_t)kMyWaitForJudgement * NSEC_PER_SEC), DISPATCH_TIME_FOREVER, 0);

    dispatch_source_set_event_handler(timer_source,
  ^{
      int exitStatus = 0;

      printf("Created a total of %d assertions.\n", result.fCount);
      printf("- AssertionTimeout notify fired %d times\n", result.timeouts);
      printf("- AssertionOverallChange notify fired %d times\n", result.systemwide);
      printf("- AssertionAnyChange notify fired %d times\n", result.any);

      if (result.timeouts == 0) {
          printf("[FAIL] AssertionTimeout didn't fire at all. Fired=%d; assertions timed out=%d.\n",
          result.timeouts, result.fCount);
      }

      if (result.any == 0) {
          printf("[FAIL]  AssertionAnyChange didn't fire at all. fired=%d; assertions that timed out=%d.\n",  result.any, result.fCount);
      }



      int k = 0;
      for (k=0; k<result.fCount; k++) {

          CFDictionaryRef   asrt = IOPMAssertionCopyProperties(result.followers[k].id);
          CFStringRef       usedAction = result.followers[k].action
                                            ? result.followers[k].action
                                            : kIOPMAssertionTimeoutActionLog;
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

              if (!(timeoutDateKey && startDateKey))
              {
                  printf("[FAIL] The dictionary for assertion %d doesn't contain start and timeout dates.\n", result.followers[k].id);
                  CFShow(asrt);
              } else {
                  aliveInterval = CFDateGetTimeIntervalSinceDate(timeoutDateKey, startDateKey);

                  if ((aliveInterval <= (kMyTimeoutInterval - 1.0))
                      || (aliveInterval >= (kMyTimeoutInterval + 1.0)))
                  {
                      printf("[FAIL] Assertion %d timed out at an incorrect time: timeout=%0.02f actual=%0.02f\n",
                             result.followers[k].id, kMyTimeoutInterval, aliveInterval);
                  }
              }

              CFNumberRef lev = CFDictionaryGetValue(asrt, kIOPMAssertionLevelKey);
              if (lev)
                  CFNumberGetValue(lev, kCFNumberIntType, &level);
          }
          if (asrt && CFEqual(kIOPMAssertionTimeoutActionRelease, usedAction)) {
              if (!timeoutDateKey)
              {
                  printf("[FAIL] CLEANUP: Assertion %d doesn't have TimedOutDateKey for action %s\n",
                         k, actionStr);
                  }
              }
          }

      printf("[PASS] Created a bunch of assertions that successfully timed out.\n");
      exit(exitStatus);
      });


    dispatch_resume(timer_source);

}
