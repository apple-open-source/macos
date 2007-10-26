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
// blob - generic extensible binary blob frame
//
#include "blob.h"
#include <security_utilities/unix++.h>

namespace Security {


//
// Read a blob from a standard file stream.
// Reads in one pass, so it's suitable for transmission over pipes and networks.
// The blob is allocated with malloc(3).
// On error, sets errno and returns NULL; in which case the input stream may
// be partially consumed.
//
BlobCore *BlobCore::readBlob(int fd, uint32_t magic, size_t blobSize)
{
	BlobCore header;
	if (read(fd, &header, sizeof(header)) != sizeof(header))
		return NULL;
	if (magic && !header.validateBlob(magic, blobSize)) {
		errno = EINVAL;
		return NULL;
	}
	if (BlobCore *blob = (BlobCore *)malloc(header.length())) {
		memcpy(blob, &header, sizeof(header));
		size_t remainder = header.length() - sizeof(header);
		if (read(fd, blob+1, remainder) != ssize_t(remainder)) {
			free(blob);
			errno = EINVAL;
			return NULL;
		}
		return blob;
	} else {
		return NULL;
	}
}

BlobCore *BlobCore::readBlob(std::FILE *file, uint32_t magic, size_t blobSize)
{
	BlobCore header;
	if (fread(&header, sizeof(header), 1, file) != 1)
		return NULL;
	if (magic && !header.validateBlob(magic, blobSize)) {
		errno = EINVAL;
		return NULL;
	}
	if (BlobCore *blob = (BlobCore *)malloc(header.length())) {
		memcpy(blob, &header, sizeof(header));
		if (fread(blob+1, header.length() - sizeof(header), 1, file) != 1) {
			free(blob);
			errno = EINVAL;
			return NULL;
		}
		return blob;
	} else {
		return NULL;
	}
}


//
// BlobWrappers
//
BlobWrapper *BlobWrapper::alloc(size_t length)
{
	size_t wrapLength = length + sizeof(BlobCore);
	BlobWrapper *w = (BlobWrapper *)malloc(wrapLength);
	w->initialize(wrapLength);
	return w;
}

BlobWrapper *BlobWrapper::alloc(const void *data, size_t length)
{
	BlobWrapper *w = alloc(length);
	memcpy(w->data(), data, w->length());
	return w;
}


}	// Security
