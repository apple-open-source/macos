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
#if TARGET_OS_EMBEDDED
#include <CoreFoundation/CFNumber.h>
#endif

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
static void dialog_phasechange(void *arg, uintptr_t p);
static void dialog_change_reminder();

/* -----------------------------------------------------------------------------
 PPP globals
----------------------------------------------------------------------------- */

extern int 		kill_link;

static CFBundleRef 	bundle = 0;		/* our bundle ref */
static CFURLRef		bundleURL = 0;
static pthread_t   	reminderthread = 0;
static int	   	reminderresult = 0;

/* option variables */
static bool 	askpasswordafter = 0;	/* Ask password after physical connection is established */
static bool 	noaskpassword = 0;	/* Don't ask for a password before to connect */
static bool 	noidleprompt = 0;	/* Don't ask user before to disconnect on idle */
static int 	reminder = 0;		/* Ask user to stay connected after reminder period */
static int 	dialogtype = 0;		/* 0 = standard ppp, 1 = vpn */

static pthread_t dialog_ui_thread; /* UI thread */
static int dialog_ui_fds[2];	/* files descriptors for UI thread */
static CFUserNotificationRef 	dialog_alert = 0; /* the dialog ref */


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
	{ "dialogtype", o_int, &dialogtype,
	  "Dialog type to display (PPP or VPN)"},
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
void dialog_phasechange(void *arg, uintptr_t p)
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

#if TARGET_OS_EMBEDDED
// extra CFUserNotification keys
static CFStringRef const SBUserNotificationTextAutocapitalizationType = CFSTR("SBUserNotificationTextAutocapitalizationType");
static CFStringRef const SBUserNotificationTextAutocorrectionType = CFSTR("SBUserNotificationTextAutocorrectionType");
static CFStringRef const SBUserNotificationGroupsTextFields = CFSTR("SBUserNotificationGroupsTextFields");
#endif

/* -----------------------------------------------------------------------------
Returns 1 on OK, 0 if cancelled
user and password have max size 256
----------------------------------------------------------------------------- */
int dialog_password(char *user, int maxuserlen, char *passwd, int maxpasswdlen, int dialog_type, char *message)
{
    CFOptionFlags 		flags;
    CFMutableDictionaryRef 	dict;
    SInt32 			err;
    CFMutableArrayRef 		array;
    CFURLRef			url;
    CFStringRef			str, str1;
    int				ret = 0, loop = 0;    
#if TARGET_OS_EMBEDDED
	int		nbfields = 0;
#endif

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
			if ((str = CFStringCreateWithCString(NULL, message, kCFStringEncodingUTF8))) {
				CFDictionaryAddValue(dict, kCFUserNotificationAlertMessageKey, str);
				CFRelease(str);
			}			
		}

		switch (dialog_type) {
			case DIALOG_PASSWORD_CHANGE:
	            CFDictionaryAddValue(dict, kCFUserNotificationAlertMessageKey, (tokencard == 1) ? CFSTR("Expired token") : CFSTR("Expired password"));
				break;
			case DIALOG_PASSWORD_RETRY:
				CFDictionaryAddValue(dict, kCFUserNotificationAlertMessageKey, (tokencard == 1) ? CFSTR("Incorrect token") : CFSTR("Incorrect password"));
				break;
		}
		
        array = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);  
        if (array) {
			switch (dialog_type) {
				case DIALOG_PASSWORD_CHANGE:
					CFArrayAppendValue(array, (tokencard == 1) ? CFSTR("New token:") : CFSTR("New password:"));
					CFArrayAppendValue(array, (tokencard == 1) ? CFSTR("Confirm new token:") : CFSTR("Confirm new password:"));
					break;
				case DIALOG_PASSWORD_RETRY:
					CFArrayAppendValue(array, CFSTR("Retry name:"));
					CFArrayAppendValue(array, (tokencard == 1) ? CFSTR("Retry token:") : CFSTR("Retry password:"));
					break;
				case DIALOG_PASSWORD_ASK:
				default:
					CFArrayAppendValue(array, CFSTR("Account Name:"));
					CFArrayAppendValue(array, (tokencard == 1) ? CFSTR("Token:") : CFSTR("Password:"));
					break;
			}
			
#if TARGET_OS_EMBEDDED
			nbfields = CFArrayGetCount(array);
#endif
            CFDictionaryAddValue(dict, kCFUserNotificationTextFieldTitlesKey, array);
            CFRelease(array);
        }

        array = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);  
        if (array) {
			switch (dialog_type) {
				case DIALOG_PASSWORD_CHANGE:
					break;
				case DIALOG_PASSWORD_RETRY:
				case DIALOG_PASSWORD_ASK:
				default:
					if ((str = CFStringCreateWithCString(NULL, user, kCFStringEncodingUTF8))) {
						CFArrayAppendValue(array, str);
						CFRelease(str);
					}			
			}
            CFDictionaryAddValue(dict, kCFUserNotificationTextFieldValuesKey, array);
            CFRelease(array);
        }

		CFDictionaryAddValue(dict, kCFUserNotificationLocalizationURLKey, bundleURL);
        CFDictionaryAddValue(dict, kCFUserNotificationAlertHeaderKey, (dialogtype == 0) ? CFSTR("Network Connection") : CFSTR("VPN Connection"));
        if (loop)
            CFDictionaryAddValue(dict, kCFUserNotificationAlertMessageKey, (tokencard == 1) ? CFSTR("Incorrectly entered token") : CFSTR("Incorrectly entered password"));
        //CFDictionaryAddValue(dict, kCFUserNotificationAlertMessageKey, CFSTR("Enter password"));
        CFDictionaryAddValue(dict, kCFUserNotificationAlternateButtonTitleKey, CFSTR("Cancel"));
        
		flags = CFUserNotificationSecureTextField(1);
		if (dialog_type == DIALOG_PASSWORD_CHANGE)
			flags += CFUserNotificationSecureTextField(0);

#if TARGET_OS_EMBEDDED
		if (nbfields > 0) {
			CFMutableArrayRef autoCapsTypes = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
			CFMutableArrayRef autoCorrectionTypes = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
			int i, zero = 0, one = 1;
			CFNumberRef zeroRef = CFNumberCreate(NULL, kCFNumberIntType, &zero);
			CFNumberRef oneRef = CFNumberCreate(NULL, kCFNumberIntType, &one);
			
			if (autoCapsTypes && autoCorrectionTypes && zeroRef && oneRef) {
				for(i = 0; i < nbfields; i++) {
					// no auto caps or autocorrection for any of our fields
					CFArrayAppendValue(autoCapsTypes, zeroRef);
					CFArrayAppendValue(autoCorrectionTypes, oneRef);
				}
				CFDictionarySetValue(dict, SBUserNotificationTextAutocapitalizationType, autoCapsTypes);
				CFDictionarySetValue(dict, SBUserNotificationTextAutocorrectionType, autoCorrectionTypes);
			}
			if (autoCapsTypes)
				CFRelease(autoCapsTypes);
			if (autoCorrectionTypes)
				CFRelease(autoCorrectionTypes);
			if (zeroRef)
				CFRelease(zeroRef);
			if (oneRef)
				CFRelease(oneRef);

			// make CFUN prettier
			CFDictionarySetValue(dict, SBUserNotificationGroupsTextFields, kCFBooleanTrue);
		}
#endif
		
        dialog_alert = CFUserNotificationCreate(NULL, 0, flags, &err, dict);
        if (dialog_alert) {
            CFUserNotificationReceiveResponse(dialog_alert, 0, &flags);
            // the 2 lower bits of the response flags will give the button pressed
            // 0 --> default
            // 1 --> alternate
            if ((flags & 3) == 1) {
                // user cancelled
            }
            else { 
                // user clicked OK
                ret = 1;
                str = CFUserNotificationGetResponseValue(dialog_alert, kCFUserNotificationTextFieldValuesKey, 0);
				str1 = CFUserNotificationGetResponseValue(dialog_alert, kCFUserNotificationTextFieldValuesKey, 1);
				
				switch (dialog_type) {
					case DIALOG_PASSWORD_CHANGE:
						if (!(str && str1 
								&& (CFStringCompare(str, str1, 0) == kCFCompareEqualTo)
								&& CFStringGetCString(str1, passwd, maxpasswdlen, kCFStringEncodingUTF8)))
							ret = -1;
						break;
					case DIALOG_PASSWORD_RETRY:
					case DIALOG_PASSWORD_ASK:
					default:
						if (!(str && str1
							&& CFStringGetCString(str, user, maxuserlen, kCFStringEncodingUTF8)
							&& CFStringGetCString(str1, passwd, maxpasswdlen, kCFStringEncodingUTF8)))
								ret = -1;
				}

           }

            CFRelease(dialog_alert);
			dialog_alert = 0;
        }
        
        CFRelease(dict);
    }
    loop++;
    } while (ret < 0);
    return ret;
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
static void
*dialog_UIThread(void *arg)
{
    /* int 	unit = (int)arg; */ 
    char	result = 0;
    int 	ret;

    if (pthread_detach(pthread_self()) == 0) {
        
		// username can be changed
		ret = dialog_password(username, MAXNAMELEN, passwd, MAXSECRETLEN, DIALOG_PASSWORD_ASK, 0);

		if (ret == 1)
			result = 1;
    }

    if (dialog_ui_fds[1] != -1)
		write(dialog_ui_fds[1], &result, 1);
		
    return 0;
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
static int readn(int ref, void *data, int len)
{
    int 	n, left = len;
    void 	*p = data;
    
    while (left > 0) {
        if ((n = read(ref, p, left)) < 0) {
            if (kill_link)
                return 0;
            if (errno != EINTR) 
                return -1;
            n = 0;
        }
        else if (n == 0)
            break; /* EOF */
            
        left -= n;
        p += n;
    }
    return (len - left);
}        

/* -----------------------------------------------------------------------------
Returns 1 if continue, 0 if cancel.
----------------------------------------------------------------------------- */
int dialog_invoke_ui_thread()
{
	int ret = 0;
	char result;

    if (pipe(dialog_ui_fds) < 0) {
        error("Dialogs failed to create pipe for User Interface...\n");
        return -1;
    }

    if (pthread_create(&dialog_ui_thread, NULL, dialog_UIThread, 0 /* unit number */)) {
        error("Dialogs failed to create thread for client User Interface...\n");
        close(dialog_ui_fds[0]);
        close(dialog_ui_fds[1]);
        return 1;
    }
    
		
	ret = readn(dialog_ui_fds[0], &result, 1);

	close(dialog_ui_fds[0]);
	dialog_ui_fds[0] = -1;
	close(dialog_ui_fds[1]);
	dialog_ui_fds[1] = -1;

	if (ret <= 0) {
		
		if (dialog_alert) {
			CFUserNotificationCancel(dialog_alert);
			// ui thread will finish itself
		}
		// cancel
		return 0;
	}
		
	ret = result;
	if (ret == 1)
		strncpy(user, username, MAXNAMELEN);

	return ret;
}

/* -----------------------------------------------------------------------------
Returns 1 if continue, 0 if cancel.
----------------------------------------------------------------------------- */
int dialog_start_link()
{

    if (noaskpassword || *passwd || askpasswordafter) 
        return 1;
   
   return dialog_invoke_ui_thread();
}

/* -----------------------------------------------------------------------------
msg can come from the server
Returns 1 if continue, 0 if cancel.
----------------------------------------------------------------------------- */
int dialog_change_password(char *msg)
{
	int ret = 0;

    if (noaskpassword) 
        return 1;
   
	// does not change the username, only the password
    ret = dialog_password(username, MAXNAMELEN, new_passwd, MAXSECRETLEN, DIALOG_PASSWORD_CHANGE, msg);

	return ret;
}

/* -----------------------------------------------------------------------------
msg can come from the server
Returns 1 if continue, 0 if cancel.
----------------------------------------------------------------------------- */
int dialog_retry_password(char *msg)
{
	int ret = 0;

    if (noaskpassword) 
        return 1;
   
	// username can be changed
    ret = dialog_password(username, MAXNAMELEN, passwd, MAXSECRETLEN, DIALOG_PASSWORD_RETRY, msg);
	
	if (ret == 1)
		strncpy(user, username, MAXNAMELEN);

	return ret;
}

/* -----------------------------------------------------------------------------
Returns 1 if continue, 0 if cancel.
----------------------------------------------------------------------------- */
int dialog_link_up()
{

    if (noaskpassword || *passwd || !askpasswordafter) 
        return 1;
   
   return dialog_invoke_ui_thread();
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
        CFDictionaryAddValue(dict, kCFUserNotificationAlertHeaderKey, (dialogtype == 0) ? CFSTR("Network Connection") : CFSTR("VPN Connection"));
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
