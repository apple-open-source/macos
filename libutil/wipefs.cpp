/*
 * Copyright (c) 2008 Apple Inc. All rights reserved.
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
//	wipefs.cpp
//

#include <fcntl.h>
#include <unistd.h>
#include <sys/uio.h>
#include <sys/ioctl.h>
#include <sys/disk.h>
#include <sys/stat.h>
#include <sys/syslimits.h>
#include <uuid/uuid.h>
#include <paths.h>
#include <string.h>
#include <spawn.h>
#include <IOKit/IOKitLib.h>
#include <IOKit/storage/IOMedia.h>
#include <IOKit/storage/IOStorageProtocolCharacteristics.h>
#include <CoreFoundation/CFNumber.h>
#include <os/log.h>

#include "ExtentManager.h"
#include "wipefs.h"

#define	wipefs_roundup(x, y)	((((x)+((y)-1))/(y))*(y))

struct __wipefs_ctx {
	int fd;
	class ExtentManager extMan;
    
	// xartutil information
	bool have_xartutil_info;
	uuid_string_t uuid_str;
	bool is_internal;
};

static void
AddExtentsForFutureFS(class ExtentManager *extMan)
{
	// we don't know what blocks future FS will use to recognize itself.  But we'd better be safe than sorry and write
	// the first and last 2MB of the volume
	off_t size = 2 * 1024 * 1024;
	extMan->AddByteRangeExtent(0, size);
	extMan->AddByteRangeExtent(extMan->totalBytes - size, size);
}

static void
AddExtentsForHFS(class ExtentManager *extMan)
{
	// first 1KB is boot block, last 512B is reserved
	// the Volume Header (512B) is after 1KB and before the last 512B
	extMan->AddByteRangeExtent(0, 1024 + 512);
	extMan->AddByteRangeExtent(extMan->totalBytes - 1024, 1024);
}

static void
AddExtentsForMSDOS(class ExtentManager *extMan)
{
	// MSDOS needs the first block (in theory, up to 32KB)
	extMan->AddByteRangeExtent(0, 32 * 1024);
}

static void
AddExtentsForNTFS(class ExtentManager *extMan)
{
	// NTFS supports block size from 256B to 32768B.  The first, middle and last block are needed
	extMan->AddByteRangeExtent(0, 32 * 1024);
	extMan->AddByteRangeExtent(extMan->totalBytes - 32 * 1024, 32 * 1024);
	// to be safe, add the rage from (mid_point - 32KB) to (mid_point + 32KB)
	extMan->AddByteRangeExtent(extMan->totalBytes / 2 - 32 * 1024, 64 * 1024);
}

static void
AddExtentsForUDF(class ExtentManager *extMan)
{
	off_t lastBlockAddr = extMan->totalBlocks - 1;

	// Volume Recognization Sequence (VRS) starts at 32KB, usually less than 7 Volume Structure Descriptors (2KB each)
	extMan->AddByteRangeExtent(32 * 1024, 14 * 1024);

	// AVDP is on 256, 512, last block, last block - 256
	extMan->AddBlockRangeExtent(256, 1);
	extMan->AddBlockRangeExtent(512, 1);
	extMan->AddBlockRangeExtent(lastBlockAddr, 1);
	extMan->AddBlockRangeExtent(lastBlockAddr - 256, 1);

	// to be safe, assume the device has 2KB block size and do it again
	if (extMan->blockSize != 2048) {
		off_t blockSize = 2048;
		// AVDP is on 256, 512, last block, last block - 256
		extMan->AddByteRangeExtent(256 * blockSize, blockSize);
		extMan->AddByteRangeExtent(512 * blockSize, blockSize);
		extMan->AddByteRangeExtent(extMan->totalBytes - blockSize, blockSize);
		extMan->AddByteRangeExtent(extMan->totalBytes - 256 * blockSize, blockSize);
	}
}

static void
AddExtentsForUFS(class ExtentManager *extMan)
{
	// UFS super block is 8KB at offset 8KB
	extMan->AddByteRangeExtent(8192, 8192);
}

static void
AddExtentsForZFS(class ExtentManager *extMan)
{
	// ZFS needs the first 512KB and last 512KB for all the 4 disk labels
	extMan->AddByteRangeExtent(0, 512 * 1024);
	extMan->AddByteRangeExtent(extMan->totalBytes - 512 * 1024, 512 * 1024);
}

static void
AddExtentsForPartitions(class ExtentManager *extMan)
{
	// MBR (Master Boot Record) needs the first sector
	// APM (Apple Partition Map) needs the second sector
	// GPT (GUID Partition Table) needs the first 34 and last 33 sectors
	extMan->AddByteRangeExtent(0, 512 * 34);
	extMan->AddByteRangeExtent(extMan->totalBytes - 512 * 33, 512 * 33);
}

static void
AddExtentsForCoreStorage(class ExtentManager *extMan)
{
	// the CoreStorage VolumeHeader structures reside in the first/last 512 bytes of each PV
	extMan->AddByteRangeExtent(0, 512);
	extMan->AddByteRangeExtent(extMan->totalBytes - 512, 512);
}

static bool
is_disk_device(const char *pathname) {
	bool is_disk_dev = false;

	if (strncmp(pathname, "/dev/disk", strlen("/dev/disk")) == 0) {
		is_disk_dev = true;
	} else if (strncmp(pathname, "/dev/rdisk", strlen("/dev/rdisk")) == 0) {
		is_disk_dev = true;
	}

	return (is_disk_dev);
}

static int
query_disk_info(int fd, char *uuid_str, int uuid_len, bool *is_internal) {
	io_service_t obj;
	io_iterator_t iter;
	kern_return_t error;
	CFBooleanRef removableRef;
	CFStringRef uuidRef;
	CFStringRef locationRef;
	CFDictionaryRef protocolCharacteristics;
	bool deviceInternal, mediaRemovable;
	int result;
	char disk_path[PATH_MAX];
	char *disk_name;

	result = EINVAL;
	deviceInternal = false;
	mediaRemovable = false;
	removableRef = NULL;
	protocolCharacteristics = NULL;
	uuidRef = NULL;
	obj = IO_OBJECT_NULL;
	iter = IO_OBJECT_NULL;

	// Fetch the path
	if (fcntl(fd, F_GETPATH, disk_path) == -1) {
		goto out;
	}

	// Make sure path begins with /dev/disk or /dev/rdisk
	if (is_disk_device(disk_path) == false) {
		goto out;
	}

	// Derive device name
	disk_name = disk_path;
	if(strncmp(disk_path, _PATH_DEV, strlen(_PATH_DEV)) == 0) {
		// Skip over leading "/dev/"
		disk_name += strlen(_PATH_DEV);
	}
	if (strncmp(disk_name, "r", strlen("r")) == 0) {
		// Raw device, skip over leading "r"
		disk_name += strlen("r");
	}
    
	// Get an iterator object
	error = IOServiceGetMatchingServices(kIOMasterPortDefault,
										 IOBSDNameMatching(kIOMasterPortDefault, 0, disk_name), &iter);
	if (error) {
		os_log_error(OS_LOG_DEFAULT, "Warning, unable to obtain UUID info (iterator), device %s", disk_name);
		goto out;
	}
    
	// Get the matching device object
	obj = IOIteratorNext(iter);
	if (!obj) {
		os_log_error(OS_LOG_DEFAULT, "Warning, unable to obtain UUID info (dev obj), device %s", disk_name);
		goto out;
	}
	
	// Get a protocol characteristics dictionary
	protocolCharacteristics = (CFDictionaryRef) IORegistryEntrySearchCFProperty(obj,
																				kIOServicePlane,
																				CFSTR(kIOPropertyProtocolCharacteristicsKey),
																				kCFAllocatorDefault,
																				kIORegistryIterateRecursively|kIORegistryIterateParents);

	if ((protocolCharacteristics == NULL) || (CFDictionaryGetTypeID() != CFGetTypeID(protocolCharacteristics)))
	{
		os_log_error(OS_LOG_DEFAULT, "Warning, failed to obtain UUID info (protocol characteristics), device %s\n", disk_name);
		goto out;
	}
	
	// Check for kIOPropertyInternalKey
	locationRef = (CFStringRef) CFDictionaryGetValue(protocolCharacteristics, CFSTR(kIOPropertyPhysicalInterconnectLocationKey));
	
	if ((locationRef == NULL) || (CFStringGetTypeID() != CFGetTypeID(locationRef))) {
		os_log_error(OS_LOG_DEFAULT, "Warning, failed to obtain UUID info (location), device %s\n", disk_name);
		goto out;
	}
	
	if (CFEqual(locationRef, CFSTR(kIOPropertyInternalKey))) {
		deviceInternal = true;
	}

	// Check for kIOMediaRemovableKey
	removableRef = (CFBooleanRef)IORegistryEntrySearchCFProperty(obj,
																 kIOServicePlane,
																 CFSTR(kIOMediaRemovableKey),
																 kCFAllocatorDefault,
																 0);
	
	if ((removableRef == NULL) || (CFBooleanGetTypeID() != CFGetTypeID(removableRef))) {
		os_log_error(OS_LOG_DEFAULT, "Warning, unable to obtain UUID info (MediaRemovable key), device %s", disk_name);
		goto out;
	}
    
	if (CFBooleanGetValue(removableRef)) {
		mediaRemovable = true;
	}
	
	// is_internal ==> DeviceInternal && !MediaRemovable
	if ((deviceInternal == true) && (mediaRemovable == false)) {
		*is_internal = true;
	} else {
		*is_internal = false;
	}

	// Get the UUID
	uuidRef = (CFStringRef)IORegistryEntrySearchCFProperty(obj,
														   kIOServicePlane,
														   CFSTR(kIOMediaUUIDKey),
														   kCFAllocatorDefault,
														   0);
	if ((uuidRef == NULL) || (CFStringGetTypeID() != CFGetTypeID(uuidRef)))
	{
		os_log_error(OS_LOG_DEFAULT, "Warning, unable to obtain UUID info (MediaUUID key), device %s", disk_name);
		goto out;
	}

	if (!CFStringGetCString(uuidRef, uuid_str, uuid_len, kCFStringEncodingASCII)) {
		os_log_error(OS_LOG_DEFAULT, "Warning, unable to obtain UUID info (convert UUID), device %s", disk_name);
		goto out;
	}

	// Success
	result = 0;

out:
	if (obj != IO_OBJECT_NULL) {
		IOObjectRelease(obj);
	}
	if (iter != IO_OBJECT_NULL) {
		IOObjectRelease(iter);
	}
	if (removableRef != NULL) {
		CFRelease(removableRef);
	}
	if (protocolCharacteristics != NULL) {
		CFRelease(protocolCharacteristics);
	}
	if (uuidRef != NULL) {
		CFRelease(uuidRef);
	}

	return (result);
}

static
int run_xartutil(uuid_string_t uuid_str, bool is_internal)
{
	char external[2];
	pid_t child_pid, wait_pid;
	posix_spawn_file_actions_t fileActions;
	bool haveFileActions = false;
	int child_status = 0;
	int result = 0;

	char arg1[] = "xartutil";
	char arg2[] = "--erase";
	char arg4[] = "--is-external";

	external[0] = (is_internal == false) ? '1' : '0';
	external[1] = 0;

	char *xartutil_argv[] = {arg1, arg2, uuid_str, arg4, external, NULL};

	result = posix_spawn_file_actions_init(&fileActions);
	if (result) {
		os_log_error(OS_LOG_DEFAULT, "Warning, init xartutil file actions error: %d", result);
		result = -1;
		goto out;
	}

	haveFileActions = true;
    
	// Redirect stdout & stderr (results not critical, so we ignore return values).
	posix_spawn_file_actions_addopen(&fileActions, STDOUT_FILENO, "/dev/null", O_WRONLY, 0);
	posix_spawn_file_actions_addopen(&fileActions, STDERR_FILENO, "/dev/null", O_WRONLY, 0);

	result = posix_spawn(&child_pid, "/usr/sbin/xartutil", &fileActions, NULL, xartutil_argv, NULL);

	if (result) {
		os_log_error(OS_LOG_DEFAULT, "Warning, unable to start xartutil, spawn error: %d", result);
		result = -1;
		goto out;
	}
    
	do {
		wait_pid = waitpid(child_pid, &child_status, 0);
	} while (wait_pid == -1 && errno == EINTR);

	if (wait_pid == -1) {
		os_log_error(OS_LOG_DEFAULT, "Warning, unable to start xartutil, waitpid error: %d", errno);
		result = -1;
		goto out;
	}

	if (WIFEXITED(child_status)) {
		// xartutil terminated normally, get exit status
		result = WEXITSTATUS(child_status);

		if (result) {
			os_log_error(OS_LOG_DEFAULT, "Warning, xartutil returned status %d", result);
		}
	} else {
		result = -1;

		if (WIFSIGNALED(child_status)) {
			os_log_error(OS_LOG_DEFAULT, "Warning, xartutil terminated by signal: %u", WTERMSIG(child_status));
		} else if (WIFSTOPPED(child_status)) {
			os_log_error(OS_LOG_DEFAULT, "Warning, xartutil stopped by signal: %u", WSTOPSIG(child_status));
		} else {
			os_log_error(OS_LOG_DEFAULT, "Warning, xartutil terminated abnormally, status 0x%x", child_status);
		}
	}

out:

	if (haveFileActions) {
		posix_spawn_file_actions_destroy(&fileActions);
	}

	return (result);
}

extern "C" int
wipefs_alloc(int fd, size_t block_size, wipefs_ctx *handle)
{
	int err = 0;
	uint64_t numBlocks = 0;
	uint32_t nativeBlockSize = 0;
	off_t totalSizeInBytes = 0;
	class ExtentManager *extMan = NULL;
	struct stat sbuf = { 0 };
	bool have_xartutil_info = false;
	uuid_string_t  uuid_str;
	bool is_internal;
	int uuid_err = 0;

	*handle = NULL;
	uuid_str[0] = 0;
	(void)fstat(fd, &sbuf);
	switch (sbuf.st_mode & S_IFMT) {
	case S_IFCHR:
	case S_IFBLK:
		if (ioctl(fd, DKIOCGETBLOCKSIZE, (char *)&nativeBlockSize) < 0) {
			err = errno;
			goto labelExit;
		}
		if (ioctl(fd, DKIOCGETBLOCKCOUNT, (char *)&numBlocks) < 0) {
			err = errno;
			goto labelExit;
		}
		totalSizeInBytes = numBlocks * nativeBlockSize;

		uuid_err = query_disk_info(fd, uuid_str, sizeof(uuid_str), &is_internal);
		if (uuid_err == 0) {
			have_xartutil_info = true;
		}
		break;
	case S_IFREG:
		nativeBlockSize = sbuf.st_blksize;
		numBlocks = sbuf.st_size / sbuf.st_blksize;
		totalSizeInBytes = sbuf.st_size;
		break;
	default:
		errno = EINVAL;
		goto labelExit;
	}
	if (block_size == 0) {
		block_size = nativeBlockSize;
	}
	if (block_size == 0 || totalSizeInBytes == 0) {
		err = EINVAL;
		goto labelExit;
	}

	try {
		*handle = new __wipefs_ctx;
		if (*handle == NULL) {
			bad_alloc e;
			throw e;
		}

		(*handle)->fd = fd;
		extMan = &(*handle)->extMan;

		extMan->Init(block_size, nativeBlockSize, totalSizeInBytes);
		AddExtentsForFutureFS(extMan);
		AddExtentsForHFS(extMan);
		AddExtentsForMSDOS(extMan);
		AddExtentsForNTFS(extMan);
		AddExtentsForUDF(extMan);
		AddExtentsForUFS(extMan);
		AddExtentsForZFS(extMan);
		AddExtentsForPartitions(extMan);
		AddExtentsForCoreStorage(extMan);
        
		(*handle)->have_xartutil_info = false;

		if (have_xartutil_info == true) {
			(*handle)->have_xartutil_info = true;
			(*handle)->is_internal = is_internal;
			memcpy((*handle)->uuid_str, uuid_str, sizeof(uuid_str));
		}
	}
	catch (bad_alloc &e) {
		err = ENOMEM;
	}
	catch (...) { // currently only ENOMEM is possible
		err = ENOMEM;
	}

  labelExit:
	if (err != 0) {
		wipefs_free(handle);
	}
	return err;
} // wipefs_alloc

extern "C" int
wipefs_include_blocks(wipefs_ctx handle, off_t block_offset, off_t nblocks)
{
	int err = 0;
	try {
		handle->extMan.AddBlockRangeExtent(block_offset, nblocks);
	}
	catch (bad_alloc &e) {
		err = ENOMEM;
	}
	catch (...) { // currently only ENOMEM is possible
		err = ENOMEM;
	}
	return err;
}

extern "C" int
wipefs_except_blocks(wipefs_ctx handle, off_t block_offset, off_t nblocks)
{
	int err = 0;
	try {
		handle->extMan.RemoveBlockRangeExtent(block_offset, nblocks);
	}
	catch (bad_alloc &e) {
		err = ENOMEM;
	}
	catch (...) { // currently only ENOMEM is possible
		err = ENOMEM;
	}
	return err;
}

extern "C" int
wipefs_wipe(wipefs_ctx handle)
{
	int err = 0;
	uint8_t *bufZero = NULL;
	ListExtIt curExt;
	size_t bufSize;
	dk_extent_t extent;
	dk_unmap_t unmap;
	bool did_write = false;

	memset(&extent, 0, sizeof(dk_extent_t));
	extent.length = handle->extMan.totalBytes;

	memset(&unmap, 0, sizeof(dk_unmap_t));
	unmap.extents = &extent;
	unmap.extentsCount = 1;

	//
	// Don't bother to check the return value since this is mostly
	// informational for the lower-level drivers.
	//
	ioctl(handle->fd, DKIOCUNMAP, (caddr_t)&unmap);
	

	bufSize = 128 * 1024; // issue large I/O to get better performance
	if (handle->extMan.nativeBlockSize > bufSize) {
	    bufSize = handle->extMan.nativeBlockSize;
	}
	bufZero = new uint8_t[bufSize];
	bzero(bufZero, bufSize);

	off_t byteOffset, totalBytes;
	size_t numBytes, numBytesToWrite, blockSize;

	blockSize = handle->extMan.blockSize;
	totalBytes = handle->extMan.totalBytes;
	// write zero to all extents
	for (curExt = handle->extMan.extentList.begin(); curExt != handle->extMan.extentList.end(); curExt++) {
		byteOffset = curExt->blockAddr * blockSize;
		numBytes = curExt->numBlocks * blockSize;
		// make both offset and numBytes on native block boundary
		if (byteOffset % handle->extMan.nativeBlockSize != 0 ||
			numBytes % handle->extMan.nativeBlockSize != 0) {
			size_t nativeBlockSize = handle->extMan.nativeBlockSize;
			off_t newOffset, newEndOffset;
			newOffset = byteOffset / nativeBlockSize * nativeBlockSize;
			newEndOffset = wipefs_roundup(byteOffset + numBytes, nativeBlockSize);
			byteOffset = newOffset;
			numBytes = newEndOffset - newOffset;
		}
		if (byteOffset + (off_t)numBytes > totalBytes) {
			numBytes = totalBytes - byteOffset;
		}
		while (numBytes > 0) {
			numBytesToWrite = min(numBytes, bufSize);
			if (pwrite(handle->fd, bufZero, numBytesToWrite, byteOffset) != (ssize_t)numBytesToWrite) {
				err = errno;
				goto labelExit;
			}
			numBytes -= numBytesToWrite;
			byteOffset += numBytesToWrite;

			if (did_write == false) {
				did_write = true;
			}
		}
	}

  labelExit:

	(void)ioctl(handle->fd, DKIOCSYNCHRONIZECACHE);
	if (bufZero != NULL)
		delete[] bufZero;

	if ((did_write == true) && (handle->have_xartutil_info == true)) {
		// We wrote some zero bytes and have UUID info, notify xART now.
		run_xartutil(handle->uuid_str, handle->is_internal);
	}

	return err;
} // wipefs_wipe

extern "C" void
wipefs_free(wipefs_ctx *handle)
{
	if (*handle != NULL) {
		delete *handle;
		*handle = NULL;
	}
}
