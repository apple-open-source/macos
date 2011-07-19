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
#include <netinet/in.h>
#include <net/if_types.h>
#include <ifaddrs.h>
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
#include "vpnoptions.h"
#include "cf_utils.h"
#include "ipsec_utils.h"


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
u_long load_kext(char *kext, int byBundleID)
{
    int pid;

    if ((pid = fork()) < 0)
        return 1;

    if (pid == 0) {
        closeall();
        // PPP kernel extension not loaded, try load it...
		if (byBundleID)
			execle("/sbin/kextload", "kextload", "-b", kext, (char *)0, (char *)0);
		else
			execle("/sbin/kextload", "kextload", kext, (char *)0, (char *)0);
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
    
#if !TARGET_OS_EMBEDDED // This file is not built for Embedded
        if (!noload && !load_kext(PPP_NKE_PATH, 0))
#else
        if (!noload && !load_kext(PPP_NKE_ID, 1))
#endif
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

/* ----------------------------------------------------------------------------
	find the interface that has address target_address assigned and return
	the interface name and its primary_address
	Return code:
	0 if successful, -1 otherwise.
 ---------------------------------------------------------------------------- */
int get_interface(struct sockaddr_in *primary_address, const struct sockaddr_in *target_address, char *interface) 
{
    struct ifaddrs *ifap = NULL;
	int				ret = -1;
	
	
	if (getifaddrs(&ifap) == 0) {
		struct ifaddrs *ifa, *ifa1;
		for (ifa = ifap; ifa; ifa = ifa->ifa_next) {

			if (ifa->ifa_name  
					&& ifa->ifa_addr
					&& ifa->ifa_addr->sa_family == target_address->sin_family
					&& ((struct sockaddr_in *)ifa->ifa_addr)->sin_addr.s_addr == target_address->sin_addr.s_addr) {
					
				strncpy(interface, ifa->ifa_name, IF_NAMESIZE);
				
				if (primary_address) {
					for (ifa1 = ifap; ifa1; ifa1 = ifa1->ifa_next) {
						
						if (ifa1->ifa_name 
							&& !strncmp(ifa1->ifa_name, interface, IFNAMSIZ)
							&& ifa1->ifa_addr
							&& target_address
							&& ifa1->ifa_addr->sa_family == target_address->sin_family) {
								
							bcopy(ifa1->ifa_addr, primary_address, sizeof(*primary_address));
							break;
						}
					}
				}
				ret = 0;
				break;
			}
					
		}
		
		freeifaddrs(ifap);
	}

	return ret;
}

/* ----------------------------------------------------------------------------
	check if a given interface (or any if null) has the address assigned
	Return code:
	1 if address is found.
 ---------------------------------------------------------------------------- */
int find_address(const struct sockaddr_in *address, char *interface) 
{
    struct ifaddrs *ifap = NULL;
	int				found = 0;
	
	if (getifaddrs(&ifap) == 0) {
		struct ifaddrs *ifa;
		for (ifa = ifap; ifa; ifa = ifa->ifa_next) {

			if (ifa->ifa_name  
					&& ifa->ifa_addr
					&& (!interface || !strncmp(interface, ifa->ifa_name, IFNAMSIZ))
					&& ifa->ifa_addr->sa_family == address->sin_family
					&& ((struct sockaddr_in *)ifa->ifa_addr)->sin_addr.s_addr ==  address->sin_addr.s_addr) {
									
				found = 1;
				break;
			}
					
		}
		
		freeifaddrs(ifap);
	}

	return found;
}

/* ----------------------------------------------------------------------------
	get the address and return the interface name and main address of the interface
	Return code:
	0 if successful, -1 otherwise.
 ---------------------------------------------------------------------------- */
int get_route_interface(struct sockaddr *src, const struct sockaddr *dst, char *if_name) 
{
	int				ret = -1;
	
	// look if the cluster address is already assigned to an interface			
	if (ret = get_interface((struct sockaddr_in *)src, (struct sockaddr_in *)dst, if_name)) {
	
		// if not, then look ask the routing table for the interface to the cluster address is already assigned to an interface			
		ret = get_src_address(src, dst, if_name);
				
	}

	return ret;
}


// ----------------------------------------------------------------------------
//	read function
// ----------------------------------------------------------------------------
int readn(int ref, void *data, int len)
{
    int 	n, left = len;
    void 	*p = data;
    
    while (left > 0) {
        if ((n = read(ref, p, left)) < 0) {
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

// ----------------------------------------------------------------------------
//	write function
// ----------------------------------------------------------------------------
int writen(int ref, void *data, int len)
{	
    int 	n, left = len;
    void 	*p = data;
    
    while (left > 0) {
        if ((n = write(ref, p, left)) <= 0) {
            if (errno != EINTR) 
                return -1;
            n = 0;
        }
        left -= n;
        p += n;
    }
    return len;
}        

