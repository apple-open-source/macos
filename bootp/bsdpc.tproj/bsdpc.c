/*
 * Copyright (c) 2002 Apple Inc. All rights reserved.
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
 * bsdpc.c
 * - command line utility for selecting a netboot image
 */

/* 
 * Modification History
 *
 * February 25, 2002	Dieter Siegmund (dieter@apple.com)
 * - initial revision
 */

#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/errno.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/fcntl.h>
#include <mach/boolean.h>
#include <limits.h>

#include "util.h"
#include "cfutil.h"
#include "BSDPClient.h"
#include "BSDPClientPrivate.h"
#include <CoreFoundation/CoreFoundation.h>

static __inline__ const char *
image_kind_string(int kind)
{
    const char * kind_str[] = {
	"Mac OS 9",
	"Mac OS X",
	"Mac OS X Server",
    };
    if (kind >= kBSDPImageKindMacOS9
	&& kind <= kBSDPImageKindMacOSXServer) {
	return (kind_str[kind]);
    }
    return (NULL);
}


#define kServerAddress		CFSTR("_bsdpc_ServerAddress")
#define kServerPriority		CFSTR("_bsdpc_ServerPriority")
#define kImageList		CFSTR("_bsdpc_ImageList")
#define kServedBy		CFSTR("_bsdpc_ServedBy")

typedef enum {
    bsdpc_state_init = 0,
    bsdpc_state_list,
    bsdpc_state_user_input,
    bsdpc_state_select,
} bsdpc_state_t;

#define INVALID_REDISPLAY	5

struct bsdpc;
typedef struct bsdpc bsdpc_t;

typedef void (*timerCallBack)(bsdpc_t * bsdpc);

struct bsdpc {
    BSDPClientRef		client;
    bsdpc_state_t		state;
    boolean_t			gathering;
    int				invalid_count;
    CFMutableArrayRef		images;
    CFMutableArrayRef		menu;
    CFRunLoopTimerRef		timer;
    timerCallBack		timer_callback;
};

static bsdpc_t	bsdpc;

static void
processTimer(CFRunLoopTimerRef timer, void * info)
{
    bsdpc_t *	bsdpc;

    bsdpc = (bsdpc_t *)info;
    (*bsdpc->timer_callback)(bsdpc);
    return;
}

static void
cancelTimer(bsdpc_t * bsdpc)
{
    if (bsdpc->timer) {
	CFRunLoopTimerInvalidate(bsdpc->timer);
	my_CFRelease(&bsdpc->timer);
    }
    bsdpc->timer_callback = NULL;
    return;
}

static void
setTimer(bsdpc_t * bsdpc, struct timeval rel_time,
	 timerCallBack callback)
{
    CFRunLoopTimerContext 	context =  { 0, NULL, NULL, NULL, NULL };
    CFAbsoluteTime 		wakeup_time;

    cancelTimer(bsdpc);
    bsdpc->timer_callback = callback;
    wakeup_time = CFAbsoluteTimeGetCurrent() + rel_time.tv_sec 
	  + ((double)rel_time.tv_usec / USECS_PER_SEC);
    context.info = bsdpc;
    bsdpc->timer
	= CFRunLoopTimerCreate(NULL, wakeup_time,
			       0.0, 0, 0,
			       processTimer,
			       &context);
    CFRunLoopAddTimer(CFRunLoopGetCurrent(), bsdpc->timer,
		      kCFRunLoopDefaultMode);
    return;
}

static void
add_menu_item(CFMutableArrayRef menu, CFDictionaryRef server_dict,
	      CFDictionaryRef image_dict)
{
    int				i;
    CFNumberRef			identifier;
    CFMutableArrayRef		served_by;
    CFMutableDictionaryRef	menu_item;

    identifier = CFDictionaryGetValue(image_dict, 
				      kBSDPImageDescriptionIdentifier);
    if (BSDPImageDescriptionIdentifierIsServerLocal(identifier) == FALSE) {
	int count = CFArrayGetCount(menu);
	for (i = 0; i < count; i++) {
	    menu_item = (CFMutableDictionaryRef)CFArrayGetValueAtIndex(menu, i);
	    if (CFEqual(identifier, 
			CFDictionaryGetValue(menu_item,
					     kBSDPImageDescriptionIdentifier))
		== TRUE) {
		served_by = (CFMutableArrayRef)
		    CFDictionaryGetValue(menu_item, kServedBy);
		CFArrayAppendValue(served_by, server_dict);
		return;
	    }
	}
    }
    menu_item = CFDictionaryCreateMutableCopy(NULL, 0, image_dict);
    served_by = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
    CFArrayAppendValue(served_by, server_dict);
    CFDictionarySetValue(menu_item, kServedBy, served_by);
    CFArrayAppendValue(menu, menu_item);
    my_CFRelease(&served_by);
    my_CFRelease(&menu_item);
    return;
}

static void
create_menu(bsdpc_t * bsdpc)
{
    int count;
    int i;
    int	s;

    my_CFRelease(&bsdpc->menu);
    bsdpc->menu = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
    count = CFArrayGetCount(bsdpc->images);
    for (s = 0; s < count; s++) {
	CFArrayRef		images;
	int			images_count;
	CFMutableDictionaryRef	server_copy;
	CFDictionaryRef		server_dict;

	server_dict = CFArrayGetValueAtIndex(bsdpc->images, s);
	server_copy = CFDictionaryCreateMutableCopy(NULL, 0, server_dict);
	CFDictionaryRemoveValue(server_copy, kImageList);
	images = CFDictionaryGetValue(server_dict, kImageList);
	images_count = CFArrayGetCount(images);
	for (i = 0; i < images_count; i++) {
	    CFDictionaryRef		image_dict;
	    
	    image_dict = CFArrayGetValueAtIndex(images, i);
	    add_menu_item(bsdpc->menu, server_copy, image_dict);
	}
	my_CFRelease(&server_copy);
    }
    return;
}

static int
cfstring_to_cstring(CFStringRef cfstr, char * str, int len)
{
    CFIndex		l;
    CFRange		range;

    range = CFRangeMake(0, CFStringGetLength(cfstr));
    (void)CFStringGetBytes(cfstr, range, kCFStringEncodingUTF8,
			   '?', TRUE, (UInt8 *)str, len, &l);
    str[l] = '\0';
    return (l);
}

static void
display_prompt(bsdpc_t * bsdpc)
{
    printf("Enter the image number (1..%ld) or q to quit) ",
	   CFArrayGetCount(bsdpc->menu));
    fflush(stdout);
    return;
}

static void
display_menu(bsdpc_t * bsdpc)
{
    int		count;
    int		i;

    count = CFArrayGetCount(bsdpc->menu);
    printf("\nNetBoot Image List:\n");
    for (i = 0; i < count; i++) {
	BSDPImageKind		kind;
	const char *		kind_str;
	CFDictionaryRef		menu_item;
	char			name[256];
	CFBooleanRef		is_default;
	CFNumberRef		identifier;
	CFBooleanRef		is_selected;
	
	menu_item = CFArrayGetValueAtIndex(bsdpc->menu, i);
	is_selected = CFDictionaryGetValue(menu_item,
					   kBSDPImageDescriptionIsSelected);
	is_default = CFDictionaryGetValue(menu_item,
					   kBSDPImageDescriptionIsDefault);
	identifier = CFDictionaryGetValue(menu_item,
					  kBSDPImageDescriptionIdentifier);
	cfstring_to_cstring(CFDictionaryGetValue(menu_item, 
						 kBSDPImageDescriptionName),
			    name, sizeof(name));
	printf("%4d. %s", i + 1, name);

	kind = BSDPImageDescriptionIdentifierImageKind(identifier);
	kind_str = image_kind_string(kind);
	if (kind_str != NULL) {
	    printf(" [%s]", kind_str);
	}
	else {
	    printf(" [Kind=%d]", kind);
	}
	if (BSDPImageDescriptionIdentifierIsInstall(identifier)) {
	    printf(" [Install]");
	}
	if (is_default != NULL
	    && CFEqual(is_default, kCFBooleanTrue)) {
	    printf(" [Default]");
	}
	if (is_selected != NULL
	    && CFEqual(is_selected, kCFBooleanTrue)) {
	    printf(" [Selected]");
	}
	printf("\n");
    }
    display_prompt(bsdpc);
    return;
}

static void
redisplay(bsdpc_t * bsdpc)
{
    bsdpc->invalid_count++;
    if (bsdpc->invalid_count == INVALID_REDISPLAY) {
	bsdpc->invalid_count = 0;
	display_menu(bsdpc);
    }
    else {
	display_prompt(bsdpc);
    }
}

static void
gatherDone(bsdpc_t * bsdpc)
{
    bsdpc->state = bsdpc_state_user_input;
    create_menu(bsdpc);
    display_menu(bsdpc);
}

static void
removeExistingServer(bsdpc_t * bsdpc, CFStringRef ServerAddress)
{
    int count;
    int i;

    count = CFArrayGetCount(bsdpc->images);
    for (i = 0; i < count; i++) {
	CFDictionaryRef	this_dict = CFArrayGetValueAtIndex(bsdpc->images, i);

	if (CFEqual(ServerAddress, 
		    CFDictionaryGetValue(this_dict, kServerAddress))) {
	    CFArrayRemoveValueAtIndex(bsdpc->images, i);
	    return;
	}
    }
    return;
}

static void
accumulateImages(bsdpc_t * bsdpc, CFStringRef ServerAddress, 
		 CFNumberRef ServerPriority, CFArrayRef images)
{
    CFMutableDictionaryRef	this_dict;

    if (bsdpc->state != bsdpc_state_list) {
	return;
    }
    this_dict = CFDictionaryCreateMutable(NULL, 0,
					  &kCFTypeDictionaryKeyCallBacks,
					  &kCFTypeDictionaryValueCallBacks);
    CFDictionarySetValue(this_dict, kServerAddress, ServerAddress);
    CFDictionarySetValue(this_dict, kServerPriority, ServerPriority);
    CFDictionarySetValue(this_dict, kImageList, images);
    removeExistingServer(bsdpc, ServerAddress);
    CFArrayAppendValue(bsdpc->images, this_dict);
    my_CFRelease(&this_dict);
    if (bsdpc->gathering == FALSE) {
	struct timeval	t;

	bsdpc->gathering = TRUE;
	t.tv_sec = 4;
	t.tv_usec = 0;
	setTimer(bsdpc, t, gatherDone);
    }
    return;
}

static void 
list_callback(BSDPClientRef client, BSDPClientStatus status,
	      CFStringRef ServerAddress,
	      CFNumberRef ServerPriority, 
	      const CFArrayRef list, void *info)
{
    bsdpc_t *	bsdpc = (bsdpc_t *)info;

    switch (status) {
    case kBSDPClientStatusOK:
	accumulateImages(bsdpc, ServerAddress, 
			 ServerPriority, list);
	break;
    case kBSDPClientStatusOperationTimedOut:
	fprintf(stderr, "No netboot servers found, exiting\n");
	exit(1);
	break;
    default:
	fprintf(stderr, "List failed, %s\n", BSDPClientStatusString(status));
	break;
    }
    return;
}

static void 
select_callback(BSDPClientRef client, BSDPClientStatus status, void * info)
{
    switch (status) {
    case kBSDPClientStatusOK:
	printf("Server confirmed selection\n");
	exit(0);
	break;
    case kBSDPClientStatusServerSentFailure:
	printf("Server sent failure, selection not confirmed!\n");
	exit(1);
	break;
    default:
	fprintf(stderr, "Select failed, %s\n", BSDPClientStatusString(status));
	break;
    }
    return;
}

static void
select_image(bsdpc_t * bsdpc, int val)
{
    CFNumberRef		best_priority = NULL;
    CFDictionaryRef	best_server_dict = NULL;
    int 		i;	
    CFDictionaryRef	menu_item;
    CFArrayRef		served_by;
    int			served_by_count;
    BSDPClientStatus	status;

    menu_item = CFArrayGetValueAtIndex(bsdpc->menu, val - 1);
    served_by = CFDictionaryGetValue(menu_item, kServedBy);
    served_by_count = CFArrayGetCount(served_by);
    /* find the "best" server for this image */
    for (i = 0; i < served_by_count; i++) {
	CFDictionaryRef	server_dict = CFArrayGetValueAtIndex(served_by, i);
	CFNumberRef	priority;

	priority = CFDictionaryGetValue(server_dict, kServerPriority);
	if (best_server_dict == NULL
	    || (CFNumberCompare(priority, best_priority, NULL) 
		== kCFCompareGreaterThan)) {
	    best_server_dict = server_dict;
	    best_priority = priority;
	}
    }
    status 
	= BSDPClientSelect(bsdpc->client, 
			   CFDictionaryGetValue(best_server_dict, 
						kServerAddress),
			   CFDictionaryGetValue(menu_item, 
						kBSDPImageDescriptionIdentifier),
			   select_callback, bsdpc);
    if (status != kBSDPClientStatusOK) {
	fprintf(stderr, "BSDPClientSelect() failed, %s\n",
		BSDPClientStatusString(status));
	exit(1);
    }
    bsdpc->state = bsdpc_state_select;
    return;
}

static void
user_input(CFSocketRef s, CFSocketCallBackType type, 
	   CFDataRef address, const void *data, void *info)
{
    bsdpc_t * 	bsdpc = (bsdpc_t *)info;
    char 	choice[128];
    char 	first_char;
    char * 	result;

    result = fgets(choice, sizeof(choice), stdin);
    if (result == NULL) {
	printf("EOF\n");
	exit(1);
    }
    if (bsdpc->state != bsdpc_state_user_input
	|| result != choice) {
	return;
    }
    first_char = choice[0];
    if (first_char >= '1' && first_char <= '9') { /* image selection */
	int		val = strtoul(choice, 0, 0);
	if (val >= 1 && val <= CFArrayGetCount(bsdpc->menu)) {
	    select_image(bsdpc, val);
	}
	else {
	    printf("Value out of range\n");
	    redisplay(bsdpc);
	}
    }
    else {
	switch (first_char) {
	case 'q':
	    exit(0);
	    break;
	default:
	    printf("Invalid entry\n");
	    redisplay(bsdpc);
	    break;
	}
    }
    return;
}


static void
initialize(const char * ifname, u_int16_t * attrs, int n_attrs)
{
    BSDPClientRef	client;
    CFSocketContext	context = { 0, NULL, NULL, NULL, NULL };
    CFRunLoopSourceRef	rls = NULL;
    CFSocketRef		socket = NULL;
    BSDPClientStatus	status;

    context.info = &bsdpc;
    socket = CFSocketCreateWithNative(NULL, fileno(stdin), kCFSocketReadCallBack,
				      user_input, &context);
    if (socket == NULL) {
	fprintf(stderr, "CFSocketCreateWithNative failed\n");
	exit(1);
    }
    rls = CFSocketCreateRunLoopSource(NULL, socket, 0);
    if (rls == NULL) {
	fprintf(stderr, "CFSocketCreateRunLoopSource failed\n");
	exit(1);
    }
    CFRunLoopAddSource(CFRunLoopGetCurrent(), rls, kCFRunLoopDefaultMode);
    my_CFRelease(&rls);
    my_CFRelease(&socket);

    client = BSDPClientCreateWithInterfaceAndAttributes(&status, ifname,
							attrs, n_attrs);
    if (client == NULL) {
	fprintf(stderr, "BSDPClientCreateWithInterface(%s) failed, %s\n",
		ifname, BSDPClientStatusString(status));
	BSDPClientFree(&client);
	exit(1);
    }
    status = BSDPClientList(client, list_callback, &bsdpc);
    if (status != kBSDPClientStatusOK) {
	fprintf(stderr, "BSDPClientList() failed, %s\n",
		BSDPClientStatusString(status));
	BSDPClientFree(&client);
	exit(1);
    }
    bsdpc.state = bsdpc_state_list;
    bsdpc.client = client;
    bsdpc.images = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
    return;
}

void
usage(const char * progname)
{
    fprintf(stderr, "usage: %s <options>\n"
	    "<options> are:\n"
	    "-i interface : interface name to do BSDP over, default is en0\n"
	    "-F attrs     : attributes filter\n",
	    progname);
    exit(1);
}

#define MAX_ATTRS	10

int
main(int argc, char * argv[])
{
    int			ch;
    const char * 	ifname = NULL;
    u_int16_t		attrs[MAX_ATTRS];
    int			n_attrs = 0;
    long		val;

    while ((ch = getopt(argc, argv, "F:i:")) != EOF) {
	switch (ch) {
	case 'i':
	    if (ifname != NULL) {
		fprintf(stderr, "can't specify interface more than once\n");
		exit(1);
	    }
	    ifname = optarg;
	    break;
	case 'F':
	    errno = 0;
	    if (n_attrs == MAX_ATTRS) {
		fprintf(stderr, "too many attributes passed\n");
		exit(1);
	    }
	    val = strtol(optarg, NULL, 0);
	    if (errno != 0 || val < 0 || val > 65535) {
		fprintf(stderr, "bad attribute value passed\n");
		exit(1);
	    }
	    attrs[n_attrs++] = val;
	    break;
	default:
	    break;
	}
    }
    if ((argc - optind) != 0) {
	usage(argv[0]);
    }
    if (ifname == NULL) {
	ifname = "en0";
    }

    printf("Discovering NetBoot servers...\n");
    if (n_attrs == 0) {
	initialize(ifname, NULL, 0);
    }
    else {
	initialize(ifname, attrs, n_attrs);
    }
    CFRunLoopRun();
    printf("CFRunLoop done\n");
    exit(0);
}
