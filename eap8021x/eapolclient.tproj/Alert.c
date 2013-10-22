/*
 * Copyright (c) 2001-2013 Apple Inc. All rights reserved.
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
 * Modification History
 *
 * November 8, 2001	Dieter Siegmund
 * - created
 */

#include <CoreFoundation/CoreFoundation.h>
#include <CoreFoundation/CFUserNotification.h>
#include <sys/queue.h>
#include "Alert.h"
#include "myCFUtil.h"
#include "mylog.h"

struct Alert_s {
    LIST_ENTRY(Alert_s)		entries;
    CFUserNotificationRef 	notif;
    CFRunLoopSourceRef		rls;
    AlertCallback *		func;
    void *			arg1;
    void *			arg2;
};

static LIST_HEAD(AlertHead, Alert_s)	S_head;
static struct AlertHead *		S_AlertHead_p = &S_head;

static Alert *
Alert_find(CFUserNotificationRef alert)
{
    Alert *	scan;

    LIST_FOREACH(scan, S_AlertHead_p, entries) {
	if (scan->notif == alert)
	    return (scan);
    }
    return (NULL);
}

static void
Alert_response(CFUserNotificationRef alert, CFOptionFlags response_flags)
{
    Alert *	alert_p;

    alert_p = Alert_find(alert);
    if (alert_p == NULL) {
	EAPLOG_FL(LOG_NOTICE, "Alert_find failed");
	return;
    }
    if (alert_p->rls) {
	CFRunLoopSourceInvalidate(alert_p->rls);
	my_CFRelease(&alert_p->rls);
    }
    if (alert_p->notif) {
	my_CFRelease(&alert_p->notif);
    }
    (*alert_p->func)(alert_p->arg1, alert_p->arg2);
    return;
}

static CFDictionaryRef
make_alert_dict(char * title, char * message)
{
    CFMutableDictionaryRef	dict = NULL;
    CFStringRef			str = NULL;
 
    dict = CFDictionaryCreateMutable(NULL, 0,
				     &kCFTypeDictionaryKeyCallBacks,
				     &kCFTypeDictionaryValueCallBacks);
    if (dict == NULL) {
	goto failed;
    }
    /* bug: the default button title must be set to get the correct flags */
    CFDictionaryAddValue(dict, kCFUserNotificationDefaultButtonTitleKey, 
			 CFSTR("OK"));

    /* title */
    if (title) {
	str = CFStringCreateWithCString(NULL, title, kCFStringEncodingUTF8);
	if (str == NULL) {
	    goto failed;
	}
	CFDictionaryAddValue(dict, kCFUserNotificationAlertHeaderKey, 
			     str);
	my_CFRelease(&str);
    }
    else {
	CFDictionaryAddValue(dict, kCFUserNotificationAlertHeaderKey, 
			     CFSTR("<default title>"));
    }

    /* message */
    if (message) {
	str = CFStringCreateWithCString(NULL, message, kCFStringEncodingUTF8);
	if (str == NULL) {
	    goto failed;
	}
	CFDictionaryAddValue(dict, kCFUserNotificationAlertMessageKey, str);
	my_CFRelease(&str);
    }
    else {
	CFDictionaryAddValue(dict, kCFUserNotificationAlertMessageKey,
			     CFSTR("<default message>"));
    }
    return (dict);

 failed:
    my_CFRelease(&dict);
    return (NULL);
}

boolean_t
Alert_update(Alert * alert_p, char * title, char * message)
{
    CFDictionaryRef 		dict = NULL;
    SInt32			error;

    dict = make_alert_dict(title, message);
    if (dict == NULL) {
	goto failed;
    }
    CFUserNotificationCancel(alert_p->notif);
    error = CFUserNotificationUpdate(alert_p->notif, 0, 0, dict);
    my_CFRelease(&dict);
    if (error != 0) {
	EAPLOG_FL(LOG_NOTICE, "CFUserNotificationUpdate failed, %d", error);
	goto failed;
    }
    return (TRUE);

 failed:
    return (FALSE);
}

Alert *
Alert_create(AlertCallback * func,
	     void * arg1, void * arg2, char * title, char * message)
{
    Alert *			alert_p;
    CFUserNotificationRef 	alert = NULL;
    CFDictionaryRef 		dict = NULL;
    SInt32			error = 0;
    CFRunLoopSourceRef		rls = NULL;

    alert_p = malloc(sizeof(*alert_p));
    if (alert_p == NULL) {
	EAPLOG_FL(LOG_NOTICE, "malloc failed");
	return (NULL);
    }
    bzero(alert_p, sizeof(*alert_p));
    dict = make_alert_dict(title, message);
    if (dict == NULL) {
	EAPLOG_FL(LOG_NOTICE, "make_alert_dict failed");
	goto failed;
    }
    alert = CFUserNotificationCreate(NULL, 0, 0, &error, dict);
    if (alert == NULL) {
	EAPLOG_FL(LOG_NOTICE, "CFUserNotificationCreate failed, %d", error);
	goto failed;
    }
    rls = CFUserNotificationCreateRunLoopSource(NULL, alert, 
						Alert_response, 0);
    if (rls == NULL) {
	EAPLOG_FL(LOG_NOTICE, "CFUserNotificationCreateRunLoopSource failed");
	goto failed;
    }
    CFRunLoopAddSource(CFRunLoopGetCurrent(), rls, kCFRunLoopDefaultMode);
    alert_p->notif = alert;
    alert_p->rls = rls;
    alert_p->func = func;
    alert_p->arg1 = arg1;
    alert_p->arg2 = arg2;
    LIST_INSERT_HEAD(S_AlertHead_p, alert_p, entries);
    my_CFRelease(&dict);
    return (alert_p);

 failed:
    free(alert_p);
    my_CFRelease(&dict);
    my_CFRelease(&alert);
    my_CFRelease(&rls);
    return (NULL);
}

void
Alert_free(Alert * * alert_p_p)
{
    Alert * alert_p = *alert_p_p;

    if (alert_p) {
	LIST_REMOVE(alert_p, entries);
	if (alert_p->rls) {
	    CFRunLoopSourceInvalidate(alert_p->rls);
	    my_CFRelease(&alert_p->rls);
	}
	if (alert_p->notif) {
	    (void)CFUserNotificationCancel(alert_p->notif);
	    my_CFRelease(&alert_p->notif);
	}
	bzero(alert_p, sizeof(*alert_p));
	free(alert_p);
    }
    *alert_p_p = NULL;
    return;
}

#ifdef TEST_ALERT
void
my_callback(void * arg1, void * arg2)
{
    Alert * * alert_p_p = arg1;
    Alert * temp = *alert_p_p;

    *alert_p_p = Alert_create(my_callback, arg1, NULL,
			      "dieter", "siegmund");

    Alert_free(&temp);

    printf("User said OK\n");
    return;
}

int
main(int argc, char * argv[])
{
    Alert * alert_p;
    Alert * alert_p2;
    Alert * alert_p3;
    Alert * * p = &alert_p;
    Alert * * p2 = &alert_p2;
    Alert * * p3 = &alert_p3;

    alert_p = Alert_create(my_callback, p, NULL,
			   "dieter", "siegmund");
    alert_p2 = Alert_create(my_callback, p2, NULL, 
			    "fool", "hardy");
    alert_p3 = Alert_create(my_callback, p3, NULL, 
			    "silliness", "making");
    
    CFRunLoopRun();
    exit(0);
    return (0);
}
#endif TEST_ALERT
