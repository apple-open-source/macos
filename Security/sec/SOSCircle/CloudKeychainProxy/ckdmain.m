/*
 * Copyright (c) 2012 Apple Computer, Inc. All Rights Reserved.
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

//
//  main.m
//  ckd-xpc
//
//  Created by John Hurley on 7/19/12.
//  Copyright (c) 2012 John Hurley. All rights reserved.
//

/*
    This XPC service is essentially just a proxy to iCloud KVS, which exists since
    the main security code cannot link against Foundation.
    
    See sendTSARequestWithXPC in tsaSupport.c for how to call the service
    
    send message to app with xpc_connection_send_message
    
    For now, build this with:
    
        ~rc/bin/buildit .  --rootsDirectory=/var/tmp -noverify -offline -target CloudKeychainProxy

    and install or upgrade with:
        
        darwinup install /var/tmp/sec.roots/sec~dst
        darwinup upgrade /var/tmp/sec.roots/sec~dst
 
    You must use darwinup during development to update system caches
*/

//------------------------------------------------------------------------------------------------

#include <stdlib.h>
#include <asl.h>
#include <Foundation/Foundation.h>

extern int ckdproxymain(int argc, const char *argv[]);

int main(int argc, const char *argv[])
{
    // TODO: Remove log before ship
    asl_log(NULL, NULL, ASL_LEVEL_NOTICE, "start");
    return ckdproxymain(argc, argv);
}
