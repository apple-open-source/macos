/*
 * Copyright (c) 2005 Apple Computer, Inc. All rights reserved.
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

#include <sys/types.h>
#include <sys/uio.h>
#include <unistd.h>
#include <CoreFoundation/CoreFoundation.h>
#include "helper_comm.h"

#include <SystemConfiguration/SCPrivate.h>


static ssize_t
readn(int ref, void *data, size_t len)
{
	size_t	left	= len;
	ssize_t	n;
	void	*p	= data;

	while (left > 0) {
		if ((n = read(ref, p, left)) == -1) {
			if (errno != EINTR) {
				return -1;
			}
			n = 0;
		} else if (n == 0) {
			break; /* EOF */
		}

		left -= n;
		p += n;
	}
	return (len - left);
}


static ssize_t
writen(int ref, const void *data, size_t len)
{
	size_t		left	= len;
	ssize_t		n;
	const void	*p	= data;

	while (left > 0) {
		if ((n = write(ref, p, left)) == -1) {
			if (errno != EINTR) {
				return -1;
			}
			n = 0;
		}
		left -= n;
		p += n;
	}
	return len;
}


Boolean
__SCHelper_txMessage(int fd, uint32_t msgID, CFDataRef data)
{
	ssize_t		n_written;
	uint32_t	header[2];

	header[0] = msgID;
	header[1] = (data != NULL) ? CFDataGetLength(data) : 0;

	n_written = writen(fd, header, sizeof(header));
	if (n_written != sizeof(header)) {
		if ((n_written == -1) && (errno != EPIPE)) {
			perror("write() failed while sending msgID");
		}
		return FALSE;
	}

	if (header[1] == 0) {
		// if no data to send
		return TRUE;
	}

	n_written = writen(fd, CFDataGetBytePtr(data), header[1]);
	if (n_written != header[1]) {
		if ((n_written == -1) && (errno != EPIPE)) {
			perror("write() failed while sending data");
		}
		return FALSE;
	}

	return TRUE;
}

Boolean
__SCHelper_rxMessage(int fd, uint32_t *msgID, CFDataRef *data)
{
	void		*bytes;
	size_t		n_read;
	uint32_t	header[2];

	n_read = readn(fd, header, sizeof(header));
	if (n_read != sizeof(header)) {
		if (n_read == -1) {
			perror("read() failed while reading msgID");
		}
		return FALSE;
	}

	if (msgID != NULL) {
		*msgID = header[0];
	}

	if (header[1] == 0) {
		if (data != NULL) {
			*data = NULL;
		}
		return TRUE;
	} else if ((int32_t)header[1] < 0) {
		perror("read() failed, invalid data length");
		return FALSE;
	}

	bytes  = CFAllocatorAllocate(NULL, header[1], 0);
	n_read = readn(fd, bytes, header[1]);
	if (n_read != header[1]) {
		if (n_read == -1) {
			perror("read() failed while reading data");
		}
		CFAllocatorDeallocate(NULL, bytes);
		return FALSE;
	}

	if (data != NULL) {
		*data = CFDataCreateWithBytesNoCopy(NULL, bytes, header[1], NULL);
	} else {
		// toss reply data
		CFAllocatorDeallocate(NULL, bytes);
	}

	return TRUE;
}


Boolean
_SCHelperExec(int fd, uint32_t msgID, CFDataRef data, uint32_t *status, CFDataRef *reply)
{
	Boolean		ok;

	ok = __SCHelper_txMessage(fd, msgID, data);
	if (!ok) {
		return FALSE;
	}

	if ((status == NULL) && (reply == NULL)) {
		// if no reply expected (one way)
		return TRUE;
	}

	ok = __SCHelper_rxMessage(fd, status, reply);
	if (!ok) {
		return FALSE;
	}

	return TRUE;
}
