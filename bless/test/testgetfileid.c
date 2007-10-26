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
#include <bless.h>
#include <AssertMacros.h>

void usage() {
    fprintf(stderr, "Usage: %s [ mountpoint cnid | path ]\n", getprogname());
    exit(1);
}

int main(int argc, char *argv[]) {

    if(argc < 2 || argc > 3) {
	usage();
    }

    if(argc == 2) {
	char *path = argv[1];
	int isHFS = 0;
	struct stat sb;
	struct statfs sf;
	uint32_t fileID = 0;
	char newpath[MAXPATHLEN];

	require_noerr(lstat(path, &sb), cantStat);
	if(S_ISLNK(sb.st_mode)) {
	  printf("Success: %s is a symlink\b", path);
	  return 0;
	}

	require_noerr(statfs(path, &sf), cantStat);
	require_noerr(BLIsMountHFS(NULL, path, &isHFS), notHFS);
	require(isHFS == 1, notHFS);

	require_noerr(BLGetFileID(NULL, path, &fileID), error);
	require(fileID == sb.st_ino, error);

	require_noerr(BLLookupFileIDOnMount(NULL,
			   sf.f_mntonname,
			   fileID,
			       newpath), error);
	
	require(strcmp(newpath, path) == 0, cantstrcmp);
	
	printf("Success: %s\n", newpath);
	
	return 0;
	
    error:
	printf("Error: %s\n", path);
	 return 1;
    cantstrcmp:
	 printf("%s != %s\n", newpath, path);
	 return 1;
cantStat:
	printf("%s: %s\n",path, strerror(errno));
	return 1;
notHFS:
	printf("%s not on an HFS+ filesystem\n", path);
	return 1;
    } else if(argc == 3) {
	char *path = argv[1];
	int isHFS = 0;
	struct statfs sf;
	uint32_t fileID = 0;
	char newpath[MAXPATHLEN];

	fileID = strtoul(argv[2], NULL, 0);
	require(fileID > 0, error2);
	
	require_noerr(statfs(path, &sf), cantStat2);
	require_noerr(BLIsMountHFS(NULL, path, &isHFS), notHFS2);
	require(isHFS == 1, notHFS2);

	require_noerr(BLLookupFileIDOnMount(NULL,
				     sf.f_mntonname,
				     fileID,
				     newpath), error2);

	printf("Success: %s\n", newpath);

	return 0;

error2:
	    return 1;
cantStat2:
	    printf("%s: %s\n",path, strerror(errno));
	return 1;
notHFS2:
	    printf("%s not on an HFS+ filesystem\n", path);
	return 1;
    }
    

	
    return 0;
}

