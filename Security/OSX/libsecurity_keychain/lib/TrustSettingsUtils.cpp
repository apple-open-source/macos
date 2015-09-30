/*
 * Copyright (c) 2005,2011-2014 Apple Inc. All Rights Reserved.
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
 * TrustSettingsUtils.cpp - Utility routines for TrustSettings module
 *
 */

#include "TrustSettingsUtils.h"
#include <Security/cssmtype.h>
#include <Security/cssmapple.h>
#include <Security/oidscert.h>
#include <assert.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/fcntl.h>

/* 
 * Preferred location for user root store is ~/Library/Keychain/UserRootCerts.keychain. 
 * If we're creating a root store and there is a file there we iterate thru  
 * ~/Library/Keychains/UserRootCerts_N.keychain, 0 <= N <= 10.
 */
#define kSecUserRootStoreBase			"~/Library/Keychains/UserRootCerts"
#define kSecUserRootStoreExtension		".keychain"

namespace Security {

namespace KeychainCore {

/*
 * Read entire file. 
 */
int tsReadFile(
	const char		*fileName,
	Allocator		&alloc,
	CSSM_DATA		&fileData)		// mallocd via alloc and RETURNED
{
	int rtn;
	int fd;
	struct stat	sb;
	unsigned size;
	
	fileData.Data = NULL;
	fileData.Length = 0;
	fd = open(fileName, O_RDONLY, 0);
	if(fd < 0) {
		return errno;
	}
	rtn = fstat(fd, &sb);
	if(rtn) {
		goto errOut;
	}
	size = (unsigned)sb.st_size;
	fileData.Data = (uint8 *)alloc.malloc(size);
	if(fileData.Data == NULL) {
		rtn = ENOMEM;
		goto errOut;
	}
	rtn = (int)lseek(fd, 0, SEEK_SET);
	if(rtn < 0) {
		goto errOut;
	}
	rtn = (int)read(fd, fileData.Data, (size_t)size);
	if(rtn != (int)size) {
		rtn = EIO;
	}
	else {
		rtn = 0;
		fileData.Length = size;
	}
errOut:
	close(fd);
	return rtn;
}

} /* end namespace KeychainCore */

} /* end namespace Security */
