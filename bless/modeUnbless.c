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
 *  modeUnbless.c
 *  bless
 *
 *  Created by Shantonu Sen on 7/23/09.
 *  Copyright 2009 Apple Inc. All rights reserved.
 *
 */


#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/mount.h>
#include <sys/param.h>

#include "enums.h"
#include "structs.h"

#include "bless.h"
#include "bless_private.h"
#include "protos.h"

int modeUnbless(BLContextPtr context, struct clarg actargs[klast]) {
	
    int ret;
    int isHFS;
	
    struct statfs sb;
    
	if (!actargs[kunbless].present) {
		blesscontextprintf(context, kBLLogLevelError, "No volume specified\n" );
		return 1;
	}
	
	ret = BLGetCommonMountPoint(context, actargs[kunbless].argument, "", actargs[kmount].argument);
	if(ret) {
		blesscontextprintf(context, kBLLogLevelError,  "Can't determine mount point of '%s'\n", actargs[kunbless].argument );
	} else {
		blesscontextprintf(context, kBLLogLevelVerbose,  "Mount point is '%s'\n", actargs[kmount].argument );
	}
		
    ret = BLIsMountHFS(context, actargs[kmount].argument, &isHFS);
    if(ret) {
		blesscontextprintf(context, kBLLogLevelError,  "Could not determine filesystem of %s\n", actargs[kmount].argument );
		return 1;
    }
    
    if(0 != statfs(actargs[kmount].argument, &sb)) {
        blesscontextprintf(context, kBLLogLevelError,  "Can't statfs %s\n" ,
                           actargs[kmount].argument);
        return 1;	    
    }
    
    
    if(isHFS) {
		uint32_t oldwords[8];
        
		ret = BLGetVolumeFinderInfo(context, actargs[kmount].argument, oldwords);
		if(ret) {
			blesscontextprintf(context, kBLLogLevelError,  "Error getting old Finder info words for %s\n", actargs[kmount].argument );
			return 1;
		}
		
		/* unbless! unbless */
		
		oldwords[0] = 0;
		oldwords[1] = 0;
		oldwords[2] = 0;
		oldwords[3] = 0;
		oldwords[4] = 0;
		oldwords[5] = 0;
		
		blesscontextprintf(context, kBLLogLevelVerbose,  "finderinfo[0] = %d\n", oldwords[0] );
		blesscontextprintf(context, kBLLogLevelVerbose,  "finderinfo[1] = %d\n", oldwords[1] );
		blesscontextprintf(context, kBLLogLevelVerbose,  "finderinfo[2] = %d\n", oldwords[2] );
		blesscontextprintf(context, kBLLogLevelVerbose,  "finderinfo[3] = %d\n", oldwords[3] );
		blesscontextprintf(context, kBLLogLevelVerbose,  "finderinfo[4] = %d\n", oldwords[4] );
		blesscontextprintf(context, kBLLogLevelVerbose,  "finderinfo[5] = %d\n", oldwords[5] );
		
		
		if(geteuid() != 0 && geteuid() != sb.f_owner) {
		    blesscontextprintf(context, kBLLogLevelError,  "Authorization required\n" );
			return 1;
		}
		
		ret = BLSetVolumeFinderInfo(context,  actargs[kmount].argument, oldwords);
		if(ret) {
			blesscontextprintf(context, kBLLogLevelError,  "Can't set Finder info fields for volume mounted at %s: %s\n", actargs[kmount].argument , strerror(errno));
			return 2;
		}
		
    }
		
    return 0;
}
