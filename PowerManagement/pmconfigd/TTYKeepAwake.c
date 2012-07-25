/*
 * Copyright (c) 2009-2010 Apple Inc. All rights reserved.
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
 * Portions Copyright (c) 1999-2007 Apple Computer, Inc. All Rights
 * Reserved.

 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.

 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 */

/*******************************************************************************
 *
 * TTYKeepAwake - active ssh and telnet sessions shall prevent idle sleep.
 *
 ******************************************************************************/

#include <notify.h>
#include <utmpx.h>
#include <sys/stat.h>
#include <err.h>
#include <asl.h>
#include <sys/queue.h>
#include <sys/param.h>
#include "PrivateLib.h"
#include "TTYKeepAwake.h"
#include "PMAssertions.h"
#include "PMSettings.h"
#include "PMConnection.h"

#if !TARGET_OS_EMBEDDED

#ifndef DEVMAXPATHSIZE
#define DEVMAXPATHSIZE 	128
#endif

// dev_path_t also in kextmanager_types.h but it needs IOKitLib.h
typedef char dev_path_t[DEVMAXPATHSIZE];

SLIST_HEAD(ttyhead, ttyentry);
struct ttyentry {
    dev_path_t ttydev;
    SLIST_ENTRY(ttyentry) next;
};

static char                     s_activetty_names[DEVMAXPATHSIZE * 4];

#define kMinIdleCheckTime 10
static CFStringRef kTTYAssertion = CFSTR("com.apple.powermanagement.ttyassertion");

// Globals protected by s_tty_queue
static struct ttyhead           s_activettys = SLIST_HEAD_INITIALIZER(s_activettys);
static time_t                   settingIdleSleepSeconds = 0;
static bool                     settingTTYSPreventSleep = true;
static IOPMAssertionID          s_assertion = 0;
static int                      s_utmpx_notify_token = -1;
static dispatch_source_t        s_timer_source;
static dispatch_queue_t         s_tty_queue;

// Protos
static void freettys(void);
static void addtty(char *ttyname);
static void read_logins(void);
static boolean_t ttys_are_active(time_t *time_to_idle_out);
static void create_assertion(void);
static void release_assertion(void);
static void rearm_timer(time_t time_to_idle);
static void pause_timer(void);
static void cleanup_tty_tracking(void);

/* __private_extern__ */
void TTYKeepAwake_prime(void)
{
    dispatch_queue_t    q_default;
    uint32_t            status;
    int                 result = -1;

    s_tty_queue = dispatch_queue_create("com.apple.powermanagement.tty", 0);
    if (!s_tty_queue) {
        goto finish;
    }

    q_default = dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0);
    if (!q_default) {
        goto finish;
    }

    status = notify_register_dispatch(UTMPX_CHANGE_NOTIFICATION,
        &s_utmpx_notify_token, q_default, ^(int t){ read_logins(); });
    if (status != NOTIFY_STATUS_OK) {
        result = -1;
        goto finish;
    }

    s_timer_source = dispatch_source_create(DISPATCH_SOURCE_TYPE_TIMER, 0, 0,
        q_default);
    if (!s_timer_source) {
        result = -1;
        goto finish;
    }
    dispatch_source_set_event_handler(s_timer_source, ^{ 
        TTYKeepAwakeConsiderAssertion();
    });
    dispatch_source_set_timer(s_timer_source, DISPATCH_TIME_FOREVER, 0, 0);
    dispatch_resume(s_timer_source);

    // read initial value for PM settings
    TTYKeepAwakePrefsHaveChanged();

    // load up current user list
    read_logins();

    result = 0;    // hooray
finish:
    if (result) cleanup_tty_tracking();

    return;
}

#define kUseDefaultMinutesWhenSystemSleepIsNever 180
#define SEC_PER_MIN 60UL

/* __private_extern__ */
void TTYKeepAwakePrefsHaveChanged( void )
{
    CFDictionaryRef     activePMSettings;
    CFNumberRef         systemIdleNum;
    CFNumberRef         ttysPreventNum;
    int                 systemIdleMinutes;
    int                 ttysPreventSleep;

    /* Re-read the idle sleep timer if settings change */

    activePMSettings = PMSettings_CopyActivePMSettings();
    if(!activePMSettings) 
        goto finish;

    systemIdleNum = isA_CFNumber(CFDictionaryGetValue(activePMSettings,
        CFSTR(kIOPMSystemSleepKey)));
    if (!systemIdleNum) 
        goto finish;

    CFNumberGetValue(systemIdleNum, kCFNumberIntType, &systemIdleMinutes);

    if (0 == systemIdleMinutes) {
        systemIdleMinutes = kUseDefaultMinutesWhenSystemSleepIsNever;
    }
    
    ttysPreventNum = isA_CFNumber(CFDictionaryGetValue(activePMSettings,
        CFSTR(kIOPMTTYSPreventSleepKey)));
    if(!ttysPreventNum) 
        goto finish;

    CFNumberGetValue(ttysPreventNum, kCFNumberIntType, &ttysPreventSleep);

    dispatch_async(s_tty_queue, ^{
        settingIdleSleepSeconds = systemIdleMinutes * SEC_PER_MIN;
        settingTTYSPreventSleep = ttysPreventSleep;
    });

    TTYKeepAwakeConsiderAssertion();

finish:
    if (activePMSettings) CFRelease(activePMSettings);

    return;
}

/* __private_extern__ */
bool  TTYKeepAwakeConsiderAssertion( void )
{
    boolean_t active;
    bool allow_sleep = true;
    time_t time_to_idle = 0;

    active = ttys_are_active(&time_to_idle);

    if (active && settingTTYSPreventSleep) 
    {
        time_to_idle = MAX(time_to_idle, kMinIdleCheckTime);
        create_assertion();
        rearm_timer(time_to_idle);
        allow_sleep = false;
    } else {
        release_assertion();
        pause_timer();
    }

    return allow_sleep;
}

static void read_logins(void)
{
    struct utmpx *ent;
    int cnt = 0;

    freettys();

    setutxent();
    while((ent = getutxent())) 
    {
        /* Track the ttys that have a string for "ut_host" - presence of
         * that field implies they're remote connected ttys.
         * (We're not interested in tracking local Terminal windows, 
         * just remote sessions.)
         */
        if (0 < strlen(ent->ut_host))
        {
            addtty(ent->ut_line);            
            cnt++;
        }
    }
    endutxent();

    if (cnt) _unclamp_silent_running();
    TTYKeepAwakeConsiderAssertion();
}

static void freettys(void)
{
    dispatch_sync(s_tty_queue, ^{
        struct ttyentry *tty;
        struct ttyentry *tmptty;

        SLIST_FOREACH_SAFE(tty, &s_activettys, next, tmptty) {
            SLIST_REMOVE(&s_activettys, tty, ttyentry, next);
            free(tty);
        }
    });
}

static void addtty(char *ttyname)
{
    struct ttyentry *tty;
    size_t len;

    tty = malloc(sizeof(*tty));
    if (!tty) 
        goto finish;

    len = strlcpy(tty->ttydev, "/dev/", sizeof(tty->ttydev));
    if (len > sizeof(tty->ttydev))
        goto finish;

    len = strlcat(tty->ttydev, ttyname, sizeof(tty->ttydev));
    if (len > sizeof(tty->ttydev)) 
        goto finish;

    dispatch_async(s_tty_queue, ^{
        SLIST_INSERT_HEAD(&s_activettys, tty, next);
    });
    tty = NULL;

finish:
    if (tty) 
        free(tty);
    return;
}

static boolean_t ttys_are_active(time_t *time_to_idle_out)
{	
    time_t curtime;
    __block time_t time_to_idle;
    __block boolean_t active = FALSE;

    *time_to_idle_out = time_to_idle = 0;

    bzero(s_activetty_names, sizeof(s_activetty_names));

    curtime = time(NULL);
    if (curtime == (time_t)-1) {
        goto finish;
    }

    dispatch_sync(s_tty_queue, ^{
        struct ttyentry *tty;
        struct stat sb;
        time_t minimum_idle_secs;
        bool isPrintedFirst = TRUE;

        minimum_idle_secs = settingIdleSleepSeconds;

        SLIST_FOREACH(tty, &s_activettys, next) {
            time_t current_idle_secs;
            int result;
            
            result = stat(tty->ttydev, &sb);
            if (result != 0) 
                continue;

            // Add one second so we aren't racing to check at expiration
            current_idle_secs = curtime - sb.st_atime + 1;
            if (current_idle_secs > settingIdleSleepSeconds) {
                continue;
            }
            minimum_idle_secs = MIN(current_idle_secs, minimum_idle_secs);
            active = TRUE;
            
            if (isPrintedFirst) {
                isPrintedFirst = FALSE;
            } else { // print a pretty comma between tty names
                strlcat(s_activetty_names, ", ", sizeof(s_activetty_names));
            }
            
            // Record the active ttys' device paths in the string s_activetty_names
            strlcat(s_activetty_names, tty->ttydev, sizeof(s_activetty_names));
        }

        time_to_idle = (active) ? (settingIdleSleepSeconds - minimum_idle_secs) : 0;
    });

    *time_to_idle_out = time_to_idle;
finish:
    return active;
}

static void create_assertion()
{
    dispatch_async(s_tty_queue, ^{
        CFMutableDictionaryRef      assertionProperties = NULL;
        CFStringRef                 activeTTYList = NULL;
        int                         i = kIOPMAssertionLevelOn;
        CFNumberRef                 n1 = NULL;


        assertionProperties = CFDictionaryCreateMutable(0, 6, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
        if (!assertionProperties)
            return;
        
        CFDictionarySetValue(assertionProperties, kIOPMAssertionTypeKey, kIOPMAssertionTypePreventUserIdleSystemSleep);
        CFDictionarySetValue(assertionProperties, kIOPMAssertionNameKey, kTTYAssertion);
        CFDictionarySetValue(assertionProperties, kIOPMAssertionHumanReadableReasonKey, kAssertionHumanReadableReasonTTY);
        CFDictionarySetValue(assertionProperties, kIOPMAssertionLocalizationBundlePathKey, kPowerManagementBundlePathString);

        n1 = CFNumberCreate(0, kCFNumberIntType, &i);
        if (n1) {
            CFDictionarySetValue(assertionProperties, kIOPMAssertionLevelKey, n1);
            CFRelease(n1);
        }
        
        activeTTYList = CFStringCreateWithFormat(0, NULL, CFSTR("%s"), s_activetty_names);        
        if (activeTTYList) {
            CFDictionarySetValue(assertionProperties, kIOPMAssertionDetailsKey, activeTTYList);
            CFRelease(activeTTYList);
        }            
            
        InternalCreateAssertion(assertionProperties, &s_assertion);

        CFRelease(assertionProperties);
        
    });
}

static void release_assertion(void)
{
    dispatch_async(s_tty_queue, ^{

        InternalReleaseAssertion(&s_assertion);
    });
}

static void rearm_timer(time_t time_to_idle)
{
    dispatch_async(s_tty_queue, ^{
        dispatch_source_set_timer(s_timer_source,
            dispatch_time(DISPATCH_TIME_NOW, time_to_idle * NSEC_PER_SEC),
            DISPATCH_TIME_FOREVER, 1 * NSEC_PER_SEC);
    });
}

static void pause_timer(void)
{
    dispatch_async(s_tty_queue, ^{
        dispatch_source_set_timer(s_timer_source, DISPATCH_TIME_FOREVER, 0, 0);
    });
}

static void cleanup_tty_tracking(void)
{
    freettys();        // no-op on empty list; clears list

    dispatch_sync(s_tty_queue, ^{
        if (s_utmpx_notify_token != -1) {
            notify_cancel(s_utmpx_notify_token);
            s_utmpx_notify_token = -1;
        }

        if (s_timer_source) {
            dispatch_release(s_timer_source);
            s_timer_source = NULL;
        }
    });

    dispatch_release(s_tty_queue);
    s_tty_queue = NULL;
}

#endif /* !TARGET_OS_EMBEDDED */
