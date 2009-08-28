#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "fileIo.h"

int cspWriteFile(
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
	rtn = lseek(fd, 0, SEEK_SET);
	if(rtn < 0) {
		return errno;
	}
	rtn = write(fd, bytes, (size_t)numBytes);
	if(rtn != (int)numBytes) {
		if(rtn >= 0) {
			printf("writeFile: short write\n");
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
int cspReadFile(
	const char		*fileName,
	unsigned char	**bytes,		// mallocd and returned
	unsigned		*numBytes)		// returned
{
	int ourRtn = 0;
	int fd;
	char *buf;
	char *thisBuf;
	struct stat	sb;
	unsigned size;
	size_t toMove;
	ssize_t thisMoved;
	int irtn;
	off_t lrtn = 0;
	
	*numBytes = 0;
	*bytes = NULL;
	fd = open(fileName, O_RDONLY, 0);
	if(fd <= 0) {
		perror("open");
		return errno;
	}
	irtn = fstat(fd, &sb);
	if(irtn) {
		ourRtn = errno;
		if(ourRtn == 0) {
			fprintf(stderr, "***Bogus zero error on fstat\n");
			ourRtn = -1;
		}
		else {
			perror("fstat");
		}
		goto errOut;
	}
	size = sb.st_size;
	buf = thisBuf = (char *)malloc(size);
	if(buf == NULL) {
		ourRtn = ENOMEM;
		goto errOut;
	}
	lrtn = lseek(fd, 0, SEEK_SET);
	if(lrtn < 0) {
		ourRtn = errno;
		if(ourRtn == 0) {
			fprintf(stderr, "***Bogus zero error on lseek\n");
			ourRtn = -1;
		}
		else {
			perror("lseek");
		}
		goto errOut;
	}
	toMove = size;
	
	/*
	 * On ppc this read ALWAYS returns the entire file. On i386, not so. 
	 */
	do {
		thisMoved = read(fd, thisBuf, toMove);
		if(thisMoved == 0) {
			/* reading empty file: done */
			break;
		}
		else if(thisMoved < 0) {
			ourRtn = errno;
			perror("read");
			break;
		}
		size_t uThisMoved = (size_t)thisMoved;
		if(uThisMoved != toMove) {
			fprintf(stderr, "===Short read: asked for %ld, got %lu\n", 
				toMove, uThisMoved);
		}
		toMove  -= thisMoved;
		thisBuf += thisMoved;
	} while(toMove);
	
	if(ourRtn == 0) {
		*bytes = (unsigned char *)buf;
		*numBytes = size;
	}
errOut:
	close(fd);
	return ourRtn;
}
