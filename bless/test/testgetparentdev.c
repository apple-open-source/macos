/*
 * Copyright (c) 2003-2007 Apple Inc. All Rights Reserved.
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
 * You can do a pretty grueling self-test with:
 * find -x /System -print0 | xargs -0 -n 1 ./build/testgetfileid  | grep -v ^Success:
 */

#define DEBUG 1

#include <libc.h>
#include <stdint.h>
#include <inttypes.h>
#include <sys/mount.h>
#include <bless.h>
#include <AssertMacros.h>

void usage() {
    fprintf(stderr, "Usage: %s device\n", getprogname());
    exit(1);
}

int main(int argc, char *argv[]) {

    char *dev;
    char parentDev[MNAMELEN];
    unsigned long slice = 0;
    BLPartitionType pmapType;
    char *typestr;
    
    if(argc != 2) {
	usage();
    }

    dev = argv[1];
    
    require_noerr(BLGetParentDeviceAndPartitionType(NULL,
					       dev,
					       parentDev,
					       &slice,
					       &pmapType),
		  failed);

    printf("Partition: %s\n", dev);
    printf("Parent: %s\n", parentDev);
    printf("Slice: %lu\n", slice);
    if(pmapType == kBLPartitionType_APM) {
	typestr = "Apple Partition Map";
    } else if(pmapType == kBLPartitionType_MBR) {
	typestr = "FDisk Partition Map";
    } else {
	typestr = "Unknown";
    }

    printf("Partition Type: %s\n", typestr);
    
    return 0;

failed:
	return 1;
}

