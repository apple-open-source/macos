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

#include <CoreFoundation/CoreFoundation.h>
#include <SystemConfiguration/SCValidation.h>
#include <IOKit/IOMessage.h>
#include <IOKit/pwr_mgt/IOPMLibPrivate.h>
#include <notify.h>
#include <utmpx.h>
#include <sys/stat.h>
#include <err.h>
#include <asl.h>
#include "TTYKeepAwake.h"
#include "PMSettings.h"

#if !TARGET_OS_EMBEDDED


#ifndef DEVMAXPATHSIZE
#define DEVMAXPATHSIZE 	128
#endif

// dev_path_t also in kextmanager_types.h but it needs IOKitLib.h :P
typedef char dev_path_t[DEVMAXPATHSIZE];

// linked list of ttys
typedef struct tl {
    struct tl *next;
    dev_path_t ttydev;
} ttylist;

// Globals
static CFRunLoopSourceRef   s_source = NULL;
static CFMachPortRef        s_cfport = NULL; 
static ttylist              *s_activettys = NULL;
static int                  s_token = -1;
static uint32_t             settingIdleSleepSeconds = 0;
static bool                 settingTTYSPreventSleep = true;

// Protos
static ttylist *addtty(char *ttyname);
static void freettys(void);
static bool check_ttys_active( CFArrayRef *preventers );
static void read_logins(CFMachPortRef port, void *m, CFIndex size, void *info);
static void cleanup_tty_tracking(void);

/* __private_extern__ */
void TTYKeepAwake_prime(void)
{
    CFRunLoopRef            rl;
    mach_port_t             mport;
    int                     rval = -1;
    int                     macherr;

    rl = CFRunLoopGetCurrent();
    if(!rl) goto finish;

    s_cfport = CFMachPortCreate(nil, read_logins, NULL, false);
    if (!s_cfport)  goto finish;    // port refcount: 1
    s_source = CFMachPortCreateRunLoopSource(nil, s_cfport, 0);
    if (!s_source)  goto finish;    // port AND source refcount: 2 (!)

    CFRunLoopAddSource(rl, s_source, kCFRunLoopDefaultMode);

    // NOTIFY_REUSE lets you use your own mach port (vs. creating one)
    mport = CFMachPortGetPort(s_cfport);
    if (mport == MACH_PORT_NULL)  goto finish;
    macherr = notify_register_mach_port(UTMPX_CHANGE_NOTIFICATION, &mport,
            NOTIFY_REUSE, &s_token);
    if (macherr) {
        goto finish;
    }

    // load up current user list
    read_logins(NULL, NULL, 0, NULL);

    // and read initial value for PM settings
    TTYKeepAwakePrefsHaveChanged();

    rval = 0;    // hooray

finish:
    if (rval) {
        cleanup_tty_tracking();    // takes care of s_source, s_token, s_cfport
    }

    return;
}

/* __private_extern__ */
void TTYKeepAwakePrefsHaveChanged( void )
{
    CFDictionaryRef     activePMSettings;
    CFNumberRef         systemIdleNum;
    CFNumberRef         ttysPreventNum;
    
    /* Re-read the idle sleep timer if settings change */
    
    activePMSettings = PMSettings_CopyActivePMSettings();
    if( activePMSettings ) 
    {
        systemIdleNum = isA_CFNumber(CFDictionaryGetValue( 
                                                activePMSettings,
                                                CFSTR(kIOPMSystemSleepKey)));
        if( systemIdleNum ) {
            // read idle sleep minutes from settings dictionary
            CFNumberGetValue(systemIdleNum, kCFNumberIntType, &settingIdleSleepSeconds);
            // and minutes to seconds
            settingIdleSleepSeconds *= 60;
        }
        
        ttysPreventNum = isA_CFNumber(CFDictionaryGetValue( 
                                                activePMSettings,
                                                CFSTR(kIOPMTTYSPreventSleepKey)));
        if( ttysPreventNum ) {
            // read idle sleep minutes from settings dictionary
            CFNumberGetValue(ttysPreventNum, kCFNumberIntType, &settingTTYSPreventSleep);
        }
        CFRelease( activePMSettings );
    }
}

/* __private_extern__  */
bool TTYKeepAwakeSleepWakeNotification ( natural_t messageType )
{
    CFArrayRef      preventers = NULL;
    CFStringRef     preventer_name;
    
    if( (0 == settingIdleSleepSeconds)
        || !settingTTYSPreventSleep )
    {
        /* TTYSPreventSleep is turned off
         *  * or *
         * unitialized setting for idle sleep timer. Idle sleep timer should never
         * be set to zero at this point, since an idle sleep is in progress here.
         */
        return false;
    }
    
    if( kIOMessageCanSystemSleep == messageType )
    {
        bool ssh_sessions_active = check_ttys_active( &preventers );

        /* diagnostic code only 
         * logs tty sleep prevention to asl log
         */
        if(preventers)
        {
            asl_log(NULL, NULL, ASL_LEVEL_INFO, 
                "PMCFGD: System Sleep prevented by active remote login session (%d second threshold).\n", settingIdleSleepSeconds);

            int i, count;
            count = CFArrayGetCount(preventers);
            for(i=0; i<count; i++) {
                if( (preventer_name = 
                        isA_CFString(CFArrayGetValueAtIndex(preventers, i))) )
                {
                    asl_log(NULL, NULL, ASL_LEVEL_INFO, 
                                        "PMCFGD: tty sleep preventer: %s\n",
                                        CFStringGetCStringPtr( preventer_name,
                                                        kCFStringEncodingMacRoman));
                }
            }
            CFRelease( preventers );
        }
        
        /* Block idle sleeps if we have non-idle open ssh connections 
         * return true -> prevent sleep
         * return false -> allow sleep
         */
        if ( ssh_sessions_active )
        {        
            return true;
        }
    }

    return false;
}

static ttylist *addtty(char *ttyname)
{
    ttylist *new = malloc(sizeof(ttylist));

    if (new) {
        if (strlcpy(new->ttydev, "/dev/", DEVMAXPATHSIZE) > DEVMAXPATHSIZE ||
                strlcat(new->ttydev, ttyname, DEVMAXPATHSIZE) > DEVMAXPATHSIZE)
            return NULL;
        new->next = s_activettys;
        s_activettys = new;
    }

    return new;
}

static void freettys()
{
    ttylist *next, *delp = s_activettys;

    while(delp) {
        next = delp->next;
        free(delp);
        delp = next;
    }

    s_activettys = NULL;    // empty list (terminates next list)
}

/*
 * check_ttys_active
 *
 * Returns false if the tty's are idle and should not prevent sleep.
 * Returns true if the tty's are active and should prevent sleep.
 */
bool check_ttys_active( CFArrayRef *preventers )
{
    CFMutableArrayRef       preventArray = NULL;
    CFStringRef             preventingTTY = NULL;
    
    bool rval = false;
    ttylist *tty = s_activettys;
    struct stat sb;
    time_t curtime;

    if( preventers ) {
        *preventers = NULL;
    }

    // fetch the current time (no sleep blocking if we can't)
    if ((curtime = time(NULL)) == (time_t)-1) {
        goto finish;
    }

    // note: remember to lock here if other threads can see the list
    while(tty) 
    {
        if (stat(tty->ttydev, &sb) == 0) 
        {   
            time_t current_idle_secs;

            current_idle_secs = curtime - sb.st_atime;
            if (current_idle_secs < settingIdleSleepSeconds) 
            {
                /* tty's are active. prevent sleep. */
                rval = true;
                
                /* Log the name of the preventing TTY 
                 * We build a CFArray of active TTYs 
                 */
                preventingTTY = CFStringCreateWithCString( kCFStringEncodingMacRoman,
                                    tty->ttydev, DEVMAXPATHSIZE);
                if( preventingTTY )
                {
                    if( !preventArray ) {
                        preventArray = CFArrayCreateMutable( 0, 0, &kCFTypeArrayCallBacks ); 
                    }
                    if( preventArray ) {
                        CFArrayAppendValue( preventArray, preventingTTY );
                    }
                    CFRelease(preventingTTY);
                    preventingTTY = NULL;
                }
            }
        } 
        tty = tty->next;
    }

finish:
    if( preventArray ) {
        if( preventers ) {
            /* caller to release */
            *preventers = preventArray;
        } else {
            /* we release */
            CFRelease(preventArray);
        }
    }
    
    return rval;
}

static void read_logins(
    CFMachPortRef port __unused, 
    void *m __unused, 
    CFIndex size __unused, 
    void *info __unused)
{
    int result = 1;
    struct utmpx *ent;

    freettys();        // no-op if list is empty

    setutxent();    // reset
    while((ent = getutxent())) 
    {
        // OS X only seems to use USER_PROCESS, but LOGIN_PROCESS seems valid
        if ( ent->ut_type == USER_PROCESS 
          || ent->ut_type == LOGIN_PROCESS) {
            addtty(ent->ut_line);
        }
    }

    result = 0;

    endutxent();
}


static void cleanup_tty_tracking()
{
    CFRunLoopRef rl = CFRunLoopGetCurrent();
    if(!rl) return;
    
    if (s_token != -1) {
        notify_cancel(s_token);
        s_token = -1;
    }

    if (s_source) {
        CFRunLoopRemoveSource(rl, s_source, kCFRunLoopDefaultMode);
        CFRelease(s_source);
        s_source = NULL;
    }

    if (s_cfport) {
        CFMachPortInvalidate(s_cfport);         // refcount -> 1;
        CFRelease(s_cfport);                    // refcount -> 0;
        s_cfport = NULL;
    }

    freettys();        // no-op on empty list; clears list
}

#endif /* !TARGET_OS_EMBEDDED */
