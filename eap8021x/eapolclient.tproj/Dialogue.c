
/*
 * Copyright (c) 2001-2008 Apple Inc. All rights reserved.
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
 
#include <CoreFoundation/CFUserNotification.h>
#include <CoreFoundation/CFString.h>
#include <CoreFoundation/CFRunLoop.h>
#include <SystemConfiguration/SCDPlugin.h>
#include <sys/types.h>
#include <pwd.h>
#include <sys/wait.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <paths.h>
#include <pthread.h>
#include <sys/queue.h>
#include "Dialogue.h"
#include "mylog.h"
#include "myCFUtil.h"

/**
 ** UserPasswordDialogue
 **/
struct UserPasswordDialogue_s {
    LIST_ENTRY(UserPasswordDialogue_s)	entries;

    CFUserNotificationRef 		notif;
    CFRunLoopSourceRef			rls;
    UserPasswordDialogueResponseCallBack func;
    const void *			arg1;
    const void *			arg2;
};

static LIST_HEAD(UserPasswordDialogueHead, UserPasswordDialogue_s) 	S_user_password_head;
static struct UserPasswordDialogueHead * S_UserPasswordDialogueHead_p = &S_user_password_head;

static UserPasswordDialogueRef
UserPasswordDialogue_find(CFUserNotificationRef notif)
{
    UserPasswordDialogueRef	scan;

    LIST_FOREACH(scan, S_UserPasswordDialogueHead_p, entries) {
	if (scan->notif == notif)
	    return (scan);
    }
    return (NULL);
}

static __inline__ CFOptionFlags
S_CFUserNotificationResponse(CFOptionFlags flags)
{
    return (flags & 0x3);
}

static void
UserPasswordDialogue_response(CFUserNotificationRef notif, 
			      CFOptionFlags response_flags)
{
    volatile UserPasswordDialogueRef	dialogue_p;
    UserPasswordDialogueResponse	response;
    CFStringRef				str;

    dialogue_p = UserPasswordDialogue_find(notif);
    if (dialogue_p == NULL) {
	/* should not happen */
	return;
    }
    bzero(&response, sizeof(response));
    
    switch (S_CFUserNotificationResponse(response_flags)) {
    case kCFUserNotificationDefaultResponse:
	str = CFUserNotificationGetResponseValue(notif, 
						 kCFUserNotificationTextFieldValuesKey, 
						 0);
	if (str != NULL && CFStringGetLength(str) > 0) {
	    response.username = CFRetain(str);
	}
	str = CFUserNotificationGetResponseValue(notif, 
						 kCFUserNotificationTextFieldValuesKey, 
						 1);
	if (str != NULL && CFStringGetLength(str) > 0) {
	    response.password = CFRetain(str);
	}
	break;
    default:
	response.user_cancelled = TRUE;
	break;
    }

    if (dialogue_p->rls != NULL) {
	CFRunLoopSourceInvalidate(dialogue_p->rls);
	my_CFRelease(&dialogue_p->rls);
    }
    if (dialogue_p->notif != NULL) {
	my_CFRelease(&dialogue_p->notif);
    }
    (*dialogue_p->func)(dialogue_p->arg1, dialogue_p->arg2, &response);
    if (response.username != NULL) {
	CFRelease(response.username);
    }
    if (response.password != NULL) {
	CFRelease(response.password);
    }
    return;
}

#define kNetworkPrefPanePath	"/System/Library/PreferencePanes/Network.prefPane"

static CFURLRef
copy_icon_url(CFStringRef icon)
{
    CFBundleRef		np_bundle;
    CFURLRef		np_url;
    CFURLRef		url = NULL;

    np_url = CFURLCreateWithFileSystemPath(NULL,
					   CFSTR(kNetworkPrefPanePath),
					   kCFURLPOSIXPathStyle, FALSE);
    if (np_url != NULL) {
	np_bundle = CFBundleCreate(NULL, np_url);
	if (np_bundle != NULL) {
	    url = CFBundleCopyResourceURL(np_bundle, icon, 
					  CFSTR("icns"), NULL);
	    CFRelease(np_bundle);
	}
	CFRelease(np_url);
    }
    return (url);
}

static CFDictionaryRef
make_notif_dict(CFStringRef icon, CFStringRef title, CFStringRef message,
		CFStringRef user, CFStringRef password)
{
    CFMutableArrayRef 		array = NULL;
    CFBundleRef			bundle;
    CFMutableDictionaryRef	dict = NULL;
    CFURLRef			url;

    dict = CFDictionaryCreateMutable(NULL, 0,
				     &kCFTypeDictionaryKeyCallBacks,
				     &kCFTypeDictionaryValueCallBacks);
    if (dict == NULL) {
	goto failed;
    }
    
    bundle = CFBundleGetMainBundle();
    if (bundle == NULL) {
	goto failed;
    }
    url = CFBundleCopyBundleURL(bundle);
    if (url != NULL) {
	CFDictionarySetValue(dict, kCFUserNotificationLocalizationURLKey,
			     url);
	CFRelease(url);
    }

    if (icon == NULL) {
	icon = CFSTR("Network");
    }

    url = copy_icon_url(icon);
    if (url != NULL) {
	CFDictionarySetValue(dict, kCFUserNotificationIconURLKey,
			     url);
	CFRelease(url);
    }

    /* button titles */
    CFDictionaryAddValue(dict, kCFUserNotificationAlternateButtonTitleKey, 
			 CFSTR("Cancel"));
    CFDictionaryAddValue(dict, kCFUserNotificationDefaultButtonTitleKey, 
			 CFSTR("OK"));
    
    /* title */
    if (title != NULL) {
	CFDictionaryAddValue(dict, kCFUserNotificationAlertHeaderKey, 
			     title);
    }
			 
    /* message */
    if (message != NULL) {
	CFDictionaryAddValue(dict, kCFUserNotificationAlertMessageKey, message);
    }

    /* labels */
    array = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
    CFArrayAppendValue(array, CFSTR("User Name:"));
    CFArrayAppendValue(array, CFSTR("Password:"));
    CFDictionaryAddValue(dict, kCFUserNotificationTextFieldTitlesKey, array);
    my_CFRelease(&array);

    /* values */
    array = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
    if (user != NULL) {
	CFArrayAppendValue(array, user);
    }
    else if (password != NULL) {
	CFArrayAppendValue(array, CFSTR(""));
    }
    if (password != NULL) {
	CFArrayAppendValue(array, password);
    }
    CFDictionaryAddValue(dict, kCFUserNotificationTextFieldValuesKey, array);
    my_CFRelease(&array);

    return (dict);

 failed:
    my_CFRelease(&dict);
    return (NULL);
}

UserPasswordDialogueRef
UserPasswordDialogue_create(UserPasswordDialogueResponseCallBack func,
			    const void * arg1, const void * arg2,
			    CFStringRef icon,
			    CFStringRef title, CFStringRef message, 
			    CFStringRef user, CFStringRef password)
{
    CFUserNotificationRef 	notif = NULL;
    UserPasswordDialogueRef	dialogue_p;
    CFDictionaryRef		dict = NULL;
    SInt32			error = 0;
    CFOptionFlags		flags;
    CFRunLoopSourceRef		rls = NULL;

    dialogue_p = malloc(sizeof(*dialogue_p));
    if (dialogue_p == NULL) {
	my_log(LOG_NOTICE, "UserPasswordDialogue_create: malloc failed");
	return (NULL);
    }
    bzero(dialogue_p, sizeof(*dialogue_p));
    dict = make_notif_dict(icon, title, message, user, password);
    if (dict == NULL) {
	goto failed;
    }
    flags = CFUserNotificationSecureTextField(1);
    notif = CFUserNotificationCreate(NULL, 0, flags, &error, dict);
    if (notif == NULL) {
	my_log(LOG_NOTICE, "CFUserNotificationCreate failed, %d",
	       error);
	goto failed;
    }
    rls = CFUserNotificationCreateRunLoopSource(NULL, notif, 
						UserPasswordDialogue_response, 0);
    if (rls == NULL) {
	my_log(LOG_NOTICE, "CFUserNotificationCreateRunLoopSource failed");
	goto failed;
    }
    CFRunLoopAddSource(CFRunLoopGetCurrent(), rls, kCFRunLoopDefaultMode);
    dialogue_p->notif = notif;
    dialogue_p->rls = rls;
    dialogue_p->func = func;
    dialogue_p->arg1 = arg1;
    dialogue_p->arg2 = arg2;
    LIST_INSERT_HEAD(S_UserPasswordDialogueHead_p, dialogue_p, entries);
    my_CFRelease(&dict);
    return (dialogue_p);

 failed:
    free(dialogue_p);
    my_CFRelease(&dict);
    my_CFRelease(&notif);
    my_CFRelease(&rls);
    return (NULL);
}

void
UserPasswordDialogue_free(UserPasswordDialogueRef * dialogue_p_p)
{
    UserPasswordDialogueRef dialogue_p = *dialogue_p_p;

    if (dialogue_p) {
	LIST_REMOVE(dialogue_p, entries);
	if (dialogue_p->rls) {
	    CFRunLoopSourceInvalidate(dialogue_p->rls);
	    my_CFRelease(&dialogue_p->rls);
	}
	if (dialogue_p->notif) {
	    (void)CFUserNotificationCancel(dialogue_p->notif);
	    my_CFRelease(&dialogue_p->notif);
	}
	bzero(dialogue_p, sizeof(*dialogue_p));
	free(dialogue_p);
    }
    *dialogue_p_p = NULL;
    return;
}

/**
 ** TrustDialogue
 **/
#include <signal.h>
#include <syslog.h>
#include <CoreFoundation/CFData.h>
#include <CoreFoundation/CFPropertyList.h>

static LIST_HEAD(TrustDialogueHead, TrustDialogue_s) 	S_trust_head;
static struct TrustDialogueHead * S_TrustDialogueHead_p = &S_trust_head;

struct TrustDialogue_s {
    LIST_ENTRY(TrustDialogue_s)		entries;
    TrustDialogueResponseCallBack 	func;
    const void *			arg1;
    const void *			arg2;
    pid_t				pid;
    int					fdp[2];
    CFDictionaryRef			trust_plist;
};

static TrustDialogueRef
TrustDialogue_find(pid_t pid)
{
    TrustDialogueRef	scan;

    LIST_FOREACH(scan, S_TrustDialogueHead_p, entries) {
	if (scan->pid == pid)
	    return (scan);
    }
    return (NULL);
}


static void
TrustDialogue_callback(pid_t pid, int status, __unused struct rusage * rusage, 
		       __unused void * context)
{
    TrustDialogueRef		dialogue_p;
    TrustDialogueResponse	response;

    dialogue_p = TrustDialogue_find(pid);
    if (dialogue_p == NULL) {
	return;
    }
    response.proceed = FALSE;
    if (WIFEXITED(status)) {
	int	exit_code = WEXITSTATUS(status);
	my_log(LOG_DEBUG, "TrustDialogue_callback: child %d exit(%d)",
	       pid, exit_code);
	if (exit_code == 0) {
	    response.proceed = 1;
	}
    }
    else if (WIFSIGNALED(status)) {
	my_log(LOG_DEBUG, "TrustDialogue_callback: child %d signaled(%d)",
	       pid, WTERMSIG(status));
    }
    dialogue_p->pid = -1;
    (*dialogue_p->func)(dialogue_p->arg1, dialogue_p->arg2, &response);
    return;
}

static void
TrustDialogue_setup_child(TrustDialogueRef dialogue_p)
{
    int	fd;
    int i;

    /* close open FD's except for the read end of the pipe */
    for (i = getdtablesize() - 1; i >= 0; i--) {
	if (i != dialogue_p->fdp[0]) {
	    close(i);
	}
    }
    if (dialogue_p->fdp[0] != STDIN_FILENO) {
	dup(dialogue_p->fdp[0]);	/* stdin */
	close(dialogue_p->fdp[0]);
    }
    fd = open(_PATH_DEVNULL, O_RDWR, 0);/* stdout */
    dup(fd);				/* stderr */
    return;
}

static void
TrustDialogue_setup_parent(TrustDialogueRef dialogue_p)
{
    size_t		count;
    CFDataRef		data;
    size_t		write_count;
    
    close(dialogue_p->fdp[0]); 	/* close the read end */
    dialogue_p->fdp[0] = -1;

    data = CFPropertyListCreateXMLData(NULL, 
				       dialogue_p->trust_plist);
    count = CFDataGetLength(data);
    /* disable SIGPIPE if it's currently enabled? XXX */
    write_count = write(dialogue_p->fdp[1], 
			(void *)CFDataGetBytePtr(data),
			count);
    /* enable SIGPIPE if it was enabled? XXX */
    my_CFRelease(&data);

    if (write_count != count) {
	if (write_count == -1) {
	    my_log(LOG_NOTICE, 
		   "TrustDialogue_setup_parent: write on pipe failed, %m");
	}
	else {
	    my_log(LOG_NOTICE, 
		   "TrustDialogue_setup_parent: wrote %d expected %d",
		   write_count, count);
	}
    }
    close(dialogue_p->fdp[1]);	/* close write end to deliver EOF to reader */
    dialogue_p->fdp[1] = -1;
    return;
}

static void
TrustDialogue_setup(pid_t pid, void * context)
{
    TrustDialogueRef	Dialogue_p = (TrustDialogueRef)context;

    if (pid == 0) {
	TrustDialogue_setup_child(Dialogue_p);
    }
    else {
	TrustDialogue_setup_parent(Dialogue_p);
    }
    return;
}

#define EAPTLSTRUST_PATH	"/System/Library/PrivateFrameworks/EAP8021X.framework/Support/eaptlstrust.app/Contents/MacOS/eaptlstrust"

static pthread_once_t initialized = PTHREAD_ONCE_INIT;

TrustDialogueRef
TrustDialogue_create(TrustDialogueResponseCallBack func,
		     const void * arg1, const void * arg2,
		     CFDictionaryRef trust_info, CFStringRef icon,
		     CFStringRef caller_label)
{
    char * 			argv[2] = {EAPTLSTRUST_PATH, NULL};
    CFMutableDictionaryRef 	dict;
    TrustDialogueRef		dialogue_p;
    extern void			_SCDPluginExecInit();

    pthread_once(&initialized, _SCDPluginExecInit);
    dialogue_p = (TrustDialogueRef)malloc(sizeof(*dialogue_p));
    bzero(dialogue_p, sizeof(*dialogue_p));
    dialogue_p->pid = -1;
    dialogue_p->fdp[0] = dialogue_p->fdp[1] = -1;
    if (pipe(dialogue_p->fdp) == -1) {
	my_log(LOG_NOTICE, "TrustDialogue_create: pipe failed, %m");
	goto failed;
    }
    dict = CFDictionaryCreateMutable(NULL, 0,
				     &kCFTypeDictionaryKeyCallBacks,
				     &kCFTypeDictionaryValueCallBacks);
    CFDictionarySetValue(dict, CFSTR("TrustInformation"), trust_info);
    if (icon == NULL) {
	icon = CFSTR("Network");
    }
    CFDictionarySetValue(dict, CFSTR("Icon"), icon);
    if (caller_label != NULL) {
	CFDictionarySetValue(dict, CFSTR("CallerLabel"), 
			     caller_label);
    }
    dialogue_p->trust_plist = CFDictionaryCreateCopy(NULL, dict);
    my_CFRelease(&dict);
    dialogue_p->pid
	= _SCDPluginExecCommand2(TrustDialogue_callback, NULL,
				 geteuid(), getegid(),
				 EAPTLSTRUST_PATH, argv,
				 TrustDialogue_setup,
				 dialogue_p);
    if (dialogue_p->pid == -1) {
	my_log(LOG_NOTICE, 
	       "TrustDialogue_create: _SCDPluginExecCommand2 failed, %m");
	goto failed;
    }
    dialogue_p->func = func;
    dialogue_p->arg1 = arg1;
    dialogue_p->arg2 = arg2;
    LIST_INSERT_HEAD(S_TrustDialogueHead_p, dialogue_p, entries);
    return (dialogue_p);

 failed:
    TrustDialogue_free(&dialogue_p);
    return (NULL);
}

CFDictionaryRef
TrustDialogue_trust_info(TrustDialogueRef dialogue_p)
{
    if (dialogue_p->trust_plist == NULL) {
	return (NULL);
    }
    return (CFDictionaryGetValue(dialogue_p->trust_plist,
				 CFSTR("TrustInformation")));
}

void
TrustDialogue_free(TrustDialogueRef * dialogue_p_p)
{
    TrustDialogueRef	dialogue_p;

    if (dialogue_p_p == NULL) {
	return;
    }
    dialogue_p = *dialogue_p_p;
    if (dialogue_p != NULL) {
	LIST_REMOVE(dialogue_p, entries);
	if (dialogue_p->pid != -1) {
	    if (kill(dialogue_p->pid, SIGHUP)) {
		my_log(LOG_NOTICE, "TrustDialogue_free kill(%d) failed, %m",
		       dialogue_p->pid);
	    }
	}
	if (dialogue_p->fdp[0] != -1) {
	    close(dialogue_p->fdp[0]);
	}
	if (dialogue_p->fdp[0] != -1) {
	    close(dialogue_p->fdp[1]);
	}
	my_CFRelease(&dialogue_p->trust_plist);
	free(dialogue_p);
	*dialogue_p_p = NULL;
    }
    return;
}

#ifdef TEST_DIALOGUE
void
my_callback(const void * arg1, const void * arg2, UserPasswordDialogueResponseRef response)
{
    UserPasswordDialogueRef *	dialogue_p_p = (UserPasswordDialogueRef *)arg1;
    UserPasswordDialogueRef	temp = *dialogue_p_p;

    if (response->username) {
	printf("User is ");
	fflush(stdout);
	CFShow(response->username);
	fflush(stdout);
	fflush(stderr);
    }
    if (response->password) {
	printf("Password is ");
	fflush(stdout);
	CFShow(response->password);
	fflush(stdout);
	fflush(stderr);
    }
    if (response->user_cancelled) {
	printf("User cancelled\n");
    }
    *dialogue_p_p = UserPasswordDialogue_create(my_callback, 
						dialogue_p_p, NULL,
						NULL,
						CFSTR("Title"),
						CFSTR("message is this"), 
						CFSTR("dieter"), 
						CFSTR("siegmund"));
    UserPasswordDialogue_free(&temp);
    return;
}

int
main(int argc, char * argv[])
{
    UserPasswordDialogueRef dialogue_p;
    UserPasswordDialogueRef dialogue_p2;
    UserPasswordDialogueRef dialogue_p3;
    UserPasswordDialogueRef * p = &dialogue_p;
    UserPasswordDialogueRef * p2 = &dialogue_p2;
    UserPasswordDialogueRef * p3 = &dialogue_p3;

    dialogue_p = UserPasswordDialogue_create(my_callback, p, NULL,
					     NULL,
					     CFSTR("Title1"), CFSTR("message"),
					     CFSTR("dieter"),
					     CFSTR("siegmund"));
    dialogue_p2 = UserPasswordDialogue_create(my_callback, p2, NULL,
					      NULL,
					      CFSTR("Title2"), CFSTR("message"),
					      CFSTR("dieter"),
					      CFSTR("siegmund"));
    dialogue_p3 = UserPasswordDialogue_create(my_callback, p3, NULL,
					      NULL,
					      CFSTR("Title3"), CFSTR("message"),
					      CFSTR("dieter"),
					      CFSTR("siegmund"));
    CFRunLoopRun();
    exit(0);
    return (0);
}

#endif TEST_DIALOGUE
