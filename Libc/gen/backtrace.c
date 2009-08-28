/*
 * Copyright (c) 2007, 2008 Apple Inc. All rights reserved.
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

#include <mach/vm_types.h>
#include <sys/uio.h>

#include <dlfcn.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "stack_logging.h"
#include "execinfo.h"

int backtrace(void** buffer, int size) {
	extern void _thread_stack_pcs(vm_address_t *buffer, unsigned max, unsigned *nb, unsigned skip);
	unsigned int num_frames;
	_thread_stack_pcs((vm_address_t*)buffer, size, &num_frames, 1);
	while (num_frames >= 1 && buffer[num_frames-1] == NULL) num_frames -= 1;
	return num_frames;
}

#if __LP64__
#define _BACKTRACE_FORMAT "%-4d%-35s 0x%016lx %s + %lu"
#define _BACKTRACE_FORMAT_SIZE 82
#else
#define _BACKTRACE_FORMAT "%-4d%-35s 0x%08lx %s + %lu"
#define _BACKTRACE_FORMAT_SIZE 65
#endif


static int _backtrace_snprintf(char* buf, size_t size, int frame, const void* addr, const Dl_info* info) {
	char symbuf[19];
	const char* image = "???";
	const char* symbol = symbuf;

	if (info->dli_fname) {
		image = strrchr(info->dli_fname, '/') + 1;
		if (image == NULL) image = info->dli_fname;
	}
	
	if (info->dli_sname) {
		symbol = info->dli_sname;
	} else {
		snprintf(symbuf, sizeof(symbuf), "0x%lx", (uintptr_t)info->dli_saddr);
	}

	return snprintf(buf, size,
			_BACKTRACE_FORMAT,
			frame,
			image,
			(uintptr_t)addr,
			symbol,
			(uintptr_t)addr - (uintptr_t)info->dli_saddr) + 1;
}

char** backtrace_symbols(void* const* buffer, int size) {
	int i;
	size_t total_bytes;
	char** result;
	char** ptrs;
	intptr_t strs;
	Dl_info* info = calloc(size, sizeof (Dl_info));
	
	if (info == NULL) return NULL;
	
	// Compute the total size for the block that is returned.
	// The block will contain size number of pointers to the
	// symbol descriptions.

	total_bytes = sizeof(char*) * size;
	
	// Plus each symbol description
	for (i = 0 ; i < size; ++i) {
		dladdr(buffer[i], &info[i]);
		total_bytes += _BACKTRACE_FORMAT_SIZE + 1;
		if (info[i].dli_sname) total_bytes += strlen(info[i].dli_sname);
	}
	
	result = (char**)malloc(total_bytes);
	if (result == NULL) {
		free(info);
		return NULL;
	}
	
	// Fill in the array of pointers and append the strings for
	// each symbol description.
	
	ptrs = result;
	strs = ((intptr_t)result) + sizeof(char*) * size;
	
	for (i = 0; i < size; ++i) {
		ptrs[i] = (char*)strs;
		strs += _backtrace_snprintf((char*)strs, total_bytes, i, buffer[i], &info[i]);
	}
	
	free(info);
	
	return result;
}

void backtrace_symbols_fd(void* const* buffer, int size, int fd) {
	int i;
	char buf[BUFSIZ];
	Dl_info info;
	struct iovec iov[2];

	iov[0].iov_base = buf;

	iov[1].iov_base = "\n";
	iov[1].iov_len = 1;

	for (i = 0; i < size; ++i) {
		memset(&info, 0, sizeof(info));
		dladdr(buffer[i], &info);

		iov[0].iov_len = _backtrace_snprintf(buf, sizeof(buf), i, buffer[i], &info);
		
		writev(fd, iov, 2);
	}
}
