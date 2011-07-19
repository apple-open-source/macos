/*
 * Copyright (c) 2011 Apple Inc. All rights reserved.
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
#include <errno.h>
#include <sys/types.h>
#include <unistd.h>

#include <SMBClient/smbclient.h>
#include <SMBClient/smbclient_internal.h>

/*
 * This is a bit ugly.
 *
 * We're doing this in a subprocess so that we run with the GSSD special
 * port that the kernel asked automountd to use.
 *
 * However, we need to be passed a Rodent Of Unusual Size, err, umm, a
 * blob of variable size to pass to the SMBRemountServer call.  That's
 * a bit ugly to pass on the command line, so we do it over a pipe.
 *
 * The first thing we read from the pipe is a big-endian 4-byte byte
 * count.  Then we allocate a buffer and read the rest of the data
 * into that.
 */
int
main()
{
	ssize_t bytes_read;
	uint32_t byte_count;
	void *blob;
	int child_pid;
	int status;

	/*
	 * Read the byte count.
	 */
	bytes_read = read(0, &byte_count, sizeof byte_count);
	if (bytes_read == -1) {
		fprintf(stderr, "smbremountserver: Error reading byte count: %s\n",
		    strerror(errno));
		return 2;
	}
	if (bytes_read < 0 || (size_t)bytes_read != sizeof byte_count) {
		fprintf(stderr, "smbremountserver: Read only %zd bytes of byte count\n",
		    bytes_read);
		return 2;
	}

	if (byte_count == 0) {
		fprintf(stderr, "smbremountserver: byte count is 0\n");
		return 2;
	}

	blob = malloc(byte_count);
	if (blob == NULL) {
		fprintf(stderr, "smbremountserver: Can't allocate %u bytes\n",
		    byte_count);
		return 2;
	}

	bytes_read = read(0, blob, byte_count);
	if (bytes_read == -1) {
		fprintf(stderr, "smbremountserver: Error reading blob: %s\n",
		    strerror(errno));
		return 2;
	}
	if (bytes_read < 0 || bytes_read != (ssize_t)byte_count) {
		fprintf(stderr, "smbremountserver: Read only %zd bytes of %u-byte blob\n",
		    bytes_read, byte_count);
		return 2;
	}
	
	/*
	 * OK, do this in a subsubprocess, so our parent can wait for us
	 * to exit and thus reap us, without blocking waiting for
	 * SMBRemountServer() to finish.
	 */
	switch ((child_pid = fork())) {

	case -1:
		/*
		 * Fork failure.  Report an error and quit.
		 */
		fprintf(stderr, "smbremountserver: Cannot fork: %s\n",
		    strerror(errno));
		status = 2;
		break;

	case 0:
		/*
		 * Child.  Make the call, and quit.
		 */
		SMBRemountServer(blob, byte_count);
		status = 0;
		break;

	default:
		/*
		 * Parent.  Just quit.
		 */
		status = 0;
		break;
	}

	return status;
}
