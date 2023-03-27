/*
 * Copyright (c) 1999-2022 Apple Inc. All rights reserved.
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
 * Copyright (c) 1992, 1993, 1994
 *    The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Rick Macklem at The University of Guelph.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *    This product includes software developed by the University of
 *    California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <System/sys/socket.h>
#include <sys/ioctl.h>
#include <oncrpc/auth.h>
#include <oncrpc/clnt.h>
#include <oncrpc/types.h>
#include <nfs/rpcv2.h>

#include "mountd.h"
#include "nfs_prot.h"
#include "pathnames.h"
#include "nfs_prot_rpc.h"

#import <XCTest/XCTest.h>

#define MESSAGES 9

static int nsock = -1;
static CLIENT *nclnt = NULL;
static nfs_fh3 rootfh = {};

enum nfsstat_mode {
	MODE_ZERO = 0,
	MODE_SERVER_STATS = 1,
};

static NSData *
runNFSStat(enum nfsstat_mode mode)
{
	NSData *data = NULL;

#if TARGET_OS_OSX /* NSTask is supported only by MacOS */
	NSPipe *pipe;
	NSTask *task;
	NSFileHandle *file;

	pipe = [NSPipe pipe];
	file = pipe.fileHandleForReading;

	task = [[NSTask alloc] init];
	task.launchPath = @"/usr/bin/nfsstat";
	task.arguments = mode == MODE_ZERO ? @[@"-z"] : @[@"-s", @"-f", @"JSON"];
	task.standardOutput = pipe;

	[task launch];

	if (!MODE_ZERO) {
		data = [file readDataToEndOfFile];
	}
	[file closeFile];
#endif /* TARGET_OS_OSX */

	return data;
}

struct serverCacheResults {
	int inprog;
	int idem;
	int nonidem;
	int misses;
};

void
verifyReplyCacheStatistics(struct serverCacheResults results)
{
	long value;
	NSNumber *number;
	NSData* returnedData;
	NSError* error = nil;
	NSDictionary *jsonDictionary, *serverInfo, *serverCacheStats;

	returnedData = runNFSStat(MODE_SERVER_STATS);
	if (!returnedData) {
		XCTFail("Failed to read stats");
	}

	id object = [NSJSONSerialization
	    JSONObjectWithData:returnedData
	    options:0
	    error:&error];

	if (error) {
		XCTFail("JSON was malformed");
	}

	if ([object isKindOfClass:[NSDictionary class]]) {
		jsonDictionary = object;
		serverInfo = [jsonDictionary objectForKey:@"Server Info"];
		serverCacheStats = [serverInfo objectForKey:@"Server Cache Stats"];

		number = [serverCacheStats objectForKey:@"Inprog"];
		XCTAssertNotNil(number);
		[number getValue:&value];
		XCTAssertEqual(results.inprog, value);

		number = [serverCacheStats objectForKey:@"Idem"];
		XCTAssertNotNil(number);
		[number getValue:&value];
		XCTAssertEqual(results.idem, value);

		number = [serverCacheStats objectForKey:@"Non-idem"];
		XCTAssertNotNil(number);
		[number getValue:&value];
		XCTAssertEqual(results.nonidem, value);

		number = [serverCacheStats objectForKey:@"Misses"];
		XCTAssertNotNil(number);
		[number getValue:&value];
		XCTAssertEqual(results.misses, value);
	} else {
		XCTFail("Unexpected error");
	}
}

@interface nfsrvTests_replyCache : XCTestCase

@end

@implementation nfsrvTests_replyCache

- (void)setUp
{
	int err;
	fhandle_t *fh;
	doMountSetUp();

	err = createClientForMountProtocol(AF_INET, SOCK_DGRAM, RPCAUTH_UNIX, 0);
	if (err) {
		XCTFail("Cannot create client mount: %d", err);
	}

	nclnt = createClientForNFSProtocol(AF_INET, SOCK_DGRAM, RPCAUTH_UNIX, 0, &nsock);
	if (nclnt == NULL) {
		XCTFail("Cannot create client for NFS");
	}

	memset(&rootfh, 0, sizeof(rootfh));
	if ((fh = doMountAndVerify(getLocalMountedPath())) == NULL) {
		XCTFail("doMountAndVerify failed");
	}

	rootfh.data.data_len = fh->fh_len;
	rootfh.data.data_val = (char *)fh->fh_data;

	runNFSStat(MODE_ZERO);
}

- (void)tearDown
{
	memset(&rootfh, 0, sizeof(rootfh));
	doMountTearDown();

	if (nclnt) {
		clnt_destroy(nclnt);
		nclnt = NULL;
	}
}


/*
 * 1. Create new file
 * 2. Obtain the filehandle using LOOKUP
 * 3. Retransmit the same LOOKUP
 * 4. Verify "misses" counter was incremented
 */
- (void)testReplyCacheMisses
{
	LOOKUP3res *res;
	int dirFD, fileFD, err;
	char *file = "new_file";
	struct serverCacheResults results = { .inprog = 0, .idem = 0, .nonidem = 0, .misses = 2 };

	err = createFileInPath(getLocalMountedPath(), file, &dirFD, &fileFD);
	if (err) {
		XCTFail("createFileInPath failed, got %d", err);
		return;
	}

	res = doLookupRPC(nclnt, &rootfh, file);
	if (res->status != NFS3_OK) {
		XCTFail("doLookupRPC failed, got %d", res->status);
	}

	res = doLookupRPC(nclnt, &rootfh, file);
	if (res->status != NFS3_OK) {
		XCTFail("doLookupRPC failed, got %d", res->status);
	}

	removeFromPath(file, dirFD, fileFD, REMOVE_FILE);

	verifyReplyCacheStatistics(results);
}

@end
