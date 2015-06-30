//
//  UtilMappedFile.cpp
//  CPPUtil
//
//  Created by James McIlree on 4/19/13.
//  Copyright (c) 2013 Apple. All rights reserved.
//

#include "CPPUtil.h"

#include <sys/mman.h>

BEGIN_UTIL_NAMESPACE

static int open_fd(const char* path, size_t& file_size)
{
	int fd = open(path, O_RDONLY, 0);
	if(fd >= 0) {
		struct stat data;
		if (fstat(fd, &data) == 0) {
			if (S_ISREG(data.st_mode)) {
				// Is it zero sized?
				if (data.st_size > 0) {
					file_size = (size_t)data.st_size;
					return fd;
				}
			}
		}
		close(fd);
	}

	return -1;
}

MappedFile::MappedFile(const char* path) :
	_address(NULL),
	_size(0)
{
        ASSERT(path, "Sanity");
	int fd = open_fd(path, _size);
	if (fd >= 0) {
		_address = (unsigned char*)mmap(NULL, _size, PROT_READ, MAP_FILE | MAP_SHARED, fd, 0);
		if (_address == (void*)-1) {
			_address = NULL;
		}
		close(fd);
	}
}

MappedFile::~MappedFile()
{
	if (_address != NULL) {
		munmap(_address, _size);
	}
}

END_UTIL_NAMESPACE
