#include <darwintest.h>
#include <darwintest_utils.h>
#include <unistd.h>
#include <stdlib.h>
#include <dispatch/dispatch.h>
#include <IOKit/pwr_mgt/IOPMLibPrivate.h>
#include <IOKit/pwr_mgt/IOPMLib.h>
#include <CoreFoundation/CoreFoundation.h>
bool gUserActiveState = false;
IOPMAssertionID gAssertionID;
IOPMAssertionID gDisplayAssertionID;
dispatch_queue_t notify_queue;

void setupSleepSettings() {
   // disable idle sleep
   char *cmd_to_run = "/usr/bin/pmset sleep 0";
   int s_err = system(cmd_to_run);
   if (s_err) {
    T_FAIL("failed to disable idle sleep");
   } else {
    T_LOG("disabled idle sleep");
   }
   cmd_to_run = "/usr/bin/pmset dwlinterval 0";
   s_err = system(cmd_to_run);
   if (s_err) {
    T_FAIL("failed to set dwlinterval to 0");
   } else {
    T_LOG("set dwlinterval to 0");
   }
}

void restoreDefaults() {
    char *cmd_to_run = "/usr/bin/pmset restoredefaults";
    int s_err = system(cmd_to_run);
    if (s_err) {
        T_FAIL("failed to restore default settings");
    } else {
        T_LOG("restored default settings");
    }

}

void registerForUserActive() {
    notify_queue = dispatch_queue_create("iopmuserisactive-notification", DISPATCH_QUEUE_SERIAL);
   IOPMNotificationHandle hdl = IOPMScheduleUserActiveChangedNotification(notify_queue,
                                        ^(bool isActive) {
                                            if (isActive) {
                                                T_LOG("IOPMUserIsActive: True");
                                                gUserActiveState = true;
                                            } else {
                                                T_LOG("IOPMUserIsActive: False");
                                                gUserActiveState = false;
                                            }
                                        });
}

void turnOnDisplay() {
    IOReturn kr = IOPMAssertionDeclareUserActivity(CFSTR("com.apple.iopmuserisactive.test"), kIOPMUserActiveLocal, &gAssertionID);
    if (kr != kIOReturnSuccess) {
        T_FAIL("Failed to create assertion to light up display");
    }

}

bool turnOffDisplay() {
    T_LOG("Turning off display");
    char *cmd_to_run = "/usr/bin/pmset displaysleepnow";
    int s_err = system(cmd_to_run);
    if (s_err) {
        T_FAIL("Failed to turn off display");
        return false;
    } else {
        T_LOG("Turned off display");
        return true;
    }
}
bool createAssertion() {
    T_LOG("Taking prevent display sleep assertion");
    CFMutableDictionaryRef properties = CFDictionaryCreateMutable(0, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    CFDictionarySetValue(properties, kIOPMAssertionTypeKey, kIOPMAssertionTypePreventUserIdleDisplaySleep);
    CFDictionarySetValue(properties, kIOPMAssertionNameKey, CFSTR("com.apple.iopmuserisactive.test"));
    IOReturn err = IOPMAssertionCreateWithProperties(properties, &gDisplayAssertionID);
    if (err != kIOReturnSuccess) {
        T_FAIL("Failed to create assertion with lid modifier 0x%x", err);
    }
    CFRelease(properties);

}
T_DECL(iopmuserisactive_test, "test iopmuserisactive") {
    T_SETUPBEGIN;

    setupSleepSettings();

    registerForUserActive();

    T_SETUPEND;

    // wait before starting 
    sleep(5);
    // first case: display off 
    T_LOG("Turning off display");
    char *cmd_to_run = "/usr/bin/pmset displaysleepnow";
    int s_err = system(cmd_to_run);
    if (s_err) {
        T_FAIL("Failed to turn off display");
    } else {
        T_LOG("Turned off display");
        // wait for useractive state change
        sleep(5);
        if (gUserActiveState) {
            T_FAIL("Expected gUserActiveState: false. Actual: %d", gUserActiveState);
        } else {
            T_PASS("gUserIsActiveState is false");
        }
    }
    // turn on display
    turnOnDisplay();
    // wait for display on
    sleep(5);
    if (!gUserActiveState) {
        T_FAIL("Expected gUserActiveState: true. Actual: %d", gUserActiveState);
    } else {
        T_PASS("gUserIsActiveState is true");
    }
    /* turn off display
     * create DisplayWake assertion. UserIsActive state should still be false
     */
    if (turnOffDisplay()) {
        // wait for useractive to change
        sleep(5);
        if (gUserActiveState) {
            T_FAIL("Expected gUserActiveState: false. Actual: %d", gUserActiveState);
        }
    }

    // display wake assertion
    IOReturn kr = IOPMAssertionDeclareNotificationEvent(CFSTR("com.apple.iopmuseractive-notification.test"), 5, &gAssertionID);
    if (kr == kIOReturnSuccess) {
        T_LOG("Created assertion for notification wake");
    } else {
        T_FAIL("Failed to create assertion for notification wake");
    }
    sleep(5);
    if (gUserActiveState) {
        T_FAIL("Expected gUserActiveState: false. Actual: %d", gUserActiveState);
    } else {
        T_PASS("gUserActiveState is false on notification wake");
    }
    sleep(5);

    T_LOG("Turning on display");
    // turn on display
    turnOnDisplay();
    sleep(5);
    if (gUserActiveState) {
        T_PASS("Expected gUserActive: true");
    } else {
        T_FAIL("Expected gUserActive: true, actual %d", gUserActiveState);
    }

    T_LOG("Notification assertion on display on");
    // notification assertion while display is on
    kr = IOPMAssertionDeclareNotificationEvent(CFSTR("com.apple.iopmuseractive-notification.test"), 5, &gAssertionID);
    if (kr == kIOReturnSuccess) {
        T_LOG("Created assertion for notification wake");
    } else {
        T_FAIL("Failed to create assertion for notification wake");
    }
    sleep(5);
    if (!gUserActiveState) {
        T_FAIL("Expected gUserActiveState: true. Actual: %d", gUserActiveState);
    } else {
        T_PASS("gUserActiveState is true on notification with display on");
    }

    // Test for prevent display sleep assertion, followed by display sleep
    // and notification wake

    createAssertion();
    sleep(5);
    if (turnOffDisplay()) {
        sleep(5);
        if (gUserActiveState) {
            T_FAIL("Expected gUserActiveState: false. Actual: %d", gUserActiveState);
        }
    }
 
    // display wake assertion
    kr = IOPMAssertionDeclareNotificationEvent(CFSTR("com.apple.iopmuseractive-notification.test"), 5, &gAssertionID);
    if (kr == kIOReturnSuccess) {
        T_LOG("Created assertion for notification wake");
    } else {
        T_FAIL("Failed to create assertion for notification wake");
    }
    sleep(5);
    if (gUserActiveState) {
        T_FAIL("Expected gUserActiveState: false. Actual: %d", gUserActiveState);
    } else {
        T_PASS("gUserActiveState is false on notification wake");
    }
    sleep(5);

    T_LOG("Turning on display");
    // turn on display
    turnOnDisplay();
}
