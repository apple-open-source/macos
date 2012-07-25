/*
 * Copyright (c) 2005-2007,2010 Apple Inc. All Rights Reserved.
 */

#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "fileIo.h"

int writeFile(
	const char			*fileName,
	const unsigned char	*bytes,
	unsigned			numBytes)
{
	int		rtn;
	int 	fd;
	
	fd = open(fileName, O_RDWR | O_CREAT | O_TRUNC, 0600);
	if(fd <= 0) {
		return errno;
	}
	rtn = write(fd, bytes, (size_t)numBytes);
	if(rtn != (int)numBytes) {
		if(rtn >= 0) {
			fprintf(stderr, "writeFile: short write\n");
		}
		rtn = EIO;
	}
	else {
		rtn = 0;
	}
	close(fd);
	return rtn;
}
	
/*
 * Read entire file. 
 */
int readFile(
	const char		*fileName,
	unsigned char	**bytes,		// mallocd and returned
	unsigned		*numBytes)		// returned
{
	int rtn;
	int fd;
	char *buf;
	struct stat	sb;
	size_t size;
	
	*numBytes = 0;
	*bytes = NULL;
	fd = open(fileName, O_RDONLY, 0);
	if(fd <= 0) {
		return errno;
	}
	rtn = fstat(fd, &sb);
	if(rtn) {
		goto errOut;
	}
	size = (size_t) sb.st_size;
	buf = (char *)malloc(size);
	if(buf == NULL) {
		rtn = ENOMEM;
		goto errOut;
	}
	rtn = read(fd, buf, (size_t)size);
	if(rtn != (int)size) {
		if(rtn >= 0) {
			fprintf(stderr, "readFile: short read\n");
		}
		rtn = EIO;
	}
	else {
		rtn = 0;
		*bytes = (unsigned char *)buf;
		*numBytes = size;
	}
errOut:
	close(fd);
	return rtn;
}
