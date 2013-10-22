/*
 * Copyright (c) 2006 Apple Computer, Inc. All Rights Reserved.
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
// cs_misc - codesign miscellaneous operations
//
#include "codesign.h"
#include <libproc.h>
#include <sys/codesign.h>
#include <security_utilities/blob.h>
#include <sys/param.h>		// MAXPATHLEN

using namespace UnixPlusPlus;


//
// Provide low-level Code Signing information from the kernel.
// This is only of interest to geeks and testers.
//
void procinfo(const char *target)
{
	if (pid_t pid = (pid_t)atol(target)) {
		char path[MAXPATHLEN * 2];
		uint32_t flags;
		int rcpath = ::proc_pidpath(pid, path, sizeof(path));
		int rcstatus = ::csops(pid, CS_OPS_STATUS, &flags, 0);
		
		if (rcpath == 0 && rcstatus == -1) {	// neither path nor status (probably bad pid)
			perror(target);
			exit(1);
		}
		
		printf("%d:", pid);
		if (rcpath == 0)
			printf(" [path:%s]", strerror(errno));
		else
			printf(" %s", path);
		if (rcstatus == -1) {
			printf(" [flags:%s]", strerror(errno));
		} else {
			if (flags & CS_VALID) printf(" VALID");
			if (flags & CS_HARD) printf(" HARD");
			if (flags & CS_KILL) printf(" KILL");
			if (flags & CS_EXEC_SET_HARD) printf(" HARD(exec)");
			if (flags & CS_EXEC_SET_KILL) printf(" KILL(exec)");
			if (flags & ~(CS_VALID|CS_HARD|CS_KILL|CS_EXEC_SET_HARD|CS_EXEC_SET_KILL))
				printf(" (0x%x)", flags);
		}
		
		SHA1::Digest hash;
		int rchash = ::csops(pid, CS_OPS_CDHASH, hash, sizeof(hash));
		if (rchash == -1)
			printf(" [cdhash:%s]", strerror(errno));
		else
			printf(" cdhash=%s", hashString(hash).c_str());

		printf("\n");

		if (verbose > 0) {
		
			BlobCore header;
			int rcent = ::csops(pid, CS_OPS_ENTITLEMENTS_BLOB, &header, sizeof(header)); // size request
			if (rcent == 0)
				printf("[no entitlement]\n");
			else if (errno == ERANGE) {
				// kernel returns a blob header with magic == 0, length == needed size
				assert(header.magic() == 0);
				uint32_t bufferLen = (uint32_t)header.length();
				if (bufferLen > 1024 * 1024)
					fail("insane entitlement length from kernel");
				uint8_t buffer[bufferLen];

				rcent = ::csops(pid, CS_OPS_ENTITLEMENTS_BLOB, buffer, bufferLen);
				if (rcent == 0) {	// kernel says it's good
					const BlobCore *blob = (const BlobCore *)buffer;
					if (blob->length() < sizeof(*blob))
						fail("runt entitlement blob returned from kernel");
					if (blob->magic() == kSecCodeMagicEntitlement)
						fwrite(blob+1, blob->length() - sizeof(*blob), 1, stdout);
					else
						printf("Entitlement blob type 0x%x not understood\n", blob->magic());
				} else
					printf("[entitlements:%s]\n", strerror(errno));
			} else
				printf("[entitlements:%s]\n", strerror(errno));
		}
	} else {
		fail("%s: not a numeric process id", target);
	}
}


//
// Directly tell the kernel to manipulate a process's Code Signing state.
// This better be only of interest to geeks and testers.
//
void procaction(const char *target)
{
	if (pid_t pid = (pid_t)atol(target)) {
		int rc;
		if (!strncmp(procAction, "invalidate", strlen(procAction)))
			rc = ::csops(pid, CS_OPS_MARKINVALID, NULL, 0);
		else if (!strncmp(procAction, "hard", strlen(procAction)))
			rc = ::csops(pid, CS_OPS_MARKHARD, NULL, 0);
		else if (!strncmp(procAction, "kill", strlen(procAction)))
			rc = ::csops(pid, CS_OPS_MARKKILL, NULL, 0);
		else if (int op = atoi(procAction))
			rc = ::csops(pid, op, NULL, 0);
		else
			fail("%s: not a recognized operation", procAction);
		if (rc == -1)
			perror(target);
		else if (verbose)
			procinfo(target);
	} else {
		fail("%s: not a numeric process id", target);
	}
}
