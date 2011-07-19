/*
 * Copyright (c) 2001-2010 Apple Inc. All rights reserved.
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
#include <SystemConfiguration/SCValidation.h>
#include <Security/SecCertificateOIDs.h>
#include "EAPCertificateUtil.h"
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
 ** CredentialsDialogue
 **/

const CFStringRef	kCredentialsDialogueSSID = CFSTR("SSID");
const CFStringRef	kCredentialsDialogueAccountName = CFSTR("AccountName");
const CFStringRef	kCredentialsDialoguePassword = CFSTR("Password");
const CFStringRef	kCredentialsDialogueCertificates = CFSTR("Certificates");
const CFStringRef	kCredentialsDialogueRememberInformation = CFSTR("RememberInformation");

struct CredentialsDialogue_s;
#define LIST_HEAD_CredentialsDialogueHead LIST_HEAD(CredentialsDialogueHead, CredentialsDialogue_s)
static LIST_HEAD_CredentialsDialogueHead  	S_CredentialsDialogueHead;
static struct CredentialsDialogueHead * S_CredentialsDialogueHead_p = &S_CredentialsDialogueHead;

#define LIST_ENTRY_CredentialsDialogue	LIST_ENTRY(CredentialsDialogue_s)

struct CredentialsDialogue_s {
    LIST_ENTRY_CredentialsDialogue	entries;

    CFUserNotificationRef 		notif;
    CFRunLoopSourceRef			rls;
    CredentialsDialogueResponseCallBack func;
    const void *			arg1;
    const void *			arg2;
    Boolean				name_enabled;
    Boolean				password_enabled;
    Boolean				checkbox_enabled;
    CFArrayRef				certificates;
};

#define kEAPOLControllerPath	"/System/Library/SystemConfiguration/EAPOLController.bundle"

static CFBundleRef
get_bundle(void)
{
    static CFBundleRef		bundle = NULL;
    CFURLRef			url;

    if (bundle != NULL) {
	return (bundle);
    }
    url = CFURLCreateWithFileSystemPath(NULL,
					CFSTR(kEAPOLControllerPath),
					kCFURLPOSIXPathStyle, FALSE);
    if (url != NULL) {
	bundle = CFBundleCreate(NULL, url);
	CFRelease(url);
    }
    return (bundle);
}

static CFStringRef
copy_localized_string(CFBundleRef bundle, CFStringRef ethernet_str,
		      CFStringRef airport_str, CFTypeRef ssid)
{
    CFStringRef		str = NULL;

    if (ssid != NULL) {
	CFStringRef	format;

	format  = CFBundleCopyLocalizedString(bundle, airport_str, airport_str,
					      NULL);
	if (format != NULL) {
	    str = CFStringCreateWithFormat(NULL, NULL, format, ssid);
	    CFRelease(format);
	}
    }
    else {
	str = CFBundleCopyLocalizedString(bundle, ethernet_str, ethernet_str,
					  NULL);
    }
    if (str == NULL) {
	str = CFRetain(ethernet_str);
    }
    return (str);
}

static CFStringRef
copy_localized_title(CFBundleRef bundle, CFTypeRef ssid)
{
#define kAirPort8021XTitleFormat  CFSTR("Authenticating to network \"%@\"")
#define kEthernet8021XTitle	  CFSTR("Authenticating to 802.1X network")

    return (copy_localized_string(bundle,
				  kEthernet8021XTitle,
				  kAirPort8021XTitleFormat,
				  ssid));
}

static CFArrayRef
copy_certificate_labels(CredentialsDialogueRef dialogue_p,
			CFArrayRef certs, CFStringRef cert_label)
{
    CFMutableArrayRef 	array = NULL;
    CFMutableArrayRef	certs_filtered = NULL;
    int			count;
    int			i;
    CFRange		r;

    count = CFArrayGetCount(certs);
    array = CFArrayCreateMutable(NULL, count + 1, &kCFTypeArrayCallBacks);
    /* add the first element which is reserved to mean no cert is selected */
    CFArrayAppendValue(array, cert_label);
    r.location = 0;
    r.length = 1;
    certs_filtered = CFArrayCreateMutable(NULL, count, &kCFTypeArrayCallBacks);

    for (i = 0; i < count; i++) {
	SecCertificateRef	cert = NULL;
	SecIdentityRef		identity;
	CFStringRef		str = NULL;

	identity = (SecIdentityRef)CFArrayGetValueAtIndex(certs, i);
	SecIdentityCopyCertificate(identity, &cert);
	if (cert != NULL) {
	    str = SecCertificateCopyShortDescription(NULL, cert, NULL);
	    CFRelease(cert);
	}
	if (str != NULL) {
	    int		instance;
	    CFStringRef	new_str;

	    for (instance = 2, new_str = CFRetain(str); 
		 CFArrayContainsValue(array, r, new_str); instance++) {
		CFRelease(new_str);
		new_str
		    = CFStringCreateWithFormat(NULL, NULL,
					       CFSTR("%@ (%d)"), str,
					       instance);
	    }
	    CFArrayAppendValue(array, new_str);
	    r.length++;
	    CFRelease(new_str);
	    CFRelease(str);
	    CFArrayAppendValue(certs_filtered, identity);
	}
    }
    dialogue_p->certificates = certs_filtered;
    return (array);
}

static CredentialsDialogueRef
CredentialsDialogue_find(CFUserNotificationRef notif)
{
    CredentialsDialogueRef	scan;

    LIST_FOREACH(scan, S_CredentialsDialogueHead_p, entries) {
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
CredentialsDialogue_response(CFUserNotificationRef notif, 
			     CFOptionFlags response_flags)
{
    int					count;
    volatile CredentialsDialogueRef	dialogue_p;
    int					i;
    CredentialsDialogueResponse		response;
    CFStringRef				str;

    dialogue_p = CredentialsDialogue_find(notif);
    if (dialogue_p == NULL) {
	/* should not happen */
	return;
    }
    bzero(&response, sizeof(response));

    switch (S_CFUserNotificationResponse(response_flags)) {
    case kCFUserNotificationDefaultResponse:
	count = 0;
	if (dialogue_p->name_enabled) {
	    count++;
	}
	if (dialogue_p->password_enabled) {
	    count++;
	}
	for (i = 0; i < count; i++) {
	    str = CFUserNotificationGetResponseValue(notif, 
						     kCFUserNotificationTextFieldValuesKey, 
						     i);
	    if (str == NULL || CFStringGetLength(str) == 0) {
		continue;
	    }
	    if (i == 0 && dialogue_p->name_enabled) {
		response.username = CFRetain(str);
	    }
	    else {
		response.password = CFRetain(str);
	    }
	}
	if (dialogue_p->checkbox_enabled) {
	    if (response_flags & CFUserNotificationCheckBoxChecked(0)) {
		response.remember_information = TRUE;
	    }
	}
	if (dialogue_p->certificates != NULL) {
	    int		which_cert;
	    
	    which_cert = (response_flags
			  & CFUserNotificationPopUpSelection(-1)) >> 24;
	    if (which_cert > 0) {
		response.chosen_identity = (SecIdentityRef)
		    CFArrayGetValueAtIndex(dialogue_p->certificates,
					   which_cert - 1);
		CFRetain(response.chosen_identity);
	    }
	    else if (dialogue_p->password_enabled == FALSE) {
		/* user did not choose a certificate, clear the username too */
		my_CFRelease(&response.username);
	    }
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
    my_CFRelease(&dialogue_p->notif);
    my_CFRelease(&dialogue_p->certificates);
    (*dialogue_p->func)(dialogue_p->arg1, dialogue_p->arg2, &response);
    my_CFRelease(&response.username);
    my_CFRelease(&response.password);
    my_CFRelease(&response.chosen_identity);
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
	    if (url == NULL) {
		url = CFBundleCopyResourceURL(np_bundle, icon, 
					      CFSTR("tiff"), NULL);
	    }
	    CFRelease(np_bundle);
	}
	CFRelease(np_url);
    }
    return (url);
}

#define kTitleAirPortCertificate	CFSTR("TitleAirPortCertificate")
#define kTitleAirPortCertificateAndPassword CFSTR("TitleAirPortCertificateAndPassword")
#define kTitleAirPortPassword		CFSTR("TitleAirPortPassword")

#define kTitleEthernetCertificate	CFSTR("TitleEthernetCertificate")
#define kTitleEthernetCertificateAndPassword  CFSTR("TitleEthernetCertificateAndPassword")
#define kTitleEthernetPassword		CFSTR("TitleEthernetPassword")

static CFUserNotificationRef
CredentialsDialogueShow(CredentialsDialogueRef dialogue_p, 
			CFBundleRef bundle,
			CFDictionaryRef details)
{
    CFMutableArrayRef 		array = NULL;
    CFArrayRef 			certs;
    CFMutableDictionaryRef	dict = NULL;
    SInt32			error = 0;
    CFOptionFlags		flags = 0;
    CFStringRef 		icon;
    CFStringRef 		name;
    CFUserNotificationRef 	notif = NULL;
    CFStringRef 		password;
    CFIndex			password_index = kCFNotFound;
    CFBooleanRef		remember_information;
    CFTypeRef			ssid;
    CFStringRef 		title;
    CFURLRef			url;

    dict = CFDictionaryCreateMutable(NULL, 0,
				     &kCFTypeDictionaryKeyCallBacks,
				     &kCFTypeDictionaryValueCallBacks);
    url = CFBundleCopyBundleURL(bundle);
    if (url != NULL) {
	CFDictionarySetValue(dict, kCFUserNotificationLocalizationURLKey,
			     url);
	CFRelease(url);
    }
    ssid = CFDictionaryGetValue(details, kCredentialsDialogueSSID);
    icon = CFSTR("Network");
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
    
    /* text field labels */
    name = CFDictionaryGetValue(details, kCredentialsDialogueAccountName);
    password = CFDictionaryGetValue(details, kCredentialsDialoguePassword);
    
    if (name != NULL && isA_CFType(name, CFNullGetTypeID())) {
	/* don't put a name label */
    }
    else {
	dialogue_p->name_enabled = TRUE;
	password_index = 1;
    }
    if (password != NULL && isA_CFType(password, CFNullGetTypeID())) {
	/* don't put a password label */
    }
    else {
	if (password_index == kCFNotFound) {
	    password_index = 0;
	}
	dialogue_p->password_enabled = TRUE;
    }

    /* text field values */
    array = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
    if (dialogue_p->name_enabled) {
	if (name != NULL) {
	    CFArrayAppendValue(array, name);
	}
	else if (password != NULL) {
	    CFArrayAppendValue(array, CFSTR(""));
	}
    }
    if (dialogue_p->password_enabled) {
	flags |= CFUserNotificationSecureTextField(password_index);
	if (password != NULL) {
	    CFArrayAppendValue(array, password);
	}
    }
    if (CFArrayGetCount(array) != 0) {
	CFDictionaryAddValue(dict, kCFUserNotificationTextFieldValuesKey,
			     array);
    }
    my_CFRelease(&array);

    /* remember information checkbox */
    remember_information
	= CFDictionaryGetValue(details,
			       kCredentialsDialogueRememberInformation);
    if (remember_information != NULL) {
	dialogue_p->checkbox_enabled = TRUE;
	array = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
	CFArrayAppendValue(array, CFSTR("Remember information"));
	CFDictionarySetValue(dict, kCFUserNotificationCheckBoxTitlesKey,
			     array);
	my_CFRelease(&array);
	if (CFBooleanGetValue(remember_information)) {
	    flags |= CFUserNotificationCheckBoxChecked(0);
	}
    }

    /* certificate pop-up */
    certs = CFDictionaryGetValue(details, kCredentialsDialogueCertificates);
    if (certs != NULL) {
	CFStringRef		cert_label;
	CFArrayRef		labels;

	cert_label = (dialogue_p->password_enabled)
	    ? CFSTR("No certificate selected")
	    : CFSTR("Select a certificate");
	labels = copy_certificate_labels(dialogue_p, certs, cert_label);
	if (labels != NULL) {
	    CFDictionarySetValue(dict, kCFUserNotificationPopUpTitlesKey,
				 labels);
	    CFRelease(labels);
	}
    }

    /* text field labels */
    if (dialogue_p->name_enabled || dialogue_p->password_enabled) {
	array = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
	if (dialogue_p->name_enabled) {
	    if (dialogue_p->password_enabled) {
		CFArrayAppendValue(array, CFSTR("User Name:"));
	    }
	    else {
		CFArrayAppendValue(array, CFSTR("Account Name (optional):"));
	    }
	}
	if (dialogue_p->password_enabled) {
	    CFArrayAppendValue(array, CFSTR("Password:"));
	}
	CFDictionaryAddValue(dict, kCFUserNotificationTextFieldTitlesKey,
			     array);
	my_CFRelease(&array);
    }


    /* title */
    if (certs != NULL && dialogue_p->password_enabled) {
	title = copy_localized_string(bundle,
				      kTitleEthernetCertificateAndPassword,
				      kTitleAirPortCertificateAndPassword,
				      ssid);
    }
    else if (certs == NULL) {
	title = copy_localized_string(bundle,
				      kTitleEthernetPassword,
				      kTitleAirPortPassword,
				      ssid);
    }
    else {
	title = copy_localized_string(bundle,
				      kTitleEthernetCertificate,
				      kTitleAirPortCertificate,
				      ssid);
    }
    CFDictionaryAddValue(dict, kCFUserNotificationAlertHeaderKey, title);
    CFRelease(title);
			 

    notif = CFUserNotificationCreate(NULL, 0, flags, &error, dict);
    if (notif == NULL) {
	my_log(LOG_NOTICE, "CFUserNotificationCreate failed, %d", error);
    }
    my_CFRelease(&dict);
    return (notif);
}

CredentialsDialogueRef
CredentialsDialogue_create(CredentialsDialogueResponseCallBack func,
			   const void * arg1, const void * arg2, 
			   CFDictionaryRef details)
{
    CFBundleRef			bundle;
    CredentialsDialogueRef	dialogue_p;
    CFUserNotificationRef 	notif = NULL;
    CFRunLoopSourceRef		rls = NULL;

    bundle = get_bundle();
    if (bundle == NULL) {
	my_log(LOG_NOTICE, "Can't get bundle");
	return (NULL);
    }
    dialogue_p = malloc(sizeof(*dialogue_p));
    if (dialogue_p == NULL) {
	my_log(LOG_NOTICE, "CredentialsDialogue_create: malloc failed");
	return (NULL);
    }
    bzero(dialogue_p, sizeof(*dialogue_p));
    notif = CredentialsDialogueShow(dialogue_p, bundle, details);
    if (notif == NULL) {
	goto failed;
    }
    rls = CFUserNotificationCreateRunLoopSource(NULL, notif, 
						CredentialsDialogue_response,
						0);
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
    LIST_INSERT_HEAD(S_CredentialsDialogueHead_p, dialogue_p, entries);
    return (dialogue_p);

 failed:
    free(dialogue_p);
    my_CFRelease(&notif);
    my_CFRelease(&rls);
    return (NULL);
}

void
CredentialsDialogue_free(CredentialsDialogueRef * dialogue_p_p)
{
    CredentialsDialogueRef dialogue_p = *dialogue_p_p;

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
	my_CFRelease(&dialogue_p->certificates);
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

struct TrustDialogue_e;
#define LIST_HEAD_TrustDialogueHead	LIST_HEAD(TrustDialogueHead, TrustDialogue_s)
static LIST_HEAD_TrustDialogueHead 	S_trust_head;
static struct TrustDialogueHead * S_TrustDialogueHead_p = &S_trust_head;

#define LIST_ENTRY_TrustDialogue	LIST_ENTRY(TrustDialogue_s)

struct TrustDialogue_s {
    LIST_ENTRY_TrustDialogue		entries;

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
		     CFDictionaryRef trust_info, CFTypeRef ssid)
{
    char * 			argv[2] = {EAPTLSTRUST_PATH, NULL};
    CFBundleRef			bundle;
    CFStringRef			caller_label;
    CFMutableDictionaryRef 	dict;
    TrustDialogueRef		dialogue_p;
    extern void			_SCDPluginExecInit();

    bundle = get_bundle();
    if (bundle == NULL) {
	my_log(LOG_NOTICE, "Can't get bundle");
	return (NULL);
    }
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
    CFDictionarySetValue(dict, CFSTR("Icon"), CFSTR("Network"));
    caller_label = copy_localized_title(bundle, ssid);
    if (caller_label != NULL) {
	CFDictionarySetValue(dict, CFSTR("CallerLabel"), 
			     caller_label);
	CFRelease(caller_label);
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

/**
 ** AlertDialogue
 **/

struct AlertDialogue_s;
#define LIST_HEAD_AlertDialogueHead LIST_HEAD(AlertDialogueHead, AlertDialogue_s)
static LIST_HEAD_AlertDialogueHead  	S_AlertDialogueHead;
static struct AlertDialogueHead * S_AlertDialogueHead_p = &S_AlertDialogueHead;

#define LIST_ENTRY_AlertDialogue	LIST_ENTRY(AlertDialogue_s)

struct AlertDialogue_s {
    LIST_ENTRY_AlertDialogue		entries;
    CFUserNotificationRef 		notif;
    CFRunLoopSourceRef			rls;
    AlertDialogueResponseCallBack 	func;
    const void *			arg1;
    const void *			arg2;
};

static CFUserNotificationRef
AlertDialogueShow(AlertDialogueRef dialogue_p, 
		  CFBundleRef bundle, CFStringRef message)
{
    CFMutableDictionaryRef	dict = NULL;
    SInt32			error = 0;
    CFStringRef 		icon;
    CFUserNotificationRef 	notif = NULL;
    CFURLRef			url;

    dict = CFDictionaryCreateMutable(NULL, 0,
				     &kCFTypeDictionaryKeyCallBacks,
				     &kCFTypeDictionaryValueCallBacks);
    url = CFBundleCopyBundleURL(bundle);
    if (url != NULL) {
	CFDictionarySetValue(dict, kCFUserNotificationLocalizationURLKey,
			     url);
	CFRelease(url);
    }
    icon = CFSTR("Network");
    url = copy_icon_url(icon);
    if (url != NULL) {
	CFDictionarySetValue(dict, kCFUserNotificationIconURLKey,
			     url);
	CFRelease(url);
    }
    CFDictionaryAddValue(dict, kCFUserNotificationDefaultButtonTitleKey, 
			 CFSTR("DISCONNECT"));
    
    CFDictionaryAddValue(dict, kCFUserNotificationAlertHeaderKey, 
			 CFSTR("AUTHENTICATION_FAILED"));
    CFDictionaryAddValue(dict, kCFUserNotificationAlertMessageKey, message);
    notif = CFUserNotificationCreate(NULL, 0, 0, &error, dict);
    if (notif == NULL) {
	my_log(LOG_NOTICE, "CFUserNotificationCreate failed, %d", error);
    }
    my_CFRelease(&dict);
    return (notif);
}

static AlertDialogueRef
AlertDialogue_find(CFUserNotificationRef notif)
{
    AlertDialogueRef	scan;

    LIST_FOREACH(scan, S_AlertDialogueHead_p, entries) {
	if (scan->notif == notif)
	    return (scan);
    }
    return (NULL);
}

static void
AlertDialogue_response(CFUserNotificationRef notif, 
		       CFOptionFlags response_flags)
{
    volatile AlertDialogueRef		dialogue_p;

    dialogue_p = AlertDialogue_find(notif);
    if (dialogue_p == NULL) {
	/* should not happen */
	return;
    }
    if (dialogue_p->rls != NULL) {
	CFRunLoopSourceInvalidate(dialogue_p->rls);
	my_CFRelease(&dialogue_p->rls);
    }
    my_CFRelease(&dialogue_p->notif);
    (*dialogue_p->func)(dialogue_p->arg1, dialogue_p->arg2);
    return;
}

AlertDialogueRef
AlertDialogue_create(AlertDialogueResponseCallBack func,
		     const void * arg1, const void * arg2, 
		     CFStringRef message)
{
    CFBundleRef			bundle;
    AlertDialogueRef		dialogue_p;
    CFUserNotificationRef 	notif = NULL;
    CFRunLoopSourceRef		rls = NULL;

    bundle = get_bundle();
    if (bundle == NULL) {
	my_log(LOG_NOTICE, "Can't get bundle");
	return (NULL);
    }
    dialogue_p = malloc(sizeof(*dialogue_p));
    if (dialogue_p == NULL) {
	my_log(LOG_NOTICE, "AlertDialogue_create: malloc failed");
	return (NULL);
    }
    bzero(dialogue_p, sizeof(*dialogue_p));
    notif = AlertDialogueShow(dialogue_p, bundle, message);
    if (notif == NULL) {
	goto failed;
    }
    rls = CFUserNotificationCreateRunLoopSource(NULL, notif, 
						AlertDialogue_response,
						0);
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
    LIST_INSERT_HEAD(S_AlertDialogueHead_p, dialogue_p, entries);
    return (dialogue_p);

 failed:
    free(dialogue_p);
    my_CFRelease(&notif);
    my_CFRelease(&rls);
    return (NULL);
}


void
AlertDialogue_free(AlertDialogueRef * dialogue_p_p)
{
    AlertDialogueRef dialogue_p = *dialogue_p_p;

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

#ifdef TEST_DIALOGUE
#include <SystemConfiguration/SCPrivate.h>

void
my_callback(const void * arg1, const void * arg2, CredentialsDialogueResponseRef response)
{
    CredentialsDialogueRef *	dialogue_p_p = (CredentialsDialogueRef *)arg1;
    CredentialsDialogueRef	temp = *dialogue_p_p;

    if (response->username) {
	SCPrint(TRUE, stdout, CFSTR("Account: %@\n"), response->username);
    }
    if (response->password) {
	SCPrint(TRUE, stdout, CFSTR("Password: %@\n"), response->password);
    }
    if (response->chosen_identity != NULL) {
	SecCertificateRef	cert;
	CFStringRef		str;

	SecIdentityCopyCertificate(response->chosen_identity, &cert);
	if (cert != NULL) {
	    str = SecCertificateCopyShortDescription(NULL, cert, NULL);
	    CFRelease(cert);
	}
	SCPrint(TRUE, stdout, CFSTR("Identity: %@\n"), str);
	CFRelease(str);
    }
    if (response->remember_information) {
	printf("Remember information checked\n");
    }
    if (response->user_cancelled) {
	printf("User cancelled\n");
    }
    CredentialsDialogue_free(&temp);
    return;
}

void
my_alert_callback(const void * arg1, const void * arg2)
{
    AlertDialogueRef	alert_p;
    AlertDialogueRef *	alert_p_p = (AlertDialogueRef *)arg1;

    if (alert_p_p == NULL) {
	printf("NULL pointer!\n");
	return;
    }
    alert_p = *alert_p_p;
    AlertDialogue_free(alert_p_p);
    printf("Alert done\n");
    return;
}

int
main(int argc, char * argv[])
{
    AlertDialogueRef	   	alert_p;
    AlertDialogueRef *	  	alert_p_p = &alert_p;
    CredentialsDialogueRef dialogue_p;
    CredentialsDialogueRef dialogue_p2;
    CredentialsDialogueRef dialogue_p3;
    CFMutableDictionaryRef	dict = NULL;
    CredentialsDialogueRef * p = &dialogue_p;
    CredentialsDialogueRef * p2 = &dialogue_p2;
    CredentialsDialogueRef * p3 = &dialogue_p3;
    CFArrayRef		   certs = NULL;

    (void)EAPSecIdentityListCreate(&certs);

    /* dialogue 1 */
    dict = CFDictionaryCreateMutable(NULL, 0,
				     &kCFTypeDictionaryKeyCallBacks,
				     &kCFTypeDictionaryValueCallBacks);
    CFDictionarySetValue(dict, kCredentialsDialogueAccountName,
			 CFSTR("dieter"));
    /* disable the Password field */
    CFDictionarySetValue(dict, kCredentialsDialoguePassword,
			 kCFNull);
    CFDictionarySetValue(dict, kCredentialsDialogueSSID, CFSTR("SSID"));
    CFDictionarySetValue(dict, kCredentialsDialogueRememberInformation,
			 kCFBooleanTrue);
    if (certs != NULL) {
	CFDictionarySetValue(dict, kCredentialsDialogueCertificates, certs);
    }
			 
    dialogue_p = CredentialsDialogue_create(my_callback, p, NULL, dict);
    CFRelease(dict);

    /* dialogue 2 */
    dict = CFDictionaryCreateMutable(NULL, 0,
				     &kCFTypeDictionaryKeyCallBacks,
				     &kCFTypeDictionaryValueCallBacks);
    CFDictionarySetValue(dict, kCredentialsDialogueAccountName,
			 CFSTR("dieter"));
    CFDictionarySetValue(dict, kCredentialsDialoguePassword,
			 CFSTR("siegmund"));
    CFDictionarySetValue(dict, kCredentialsDialogueRememberInformation,
			 kCFBooleanTrue);
    if (certs != NULL) {
	CFDictionarySetValue(dict, kCredentialsDialogueCertificates, certs);
    }
    dialogue_p2 = CredentialsDialogue_create(my_callback, p2, NULL, dict);
    CFRelease(dict);

    /* dialogue 3 */
    dict = CFDictionaryCreateMutable(NULL, 0,
				     &kCFTypeDictionaryKeyCallBacks,
				     &kCFTypeDictionaryValueCallBacks);
    CFDictionarySetValue(dict, kCredentialsDialogueAccountName,
			 CFSTR("dieter"));
    CFDictionarySetValue(dict, kCredentialsDialoguePassword,
			 CFSTR("siegmund"));
    CFDictionarySetValue(dict, kCredentialsDialogueSSID, CFSTR("SomeSSID"));
    CFDictionarySetValue(dict, kCredentialsDialogueRememberInformation,
			 kCFBooleanFalse);
    dialogue_p3 = CredentialsDialogue_create(my_callback, p3, NULL,
					     dict);
    alert_p = AlertDialogue_create(my_alert_callback, alert_p_p, NULL, 
				   CFSTR("Here we are"));
    CFRunLoopRun();
    exit(0);
    return (0);
}

#endif /* TEST_DIALOGUE */
