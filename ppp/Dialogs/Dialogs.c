/*
 * Copyright (c) 2000 Apple Computer, Inc. All rights reserved.
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

/* -----------------------------------------------------------------------------
 *
 *  Theory of operation :
 *
 *  plugin to add a generic socket support to pppd, instead of tty.
 *
----------------------------------------------------------------------------- */


/* -----------------------------------------------------------------------------
  Includes
----------------------------------------------------------------------------- */

#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <fcntl.h>
#include <syslog.h>
#include <netdb.h>
#include <utmp.h>
#include <pwd.h>
#include <setjmp.h>
#include <sys/param.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/ioctl.h>
#include <pthread.h>

#include <net/if.h>
#include <CoreFoundation/CFBundle.h>
#include <CoreFoundation/CFUserNotification.h>

#define APPLE 1

#include "../Family/ppp_defs.h"
#include "../Family/if_ppp.h"
#include "../Family/ppp_domain.h"
#include "../Helpers/pppd/pppd.h"
#include "../Helpers/pppd/fsm.h"
#include "../Helpers/pppd/lcp.h"


/* -----------------------------------------------------------------------------
 Definitions
----------------------------------------------------------------------------- */

#define ICON 	"NetworkConnect.icns"

#define DIALOG_PASSWORD_ASK		0
#define DIALOG_PASSWORD_RETRY	1
#define DIALOG_PASSWORD_CHANGE	2

/* -----------------------------------------------------------------------------
 Forward declarations
----------------------------------------------------------------------------- */

#ifdef UNUSED
static int dialog_idle(struct ppp_idle *idle);
#endif
static int dialog_start_link();
static int dialog_change_password();
static int dialog_retry_password();
static int dialog_link_up();
static int dialog_ask(CFStringRef message, CFStringRef ok, CFStringRef cancel, int timeout);
static void dialog_reminder(void *arg);
static void dialog_phasechange(void *arg, int p);
static void dialog_change_reminder();

/* -----------------------------------------------------------------------------
 PPP globals
----------------------------------------------------------------------------- */


static CFBundleRef 	bundle = 0;		/* our bundle ref */
static CFURLRef		bundleURL = 0;
static pthread_t   	reminderthread = 0;
static int	   	reminderresult = 0;

/* option variables */
static bool 	askpasswordafter = 0;	/* Ask password after physical connection is established */
static bool 	noaskpassword = 0;	/* Don't ask for a password before to connect */
static bool 	noidleprompt = 0;	/* Don't ask user before to disconnect on idle */
static int 	reminder = 0;		/* Ask user to stay connected after reminder period */


/* option descriptors */
option_t dialogs_options[] = {
    { "noaskpassword", o_bool, &noaskpassword,
      "Don't ask for a password at all", 1 },
    { "askpasswordafter", o_bool, &askpasswordafter,
      "Don't ask for a password before to connect", 1 },
    { "noidleprompt", o_bool, &noidleprompt,
      "Don't ask user before to disconnect on idle", 1 },
    { "reminder", o_int, &reminder,
      "Ask user to stay connected after reminder period", 0, 0, 0, 0xFFFFFFFF, 0, 0, 0, dialog_change_reminder },
    { NULL }
};

/* -----------------------------------------------------------------------------
plugin entry point, called by pppd
----------------------------------------------------------------------------- */
int start(CFBundleRef ref)
{

    bundle = ref;
    bundleURL = CFBundleCopyBundleURL(bundle);
    if (!bundleURL)
        return 1;
        
    CFRetain(bundle);

    add_options(dialogs_options);
    
    add_notifier(&phasechange, dialog_phasechange, 0);

    start_link_hook = dialog_start_link;
    change_password_hook = dialog_change_password;
    retry_password_hook = dialog_retry_password;
    link_up_hook = dialog_link_up;
    //idle_time_hook = dialog_idle;
        
    return 0;
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
void dialog_phasechange(void *arg, int p)
{
    if (reminder && p == PHASE_RUNNING) 
        TIMEOUT(dialog_reminder, 0, reminder);
    else 
        UNTIMEOUT(dialog_reminder, 0);
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
void dialog_change_reminder()
{
    UNTIMEOUT(dialog_reminder, 0);
    if (reminder && phase == PHASE_RUNNING) 
        TIMEOUT(dialog_reminder, 0, reminder);
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
void *dialog_reminder_thread(void *arg)
{
    int status, timeout;

    status = pthread_detach(pthread_self());
    if (status == noErr) {
        
        // use an adaptative timeout
        if (reminder < (60 * 5)) timeout = 30;
        else if (reminder < (60 * 10)) timeout = 60;
        else if (reminder < (60 * 20)) timeout = 120;
        else if (reminder < (60 * 30)) timeout = 180;
        else timeout = 240;
        
        reminderresult = dialog_ask(CFSTR("Reminder timeout"), CFSTR("Stay connected"), CFSTR("Disconnect"), timeout);
    }
    return 0;
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
void dialog_reminder_watch(void *arg)
{
    int tlim;

    switch (reminderresult) {
        case -1: 
            // rearm reminder watch dog every 2 seconds
            TIMEOUT(dialog_reminder_watch, 0, 2);
            break;
        case 0:
            // user click stay connected
            TIMEOUT(dialog_reminder, 0, reminder);
            // reset the idle timer
            UNTIMEOUT(check_idle, NULL);
            if (idle_time_hook != 0)
                tlim = (*idle_time_hook)(NULL);
            else
                tlim = idle_time_limit;
            if (tlim > 0)
                TIMEOUT(check_idle, NULL, tlim);
            break;
        default :
            // user clicked Disconnect or timeout expired
            lcp_close(0, "User request");
            status = EXIT_USER_REQUEST;
    }
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
void dialog_reminder(void *arg)
{
    int status;

    reminderresult = -1;
    status = pthread_create(&reminderthread, NULL, dialog_reminder_thread, NULL);
    if (status == noErr) {
        // install a reminder watch dog every 2 seconds
        TIMEOUT(dialog_reminder_watch, 0, 2);
    }
}        

/* -----------------------------------------------------------------------------
Returns 1 on OK, 0 if cancelled
user and password have max size 256

Note: in this function, the strings related to retry and change password are hardcoded
radar 3942343 & 3968665
They were originally defined as followed :

"Token:" = "Please enter your token:";
"Incorrectly entered token" = "Your token was entered incorrectly.";

"Expired password" = "Your password has expired.";
"Expired token" = "Your PIN has expired.";

"Incorrect password" = "Your name or password was incorrect.";
"Incorrect token" = "Your name or token was incorrect.";

"New password:" = "Please enter your new password:";
"Confirm new password:" = "Confirm your new password:";
"New token:" = "Please enter your new PIN:";
"Confirm new token:" = "Confirm your new PIN:";

"Retry name:" = "Please enter your name:";
"Retry password:" = "Please enter your password:";
"Retry token:" = "Please enter your token:";
----------------------------------------------------------------------------- */
int dialog_password(char *user, int maxuserlen, char *passwd, int maxpasswdlen, int dialog_type, char *message)
{
    CFUserNotificationRef 	alert;
    CFOptionFlags 		flags;
    CFMutableDictionaryRef 	dict;
    SInt32 			err;
    CFMutableArrayRef 		array;
    CFURLRef			url;
    CFStringRef			str, str1;
    int				ret = 0, loop = 0;    

    do {
    ret = 0;
    dict = CFDictionaryCreateMutable(NULL, 0, &kCFCopyStringDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    if (dict) {

        url = CFBundleCopyResourceURL(bundle, CFSTR(ICON), NULL, NULL);
        if (url) {
            CFDictionaryAddValue(dict, kCFUserNotificationIconURLKey, url);
            CFRelease(url);
        }
		
		/* if there is a message, set it first, so it is not overriden by other text */
		if (message) {
			if (str = CFStringCreateWithCString(NULL, message, kCFStringEncodingUTF8)) {
				CFDictionaryAddValue(dict, kCFUserNotificationAlertMessageKey, str);
				CFRelease(str);
			}			
		}

        if (dialog_type == DIALOG_PASSWORD_CHANGE)
            CFDictionaryAddValue(dict, kCFUserNotificationAlertMessageKey, (tokencard == 1) ? CFSTR("Your PIN has expired") : CFSTR("Your password has expired."));
            // HARDCODED STRING CFDictionaryAddValue(dict, kCFUserNotificationAlertMessageKey, (tokencard == 1) ? CFSTR("Expired token") : CFSTR("Expired password"));
		else if (dialog_type == DIALOG_PASSWORD_RETRY)
            CFDictionaryAddValue(dict, kCFUserNotificationAlertMessageKey, (tokencard == 1) ? CFSTR("Your name or token was incorrect.") : CFSTR("Your name or password was incorrect."));
            // HARDCODED STRING CFDictionaryAddValue(dict, kCFUserNotificationAlertMessageKey, (tokencard == 1) ? CFSTR("Incorrect token") : CFSTR("Incorrect password"));
		
        array = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);  
        if (array) {
			if (dialog_type == DIALOG_PASSWORD_CHANGE) {
				CFArrayAppendValue(array, (tokencard == 1) ? CFSTR("Please enter your new PIN:") : CFSTR("Please enter your new password:"));
				// HARDCODED STRING CFArrayAppendValue(array, (tokencard == 1) ? CFSTR("New token:") : CFSTR("New password:"));
				CFArrayAppendValue(array, (tokencard == 1) ? CFSTR("Confirm your new PIN:") : CFSTR("Confirm your new password:"));
				// HARDCODED STRING CFArrayAppendValue(array, (tokencard == 1) ? CFSTR("Confirm new token:") : CFSTR("Confirm new password:"));
			}
			else if (dialog_type == DIALOG_PASSWORD_RETRY) {
				CFArrayAppendValue(array, CFSTR("Please enter your name:"));
				// HARDCODED STRING CFArrayAppendValue(array, CFSTR("Retry name:"));
				CFArrayAppendValue(array, (tokencard == 1) ? CFSTR("Please enter your token:") : CFSTR("Please enter your password:"));
				// HARDCODED STRING CFArrayAppendValue(array, (tokencard == 1) ? CFSTR("Retry token:") : CFSTR("Retry password:"));
			}
			else 
				CFArrayAppendValue(array, (tokencard == 1) ? CFSTR("Please enter your token:") : CFSTR("Password:"));
				// HARDCODED STRING CFArrayAppendValue(array, (tokencard == 1) ? CFSTR("Token:") : CFSTR("Password:"));
            CFDictionaryAddValue(dict, kCFUserNotificationTextFieldTitlesKey, array);
            CFRelease(array);
        }

        array = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);  
        if (array) {
			if (dialog_type == DIALOG_PASSWORD_RETRY) {
				if (str = CFStringCreateWithCString(NULL, user, kCFStringEncodingUTF8)) {
					CFArrayAppendValue(array, str);
					CFRelease(str);
				}			
			}
            CFDictionaryAddValue(dict, kCFUserNotificationTextFieldValuesKey, array);
            CFRelease(array);
        }

       CFDictionaryAddValue(dict, kCFUserNotificationLocalizationURLKey, bundleURL);
        CFDictionaryAddValue(dict, kCFUserNotificationAlertHeaderKey, CFSTR("Internet Connect"));
        if (loop)
            CFDictionaryAddValue(dict, kCFUserNotificationAlertMessageKey, (tokencard == 1) ? CFSTR("Your token was entered incorrectly") : CFSTR("Incorrectly entered password"));
            // HARDCODED STRING CFDictionaryAddValue(dict, kCFUserNotificationAlertMessageKey, (tokencard == 1) ? CFSTR("Incorrectly entered token") : CFSTR("Incorrectly entered password"));
        //CFDictionaryAddValue(dict, kCFUserNotificationAlertMessageKey, CFSTR("Enter password"));
        CFDictionaryAddValue(dict, kCFUserNotificationAlternateButtonTitleKey, CFSTR("Cancel"));
        
        alert = CFUserNotificationCreate(NULL, 0, 
			(dialog_type != DIALOG_PASSWORD_RETRY ? CFUserNotificationSecureTextField(0) : 0)
			+ ((dialog_type != DIALOG_PASSWORD_ASK) ? CFUserNotificationSecureTextField(1) : 0), &err, dict);
        if (alert) {
            CFUserNotificationReceiveResponse(alert, 0, &flags);
            // the 2 lower bits of the response flags will give the button pressed
            // 0 --> default
            // 1 --> alternate
            if ((flags & 3) == 1) {
                // user cancelled
            }
            else { 
                // user clicked OK
                ret = 1;
                str = CFUserNotificationGetResponseValue(alert, kCFUserNotificationTextFieldValuesKey, 0);
				
				if (dialog_type == DIALOG_PASSWORD_CHANGE) {
					str1 = CFUserNotificationGetResponseValue(alert, kCFUserNotificationTextFieldValuesKey, 1);
					if (!(str && str1 && CFStringCompare(str, str1, 0) == kCFCompareEqualTo))
						ret = -1;
				}
				
				if (dialog_type == DIALOG_PASSWORD_RETRY) {
					str1 = CFUserNotificationGetResponseValue(alert, kCFUserNotificationTextFieldValuesKey, 1);
					if (str && str1) {
						if (CFStringGetCString(str, user, maxuserlen, kCFStringEncodingUTF8) == FALSE)
							ret = -1;
						if (CFStringGetCString(str1, passwd, maxpasswdlen, kCFStringEncodingUTF8) == FALSE)
							ret = -1;
					}
				}
				else {
					if (ret > 0 && str) 
						if (CFStringGetCString(str, passwd, maxpasswdlen, kCFStringEncodingUTF8) == FALSE)
							ret = -1;
				}
           }

            CFRelease(alert);
        }
        
        CFRelease(dict);
    }
    loop++;
    } while (ret < 0);
    return ret;
}

/* -----------------------------------------------------------------------------
Returns 1 if continue, 0 if cancel.
----------------------------------------------------------------------------- */
int dialog_start_link()
{

    if (noaskpassword || *passwd || askpasswordafter) 
        return 1;
   
    return dialog_password(user, MAXNAMELEN, passwd, MAXSECRETLEN, DIALOG_PASSWORD_ASK, 0);
}

/* -----------------------------------------------------------------------------
msg can come from the server
Returns 1 if continue, 0 if cancel.
----------------------------------------------------------------------------- */
int dialog_change_password(char *msg)
{
   // if (noaskpassword) 
   //     return 1;
   
    return dialog_password(user, MAXNAMELEN, new_passwd, MAXSECRETLEN, DIALOG_PASSWORD_CHANGE, msg);
}

/* -----------------------------------------------------------------------------
msg can come from the server
Returns 1 if continue, 0 if cancel.
----------------------------------------------------------------------------- */
int dialog_retry_password(char *msg)
{
   // if (noaskpassword) 
   //     return 1;
   
    return dialog_password(user, MAXNAMELEN, passwd, MAXSECRETLEN, DIALOG_PASSWORD_RETRY, msg);
}

/* -----------------------------------------------------------------------------
Returns 1 if continue, 0 if cancel.
----------------------------------------------------------------------------- */
int dialog_link_up()
{

    if (noaskpassword || *passwd || !askpasswordafter) 
        return 1;
   
    return dialog_password(user, MAXNAMELEN, passwd, MAXSECRETLEN, DIALOG_PASSWORD_ASK, 0);
}

#ifdef UNUSED
/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
int dialog_idle(struct ppp_idle *idle)
{
    int  	itime = 0;
         
    // called at init time
    if (idle == 0)
        return idle_time_limit;
    
    itime = MIN(idle->xmit_idle, idle->recv_idle);
    if ((idle_time_limit - itime) <= 0) {

	if (noidleprompt || dialog_ask(CFSTR("Idle timeout"), CFSTR("Stay connected"), CFSTR("Disconnect"), 30)) {
            // user clicked Disconnect
            return 0;
        }
        
        // user clicked Stay Connected
        return idle_time_limit;
    }

    // will rearm the timer 
    return idle_time_limit - itime;
}
#endif

/* -----------------------------------------------------------------------------
return 0 : OK was pressed
return 1 : Cancel was pressed
return 2 : should not happen
return 3 : nothing was pressed, timeout expired
----------------------------------------------------------------------------- */
int dialog_ask(CFStringRef message, CFStringRef ok, CFStringRef cancel, int timeout)
{
    CFUserNotificationRef 	alert;
    CFOptionFlags 		flags;
    CFMutableDictionaryRef 	dict;
    SInt32 			error;
    CFURLRef			url;
        
    flags = 3; // nothing has been pressed
    dict = CFDictionaryCreateMutable(NULL, 0, 
        &kCFCopyStringDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    if (dict) {
    
        url = CFBundleCopyResourceURL(bundle, CFSTR(ICON), NULL, NULL);
        if (url) {
            CFDictionaryAddValue(dict, kCFUserNotificationIconURLKey, url);
            CFRelease(url);
        }
    
        CFDictionaryAddValue(dict, kCFUserNotificationLocalizationURLKey, bundleURL);
        CFDictionaryAddValue(dict, kCFUserNotificationAlertHeaderKey, CFSTR("Internet Connect"));
        CFDictionaryAddValue(dict, kCFUserNotificationAlertMessageKey, message);
        if (ok) CFDictionaryAddValue(dict, kCFUserNotificationDefaultButtonTitleKey, ok);
        if (cancel) CFDictionaryAddValue(dict, kCFUserNotificationAlternateButtonTitleKey, cancel);
        alert = CFUserNotificationCreate(NULL, timeout, kCFUserNotificationCautionAlertLevel, &error, dict);
        if (alert) {
            CFUserNotificationReceiveResponse(alert, timeout, &flags);
            CFRelease(alert);
        }
        CFRelease(dict);
    }

    // the 2 lower bits of the response flags will give the button pressed
    // 0 --> default
    // 1 --> alternate
    // 2 --> other
    // 3 --> none of them, timeout expired
    return (flags & 3);
}
