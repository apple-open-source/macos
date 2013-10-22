/*
 * Copyright (c) 2005-2007 Apple Inc. All Rights Reserved.
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
 *  BLPreserveBootArgs.c
 *  bless
 *
 *  Created by Shantonu Sen on 11/16/05.
 *  Copyright 2005-2007 Apple Inc. All Rights Reserved.
 *
 */

#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>

#include "bless.h"
#include "bless_private.h"

static const char *remove_boot_args[] = {
	"rd",	/* rd=(enet|disk0|md0) */
	"rp",	/* rp=nfs:1.2.3.4:/foo:bar.dmg, netboot */
	"boot-uuid",	/* UUID of filesystem/partition for rooting */
	NULL
};

int BLPreserveBootArgs(BLContextPtr context,
                       const char *input,
                       char *output,
                       int outputLen)
{
    char oldbootargs[1024];
    char bootargs[1024];
    size_t bootleft=sizeof(bootargs)-1;
    char *token, *restargs;
    int firstarg=1;
    
    
    strncpy(oldbootargs, input, sizeof(oldbootargs)-1);
    oldbootargs[sizeof(oldbootargs)-1] = '\0';
    
    memset(bootargs, 0, sizeof(bootargs));
    
    contextprintf(context, kBLLogLevelVerbose,  "Old boot-args: %s\n", oldbootargs);
    
    restargs = oldbootargs;
    while((token = strsep(&restargs, " ")) != NULL) {
        int shouldbesaved = 1, i;
        contextprintf(context, kBLLogLevelVerbose, "\tGot token: %s\n", token);
        for(i=0; remove_boot_args[i]; i++) {
            // see if it's something we want
            if(remove_boot_args[i][0] == '-') {
                // -v style
                if(strcmp(remove_boot_args[i], token) == 0) {
                    shouldbesaved = 0;
                    break;
                }
            } else {
                // debug= style
                int keylen = strlen(remove_boot_args[i]);
                if(strlen(token) >= keylen+1
                   && strncmp(remove_boot_args[i], token, keylen) == 0
                   && token[keylen] == '=') {
                    shouldbesaved = 0;
                    break;
                }
            }
        }
        
        if(shouldbesaved) {
            // append to bootargs if it should be preserved
            if(firstarg) {
                firstarg = 0;
            } else {
                strncat(bootargs, " ", bootleft);
                bootleft--;
            }
            
            contextprintf(context, kBLLogLevelVerbose,  "\tPreserving: %s\n", token);
            strncat(bootargs, token, bootleft);
            bootleft -= strlen(token);
        }
    }
    
    strlcpy(output, bootargs, outputLen);
    
    return 0;
}
