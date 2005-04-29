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
includes
----------------------------------------------------------------------------- */
#include <string.h>
#include <stdio.h>
#include <sys/errno.h>
#include <sys/signal.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <unistd.h>
#include <sys/param.h>
#include <sys/fcntl.h>
#include <CoreFoundation/CoreFoundation.h>
#include <SystemConfiguration/SystemConfiguration.h>
#ifdef DEBUG
#include <SystemConfiguration/SCPrivate.h>      // for SCLog()
#endif

#include "ppp_msg.h"
#include "ppp_privmsg.h"
#include "ppp_client.h"
#include "ppp_manager.h"
#include "ppp_option.h"
#include "ppp_mach_server.h"
#include "ppp_socket_server.h"

/* -----------------------------------------------------------------------------
definitions
----------------------------------------------------------------------------- */

enum {
    do_nothing = 0,
    do_process,
    do_close,
    do_error
};

#define ICON 	"NetworkConnect.icns"

/* -----------------------------------------------------------------------------
forward declarations
----------------------------------------------------------------------------- */


int initThings();


/* -----------------------------------------------------------------------------
globals
----------------------------------------------------------------------------- */

CFStringRef 		gPluginsDir = 0;
CFBundleRef 		gBundleRef = 0;
CFURLRef 		gBundleURLRef = 0;
CFStringRef 		gCancelRef = 0;
CFStringRef 		gInternetConnectRef = 0;
CFURLRef 		gIconURLRef = 0;

/* -----------------------------------------------------------------------------
plugin entry points, called by configd
----------------------------------------------------------------------------- */
void load(CFBundleRef bundle, Boolean debug)
{
    gBundleRef = bundle;

    if (initThings())
        return;
    if (ppp_socket_start_server())
        return;
	if (ppp_mach_start_server())
		return;
    if (client_init_all())
        return;

    CFRetain(bundle);
}

void prime()
{
    ppp_init_all();
}

void stop(CFRunLoopSourceRef stopRls)
{
    ppp_stop_all(stopRls);
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
int initThings()
{
    CFURLRef 		urlref, absurlref;
    
    
    gBundleURLRef = CFBundleCopyBundleURL(gBundleRef);
    
    // create plugins dir
    urlref = CFBundleCopyBuiltInPlugInsURL(gBundleRef);
    if (urlref) {
        absurlref = CFURLCopyAbsoluteURL(urlref);
	if (absurlref) {
            gPluginsDir = CFURLCopyPath(absurlref);
            CFRelease(absurlref);
        }
        CFRelease(urlref);
    }
  
    // create misc notification strings
    gCancelRef = CFBundleCopyLocalizedString(gBundleRef, CFSTR("Cancel"), CFSTR("Cancel"), NULL);
    gInternetConnectRef = CFBundleCopyLocalizedString(gBundleRef, CFSTR("Internet Connect"), CFSTR("Internet Connect"), NULL);
    gIconURLRef = CFBundleCopyResourceURL(gBundleRef, CFSTR(ICON), NULL, NULL);
    return 0;
}
