/*
 * Copyright (c) 2007 Apple Inc. All rights reserved.
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
#include <unistd.h>
#include <sys/types.h>
#include <sys/attr.h>
#include <stdint.h>
#include <stdio.h>
#include <err.h>

struct volinfobuf {
  uint32_t info_length;
  uint32_t finderinfo[8];
}; 


int main(int argc, char *argv[]) {
    int ret, i;
    struct volinfobuf vinfo;
    struct attrlist alist;


    alist.bitmapcount = 5;
    alist.reserved = 0;
    alist.commonattr = ATTR_CMN_FNDRINFO;
    alist.volattr = ATTR_VOL_INFO;
    alist.dirattr = 0;
    alist.fileattr = 0;
    alist.forkattr = 0;
    
    ret = getattrlist(argv[1], &alist, &vinfo, sizeof(vinfo), 0);
    if(ret)
      err(1, "getattrlist");

    printf("%u\n", ntohl(vinfo.finderinfo[0]));
    
    return 0;
}

