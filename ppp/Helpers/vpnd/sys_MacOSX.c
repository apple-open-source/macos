/*
 * Copyright (c) 2000 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
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


#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <termios.h>
#include <signal.h>
#include <util.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <sys/param.h>
#include <sys/wait.h>
#include <net/if.h>
#include <net/if_types.h>
#include <mach-o/dyld.h>
#include <dirent.h>
#include <NSSystemDirectories.h>
#include <mach/mach_time.h>
#include <SystemConfiguration/SystemConfiguration.h>
#include <CoreFoundation/CFBundle.h>
#include <ppp_defs.h>
#include <ppp_domain.h>
#include <ppp_msg.h>
#include <ppp_privmsg.h>

#include "vpnd.h"


/* -----------------------------------------------------------------------------
 Definitions
----------------------------------------------------------------------------- */

#define PPP_NKE_PATH 	"/System/Library/Extensions/PPP.kext"


/* -----------------------------------------------------------------------------
 Globals
----------------------------------------------------------------------------- */

bool	 		noload = 0;		/* don't load the kernel extension */




/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
void closeall()
{
    int i;

    for (i = getdtablesize() - 1; i >= 0; i--) close(i);
    open("/dev/null", O_RDWR, 0);
    dup(0);
    dup(0);
    return;
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
u_long load_kext(char *kext)
{
    int pid;

    if ((pid = fork()) < 0)
        return 1;

    if (pid == 0) {
        closeall();
        // PPP kernel extension not loaded, try load it...
        execl("/sbin/kextload", "kextload", kext, (char *)0);
        exit(1);
    }

    while (waitpid(pid, 0, 0) < 0) {
        if (errno == EINTR)
            continue;
       return 1;
    }
    return 0;
}

/* ----------------------------------------------------------------------------- 
check if the kernel supports PPP
----------------------------------------------------------------------------- */
int ppp_available()
{
    int 	s;
    
    // open to socket to the PF_PPP family
    // if that works, the kernel extension is loaded.
    if ((s = socket(PF_PPP, SOCK_RAW, PPPPROTO_CTL)) < 0) {
    
        if (!noload && !load_kext(PPP_NKE_PATH))
            s = socket(PF_PPP, SOCK_RAW, PPPPROTO_CTL);
            
        if (s < 0)
            return 0;
    }
    
    // could be smarter and get the version of the ppp family, 
    // using get option or ioctl

    close(s);

    return 1;
}

/* ----------------------------------------------------------------------------- 
Copy the IPAddress of the default interface
----------------------------------------------------------------------------- */
CFStringRef CopyDefaultIPAddress()
{
    SCDynamicStoreRef 	store;
    CFDictionaryRef	dict = 0;
    CFStringRef		string, key;
    CFArrayRef		array;
    
    store = SCDynamicStoreCreate(0, CFSTR("vpnd"), 0, 0);
    if (store == 0)
        return 0;
    
    key = SCDynamicStoreKeyCreateNetworkGlobalEntity(0, kSCDynamicStoreDomainState, kSCEntNetIPv4);
    dict = SCDynamicStoreCopyValue(store, key);
    CFRelease(key);

    if (!isDictionary(dict)) 
        goto error;

    string = CFDictionaryGetValue(dict, kSCDynamicStorePropNetPrimaryService);
    if (!isString(string)) 
        goto error;
    
    key = SCDynamicStoreKeyCreateNetworkServiceEntity(0, kSCDynamicStoreDomainState, string, kSCEntNetIPv4);
    CFRelease(dict);
    dict = SCDynamicStoreCopyValue(store, key);
    CFRelease(key);

    if (!isDictionary(dict)) 
        goto error;

    array = CFDictionaryGetValue(dict, kSCPropNetIPv4Addresses);
    if (!isArray(array)) 
        goto error;
    
    string = CFArrayGetValueAtIndex(array, 0);
    if (!isString(string)) 
        goto error;
    
    /* we got the address ! */
    CFRetain(string);
    
    CFRelease(dict);
    CFRelease(store);
    return string;
    
error:
    if (dict)
        CFRelease(dict);
    CFRelease(store);
    return 0;
}

