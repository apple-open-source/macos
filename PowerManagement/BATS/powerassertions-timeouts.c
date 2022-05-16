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
#include "PMtests.h"

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
int gFailCnt = 0, gPassCnt = 0;


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

    START_TEST("Test Assertion timeouts\n");
    LOG("Executing powerassertions-timeouts: schedule and verify timeouts & their effects.\n");
    LOG("Performing %d assert & wait for timeout cycles across all TimeoutActions.\n", DO_ITERATIONS);
    LOG("Assertions will timeout after %0.02fs\n", kMyTimeoutInterval);
    LOG("Test tool will check for timeouts after %0.02fs\n", (CFTimeInterval)kMyWaitForJudgement);


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

    START_TEST_CASE("Register for assertion change notifications\n");

    ret = IOPMAssertionNotify(kIOPMAssertionsAnyChangedNotifyString, kIOPMNotifyRegister);
    if (kIOReturnSuccess != ret) {
        LOG("anychange error 0x%08x\n",ret);
    }
    ret = IOPMAssertionNotify(kIOPMAssertionsChangedNotifyString, kIOPMNotifyRegister);
    if (kIOReturnSuccess != ret) {
        LOG("changed error 0x%08x\n",ret);
    }
    ret = IOPMAssertionNotify(kIOPMAssertionTimedOutNotifyString, kIOPMNotifyRegister);
    if (kIOReturnSuccess != ret) {
        LOG("timedout error 0x%08x\n",ret);
    }


    status = notify_register_dispatch(kIOPMAssertionTimedOutNotifyString,
                                      &notifyxx,
                                      handlerq,
                                      ^(int x){ result.timeouts++; });
    if (status != NOTIFY_STATUS_OK) {
        FAIL("%s:%d Notify status %d on XXX\n", __FILE__, __LINE__, status);
        return;
    }
    status = notify_register_dispatch(kIOPMAssertionsAnyChangedNotifyString,
                                      &notifyxx,
                                      handlerq,
                                      ^(int x){ result.any++; });
    if (status != NOTIFY_STATUS_OK) {
        FAIL("Notify status %d on YYY\n", status);
        return;
    }

    status = notify_register_dispatch(kIOPMAssertionsChangedNotifyString,
                                      &notifyxx,
                                      handlerq,
                                      ^(int x) { result.systemwide++; });
    if (status != NOTIFY_STATUS_OK) {
        FAIL("Notify status %d on ZZZ\n", status);
        return;
    }
    PASS("Register for assertion change notifications\n");
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
           START_TEST_CASE("Create assertions\n");

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
                       FAIL("CREATION: Error 0x%08x from IOPMAssertionCreateWithDescription(%s)\n", ret,
                       failureString);
                       break;
                   }

                   CFRelease(name);
               }
           }

           PASS("Create assertions\n");
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

      START_TEST_CASE("Check assertion notifications and timeouts\n");
      LOG("Created a total of %d assertions.\n", result.fCount);
      LOG("- AssertionTimeout notify fired %d times\n", result.timeouts);
      LOG("- AssertionOverallChange notify fired %d times\n", result.systemwide);
      LOG("- AssertionAnyChange notify fired %d times\n", result.any);

      if (result.timeouts == 0) {
          FAIL("AssertionTimeout didn't fire at all. Fired=%d; assertions timed out=%d.\n",
          result.timeouts, result.fCount);
          return;
      }

      if (result.any == 0) {
          FAIL("AssertionAnyChange didn't fire at all. fired=%d; assertions that timed out=%d.\n",  result.any, result.fCount);
          return;
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
                  FAIL("The dictionary for assertion %d doesn't contain start and timeout dates.\n", result.followers[k].id);
                  CFShow(asrt);
                  return;
              } else {
                  aliveInterval = CFDateGetTimeIntervalSinceDate(timeoutDateKey, startDateKey);

                  if ((aliveInterval <= (kMyTimeoutInterval - 1.0))
                      || (aliveInterval >= (kMyTimeoutInterval + 1.0)))
                  {
                      FAIL("Assertion %d timed out at an incorrect time: timeout=%0.02f actual=%0.02f\n",
                             result.followers[k].id, kMyTimeoutInterval, aliveInterval);
                      return;
                  }
              }

              CFNumberRef lev = CFDictionaryGetValue(asrt, kIOPMAssertionLevelKey);
              if (lev)
                  CFNumberGetValue(lev, kCFNumberIntType, &level);
          }
          if (asrt && CFEqual(kIOPMAssertionTimeoutActionRelease, usedAction)) {
              if (!timeoutDateKey)
              {
                  FAIL("CLEANUP: Assertion %d doesn't have TimedOutDateKey for action %s\n",
                         k, actionStr);
                  return;
                  }
              }
          }

      PASS("Check assertion notifications and timeouts\n");
      SUMMARY("Test Assertion timeouts");
      exit(exitStatus);
      });


    dispatch_resume(timer_source);

}
