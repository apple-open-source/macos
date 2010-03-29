/*
 * Copyright (c) 2001-2007 Apple Inc. All Rights Reserved.
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
 *  BLMiscUtilities.c
 *  bless
 *
 *  Created by Shantonu Sen on Sat Apr 19 2003.
 *  Copyright (c) 2003-2007 Apple Inc. All Rights Reserved.
 *
 *  $Id: BLMiscUtilities.c,v 1.7 2006/02/20 22:49:56 ssen Exp $
 *
 */

#include "bless.h"
#include "bless_private.h"

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mount.h>

char * blostype2string(uint32_t type, char buf[5])
{
    bzero(buf, 5);
    if(type == 0) return buf;

    sprintf(buf, "%c%c%c%c",
	    (type >> 24)&0xFF,
	    (type >> 16)&0xFF,
	    (type >> 8)&0xFF,
	    (type >> 0)&0xFF);

    return buf;    
}

int blsustatfs(const char *path, struct statfs *buf)
{
    int ret;
    struct stat sb;
    char *dev = NULL;
    
    ret = statfs(path, buf);    
    if(ret)
        return ret;
	
	
    ret = stat(path, &sb);
    if(ret) 
        return ret;
    
    // figure out the true device we live on
    dev = devname(sb.st_dev, S_IFBLK);
    if(dev == NULL) {
        errno = ENOENT;
        return -1;
    }
    
    snprintf(buf->f_mntfromname, sizeof(buf->f_mntfromname), "/dev/%s", dev);
    
    return 0;
}

